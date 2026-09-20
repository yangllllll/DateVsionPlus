#include "edgedetectionplugin.h"

#include "../core/cvutils.h"

#include "../core/opencvcompat.h"

namespace OVP {

EdgeDetectionPlugin::EdgeDetectionPlugin(QObject *parent)
    : PluginBase(parent)
{
}

QList<PortDef> EdgeDetectionPlugin::inputPorts() const
{
    return {port(QStringLiteral("input"), PortType::Image, QStringLiteral("输入图像"))};
}

QList<PortDef> EdgeDetectionPlugin::outputPorts() const
{
    return {port(QStringLiteral("output"), PortType::Image, QStringLiteral("边缘图像"))};
}

QList<ParamDef> EdgeDetectionPlugin::inputParams() const
{
    return {
        choiceParam(QStringLiteral("method"), QStringLiteral("方法"), QStringLiteral("Canny"),
                    {QStringLiteral("Canny"), QStringLiteral("Sobel"), QStringLiteral("Laplacian")},
                    QStringLiteral("边缘检测算法")),
        sliderParam(QStringLiteral("low_threshold"), QStringLiteral("低阈值"), 50, 0, 255, 1,
                    QStringLiteral("Canny低阈值")),
        sliderParam(QStringLiteral("high_threshold"), QStringLiteral("高阈值"), 150, 0, 255, 1,
                    QStringLiteral("Canny高阈值")),
        choiceParam(QStringLiteral("aperture"), QStringLiteral("孔径大小"), QStringLiteral("3"),
                    {QStringLiteral("3"), QStringLiteral("5"), QStringLiteral("7")},
                    QStringLiteral("Sobel/Laplacian孔径")),
        intParam(QStringLiteral("dx"), QStringLiteral("X方向阶数"), 1, 0, 2, 1,
                 QStringLiteral("Sobel X方向导数阶数")),
        intParam(QStringLiteral("dy"), QStringLiteral("Y方向阶数"), 0, 0, 2, 1,
                 QStringLiteral("Sobel Y方向导数阶数"))
    };
}

bool EdgeDetectionPlugin::execute()
{
    clearError();

    const cv::Mat image = inputImage(QStringLiteral("input"));
    if (image.empty()) {
        setError(QStringLiteral("没有输入图像"));
        return false;
    }

    const cv::Mat gray = ensureGray(image);
    const QString method = paramString(QStringLiteral("method"), QStringLiteral("Canny"));
    const int aperture = paramString(QStringLiteral("aperture"), QStringLiteral("3")).toInt();

    cv::Mat result;
    if (method == QStringLiteral("Canny")) {
        cv::Canny(gray, result, paramDouble(QStringLiteral("low_threshold"), 50.0),
                  paramDouble(QStringLiteral("high_threshold"), 150.0));
    } else if (method == QStringLiteral("Sobel")) {
        cv::Mat grad;
        cv::Sobel(gray, grad, CV_64F, paramInt(QStringLiteral("dx"), 1),
                  paramInt(QStringLiteral("dy"), 0), aperture);
        cv::convertScaleAbs(grad, result);
    } else if (method == QStringLiteral("Laplacian")) {
        cv::Mat grad;
        cv::Laplacian(gray, grad, CV_64F, aperture);
        cv::convertScaleAbs(grad, result);
    } else {
        result = gray;
    }

    if (result.empty()) {
        setError(QStringLiteral("边缘检测失败"));
        return false;
    }

    setOutput(QStringLiteral("output"), imageValue(result));
    return true;
}

} // namespace OVP
