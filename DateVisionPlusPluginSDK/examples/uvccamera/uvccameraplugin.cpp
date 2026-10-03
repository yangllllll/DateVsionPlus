#include "uvccameraplugin.h"
#include "uvccameradialog.h"

namespace OVP {
namespace {

UVC::Backend backendFromText(const QString &text)
{
    if (text.contains(QStringLiteral("MSMF"), Qt::CaseInsensitive))
        return UVC::Backend::MSMF;
    if (text.contains(QStringLiteral("DirectShow"), Qt::CaseInsensitive))
        return UVC::Backend::DirectShow;
    if (text.contains(QStringLiteral("V4L2"), Qt::CaseInsensitive))
        return UVC::Backend::V4L2;
    if (text.contains(QStringLiteral("AVFoundation"), Qt::CaseInsensitive))
        return UVC::Backend::AVFoundation;
    if (text == QStringLiteral("任意") || text.compare(QStringLiteral("Any"), Qt::CaseInsensitive) == 0)
        return UVC::Backend::Any;
    return UVC::Backend::Auto;
}

} // namespace

static PluginBase *createUvcCameraPlugin(QObject *parent)
{
    return new UvcCameraPlugin(parent);
}

// ------------------------------------------------------------- UvcCameraPlugin

UvcCameraPlugin::UvcCameraPlugin(QObject *parent)
    : PluginBase(parent)
{
}

UvcCameraPlugin::~UvcCameraPlugin()
{
    m_grabber.stopGrab();
}

QList<PortDef> UvcCameraPlugin::outputPorts() const
{
    return {port(QStringLiteral("output"), PortType::Image, QStringLiteral("采集图像"))};
}

QList<ParamDef> UvcCameraPlugin::inputParams() const
{
    return {
        intParam(QStringLiteral("camera_index"), QStringLiteral("摄像头序号"), 0, 0, 20, 1,
                 QStringLiteral("摄像头列表中的序号")),
        intParam(QStringLiteral("width"), QStringLiteral("图像宽度"), 0, 0, 7680, 1,
                 QStringLiteral("0 = 使用摄像头默认分辨率")),
        intParam(QStringLiteral("height"), QStringLiteral("图像高度"), 0, 0, 4320, 1,
                 QStringLiteral("0 = 使用摄像头默认分辨率")),
        intParam(QStringLiteral("fps"), QStringLiteral("帧率"), 0, 0, 120, 1,
                 QStringLiteral("0 = 使用摄像头默认帧率")),
        choiceParam(QStringLiteral("backend"), QStringLiteral("采集后端"), QStringLiteral("自动"),
                    {QStringLiteral("自动"), QStringLiteral("MSMF"), QStringLiteral("DirectShow"),
                     QStringLiteral("V4L2"), QStringLiteral("AVFoundation"), QStringLiteral("任意")},
                    QStringLiteral("OpenCV VideoCapture 后端，默认按平台自动选择")),
        boolParam(QStringLiteral("wait_new_frame"), QStringLiteral("等待新帧"), true,
                  QStringLiteral("开启后每次执行都等待一帧新图（不会取到同一帧）；"
                                 "关闭则直接输出后台线程当前最新帧，速度最快")),
        intParam(QStringLiteral("timeout"), QStringLiteral("取帧超时(ms)"), 2000, 100, 30000, 100,
                 QStringLiteral("等待新帧的最长时间，超时则本次执行失败"))
    };
}

UVC::CameraConfig UvcCameraPlugin::configFromParams() const
{
    UVC::CameraConfig cfg;
    cfg.index = paramInt(QStringLiteral("camera_index"), 0);
    cfg.width = paramInt(QStringLiteral("width"), 0);
    cfg.height = paramInt(QStringLiteral("height"), 0);
    cfg.fps = paramInt(QStringLiteral("fps"), 0);
    cfg.backend = backendFromText(paramString(QStringLiteral("backend"), QStringLiteral("自动")));
    return cfg;
}

bool UvcCameraPlugin::ensureStarted(QString *error)
{
    m_grabber.setConfig(configFromParams());
    return m_grabber.startGrab(error);
}

bool UvcCameraPlugin::execute()
{
    clearError();

    // 摄像头未打开（例如没打开过对话框）时按需自动打开，之后线程常驻
    QString error;
    if (!m_grabber.isGrabbing() && !ensureStarted(&error)) {
        setError(QStringLiteral("摄像头打开失败: %1").arg(error));
        return false;
    }

    const int timeout = paramInt(QStringLiteral("timeout"), 2000);
    const bool waitNew = paramBool(QStringLiteral("wait_new_frame"), true);

    cv::Mat frame;
    if (waitNew) {
        // 只等「比上次更新」的那一帧，杜绝过时图像
        if (!m_grabber.waitFrame(frame, timeout, m_lastSeq, &error)) {
            setError(QStringLiteral("取帧失败: %1").arg(error));
            return false;
        }
    } else if (!m_grabber.lastFrame(frame, nullptr)) {
        // 后台线程还没出图，给一次带超时的机会
        if (!m_grabber.waitFrame(frame, timeout, 0, &error)) {
            setError(QStringLiteral("取帧失败: %1").arg(error));
            return false;
        }
    }

    if (frame.empty()) {
        setError(QStringLiteral("获取图像失败"));
        return false;
    }

    m_lastSeq = m_grabber.frameSeq();
    setOutput(QStringLiteral("output"), imageValue(frame));

    return true;
}

QDialog *UvcCameraPlugin::createDialog(const cv::Mat &inputImage, QWidget *parent)
{
    Q_UNUSED(inputImage);
    return new UvcCameraDialog(this, parent);
}

// -------------------------------------------------- 供对话框调用的连接管理

bool UvcCameraPlugin::isCameraOpen() const
{
    return m_grabber.isGrabbing();
}

bool UvcCameraPlugin::openCamera(int index)
{
    if (index >= 0)
        setParam(QStringLiteral("camera_index"), index);

    // 参数可能变化（分辨率 / 后端 / 序号），统一重启取图线程
    m_grabber.stopGrab();

    QString error;
    m_grabber.setConfig(configFromParams());
    if (!m_grabber.startGrab(&error)) {
        m_grabber.stopGrab();
        return false;
    }

    m_lastSeq = 0;
    return true;
}

void UvcCameraPlugin::closeCamera()
{
    m_grabber.stopGrab();
}

cv::Mat UvcCameraPlugin::previewFrame(quint64 *seq)
{
    cv::Mat frame;
    if (!m_grabber.lastFrame(frame, seq))
        return cv::Mat();
    return frame;
}

} // namespace OVP

// ------------------------------------------------------------------ DLL 导出

QList<OpenVisionPluginProvider::Entry> UvcCameraPluginProvider::availablePlugins() const
{
    return {OpenVisionPluginProvider::Entry{OVP::createUvcCameraPlugin}};
}
