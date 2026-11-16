#ifndef MATERIALZONEPLUGIN_H
#define MATERIALZONEPLUGIN_H

#include "pluginbase.h"
#include "plugininterface.h"

#include <QList>
#include <QObject>
#include <QString>
#include <QVariantMap>

#include <opencv2/core.hpp>

class QDialog;
class QWidget;

namespace OVP {

/**
 * @brief 检测区（归一化坐标 0~1，随图像分辨率自动缩放）
 */
struct Zone
{
    int id = 0;          //!< 检测区编号（从 1 开始）
    cv::Rect2f rect;     //!< x / y / w / h，取值 0~1
};

/**
 * @brief 物料识别（检测区）
 *
 * 配合「YOLO目标识别」使用：
 *   输入 detections = YOLO 的检测结果，判断每个目标中心落在哪个检测区内；
 *   输出「存在物体的检测区中编号最小的那个」编号（没有则为 -1）。
 *
 * 检测区在对话框中用鼠标拖拽建立，保存在 extraData 里随项目保存。
 */
class MaterialZonePlugin : public PluginBase
{
    Q_OBJECT

public:
    explicit MaterialZonePlugin(QObject *parent = nullptr);
    ~MaterialZonePlugin() override = default;

    QString id() const override { return QStringLiteral("material_zone"); }
    QString name() const override { return QStringLiteral("物料识别(检测区)"); }
    QString category() const override { return QStringLiteral("检测定位"); }
    QString description() const override
    {
        return QStringLiteral("根据 YOLO 检测结果判断物体中心所在的检测区，输出最小命中编号");
    }

    QList<PortDef> inputPorts() const override;
    QList<PortDef> outputPorts() const override;
    QList<ParamDef> inputParams() const override;

    bool execute() override;

    QVariantMap extraData() const override;
    void setExtraData(const QVariantMap &data) override;

    QDialog *createDialog(const cv::Mat &inputImage, QWidget *parent) override;

    // ---- 供对话框读写 ----
    QList<Zone> zones() const { return m_zones; }
    void setZones(const QList<Zone> &zones);

private:
    QList<Zone> m_zones;
};

} // namespace OVP

class MaterialZonePluginProvider : public QObject, public OpenVisionPluginProvider
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID OpenVisionPluginProvider_iid)
    Q_INTERFACES(OpenVisionPluginProvider)

public:
    QList<Entry> availablePlugins() const override;
};

#endif // MATERIALZONEPLUGIN_H
