#ifndef BLOBANALYSISPLUGIN_H
#define BLOBANALYSISPLUGIN_H

#include "../core/pluginbase.h"

namespace OVP {

/** 斑点分析：连通区域检测与特征提取 */
class BlobAnalysisPlugin : public PluginBase
{
    Q_OBJECT

public:
    explicit BlobAnalysisPlugin(QObject *parent = nullptr);

    QString id() const override { return QStringLiteral("blob_analysis"); }
    QString name() const override { return QStringLiteral("斑点分析"); }
    QString category() const override { return QStringLiteral("检测定位"); }
    QString description() const override { return QStringLiteral("检测图像中的斑点/连通区域，并提取特征"); }

    QList<PortDef> inputPorts() const override;
    QList<PortDef> outputPorts() const override;
    QList<ParamDef> inputParams() const override;

    bool execute() override;
};

} // namespace OVP

#endif // BLOBANALYSISPLUGIN_H
