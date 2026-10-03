#include "uvccameradialog.h"

#include "cvutils.h"

#include <QCloseEvent>
#include <QComboBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPixmap>
#include <QPushButton>
#include <QSpinBox>
#include <QTimer>
#include <QVBoxLayout>

namespace {
constexpr int kPreviewIntervalMs = 33;   // ~30 fps
constexpr int kMaxMissedFrames = 30;     // 长时间没有新帧则认为相机已断开
}

UvcCameraDialog::UvcCameraDialog(OVP::UvcCameraPlugin *plugin, QWidget *parent)
    : QDialog(parent)
    , m_plugin(plugin)
{
    setWindowTitle(QStringLiteral("UVC 摄像头 - 预览与设置"));
    resize(800, 640);
    setMinimumSize(640, 480);

    setupUi();

    m_previewTimer = new QTimer(this);
    m_previewTimer->setInterval(kPreviewIntervalMs);
    connect(m_previewTimer, &QTimer::timeout, this, &UvcCameraDialog::onPreviewTick);

    refreshCameraList();

    const int savedIndex = m_plugin ? m_plugin->paramInt(QStringLiteral("camera_index"), 0) : 0;
    for (int i = 0; i < m_cameraCombo->count(); ++i) {
        if (m_cameraCombo->itemData(i).toInt() == savedIndex) {
            m_cameraCombo->setCurrentIndex(i);
            break;
        }
    }

    setConnectedState(m_plugin && m_plugin->isCameraOpen());
    if (m_plugin && m_plugin->isCameraOpen())
        m_previewTimer->start();
}

UvcCameraDialog::~UvcCameraDialog()
{
    stopPreview();
}

// ------------------------------------------------------------------- 界面搭建

void UvcCameraDialog::setupUi()
{
    auto *mainLayout = new QVBoxLayout(this);

    // ---- 摄像头选择 ----
    auto *cameraGroup = new QGroupBox(QStringLiteral("摄像头选择"), this);
    auto *cameraLayout = new QHBoxLayout(cameraGroup);
    cameraLayout->addWidget(new QLabel(QStringLiteral("可用摄像头:"), cameraGroup));

    m_cameraCombo = new QComboBox(cameraGroup);
    m_cameraCombo->setMinimumWidth(300);
    cameraLayout->addWidget(m_cameraCombo, 1);

    m_refreshButton = new QPushButton(QStringLiteral("刷新列表"), cameraGroup);
    connect(m_refreshButton, &QPushButton::clicked, this, &UvcCameraDialog::onRefreshClicked);
    cameraLayout->addWidget(m_refreshButton);

    m_connectButton = new QPushButton(QStringLiteral("打开摄像头"), cameraGroup);
    connect(m_connectButton, &QPushButton::clicked, this, &UvcCameraDialog::onConnectClicked);
    cameraLayout->addWidget(m_connectButton);

    mainLayout->addWidget(cameraGroup);

    // ---- 采集参数 ----
    auto *paramGroup = new QGroupBox(QStringLiteral("采集参数（0 = 使用摄像头默认值）"), this);
    auto *paramLayout = new QFormLayout(paramGroup);

    m_widthSpin = new QSpinBox(paramGroup);
    m_widthSpin->setRange(0, 7680);
    m_widthSpin->setSingleStep(160);
    m_widthSpin->setValue(m_plugin ? m_plugin->paramInt(QStringLiteral("width"), 0) : 0);
    paramLayout->addRow(QStringLiteral("宽度:"), m_widthSpin);

    m_heightSpin = new QSpinBox(paramGroup);
    m_heightSpin->setRange(0, 4320);
    m_heightSpin->setSingleStep(120);
    m_heightSpin->setValue(m_plugin ? m_plugin->paramInt(QStringLiteral("height"), 0) : 0);
    paramLayout->addRow(QStringLiteral("高度:"), m_heightSpin);

    m_fpsSpin = new QSpinBox(paramGroup);
    m_fpsSpin->setRange(0, 120);
    m_fpsSpin->setValue(m_plugin ? m_plugin->paramInt(QStringLiteral("fps"), 0) : 0);
    paramLayout->addRow(QStringLiteral("帧率:"), m_fpsSpin);

    m_backendCombo = new QComboBox(paramGroup);
    m_backendCombo->addItems({QStringLiteral("自动"), QStringLiteral("MSMF"),
                              QStringLiteral("DirectShow"), QStringLiteral("V4L2"),
                              QStringLiteral("AVFoundation"), QStringLiteral("任意")});
    if (m_plugin) {
        const QString backend = m_plugin->paramString(QStringLiteral("backend"),
                                                      QStringLiteral("自动"));
        const int idx = m_backendCombo->findText(backend);
        if (idx >= 0)
            m_backendCombo->setCurrentIndex(idx);
    }
    paramLayout->addRow(QStringLiteral("后端:"), m_backendCombo);

    mainLayout->addWidget(paramGroup);

    // ---- 图像预览 ----
    auto *previewGroup = new QGroupBox(QStringLiteral("实时预览"), this);
    auto *previewLayout = new QVBoxLayout(previewGroup);

    m_previewLabel = new QLabel(QStringLiteral("未打开摄像头"), previewGroup);
    m_previewLabel->setAlignment(Qt::AlignCenter);
    m_previewLabel->setStyleSheet(
        QStringLiteral("background: #1e1e1e; border: 1px solid #3e3e42; color: #666; font-size: 14px;"));
    m_previewLabel->setMinimumSize(640, 360);
    previewLayout->addWidget(m_previewLabel);

    m_statusLabel = new QLabel(QStringLiteral("就绪"), previewGroup);
    m_statusLabel->setStyleSheet(QStringLiteral("color: #888; font-size: 11px;"));
    previewLayout->addWidget(m_statusLabel);

    mainLayout->addWidget(previewGroup, 1);

    // ---- 底部按钮 ----
    auto *buttonLayout = new QHBoxLayout();
    buttonLayout->addStretch();

    auto *okButton = new QPushButton(QStringLiteral("确定"), this);
    connect(okButton, &QPushButton::clicked, this, &UvcCameraDialog::onOkClicked);
    buttonLayout->addWidget(okButton);

    auto *cancelButton = new QPushButton(QStringLiteral("取消"), this);
    connect(cancelButton, &QPushButton::clicked, this, &UvcCameraDialog::reject);
    buttonLayout->addWidget(cancelButton);

    mainLayout->addLayout(buttonLayout);
}

void UvcCameraDialog::refreshCameraList()
{
    m_cameraCombo->clear();

    m_statusLabel->setText(QStringLiteral("正在扫描摄像头..."));
    m_statusLabel->repaint();

    QString error;
    const QList<UVC::CameraInfo> cameras = UVC::UvcGrabber::enumCameras(10, &error);
    for (const UVC::CameraInfo &info : cameras)
        m_cameraCombo->addItem(info.displayName(), info.index);

    if (cameras.isEmpty())
        m_statusLabel->setText(error.isEmpty() ? QStringLiteral("未检测到可用摄像头") : error);
    else
        m_statusLabel->setText(QStringLiteral("发现 %1 个摄像头").arg(cameras.size()));
}

void UvcCameraDialog::setConnectedState(bool connected)
{
    m_connectButton->setText(connected ? QStringLiteral("关闭摄像头") : QStringLiteral("打开摄像头"));
    m_connectButton->setStyleSheet(connected ? QStringLiteral("background: #c62828; color: #fff;")
                                             : QStringLiteral("background: #2e7d32; color: #fff;"));
    m_cameraCombo->setEnabled(!connected);
    m_refreshButton->setEnabled(!connected);
    m_backendCombo->setEnabled(!connected);
}

void UvcCameraDialog::stopPreview()
{
    if (m_previewTimer)
        m_previewTimer->stop();
    m_missedFrames = 0;
}

void UvcCameraDialog::applyUiToParams()
{
    if (!m_plugin)
        return;

    m_plugin->setParam(QStringLiteral("width"), m_widthSpin->value());
    m_plugin->setParam(QStringLiteral("height"), m_heightSpin->value());
    m_plugin->setParam(QStringLiteral("fps"), m_fpsSpin->value());
    m_plugin->setParam(QStringLiteral("backend"), m_backendCombo->currentText());
}

// ---------------------------------------------------------------------- 槽函数

void UvcCameraDialog::onRefreshClicked()
{
    refreshCameraList();
}

void UvcCameraDialog::onConnectClicked()
{
    if (!m_plugin)
        return;

    if (m_plugin->isCameraOpen()) {
        stopPreview();
        m_plugin->closeCamera();
        m_previewLabel->setPixmap(QPixmap());
        m_previewLabel->setText(QStringLiteral("未打开摄像头"));
        setConnectedState(false);
        m_statusLabel->setText(QStringLiteral("已关闭"));
        return;
    }

    if (m_cameraCombo->count() == 0) {
        QMessageBox::warning(this, QStringLiteral("提示"), QStringLiteral("未检测到可用摄像头"));
        return;
    }

    applyUiToParams();

    const int index = m_cameraCombo->currentData().toInt();
    m_statusLabel->setText(QStringLiteral("正在打开摄像头..."));
    m_statusLabel->repaint();

    if (!m_plugin->openCamera(index)) {
        const QString error = m_plugin->cameraError();
        QMessageBox::critical(this, QStringLiteral("打开失败"), error);
        m_statusLabel->setText(error);
        return;
    }

    stopPreview();
    m_previewTimer->start();
    setConnectedState(true);
    m_statusLabel->setText(QStringLiteral("已打开，后台线程正在连续取图"));
}

void UvcCameraDialog::onOkClicked()
{
    if (!m_plugin) {
        accept();
        return;
    }

    if (m_cameraCombo->currentIndex() >= 0)
        m_plugin->setParam(QStringLiteral("camera_index"), m_cameraCombo->currentData().toInt());
    applyUiToParams();

    accept();
}

void UvcCameraDialog::onPreviewTick()
{
    if (!m_plugin)
        return;

    if (!m_plugin->isCameraOpen()) {
        stopPreview();
        const QString error = m_plugin->cameraError();
        m_statusLabel->setText(error.isEmpty() ? QStringLiteral("摄像头已关闭") : error);
        m_previewLabel->setPixmap(QPixmap());
        m_previewLabel->setText(QStringLiteral("摄像头已关闭"));
        setConnectedState(false);
        return;
    }

    quint64 seq = 0;
    cv::Mat frame = m_plugin->previewFrame(&seq);
    if (frame.empty()) {
        if (++m_missedFrames < kMaxMissedFrames)
            return;

        stopPreview();
        const QString error = m_plugin->cameraError();
        m_statusLabel->setText(error.isEmpty() ? QStringLiteral("长时间未收到图像") : error);
        m_plugin->closeCamera();
        setConnectedState(false);
        return;
    }

    m_missedFrames = 0;

    const QPixmap pixmap = OVP::matToQPixmap(frame);
    if (pixmap.isNull())
        return;

    m_previewLabel->setPixmap(pixmap.scaled(m_previewLabel->size(), Qt::KeepAspectRatio,
                                            Qt::SmoothTransformation));

    m_statusLabel->setText(QStringLiteral("取图中  %1x%2  |  帧率 %3  |  帧号 %4")
                               .arg(frame.cols)
                               .arg(frame.rows)
                               .arg(m_plugin->grabber()->measuredFps(), 0, 'f', 1)
                               .arg(seq));
}

void UvcCameraDialog::closeEvent(QCloseEvent *event)
{
    // 只停预览计时器，摄像头保持打开，流程执行时可直接使用最新帧
    stopPreview();
    QDialog::closeEvent(event);
}
