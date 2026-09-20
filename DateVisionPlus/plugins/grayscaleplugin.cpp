#include "grayscaleplugin.h"

#include "../core/cvutils.h"

namespace OVP {

GrayscalePlugin::GrayscalePlugin(QObject *parent)
    : PluginBase(parent)
{
}

QList<PortDef> GrayscalePlugin::inputPorts() const
{
    return {port(QStringLiteral("input"), PortType::Image, QStringLiteral("输入图像"))};
}

QList<PortDef> GrayscalePlugin::outputPorts() const
{
    return {port(QStringLiteral("output"), PortType::Image, QStringLiteral("灰度图像"))};
}

bool GrayscalePlugin::execute()
{
    clearError();

    const cv::Mat image = inputImage(QStringLiteral("input"));
    if (image.empty()) {
        setError(QStringLiteral("没有输入图像"));
        return false;
    }

    setOutput(QStringLiteral("output"), imageValue(ensureGray(image)));
    return true;
}

} // namespace OVP
