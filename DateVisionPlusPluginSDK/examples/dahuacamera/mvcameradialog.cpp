#include "mvcameradialog.h"

#include "cvutils.h"

#include <QCloseEvent>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPixmap>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>

namespace {
constexpr int kPreviewIntervalMs = 33;   // ~30 fps
constexpr int kMaxMissedFrames = 30;     // 长时间没有新帧则认为相机已断开
}

MVCameraDialog::MVCameraDialog(OVP::MVCameraPlugin *plugin, QWidget *parent)
    : QDialog(parent)
    , m_plugin(plugin)
{
    setWindowTitle(QStringLiteral("大华相机 - 预览与设置"));
    resize(800, 600);
    setMinimumSize(640, 480);

    setupUi();

    if (m_plugin)
        m_fpsSpin->setValue(m_plugin->paramDouble(QStringLiteral("fps"), 0.0));

    m_previewTimer = new QTimer(this);
    m_previewTimer->setInterval(kPreviewIntervalMs);
    connect(m_previewTimer, &QTimer::timeout, this, &MVCameraDialog::onPreviewTick);

    // 已连接并正在拉流：不再探测相机，直接显示当前相机的实时图像
    const bool streaming = m_plugin && m_plugin->isConnected();
    if (streaming) {
        populateCurrentCamera();
        m_statusLabel->setText(QStringLiteral("已连接，后台线程正在连续拉流"));
    } else {
        refreshCameraList();
        const int savedIndex = m_plugin ? m_plugin->paramInt(QStringLiteral("camera_index"), 0) : 0;
        for (int i = 0; i < m_cameraCombo->count(); ++i) {
            if (m_cameraCombo->itemData(i).toInt() == savedIndex) {
                m_cameraCombo->setCurrentIndex(i);
                break;
            }
        }
    }

    setConnectedState(streaming);
    if (streaming)
        m_previewTimer->start();
}

MVCameraDialog::~MVCameraDialog()
{
    stopPreview();
}

// ------------------------------------------------------------------- 界面搭建

void MVCameraDialog::setupUi()
{
    auto *mainLayout = new QVBoxLayout(this);

    // ---- 相机选择 ----
    auto *cameraGroup = new QGroupBox(QStringLiteral("相机选择"), this);
    auto *cameraLayout = new QHBoxLayout(cameraGroup);
    cameraLayout->addWidget(new QLabel(QStringLiteral("可用相机:"), cameraGroup));

    m_cameraCombo = new QComboBox(cameraGroup);
    m_cameraCombo->setMinimumWidth(300);
    cameraLayout->addWidget(m_cameraCombo, 1);

    m_refreshButton = new QPushButton(QStringLiteral("刷新列表"), cameraGroup);
    connect(m_refreshButton, &QPushButton::clicked, this, &MVCameraDialog::onRefreshClicked);
    cameraLayout->addWidget(m_refreshButton);

    m_connectButton = new QPushButton(QStringLiteral("连接"), cameraGroup);
    connect(m_connectButton, &QPushButton::clicked, this, &MVCameraDialog::onConnectClicked);
    cameraLayout->addWidget(m_connectButton);

    mainLayout->addWidget(cameraGroup);

    // ---- 取流参数 ----
    auto *streamGroup = new QGroupBox(QStringLiteral("取流参数"), this);
    auto *streamLayout = new QHBoxLayout(streamGroup);
    streamLayout->addWidget(new QLabel(QStringLiteral("取流帧率:"), streamGroup));

    m_fpsSpin = new QDoubleSpinBox(streamGroup);
    m_fpsSpin->setRange(0.0, 1000.0);
    m_fpsSpin->setDecimals(1);
    m_fpsSpin->setSingleStep(1.0);
    m_fpsSpin->setSpecialValueText(QStringLiteral("不限（相机最大）"));
    m_fpsSpin->setSuffix(QStringLiteral(" fps"));
    m_fpsSpin->setMinimumWidth(180);
    m_fpsSpin->setToolTip(QStringLiteral("0 = 关闭帧率控制，按相机最大能力出图"));
    streamLayout->addWidget(m_fpsSpin);

    auto *streamHint = new QLabel(QStringLiteral("修改后需重新连接相机生效"), streamGroup);
    streamHint->setStyleSheet(QStringLiteral("color: #888; font-size: 11px;"));
    streamLayout->addWidget(streamHint);
    streamLayout->addStretch();

    mainLayout->addWidget(streamGroup);

    // ---- 图像预览 ----
    auto *previewGroup = new QGroupBox(QStringLiteral("实时预览"), this);
    auto *previewLayout = new QVBoxLayout(previewGroup);

    m_previewLabel = new QLabel(QStringLiteral("未连接相机"), previewGroup);
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
    connect(okButton, &QPushButton::clicked, this, &MVCameraDialog::onOkClicked);
    buttonLayout->addWidget(okButton);

    auto *cancelButton = new QPushButton(QStringLiteral("取消"), this);
    connect(cancelButton, &QPushButton::clicked, this, &MVCameraDialog::reject);
    buttonLayout->addWidget(cancelButton);

    mainLayout->addLayout(buttonLayout);
}

void MVCameraDialog::refreshCameraList()
{
    m_cameraCombo->clear();

    const QString sdkPath = m_plugin ? m_plugin->paramString(QStringLiteral("sdk_path")) : QString();

    m_statusLabel->setText(QStringLiteral("正在探测相机..."));
    m_statusLabel->repaint();

    QString error;
    const QList<DahuaMV::DeviceInfo> devices = DahuaMV::DahuaCamera::enumDevices(&error, sdkPath);
    for (const DahuaMV::DeviceInfo &device : devices)
        m_cameraCombo->addItem(device.displayName(), device.index);

    if (!error.isEmpty()) {
        m_statusLabel->setText(error);
    } else {
        m_statusLabel->setText(QStringLiteral("发现 %1 台相机").arg(devices.size()));
        DahuaMV::ImvApi *api = DahuaMV::ImvApi::instance();
        const QString version = api->version();
        if (!version.isEmpty())
            m_statusLabel->setText(m_statusLabel->text() + QStringLiteral("  |  SDK %1").arg(version));
    }
}

void MVCameraDialog::populateCurrentCamera()
{
    m_cameraCombo->clear();
    if (!m_plugin)
        return;

    const int index = m_plugin->paramInt(QStringLiteral("camera_index"), 0);
    m_cameraCombo->addItem(QStringLiteral("[%1] 当前相机（已连接）").arg(index), index);
    m_cameraCombo->setCurrentIndex(0);
}

void MVCameraDialog::setConnectedState(bool connected)
{
    m_connectButton->setText(connected ? QStringLiteral("断开") : QStringLiteral("连接"));
    m_connectButton->setStyleSheet(connected ? QStringLiteral("background: #c62828; color: #fff;")
                                             : QStringLiteral("background: #2e7d32; color: #fff;"));
    m_cameraCombo->setEnabled(!connected);
    m_refreshButton->setEnabled(!connected);
}

void MVCameraDialog::stopPreview()
{
    if (m_previewTimer)
        m_previewTimer->stop();
    m_missedFrames = 0;
    m_lastShownSeq = 0;
}

// ---------------------------------------------------------------------- 槽函数

void MVCameraDialog::onRefreshClicked()
{
    refreshCameraList();
}

void MVCameraDialog::onConnectClicked()
{
    if (!m_plugin)
        return;

    // —— 断开：停止后台拉流并关闭相机 ——
    if (m_plugin->isConnected()) {
        stopPreview();
        m_plugin->disconnectCamera();
        m_previewLabel->setPixmap(QPixmap());
        m_previewLabel->setText(QStringLiteral("未连接相机"));
        setConnectedState(false);
        refreshCameraList();   // 已断开，重新探测相机
        m_statusLabel->setText(QStringLiteral("已断开"));
        return;
    }

    // —— 连接：建立长连接，后台线程持续拉流 ——
    if (m_cameraCombo->count() == 0) {
        QMessageBox::warning(this, QStringLiteral("提示"), QStringLiteral("请先选择一台相机"));
        return;
    }

    // 帧率在打开相机时下发，重新连接即可生效
    m_plugin->setParam(QStringLiteral("fps"), m_fpsSpin->value());

    const int index = m_cameraCombo->currentData().toInt();
    m_statusLabel->setText(QStringLiteral("正在连接相机..."));
    m_statusLabel->repaint();

    if (!m_plugin->connectCamera(index)) {
        const QString error = m_plugin->cameraError();
        QMessageBox::critical(this, QStringLiteral("连接失败"), error);
        m_statusLabel->setText(error);
        return;
    }

    stopPreview();
    m_previewTimer->start();
    setConnectedState(true);

    const QString warning = m_plugin->cameraWarning();
    m_statusLabel->setText(warning.isEmpty()
                               ? QStringLiteral("已连接，后台线程正在连续拉流")
                               : QStringLiteral("已连接 | %1").arg(warning));
}

void MVCameraDialog::onOkClicked()
{
    if (m_plugin) {
        if (m_cameraCombo->currentIndex() >= 0)
            m_plugin->setParam(QStringLiteral("camera_index"),
                               m_cameraCombo->currentData().toInt());
        m_plugin->setParam(QStringLiteral("fps"), m_fpsSpin->value());
    }
    accept();
}

void MVCameraDialog::onPreviewTick()
{
    if (!m_plugin)
        return;

    if (!m_plugin->isConnected()) {
        stopPreview();
        const QString error = m_plugin->cameraError();
        m_statusLabel->setText(error.isEmpty() ? QStringLiteral("相机已断开") : error);
        m_previewLabel->setPixmap(QPixmap());
        m_previewLabel->setText(QStringLiteral("相机已断开"));
        setConnectedState(false);
        return;
    }

    // 只读后台线程写入的最新帧变量，不再直接调用 SDK 取帧
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

    // 同一帧不重复转 QPixmap
    if (seq == m_lastShownSeq)
        return;
    m_lastShownSeq = seq;

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

void MVCameraDialog::closeEvent(QCloseEvent *event)
{
    // 只停预览计时器，长连接与后台拉流保持不变，流程执行时直接使用最新帧
    stopPreview();
    QDialog::closeEvent(event);
}
