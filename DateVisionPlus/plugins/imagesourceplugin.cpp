#include "imagesourceplugin.h"

#include "../core/cvutils.h"

#include <opencv2/imgcodecs.hpp>

namespace OVP {

ImageSourcePlugin::ImageSourcePlugin(QObject *parent)
    : PluginBase(parent)
{
}

QList<PortDef> ImageSourcePlugin::outputPorts() const
{
    return {port(QStringLiteral("output"), PortType::Image, QStringLiteral("输出图像"))};
}

QList<ParamDef> ImageSourcePlugin::inputParams() const
{
    return {fileParam(QStringLiteral("file_path"), QStringLiteral("图像路径"), QString(),
                      QStringLiteral("选择要加载的图像文件"))};
}

bool ImageSourcePlugin::execute()
{
    const QString path = paramString(QStringLiteral("file_path"));
    clearError();

    if (path.isEmpty()) {
        setError(QStringLiteral("未选择图像文件"));
        return false;
    }

    const cv::Mat image = cv::imread(path.toStdString(), cv::IMREAD_COLOR);
    if (image.empty()) {
        setError(QStringLiteral("无法读取图像: %1").arg(path));
        return false;
    }

    setOutput(QStringLiteral("output"), imageValue(image));
    return true;
}

} // namespace OVP
