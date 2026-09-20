#include "mvcameraplugin.h"
#include "mvcameradialog.h"

namespace OVP {

static PluginBase *createMVCameraPlugin(QObject *parent)
{
    return new MVCameraPlugin(parent);
}

// ------------------------------------------------------------- MVCameraPlugin

MVCameraPlugin::MVCameraPlugin(QObject *parent)
    : PluginBase(parent)
{
}

MVCameraPlugin::~MVCameraPlugin()
{
    m_camera.close();
}

QList<PortDef> MVCameraPlugin::outputPorts() const
{
    return {port(QStringLiteral("output"), PortType::Image, QStringLiteral("采集图像"))};
}

QList<ParamDef> MVCameraPlugin::inputParams() const
{
    return {
        intParam(QStringLiteral("camera_index"), QStringLiteral("相机序号"), 0, 0, 99, 1,
                 QStringLiteral("相机列表中的序号")),
        intParam(QStringLiteral("timeout"), QStringLiteral("取帧超时(ms)"), 2000, 100, 10000, 100,
                 QStringLiteral("获取帧的超时时间")),
        stringParam(QStringLiteral("sdk_path"), QStringLiteral("MVSDK 路径"), QString(),
                    QStringLiteral("MVSDKmd.dll 的完整路径；留空则自动探测，"
                                   "也可用环境变量 DAHUA_MVSDK_PATH 指定"))
    };
}

void MVCameraPlugin::ensureSdkLoaded()
{
    DahuaMV::ImvApi *api = DahuaMV::ImvApi::instance();
    if (api->isLoaded())
        return;
    QString error;
    api->load(paramString(QStringLiteral("sdk_path")), &error);
}

bool MVCameraPlugin::execute()
{
    clearError();
    ensureSdkLoaded();

    if (!DahuaMV::ImvApi::instance()->isLoaded()) {
        setError(DahuaMV::ImvApi::instance()->lastError());
        return false;
    }

    // 未连接则自动重连（首次运行或意外断联后），最多尝试 3 次
    const int index = paramInt(QStringLiteral("camera_index"), 0);
    QString error;
    for (int attempt = 0; attempt < 3 && !m_camera.isOpen(); ++attempt)
        m_camera.open(index, &error);

    if (!m_camera.isOpen()) {
        setError(QStringLiteral("相机连接失败: %1").arg(error));
        return false;
    }

    cv::Mat frame;
    const int timeout = paramInt(QStringLiteral("timeout"), 2000);
    if (!m_camera.grab(frame, static_cast<unsigned int>(timeout), &error)) {
        setError(QStringLiteral("取帧失败: %1").arg(error));
        // 连接异常，关闭以允许下次自动重连
        m_camera.close();
        return false;
    }

    if (frame.empty()) {
        setError(QStringLiteral("获取图像失败"));
        return false;
    }

    setOutput(QStringLiteral("output"), imageValue(frame));

    return true;
}

QDialog *MVCameraPlugin::createDialog(const cv::Mat &inputImage, QWidget *parent)
{
    Q_UNUSED(inputImage);
    return new MVCameraDialog(this, parent);
}

// -------------------------------------------------- 供对话框调用的连接管理

bool MVCameraPlugin::isConnected() const
{
    return m_camera.isOpen();
}

bool MVCameraPlugin::connectCamera(int index)
{
    ensureSdkLoaded();

    if (index >= 0)
        setParam(QStringLiteral("camera_index"), index);

    QString error;
    if (!m_camera.open(paramInt(QStringLiteral("camera_index"), 0), &error)) {
        m_camera.close();
        return false;
    }

    // 对话框预览需要连续取流
    if (!m_camera.startGrabbing(&error)) {
        m_camera.close();
        return false;
    }

    return true;
}

void MVCameraPlugin::disconnectCamera()
{
    m_camera.close();
}

cv::Mat MVCameraPlugin::grabFrame(int timeoutMs)
{
    if (timeoutMs < 0)
        timeoutMs = paramInt(QStringLiteral("timeout"), 2000);

    cv::Mat frame;
    QString error;
    if (!m_camera.grab(frame, static_cast<unsigned int>(timeoutMs), &error))
        return cv::Mat();

    return frame;
}

} // namespace OVP

// ------------------------------------------------------------------ DLL 导出

QList<OpenVisionPluginProvider::Entry> DahuaCameraPluginProvider::availablePlugins() const
{
    return {OpenVisionPluginProvider::Entry{OVP::createMVCameraPlugin}};
}
