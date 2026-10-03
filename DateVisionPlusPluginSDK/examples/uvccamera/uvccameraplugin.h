#ifndef UVCCAMERAPLUGIN_H
#define UVCCAMERAPLUGIN_H

// ---- 主程序 SDK ----
#include "pluginbase.h"
#include "plugininterface.h"

// ---- UVC 采集封装 ----
#include "uvccapture.h"

#include <QList>
#include <QObject>
#include <QString>
#include <QtGlobal>

class QDialog;
class QWidget;

namespace OVP {

/**
 * @brief UVC 摄像头插件
 *
 * 在对话框里「打开摄像头」后，后台线程持续取图并写入最新帧变量；
 * 流程执行时不做任何开关流动作，直接输出该变量中的帧，
 * 因此既快又不会拿到过时图像。
 */
class UvcCameraPlugin : public PluginBase
{
    Q_OBJECT

public:
    explicit UvcCameraPlugin(QObject *parent = nullptr);
    ~UvcCameraPlugin() override;

    QString id() const override { return QStringLiteral("uvc_camera"); }
    QString name() const override { return QStringLiteral("UVC 摄像头"); }
    QString category() const override { return QStringLiteral("输入输出"); }
    QString description() const override
    {
        return QStringLiteral("打开标准 UVC 摄像头，后台线程持续取图，执行时输出最新帧");
    }

    QList<PortDef> inputPorts() const override { return {}; }
    QList<PortDef> outputPorts() const override;
    QList<ParamDef> inputParams() const override;

    bool execute() override;

    QDialog *createDialog(const cv::Mat &inputImage, QWidget *parent) override;

    // ---- 供对话框调用 ----
    UVC::UvcGrabber *grabber() { return &m_grabber; }
    bool isCameraOpen() const;
    /** 按当前参数打开摄像头并启动后台取图线程；index < 0 表示沿用参数中的序号 */
    bool openCamera(int index = -1);
    /** 停止后台取图并关闭摄像头 */
    void closeCamera();
    /** 直接取后台线程里的最新帧（不等待） */
    cv::Mat previewFrame(quint64 *seq = nullptr);
    QString cameraError() const { return m_grabber.error(); }

private:
    UVC::CameraConfig configFromParams() const;
    bool ensureStarted(QString *error);

    UVC::UvcGrabber m_grabber;
    quint64 m_lastSeq = 0;
};

} // namespace OVP

/**
 * @brief 插件提供者：一个 DLL 可导出多个工具
 */
class UvcCameraPluginProvider : public QObject, public OpenVisionPluginProvider
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID OpenVisionPluginProvider_iid)
    Q_INTERFACES(OpenVisionPluginProvider)

public:
    QList<Entry> availablePlugins() const override;
};

#endif // UVCCAMERAPLUGIN_H
