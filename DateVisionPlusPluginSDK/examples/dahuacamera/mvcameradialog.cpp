#include "mvcameradialog.h"

#include "cvutils.h"

#include <QCloseEvent>
#include <QComboBox>
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
constexpr unsigned int kPreviewTimeoutMs = 100;
constexpr int kMaxConsecutiveFailures = 3;
}

MVCameraDialog::MVCameraDialog(OVP::MVCameraPlugin *plugin, QWidget *parent)
    : QDialog(parent)
    , m_plugin(plugin)
{
    setWindowTitle(QStringLiteral("大华相机 - 预览与设置"));
    resize(800, 600);
    setMinimumSize(640, 480);

    setupUi();

    m_previewTimer = new QTimer(this);
    m_previewTimer->setInterval(kPreviewIntervalMs);
    connect(m_previewTimer, &QTimer::timeout, this, &MVCameraDialog::onPreviewTick);

    refreshCameraList();

    const int savedIndex = m_plugin ? m_plugin->paramInt(QStringLiteral("camera_index"), 0) : 0;
    if (savedIndex >= 0 && savedIndex < m_cameraCombo->count())
        m_cameraCombo->setCurrentIndex(savedIndex);

    setConnectedState(m_plugin && m_plugin->isConnected());
    if (m_plugin && m_plugin->isConnected())
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
    m_consecutiveFailures = 0;
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

    if (m_plugin->isConnected()) {
        stopPreview();
        m_plugin->disconnectCamera();
        m_previewLabel->setPixmap(QPixmap());
        m_previewLabel->setText(QStringLiteral("未连接相机"));
        setConnectedState(false);
        m_statusLabel->setText(QStringLiteral("已断开"));
        return;
    }

    if (m_cameraCombo->count() == 0) {
        QMessageBox::warning(this, QStringLiteral("提示"), QStringLiteral("请先选择一台相机"));
        return;
    }

    const int index = m_cameraCombo->currentData().toInt();
    if (!m_plugin->connectCamera(index)) {
        QMessageBox::critical(this, QStringLiteral("连接失败"), m_plugin->cameraError());
        m_statusLabel->setText(m_plugin->cameraError());
        return;
    }

    stopPreview();
    m_previewTimer->start();
    setConnectedState(true);
    m_statusLabel->setText(QStringLiteral("已连接"));
}

void MVCameraDialog::onOkClicked()
{
    if (m_plugin && m_cameraCombo->currentIndex() >= 0)
        m_plugin->setParam(QStringLiteral("camera_index"), m_cameraCombo->currentData().toInt());
    accept();
}

void MVCameraDialog::onPreviewTick()
{
    if (!m_plugin || !m_plugin->isConnected())
        return;

    DahuaMV::DahuaCamera *camera = m_plugin->camera();
    cv::Mat frame;
    QString error;
    if (!camera->getFrame(frame, kPreviewTimeoutMs, &error)) {
        // 偶发超时不立即断开，连续失败才认为连接已丢失
        if (++m_consecutiveFailures < kMaxConsecutiveFailures)
            return;

        stopPreview();
        m_statusLabel->setText(QStringLiteral("取帧失败: %1，连接已断开").arg(error));
        m_previewLabel->setText(QStringLiteral("连接已断开"));
        m_previewLabel->setPixmap(QPixmap());
        m_plugin->disconnectCamera();
        setConnectedState(false);
        return;
    }

    m_consecutiveFailures = 0;
    if (frame.empty())
        return;

    const QPixmap pixmap = OVP::matToQPixmap(frame);
    if (pixmap.isNull())
        return;

    m_previewLabel->setPixmap(pixmap.scaled(m_previewLabel->size(), Qt::KeepAspectRatio,
                                            Qt::SmoothTransformation));
}

void MVCameraDialog::closeEvent(QCloseEvent *event)
{
    stopPreview();
    if (m_plugin)
        m_plugin->disconnectCamera();
    QDialog::closeEvent(event);
}
