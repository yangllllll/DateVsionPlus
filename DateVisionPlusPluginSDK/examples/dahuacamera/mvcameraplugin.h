#ifndef MVCAMERAPLUGIN_H
#define MVCAMERAPLUGIN_H

// ---- 主程序 SDK ----
#include "pluginbase.h"
#include "plugininterface.h"

// ---- 大华 MVSDK 封装 ----
#include "dahuaimvsdk.h"

#include <QList>
#include <QObject>
#include <QString>
#include <QtGlobal>

class QDialog;
class QWidget;

namespace OVP {

/**
 * @brief 大华相机插件（对应 Python 版 MVCameraPlugin）
 *
 * 与 UVC 插件采用同一取图模型：在对话框点击「连接」后，
 * DahuaGrabber 在独立线程里长连接连续拉流，把最新帧写入内部变量；
 * execute() 只读该变量，不做任何开关流动作。
 */
class MVCameraPlugin : public PluginBase
{
    Q_OBJECT

public:
    explicit MVCameraPlugin(QObject *parent = nullptr);
    ~MVCameraPlugin() override;

    QString id() const override { return QStringLiteral("mv_camera"); }
    QString name() const override { return QStringLiteral("GigE相机"); }
    QString category() const override { return QStringLiteral("输入输出"); }
    QString description() const override
    {
        return QStringLiteral("连接GigE相机，后台线程连续拉流，执行时输出最新帧");
    }

    QList<PortDef> inputPorts() const override { return {}; }
    QList<PortDef> outputPorts() const override;
    QList<ParamDef> inputParams() const override;

    bool execute() override;

    QDialog *createDialog(const cv::Mat &inputImage, QWidget *parent) override;

    // ---- 供对话框调用 ----
    DahuaMV::DahuaGrabber *grabber() { return &m_grabber; }
    bool isConnected() const;
    /** 打开相机并开始后台连续取流（长连接）；index < 0 表示沿用参数中的序号 */
    bool connectCamera(int index = -1);
    /** 停止取流并关闭相机（内部/异常路径使用，不影响自动重连策略） */
    void closeCamera();
    /** 用户在弹窗中主动断开：停止取流，并且执行时不再自动重连 */
    void disconnectCamera();
    /** 直接取后台线程里的最新帧（不等待） */
    cv::Mat previewFrame(quint64 *seq = nullptr);
    QString cameraError() const { return m_grabber.error(); }
    /** 非致命提示，例如相机不支持设置取流帧率 */
    QString cameraWarning() const { return m_grabber.warning(); }

private:
    void ensureSdkLoaded();
    bool ensureStarted(QString *error);

    DahuaMV::DahuaGrabber m_grabber;
    quint64 m_lastSeq = 0;
    /** 用户在弹窗中主动断开后，执行时不再自动重连 */
    bool m_autoStart = true;
};

} // namespace OVP

/**
 * @brief 插件提供者：一个 DLL 可导出多个工具
 */
class DahuaCameraPluginProvider : public QObject, public OpenVisionPluginProvider
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID OpenVisionPluginProvider_iid)
    Q_INTERFACES(OpenVisionPluginProvider)

public:
    QList<Entry> availablePlugins() const override;
};

#endif // MVCAMERAPLUGIN_H
