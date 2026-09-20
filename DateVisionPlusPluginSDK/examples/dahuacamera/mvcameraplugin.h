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

class QDialog;
class QWidget;

namespace OVP {

/**
 * @brief 大华相机插件（对应 Python 版 MVCameraPlugin）
 *
 * 通过 MVSDK 连接大华工业相机并采集图像；双击节点可打开预览/设置对话框。
 * 相机句柄常驻，运行期意外断线后 execute() 会自动重连。
 */
class MVCameraPlugin : public PluginBase
{
    Q_OBJECT

public:
    explicit MVCameraPlugin(QObject *parent = nullptr);
    ~MVCameraPlugin() override;

    QString id() const override { return QStringLiteral("mv_camera"); }
    QString name() const override { return QStringLiteral("大华相机"); }
    QString category() const override { return QStringLiteral("输入输出"); }
    QString description() const override
    {
        return QStringLiteral("连接大华工业相机，通过 MVSDK 采集图像");
    }

    QList<PortDef> inputPorts() const override { return {}; }
    QList<PortDef> outputPorts() const override;
    QList<ParamDef> inputParams() const override;

    bool execute() override;

    QDialog *createDialog(const cv::Mat &inputImage, QWidget *parent) override;

    // ---- 供对话框调用 ----
    DahuaMV::DahuaCamera *camera() { return &m_camera; }
    bool isConnected() const;
    /** 打开相机并开始连续取流（预览用）；index < 0 表示沿用参数中的序号 */
    bool connectCamera(int index = -1);
    /** 停止取流并关闭相机 */
    void disconnectCamera();
    /** 取一帧；timeoutMs < 0 时使用参数「取帧超时」 */
    cv::Mat grabFrame(int timeoutMs = -1);
    QString cameraError() const { return m_camera.lastError(); }

private:
    void ensureSdkLoaded();

    DahuaMV::DahuaCamera m_camera;
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
