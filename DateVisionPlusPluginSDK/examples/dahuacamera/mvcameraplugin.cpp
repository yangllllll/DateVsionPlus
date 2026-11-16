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
    m_grabber.stopGrab();
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
        floatParam(QStringLiteral("fps"), QStringLiteral("取流帧率(fps)"), 0.0, 0.0, 1000.0, 1.0,
                   QStringLiteral("相机出图帧率；0 = 不限制（按相机最大能力出图）。"
                                  "修改后需重新连接相机生效")),
        boolParam(QStringLiteral("wait_new_frame"), QStringLiteral("等待新帧"), true,
                  QStringLiteral("开启后每次执行都等待一帧新图（不会取到同一帧）；"
                                 "关闭则直接输出后台线程当前最新帧，速度最快")),
        intParam(QStringLiteral("timeout"), QStringLiteral("取帧超时(ms)"), 2000, 100, 30000, 100,
                 QStringLiteral("等待新帧的最长时间，超时则本次执行失败")),
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

bool MVCameraPlugin::ensureStarted(QString *error)
{
    m_grabber.setOpenOptions(paramInt(QStringLiteral("camera_index"), 0),
                             paramString(QStringLiteral("sdk_path")),
                             paramDouble(QStringLiteral("fps"), 0.0));
    return m_grabber.startGrab(error);
}

bool MVCameraPlugin::execute()
{
    clearError();
    ensureSdkLoaded();

    if (!DahuaMV::ImvApi::instance()->isLoaded()) {
        setError(DahuaMV::ImvApi::instance()->lastError());
        return false;
    }

    // 未连接时：从未主动断开过则自动建立长连接；用户主动断开过则不再自动重连
    QString error;
    if (!m_grabber.isGrabbing()) {
        if (!m_autoStart) {
            setError(QStringLiteral("相机未连接，请先在插件窗口中连接相机"));
            return false;
        }
        if (!ensureStarted(&error)) {
            setError(QStringLiteral("相机连接失败: %1").arg(error));
            return false;
        }
    }

    const int timeout = paramInt(QStringLiteral("timeout"), 2000);
    const bool waitNew = paramBool(QStringLiteral("wait_new_frame"), true);

    cv::Mat frame;
    quint64 frameSeq = 0;
    if (waitNew) {
        // 只等「比上次更新」的那一帧，杜绝过时图像
        if (!m_grabber.waitFrame(frame, timeout, m_lastSeq, &error, &frameSeq)) {
            setError(QStringLiteral("取帧失败: %1").arg(error));
            return false;
        }
    } else if (!m_grabber.lastFrame(frame, &frameSeq)) {
        if (!m_grabber.waitFrame(frame, timeout, 0, &error, &frameSeq)) {
            setError(QStringLiteral("取帧失败: %1").arg(error));
            return false;
        }
    }

    if (frame.empty()) {
        setError(QStringLiteral("获取图像失败"));
        return false;
    }

    m_lastSeq = frameSeq;
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
    return m_grabber.isGrabbing();
}

bool MVCameraPlugin::connectCamera(int index)
{
    ensureSdkLoaded();

    if (index >= 0)
        setParam(QStringLiteral("camera_index"), index);

    // 参数（序号 / SDK 路径）可能变化，统一重启取图线程
    m_grabber.stopGrab();

    QString error;
    if (!ensureStarted(&error)) {
        m_grabber.stopGrab();
        return false;
    }

    m_lastSeq = 0;
    m_autoStart = true;
    return true;
}

void MVCameraPlugin::closeCamera()
{
    m_grabber.stopGrab();
}

void MVCameraPlugin::disconnectCamera()
{
    m_autoStart = false;
    m_grabber.stopGrab();
}

cv::Mat MVCameraPlugin::previewFrame(quint64 *seq)
{
    cv::Mat frame;
    if (!m_grabber.lastFrame(frame, seq))
        return cv::Mat();
    return frame;
}

} // namespace OVP

// ------------------------------------------------------------------ DLL 导出

QList<OpenVisionPluginProvider::Entry> DahuaCameraPluginProvider::availablePlugins() const
{
    return {OpenVisionPluginProvider::Entry{OVP::createMVCameraPlugin}};
}
