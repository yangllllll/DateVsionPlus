#ifndef EDGEDETECTIONPLUGIN_H
#define EDGEDETECTIONPLUGIN_H

#include "../core/pluginbase.h"

namespace OVP {

/** 边缘检测：Canny / Sobel / Laplacian */
class EdgeDetectionPlugin : public PluginBase
{
    Q_OBJECT

public:
    explicit EdgeDetectionPlugin(QObject *parent = nullptr);

    QString id() const override { return QStringLiteral("edge_detection"); }
    QString name() const override { return QStringLiteral("边缘检测"); }
    QString category() const override { return QStringLiteral("图像处理"); }
    QString description() const override { return QStringLiteral("检测图像边缘（Canny / Sobel / Laplacian）"); }

    QList<PortDef> inputPorts() const override;
    QList<PortDef> outputPorts() const override;
    QList<ParamDef> inputParams() const override;

    bool execute() override;
};

} // namespace OVP

#endif // EDGEDETECTIONPLUGIN_H
