#include "morphologyplugin.h"

#include "../core/opencvcompat.h"

namespace OVP {

MorphologyPlugin::MorphologyPlugin(QObject *parent)
    : PluginBase(parent)
{
}

QList<PortDef> MorphologyPlugin::inputPorts() const
{
    return {port(QStringLiteral("input"), PortType::Image, QStringLiteral("输入图像"))};
}

QList<PortDef> MorphologyPlugin::outputPorts() const
{
    return {port(QStringLiteral("output"), PortType::Image, QStringLiteral("处理结果"))};
}

QList<ParamDef> MorphologyPlugin::inputParams() const
{
    return {
        choiceParam(QStringLiteral("operation"), QStringLiteral("操作"), QStringLiteral("ERODE"),
                    {QStringLiteral("ERODE"), QStringLiteral("DILATE"), QStringLiteral("OPEN"),
                     QStringLiteral("CLOSE"), QStringLiteral("GRADIENT"), QStringLiteral("TOPHAT"),
                     QStringLiteral("BLACKHAT")},
                    QStringLiteral("形态学操作类型")),
        choiceParam(QStringLiteral("kernel_shape"), QStringLiteral("核形状"), QStringLiteral("RECT"),
                    {QStringLiteral("RECT"), QStringLiteral("ELLIPSE"), QStringLiteral("CROSS")},
                    QStringLiteral("结构元素形状")),
        intParam(QStringLiteral("kernel_size"), QStringLiteral("核大小"), 3, 1, 31, 2,
                 QStringLiteral("结构元素大小")),
        intParam(QStringLiteral("iterations"), QStringLiteral("迭代次数"), 1, 1, 10, 1,
                 QStringLiteral("操作迭代次数"))
    };
}

bool MorphologyPlugin::execute()
{
    clearError();

    const cv::Mat image = inputImage(QStringLiteral("input"));
    if (image.empty()) {
        setError(QStringLiteral("没有输入图像"));
        return false;
    }

    const QString shape = paramString(QStringLiteral("kernel_shape"), QStringLiteral("RECT"));
    int shapeType = cv::MORPH_RECT;
    if (shape == QStringLiteral("ELLIPSE"))
        shapeType = cv::MORPH_ELLIPSE;
    else if (shape == QStringLiteral("CROSS"))
        shapeType = cv::MORPH_CROSS;

    const int kernelSize = paramInt(QStringLiteral("kernel_size"), 3);
    const cv::Mat kernel = cv::getStructuringElement(shapeType, cv::Size(kernelSize, kernelSize));

    const QString op = paramString(QStringLiteral("operation"), QStringLiteral("ERODE"));
    int opType = cv::MORPH_ERODE;
    if (op == QStringLiteral("DILATE")) opType = cv::MORPH_DILATE;
    else if (op == QStringLiteral("OPEN")) opType = cv::MORPH_OPEN;
    else if (op == QStringLiteral("CLOSE")) opType = cv::MORPH_CLOSE;
    else if (op == QStringLiteral("GRADIENT")) opType = cv::MORPH_GRADIENT;
    else if (op == QStringLiteral("TOPHAT")) opType = cv::MORPH_TOPHAT;
    else if (op == QStringLiteral("BLACKHAT")) opType = cv::MORPH_BLACKHAT;

    cv::Mat result;
    cv::morphologyEx(image, result, opType, kernel, cv::Point(-1, -1),
                     paramInt(QStringLiteral("iterations"), 1));

    if (result.empty()) {
        setError(QStringLiteral("形态学处理失败"));
        return false;
    }

    setOutput(QStringLiteral("output"), imageValue(result));
    return true;
}

} // namespace OVP
