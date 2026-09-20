#include "imageoutputplugin.h"

#include <opencv2/imgcodecs.hpp>

namespace OVP {

ImageOutputPlugin::ImageOutputPlugin(QObject *parent)
    : PluginBase(parent)
{
}

QList<PortDef> ImageOutputPlugin::inputPorts() const
{
    return {port(QStringLiteral("input"), PortType::Image, QStringLiteral("输入图像"))};
}

QList<ParamDef> ImageOutputPlugin::inputParams() const
{
    return {fileParam(QStringLiteral("file_path"), QStringLiteral("保存路径"), QString(),
                      QStringLiteral("图像保存路径"))};
}

bool ImageOutputPlugin::execute()
{
    clearError();

    const cv::Mat image = inputImage(QStringLiteral("input"));
    const QString path = paramString(QStringLiteral("file_path"));

    if (image.empty() || path.isEmpty()) {
        setError(QStringLiteral("没有输入图像或未指定保存路径"));
        return false;
    }

    if (!cv::imwrite(path.toStdString(), image)) {
        setError(QStringLiteral("保存失败: %1").arg(path));
        return false;
    }
    return true;
}

} // namespace OVP
