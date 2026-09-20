#include "thresholdplugin.h"

#include "../core/cvutils.h"

#include "../core/opencvcompat.h"

namespace OVP {

ThresholdPlugin::ThresholdPlugin(QObject *parent)
    : PluginBase(parent)
{
}

QList<PortDef> ThresholdPlugin::inputPorts() const
{
    return {port(QStringLiteral("input"), PortType::Image, QStringLiteral("输入图像"))};
}

QList<PortDef> ThresholdPlugin::outputPorts() const
{
    return {port(QStringLiteral("output"), PortType::Image, QStringLiteral("二值图像")),
            port(QStringLiteral("threshold_value"), PortType::Number, QStringLiteral("使用的阈值"))};
}

QList<ParamDef> ThresholdPlugin::inputParams() const
{
    return {
        sliderParam(QStringLiteral("threshold"), QStringLiteral("阈值"), 128, 0, 255, 1,
                    QStringLiteral("分割阈值")),
        sliderParam(QStringLiteral("max_value"), QStringLiteral("最大值"), 255, 0, 255, 1,
                    QStringLiteral("最大值")),
        choiceParam(QStringLiteral("method"), QStringLiteral("方法"), QStringLiteral("BINARY"),
                    {QStringLiteral("BINARY"), QStringLiteral("BINARY_INV"), QStringLiteral("OTSU"),
                     QStringLiteral("TRIANGLE"), QStringLiteral("ADAPTIVE_MEAN"),
                     QStringLiteral("ADAPTIVE_GAUSSIAN")},
                    QStringLiteral("阈值方法")),
        intParam(QStringLiteral("block_size"), QStringLiteral("自适应块大小"), 11, 3, 99, 2,
                 QStringLiteral("自适应阈值块大小（奇数）")),
        floatParam(QStringLiteral("c"), QStringLiteral("常数C"), 2.0, -50.0, 50.0, 0.5,
                   QStringLiteral("自适应阈值常数"))
    };
}

bool ThresholdPlugin::execute()
{
    clearError();

    const cv::Mat image = inputImage(QStringLiteral("input"));
    if (image.empty()) {
        setError(QStringLiteral("没有输入图像"));
        return false;
    }

    const cv::Mat gray = ensureGray(image);
    const QString method = paramString(QStringLiteral("method"), QStringLiteral("BINARY"));
    const double thresh = paramDouble(QStringLiteral("threshold"), 128.0);
    const double maxValue = paramDouble(QStringLiteral("max_value"), 255.0);
    int blockSize = paramInt(QStringLiteral("block_size"), 11);
    const double c = paramDouble(QStringLiteral("c"), 2.0);

    blockSize = oddKernelSize(blockSize);

    cv::Mat result;
    double usedThreshold = 0.0;

    if (method == QStringLiteral("BINARY")) {
        usedThreshold = cv::threshold(gray, result, thresh, maxValue, cv::THRESH_BINARY);
    } else if (method == QStringLiteral("BINARY_INV")) {
        usedThreshold = cv::threshold(gray, result, thresh, maxValue, cv::THRESH_BINARY_INV);
    } else if (method == QStringLiteral("OTSU")) {
        usedThreshold = cv::threshold(gray, result, 0, maxValue, cv::THRESH_BINARY | cv::THRESH_OTSU);
    } else if (method == QStringLiteral("TRIANGLE")) {
        usedThreshold = cv::threshold(gray, result, 0, maxValue, cv::THRESH_BINARY | cv::THRESH_TRIANGLE);
    } else if (method == QStringLiteral("ADAPTIVE_MEAN")) {
        cv::adaptiveThreshold(gray, result, maxValue, cv::ADAPTIVE_THRESH_MEAN_C,
                              cv::THRESH_BINARY, blockSize, c);
        usedThreshold = 0.0;
    } else if (method == QStringLiteral("ADAPTIVE_GAUSSIAN")) {
        cv::adaptiveThreshold(gray, result, maxValue, cv::ADAPTIVE_THRESH_GAUSSIAN_C,
                              cv::THRESH_BINARY, blockSize, c);
        usedThreshold = 0.0;
    } else {
        result = gray;
    }

    if (result.empty()) {
        setError(QStringLiteral("阈值分割失败"));
        return false;
    }

    setOutput(QStringLiteral("output"), imageValue(result));
    setOutput(QStringLiteral("threshold_value"), usedThreshold);
    return true;
}

} // namespace OVP
