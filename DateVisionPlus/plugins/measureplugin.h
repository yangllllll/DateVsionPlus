#ifndef MEASUREPLUGIN_H
#define MEASUREPLUGIN_H

#include "../core/pluginbase.h"

namespace OVP {

/** 测量工具：线段 / 矩形 / 圆形 / 最小外接矩形 / 最小外接圆 */
class MeasurePlugin : public PluginBase
{
    Q_OBJECT

public:
    explicit MeasurePlugin(QObject *parent = nullptr);

    QString id() const override { return QStringLiteral("measure"); }
    QString name() const override { return QStringLiteral("测量工具"); }
    QString category() const override { return QStringLiteral("检测定位"); }
    QString description() const override { return QStringLiteral("测量图像中的距离、圆、角度等"); }

    QList<PortDef> inputPorts() const override;
    QList<PortDef> outputPorts() const override;
    QList<ParamDef> inputParams() const override;

    bool execute() override;
};

} // namespace OVP

#endif // MEASUREPLUGIN_H
