#ifndef GRAYSCALEPLUGIN_H
#define GRAYSCALEPLUGIN_H

#include "../core/pluginbase.h"

namespace OVP {

/** 灰度化 */
class GrayscalePlugin : public PluginBase
{
    Q_OBJECT

public:
    explicit GrayscalePlugin(QObject *parent = nullptr);

    QString id() const override { return QStringLiteral("grayscale"); }
    QString name() const override { return QStringLiteral("灰度化"); }
    QString category() const override { return QStringLiteral("图像处理"); }
    QString description() const override { return QStringLiteral("将彩色图像转换为灰度图像"); }

    QList<PortDef> inputPorts() const override;
    QList<PortDef> outputPorts() const override;

    bool execute() override;
};

} // namespace OVP

#endif // GRAYSCALEPLUGIN_H
