#ifndef RANDOMPICKPLUGIN_H
#define RANDOMPICKPLUGIN_H

#include "pluginbase.h"
#include "plugininterface.h"

#include <QList>
#include <QObject>
#include <QString>

class QDialog;
class QWidget;

namespace OVP {

/**
 * @brief 无序抓取
 *
 * 读取 N 点标定文件，把 YOLO 检测到的目标（默认第一个，即置信度最高者）
 * 的检测框中心从图像坐标转换为机器人坐标。
 *
 * 输入：input（图像，可选，用于标注显示）、detections（YOLO 检测结果）
 * 输出：robot_x / robot_y / robot_point、pixel_x / pixel_y、类别与置信度等
 */
class RandomPickPlugin : public PluginBase
{
    Q_OBJECT

public:
    explicit RandomPickPlugin(QObject *parent = nullptr);
    ~RandomPickPlugin() override = default;

    QString id() const override { return QStringLiteral("random_pick"); }
    QString name() const override { return QStringLiteral("标定转换"); }
    QString category() const override { return QStringLiteral("机器人"); }
    QString description() const override
    {
        return QStringLiteral("按 N 点标定文件把 YOLO 目标的中心点转换为机器人坐标");
    }

    QList<PortDef> inputPorts() const override;
    QList<PortDef> outputPorts() const override;
    QList<ParamDef> inputParams() const override;

    bool execute() override;

    QDialog *createDialog(const cv::Mat &inputImage, QWidget *parent) override;

private:
    QString m_lastCalibrationFile;
};

} // namespace OVP

class RandomPickPluginProvider : public QObject, public OpenVisionPluginProvider
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID OpenVisionPluginProvider_iid)
    Q_INTERFACES(OpenVisionPluginProvider)

public:
    QList<Entry> availablePlugins() const override;
};

#endif // RANDOMPICKPLUGIN_H
