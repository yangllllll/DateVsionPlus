#ifndef IMAGESOURCEPLUGIN_H
#define IMAGESOURCEPLUGIN_H

#include "../core/pluginbase.h"

namespace OVP {

/** 图像源：从文件加载图像 */
class ImageSourcePlugin : public PluginBase
{
    Q_OBJECT

public:
    explicit ImageSourcePlugin(QObject *parent = nullptr);

    QString id() const override { return QStringLiteral("image_source"); }
    QString name() const override { return QStringLiteral("图像源"); }
    QString category() const override { return QStringLiteral("输入输出"); }
    QString description() const override { return QStringLiteral("从文件加载图像"); }

    QList<PortDef> outputPorts() const override;
    QList<ParamDef> inputParams() const override;

    bool execute() override;
};

} // namespace OVP

#endif // IMAGESOURCEPLUGIN_H
