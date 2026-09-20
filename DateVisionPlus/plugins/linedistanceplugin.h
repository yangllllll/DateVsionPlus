#ifndef LINEDISTANCEPLUGIN_H
#define LINEDISTANCEPLUGIN_H

#include "../core/pluginbase.h"

#include <QList>
#include <QVector>
#include <QVariantMap>

namespace OVP {

/** 线间距：计算两个线查找工具输出线之间的水平 / 垂直距离 */
class LineDistancePlugin : public PluginBase
{
    Q_OBJECT

public:
    explicit LineDistancePlugin(QObject *parent = nullptr);

    QString id() const override { return QStringLiteral("line_distance"); }
    QString name() const override { return QStringLiteral("线间距"); }
    QString category() const override { return QStringLiteral("检测定位"); }
    QString description() const override
    {
        return QStringLiteral("接收两个线查找工具的线坐标，计算垂直距离和水平距离");
    }

    QList<PortDef> inputPorts() const override;
    QList<PortDef> outputPorts() const override;
    QList<ParamDef> inputParams() const override;

    bool execute() override;

private:
    QList<QVector<double>> m_distances;
};

} // namespace OVP

#endif // LINEDISTANCEPLUGIN_H
