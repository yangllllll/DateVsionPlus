#include "measureplugin.h"

#include "../core/cvutils.h"

#include "../core/opencvcompat.h"

namespace OVP {

MeasurePlugin::MeasurePlugin(QObject *parent)
    : PluginBase(parent)
{
}

QList<PortDef> MeasurePlugin::inputPorts() const
{
    return {port(QStringLiteral("input"), PortType::Image, QStringLiteral("输入图像"))};
}

QList<PortDef> MeasurePlugin::outputPorts() const
{
    return {port(QStringLiteral("output"), PortType::Image, QStringLiteral("标注图像")),
            port(QStringLiteral("measure_count"), PortType::Number, QStringLiteral("测量对象数量"))};
}

QList<ParamDef> MeasurePlugin::inputParams() const
{
    return {
        choiceParam(QStringLiteral("measure_type"), QStringLiteral("测量类型"), QStringLiteral("线段"),
                    {QStringLiteral("线段"), QStringLiteral("矩形"), QStringLiteral("圆形"),
                     QStringLiteral("最小外接矩形"), QStringLiteral("最小外接圆")},
                    QStringLiteral("测量类型")),
        choiceParam(QStringLiteral("line_color"), QStringLiteral("绘制颜色"), QStringLiteral("绿色"),
                    {QStringLiteral("绿色"), QStringLiteral("红色"), QStringLiteral("蓝色"),
                     QStringLiteral("黄色"), QStringLiteral("白色")},
                    QStringLiteral("绘制颜色"))
    };
}

bool MeasurePlugin::execute()
{
    clearError();

    const cv::Mat image = inputImage(QStringLiteral("input"));
    if (image.empty()) {
        setError(QStringLiteral("没有输入图像"));
        return false;
    }

    const cv::Scalar color = bgrColor(paramString(QStringLiteral("line_color"), QStringLiteral("绿色")));
    const QString measureType = paramString(QStringLiteral("measure_type"), QStringLiteral("线段"));

    cv::Mat gray = ensureGray(image);
    if (gray.depth() != CV_8U)
        gray.convertTo(gray, CV_8U);

    double minVal = 0.0;
    double maxVal = 0.0;
    cv::minMaxLoc(gray, &minVal, &maxVal);
    cv::Mat binary = gray;
    if (maxVal > minVal)
        cv::threshold(gray, binary, 0, 255, cv::THRESH_BINARY | cv::THRESH_OTSU);

    cv::Mat output = ensureBgr(image);

    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(binary, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);

    int measureCount = 0;
    const int rows = output.rows;
    const int cols = output.cols;

    for (const std::vector<cv::Point> &cnt : contours) {
        if (cnt.size() < 5)
            continue;
        if (cv::contourArea(cnt) < 50.0)
            continue;

        if (measureType == QStringLiteral("线段")) {
            cv::Vec4f lineParams;
            cv::fitLine(cnt, lineParams, cv::DIST_L2, 0, 0.01, 0.01);
            const double vx = lineParams[0];
            const double vy = lineParams[1];
            const double x0 = lineParams[2];
            const double y0 = lineParams[3];
            const int lefty = std::abs(vx) > 1e-9 ? static_cast<int>((-x0 * vy / vx) + y0) : 0;
            const int righty = std::abs(vx) > 1e-9
                                   ? static_cast<int>(((cols - x0) * vy / vx) + y0)
                                   : 0;
            cv::line(output, cv::Point(cols - 1, righty), cv::Point(0, lefty), color, 2);
            ++measureCount;
        } else if (measureType == QStringLiteral("矩形")) {
            const cv::Rect rect = cv::boundingRect(cnt);
            cv::rectangle(output, rect, color, 2);
            cv::putText(output, QStringLiteral("%1x%2").arg(rect.width).arg(rect.height).toStdString(),
                        cv::Point(rect.x, rect.y - 5), cv::FONT_HERSHEY_SIMPLEX, 0.5, color, 1);
            ++measureCount;
        } else if (measureType == QStringLiteral("圆形")) {
            cv::Point2f center;
            float radius = 0.0f;
            cv::minEnclosingCircle(cnt, center, radius);
            cv::circle(output, center, static_cast<int>(radius), color, 2);
            cv::circle(output, center, 3, color, -1);
            cv::putText(output, QStringLiteral("R=%1").arg(static_cast<int>(radius)).toStdString(),
                        cv::Point(static_cast<int>(center.x) + 10, static_cast<int>(center.y)),
                        cv::FONT_HERSHEY_SIMPLEX, 0.5, color, 1);
            ++measureCount;
        } else if (measureType == QStringLiteral("最小外接矩形")) {
            const cv::RotatedRect rect = cv::minAreaRect(cnt);
            cv::Point2f points[4];
            rect.points(points);
            std::vector<cv::Point> box;
            for (const cv::Point2f &p : points)
                box.emplace_back(cvRound(p.x), cvRound(p.y));
            cv::drawContours(output, std::vector<std::vector<cv::Point>>{box}, 0, color, 2);
            ++measureCount;
        } else if (measureType == QStringLiteral("最小外接圆")) {
            cv::Point2f center;
            float radius = 0.0f;
            cv::minEnclosingCircle(cnt, center, radius);
            cv::circle(output, center, static_cast<int>(radius), color, 2);
            ++measureCount;
        }
    }

    setOutput(QStringLiteral("output"), imageValue(output));
    setOutput(QStringLiteral("measure_count"), measureCount);
    return true;
}

} // namespace OVP
