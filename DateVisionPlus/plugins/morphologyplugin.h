#ifndef MORPHOLOGYPLUGIN_H
#define MORPHOLOGYPLUGIN_H

#include "../core/pluginbase.h"

namespace OVP {

/** 形态学处理：腐蚀 / 膨胀 / 开 / 闭 / 梯度 / 顶帽 / 黑帽 */
class MorphologyPlugin : public PluginBase
{
    Q_OBJECT

public:
    explicit MorphologyPlugin(QObject *parent = nullptr);

    QString id() const override { return QStringLiteral("morphology"); }
    QString name() const override { return QStringLiteral("形态学处理"); }
    QString category() const override { return QStringLiteral("图像处理"); }
    QString description() const override
    {
        return QStringLiteral("膨胀/腐蚀/开运算/闭运算等形态学操作");
    }

    QList<PortDef> inputPorts() const override;
    QList<PortDef> outputPorts() const override;
    QList<ParamDef> inputParams() const override;

    bool execute() override;
};

} // namespace OVP

#endif // MORPHOLOGYPLUGIN_H
