#ifndef IMAGEFILTERPLUGIN_H
#define IMAGEFILTERPLUGIN_H

#include "../core/pluginbase.h"

namespace OVP {

/** 图像滤波：均值 / 高斯 / 中值 / 双边 / 锐化 / 非局部均值去噪 */
class ImageFilterPlugin : public PluginBase
{
    Q_OBJECT

public:
    explicit ImageFilterPlugin(QObject *parent = nullptr);

    QString id() const override { return QStringLiteral("image_filter"); }
    QString name() const override { return QStringLiteral("图像滤波"); }
    QString category() const override { return QStringLiteral("图像处理"); }
    QString description() const override { return QStringLiteral("对图像进行平滑/锐化/去噪等滤波操作"); }

    QList<PortDef> inputPorts() const override;
    QList<PortDef> outputPorts() const override;
    QList<ParamDef> inputParams() const override;

    bool execute() override;
};

} // namespace OVP

#endif // IMAGEFILTERPLUGIN_H
