#ifndef THRESHOLDPLUGIN_H
#define THRESHOLDPLUGIN_H

#include "../core/pluginbase.h"

namespace OVP {

/** 阈值分割：BINARY / OTSU / TRIANGLE / 自适应 */
class ThresholdPlugin : public PluginBase
{
    Q_OBJECT

public:
    explicit ThresholdPlugin(QObject *parent = nullptr);

    QString id() const override { return QStringLiteral("threshold"); }
    QString name() const override { return QStringLiteral("阈值分割"); }
    QString category() const override { return QStringLiteral("图像处理"); }
    QString description() const override { return QStringLiteral("对图像进行阈值分割"); }

    QList<PortDef> inputPorts() const override;
    QList<PortDef> outputPorts() const override;
    QList<ParamDef> inputParams() const override;

    bool execute() override;
};

} // namespace OVP

#endif // THRESHOLDPLUGIN_H
