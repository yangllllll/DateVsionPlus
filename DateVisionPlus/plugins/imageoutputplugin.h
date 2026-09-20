#ifndef IMAGEOUTPUTPLUGIN_H
#define IMAGEOUTPUTPLUGIN_H

#include "../core/pluginbase.h"

namespace OVP {

/** 图像输出：把图像保存到文件 */
class ImageOutputPlugin : public PluginBase
{
    Q_OBJECT

public:
    explicit ImageOutputPlugin(QObject *parent = nullptr);

    QString id() const override { return QStringLiteral("image_output"); }
    QString name() const override { return QStringLiteral("图像输出"); }
    QString category() const override { return QStringLiteral("输入输出"); }
    QString description() const override { return QStringLiteral("将图像保存到文件"); }

    QList<PortDef> inputPorts() const override;
    QList<ParamDef> inputParams() const override;

    bool execute() override;
};

} // namespace OVP

#endif // IMAGEOUTPUTPLUGIN_H
