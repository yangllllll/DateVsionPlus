#include "imagefilterplugin.h"

#include "../core/cvutils.h"

#include "../core/opencvcompat.h"
#include <opencv2/photo.hpp>

namespace OVP {

ImageFilterPlugin::ImageFilterPlugin(QObject *parent)
    : PluginBase(parent)
{
}

QList<PortDef> ImageFilterPlugin::inputPorts() const
{
    return {port(QStringLiteral("input"), PortType::Image, QStringLiteral("输入图像"))};
}

QList<PortDef> ImageFilterPlugin::outputPorts() const
{
    return {port(QStringLiteral("output"), PortType::Image, QStringLiteral("滤波结果"))};
}

QList<ParamDef> ImageFilterPlugin::inputParams() const
{
    return {
        choiceParam(QStringLiteral("filter_type"), QStringLiteral("滤波类型"),
                    QStringLiteral("高斯模糊"),
                    {QStringLiteral("均值模糊"), QStringLiteral("高斯模糊"), QStringLiteral("中值滤波"),
                     QStringLiteral("双边滤波"), QStringLiteral("锐化"), QStringLiteral("非局部均值去噪")},
                    QStringLiteral("滤波类型")),
        intParam(QStringLiteral("kernel_size"), QStringLiteral("核大小"), 5, 1, 31, 2,
                 QStringLiteral("滤波核大小（奇数）")),
        floatParam(QStringLiteral("sigma"), QStringLiteral("Sigma"), 1.0, 0.0, 10.0, 0.1,
                   QStringLiteral("高斯Sigma")),
        floatParam(QStringLiteral("bilateral_sigma_color"), QStringLiteral("双边-颜色Sigma"), 75.0, 1.0,
                   200.0, 1.0, QStringLiteral("双边滤波颜色空间Sigma")),
        floatParam(QStringLiteral("bilateral_sigma_space"), QStringLiteral("双边-空间Sigma"), 75.0, 1.0,
                   200.0, 1.0, QStringLiteral("双边滤波空间Sigma"))
    };
}

bool ImageFilterPlugin::execute()
{
    clearError();

    const cv::Mat image = inputImage(QStringLiteral("input"));
    if (image.empty()) {
        setError(QStringLiteral("没有输入图像"));
        return false;
    }

    const QString type = paramString(QStringLiteral("filter_type"), QStringLiteral("高斯模糊"));
    const int kernelSize = oddKernelSize(paramInt(QStringLiteral("kernel_size"), 5));
    const double sigma = paramDouble(QStringLiteral("sigma"), 1.0);

    cv::Mat result;
    if (type == QStringLiteral("均值模糊")) {
        cv::blur(image, result, cv::Size(kernelSize, kernelSize));
    } else if (type == QStringLiteral("高斯模糊")) {
        cv::GaussianBlur(image, result, cv::Size(kernelSize, kernelSize), sigma);
    } else if (type == QStringLiteral("中值滤波")) {
        cv::medianBlur(image, result, kernelSize);
    } else if (type == QStringLiteral("双边滤波")) {
        cv::bilateralFilter(image, result, kernelSize,
                            paramDouble(QStringLiteral("bilateral_sigma_color"), 75.0),
                            paramDouble(QStringLiteral("bilateral_sigma_space"), 75.0));
    } else if (type == QStringLiteral("锐化")) {
        cv::Mat kernel = (cv::Mat_<float>(3, 3) << -1, -1, -1, -1, 9, -1, -1, -1, -1);
        cv::filter2D(image, result, -1, kernel);
    } else if (type == QStringLiteral("非局部均值去噪")) {
        cv::fastNlMeansDenoising(image, result, static_cast<float>(sigma * 10.0), kernelSize, 21);
    } else {
        result = image;
    }

    if (result.empty()) {
        setError(QStringLiteral("滤波失败"));
        return false;
    }

    setOutput(QStringLiteral("output"), imageValue(result));
    return true;
}

} // namespace OVP
