#include "blobanalysisplugin.h"

#include "../core/cvutils.h"

#include "../core/opencvcompat.h"

namespace OVP {

BlobAnalysisPlugin::BlobAnalysisPlugin(QObject *parent)
    : PluginBase(parent)
{
}

QList<PortDef> BlobAnalysisPlugin::inputPorts() const
{
    return {port(QStringLiteral("input"), PortType::Image, QStringLiteral("输入图像（二值图）"))};
}

QList<PortDef> BlobAnalysisPlugin::outputPorts() const
{
    return {port(QStringLiteral("output"), PortType::Image, QStringLiteral("标注图像")),
            port(QStringLiteral("blob_count"), PortType::Number, QStringLiteral("斑点数量"))};
}

QList<ParamDef> BlobAnalysisPlugin::inputParams() const
{
    return {
        intParam(QStringLiteral("min_area"), QStringLiteral("最小面积"), 100, 0, 100000, 10,
                 QStringLiteral("过滤的最小面积")),
        intParam(QStringLiteral("max_area"), QStringLiteral("最大面积"), 100000, 0, 1000000, 100,
                 QStringLiteral("过滤的最大面积")),
        floatParam(QStringLiteral("min_circularity"), QStringLiteral("最小圆度"), 0.0, 0.0, 1.0, 0.01,
                   QStringLiteral("最小圆度")),
        boolParam(QStringLiteral("draw_contours"), QStringLiteral("绘制轮廓"), true,
                  QStringLiteral("是否绘制轮廓")),
        choiceParam(QStringLiteral("contour_color"), QStringLiteral("轮廓颜色"), QStringLiteral("绿色"),
                    {QStringLiteral("绿色"), QStringLiteral("红色"), QStringLiteral("蓝色"),
                     QStringLiteral("黄色"), QStringLiteral("青色"), QStringLiteral("白色")},
                    QStringLiteral("轮廓颜色")),
        boolParam(QStringLiteral("fill_contours"), QStringLiteral("填充轮廓"), false,
                  QStringLiteral("是否填充轮廓"))
    };
}

bool BlobAnalysisPlugin::execute()
{
    clearError();

    const cv::Mat image = inputImage(QStringLiteral("input"));
    if (image.empty()) {
        setError(QStringLiteral("没有输入图像"));
        return false;
    }

    cv::Mat gray = ensureGray(image).clone();
    if (gray.depth() != CV_8U)
        gray.convertTo(gray, CV_8U);

    // 确保是二值图（多灰度级时用 OTSU 自动二值化）
    double minVal = 0.0;
    double maxVal = 0.0;
    cv::minMaxLoc(gray, &minVal, &maxVal);
    if (maxVal > minVal)
        cv::threshold(gray, gray, 0, 255, cv::THRESH_BINARY | cv::THRESH_OTSU);

    const double minArea = paramDouble(QStringLiteral("min_area"), 100.0);
    const double maxArea = paramDouble(QStringLiteral("max_area"), 100000.0);
    const double minCircularity = paramDouble(QStringLiteral("min_circularity"), 0.0);
    const bool drawContours = paramBool(QStringLiteral("draw_contours"), true);
    const bool fillContours = paramBool(QStringLiteral("fill_contours"), false);
    const cv::Scalar color = bgrColor(paramString(QStringLiteral("contour_color"), QStringLiteral("绿色")));

    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(gray, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);

    cv::Mat output = ensureBgr(image);
    int validCount = 0;

    for (const std::vector<cv::Point> &cnt : contours) {
        const double area = cv::contourArea(cnt);
        if (area < minArea || area > maxArea)
            continue;

        const double perimeter = cv::arcLength(cnt, true);
        const double circularity = (perimeter > 0.0)
                                       ? (4.0 * CV_PI * area / (perimeter * perimeter))
                                       : 0.0;
        if (circularity < minCircularity)
            continue;

        ++validCount;

        if (drawContours) {
            cv::drawContours(output, std::vector<std::vector<cv::Point>>{cnt}, -1, color,
                             fillContours ? -1 : 2);

            const cv::Moments m = cv::moments(cnt);
            if (std::abs(m.m00) > 1e-9) {
                const int cx = static_cast<int>(m.m10 / m.m00);
                const int cy = static_cast<int>(m.m01 / m.m00);
                cv::circle(output, cv::Point(cx, cy), 3, cv::Scalar(0, 0, 255), -1);
            }
        }
    }

    setOutput(QStringLiteral("output"), imageValue(output));
    setOutput(QStringLiteral("blob_count"), validCount);
    return true;
}

} // namespace OVP
