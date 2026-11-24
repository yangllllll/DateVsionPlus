#ifndef NPOINTCALIBPLUGIN_H
#define NPOINTCALIBPLUGIN_H

#include "pluginbase.h"
#include "plugininterface.h"

#include "calibdata.h"

#include <QList>
#include <QObject>
#include <QString>

class QDialog;
class QWidget;

namespace OVP {

/**
 * @brief N 点标定
 *
 * 在对话框中录入 N 组「图像坐标 -> 机器人坐标」对应点：
 *   - 图像坐标可以直接用 YOLO 检测框中心填充（先运行一次流程拿到 detections）；
 *   - 机器人坐标手动输入；
 *   - 至少 3 组点，最小二乘拟合二维仿射变换；
 *   - 可保存为 .ncal 标定文件，供「无序抓取」使用。
 */
class NPointCalibPlugin : public PluginBase
{
    Q_OBJECT

public:
    explicit NPointCalibPlugin(QObject *parent = nullptr);
    ~NPointCalibPlugin() override = default;

    QString id() const override { return QStringLiteral("npoint_calib"); }
    QString name() const override { return QStringLiteral("N点标定"); }
    QString category() const override { return QStringLiteral("标定"); }
    QString description() const override
    {
        return QStringLiteral("录入 N 组图像/机器人对应点，拟合仿射变换并保存标定文件");
    }

    QList<PortDef> inputPorts() const override;
    QList<PortDef> outputPorts() const override;
    QList<ParamDef> inputParams() const override;

    bool execute() override;

    QVariantMap extraData() const override;
    void setExtraData(const QVariantMap &data) override;

    QDialog *createDialog(const cv::Mat &inputImage, QWidget *parent) override;

    // ---- 供对话框读写 ----
    CALIB::CalibData calibration() const { return m_calib; }
    void setCalibration(const CALIB::CalibData &data) { m_calib = data; }

private:
    CALIB::CalibData m_calib;
};

} // namespace OVP

class NPointCalibPluginProvider : public QObject, public OpenVisionPluginProvider
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID OpenVisionPluginProvider_iid)
    Q_INTERFACES(OpenVisionPluginProvider)

public:
    QList<Entry> availablePlugins() const override;
};

#endif // NPOINTCALIBPLUGIN_H
