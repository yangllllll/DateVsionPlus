#ifndef YOLODETECTPLUGIN_H
#define YOLODETECTPLUGIN_H

#include "pluginbase.h"
#include "plugininterface.h"

#include "yolodetector.h"

#include <QList>
#include <QObject>
#include <QString>

class QDialog;
class QWidget;

namespace OVP {

/**
 * @brief YOLO 目标识别插件（Python 版 yolo_detect.py 的 C++ 实现）
 *
 * 工具 ID 保持与 Python 版一致（user_yolo_detect），已有项目文件可直接复用参数。
 *
 * 输出端口：
 *   output      标注图像
 *   det_count   目标数量
 *   detections  检测结果 QVariantList<QVariantMap>，字段见 execute()
 *   top_class   最高置信度类别
 *   top_score   最高置信度
 *   device_used 实际推理设备
 */
class YoloDetectPlugin : public PluginBase
{
    Q_OBJECT

public:
    explicit YoloDetectPlugin(QObject *parent = nullptr);
    ~YoloDetectPlugin() override = default;

    QString id() const override { return QStringLiteral("user_yolo_detect"); }
    QString name() const override { return QStringLiteral("YOLO目标识别"); }
    QString category() const override { return QStringLiteral("检测定位"); }
    QString description() const override
    {
        return QStringLiteral("使用 YOLO(ONNX) 模型检测目标，可选模型文件与 CUDA 加速");
    }

    QList<PortDef> inputPorts() const override;
    QList<PortDef> outputPorts() const override;
    QList<ParamDef> inputParams() const override;

    bool execute() override;

    QDialog *createDialog(const cv::Mat &inputImage, QWidget *parent) override;

    // ---- 供对话框读取本次结果 ----
    QList<YOLO::Detection> lastDetections() const { return m_lastDetections; }
    QString deviceUsed() const { return m_deviceUsed; }
    double lastInferenceMs() const { return m_lastInferenceMs; }

private:
    QList<YOLO::Detection> m_lastDetections;
    QString m_deviceUsed = QStringLiteral("cpu");
    double m_lastInferenceMs = 0.0;
};

} // namespace OVP

class YoloDetectPluginProvider : public QObject, public OpenVisionPluginProvider
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID OpenVisionPluginProvider_iid)
    Q_INTERFACES(OpenVisionPluginProvider)

public:
    QList<Entry> availablePlugins() const override;
};

#endif // YOLODETECTPLUGIN_H
