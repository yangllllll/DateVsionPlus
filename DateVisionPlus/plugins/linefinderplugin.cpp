#include "linefinderplugin.h"

#include "../core/cvutils.h"
#include "../core/plugintypes.h"
#include "../dialogs/linefinderdialog.h"

#include "../core/opencvcompat.h"

#include <QDialog>

#include <algorithm>
#include <cmath>

namespace OVP {

LineFinderPlugin::LineFinderPlugin(QObject *parent)
    : PluginBase(parent)
{
}

QList<PortDef> LineFinderPlugin::inputPorts() const
{
    return {port(QStringLiteral("input"), PortType::Image, QStringLiteral("输入图像"))};
}

QList<PortDef> LineFinderPlugin::outputPorts() const
{
    return {port(QStringLiteral("output"), PortType::Image, QStringLiteral("标记图像")),
            port(QStringLiteral("line_count"), PortType::Number, QStringLiteral("检测到的线段数量")),
            port(QStringLiteral("line_coords"), PortType::Any, QStringLiteral("线段坐标列表"))};
}

QList<ParamDef> LineFinderPlugin::inputParams() const
{
    return {
        choiceParam(QStringLiteral("search_direction"), QStringLiteral("搜索方向"),
                    QStringLiteral("垂直"), {QStringLiteral("垂直"), QStringLiteral("水平")},
                    QStringLiteral("ROI内搜索方向：垂直=从上到下找水平线，水平=从左到右找竖直线")),
        choiceParam(QStringLiteral("edge_polarity"), QStringLiteral("边缘极性"),
                    QStringLiteral("明到暗"),
                    {QStringLiteral("明到暗"), QStringLiteral("暗到明"), QStringLiteral("任意")},
                    QStringLiteral("从明到暗=白底黑线，从暗到明=黑底白线")),
        sliderParam(QStringLiteral("edge_threshold1"), QStringLiteral("边缘低阈值"), 30, 0, 255, 1,
                    QStringLiteral("Canny边缘检测低阈值")),
        sliderParam(QStringLiteral("edge_threshold2"), QStringLiteral("边缘高阈值"), 90, 0, 255, 1,
                    QStringLiteral("Canny边缘检测高阈值")),
        sliderParam(QStringLiteral("hough_threshold"), QStringLiteral("霍夫阈值"), 30, 1, 300, 1,
                    QStringLiteral("霍夫直线检测投票阈值")),
        sliderParam(QStringLiteral("min_line_length"), QStringLiteral("最小线长"), 30, 5, 500, 1,
                    QStringLiteral("最小线段长度")),
        sliderParam(QStringLiteral("max_line_gap"), QStringLiteral("最大断距"), 15, 0, 100, 1,
                    QStringLiteral("线段最大断裂间隙")),
        intParam(QStringLiteral("blur_ksize"), QStringLiteral("模糊核大小"), 3, 1, 15, 2,
                 QStringLiteral("预处理高斯模糊核大小")),
        boolParam(QStringLiteral("learn_mode"), QStringLiteral("自动学习"), false,
                  QStringLiteral("启用后根据ROI内容自动学习色差阈值")),
        choiceParam(QStringLiteral("draw_color"), QStringLiteral("标记颜色"), QStringLiteral("绿色"),
                    {QStringLiteral("绿色"), QStringLiteral("红色"), QStringLiteral("蓝色"),
                     QStringLiteral("黄色"), QStringLiteral("青色"), QStringLiteral("白色")},
                    QStringLiteral("绘制直线的颜色")),
        intParam(QStringLiteral("line_thickness"), QStringLiteral("线宽"), 2, 1, 10, 1,
                 QStringLiteral("标记线宽度"))
    };
}

QVector<double> LineFinderPlugin::autoLearn(const cv::Mat &image, const QRect &roi, bool *ok)
{
    if (ok)
        *ok = false;

    if (image.empty() || roi.width() < 5 || roi.height() < 5)
        return {};

    const cv::Mat gray = ensureGray(image);
    const int rx = qMax(0, roi.x());
    const int ry = qMax(0, roi.y());
    const int rw = qMin(roi.width(), gray.cols - rx);
    const int rh = qMin(roi.height(), gray.rows - ry);
    if (rw < 5 || rh < 5)
        return {};

    const cv::Rect roiRect(rx, ry, rw, rh);
    const cv::Mat roiImg = gray(roiRect);

    const QString searchDir = paramString(QStringLiteral("search_direction"), QStringLiteral("垂直"));
    const QString polarity = paramString(QStringLiteral("edge_polarity"), QStringLiteral("明到暗"));
    const int blurKs = oddKernelSize(paramInt(QStringLiteral("blur_ksize"), 3));

    cv::Mat roiBlur;
    cv::GaussianBlur(roiImg, roiBlur, cv::Size(blurKs, blurKs), 0);

    cv::Mat grad;
    std::vector<cv::Point2f> points;

    if (searchDir == QStringLiteral("垂直")) {
        // 从上到下扫描，找水平线
        cv::Sobel(roiBlur, grad, CV_64F, 0, 1, 3);

        const int step = qMax(1, rw / 30);
        for (int col = 0; col < rw; col += step) {
            const cv::Mat colGrad = grad.col(col);
            double minVal = 0.0;
            double maxVal = 0.0;
            cv::Point minLoc;
            cv::Point maxLoc;
            cv::minMaxLoc(colGrad, &minVal, &maxVal, &minLoc, &maxLoc);

            int bestRow = 0;
            double gradValue = 0.0;
            if (polarity == QStringLiteral("明到暗")) {
                bestRow = minLoc.y;
                gradValue = std::abs(minVal);
            } else if (polarity == QStringLiteral("暗到明")) {
                bestRow = maxLoc.y;
                gradValue = std::abs(maxVal);
            } else {
                if (std::abs(minVal) >= std::abs(maxVal)) {
                    bestRow = minLoc.y;
                    gradValue = std::abs(minVal);
                } else {
                    bestRow = maxLoc.y;
                    gradValue = std::abs(maxVal);
                }
            }

            if (gradValue > 5.0)
                points.emplace_back(static_cast<float>(col), static_cast<float>(bestRow));
        }
    } else {
        // 从左到右扫描，找竖直线
        cv::Sobel(roiBlur, grad, CV_64F, 1, 0, 3);

        const int step = qMax(1, rh / 30);
        for (int row = 0; row < rh; row += step) {
            const cv::Mat rowGrad = grad.row(row);
            double minVal = 0.0;
            double maxVal = 0.0;
            cv::Point minLoc;
            cv::Point maxLoc;
            cv::minMaxLoc(rowGrad, &minVal, &maxVal, &minLoc, &maxLoc);

            int bestCol = 0;
            double gradValue = 0.0;
            if (polarity == QStringLiteral("明到暗")) {
                bestCol = minLoc.x;
                gradValue = std::abs(minVal);
            } else if (polarity == QStringLiteral("暗到明")) {
                bestCol = maxLoc.x;
                gradValue = std::abs(maxVal);
            } else {
                if (std::abs(minVal) >= std::abs(maxVal)) {
                    bestCol = minLoc.x;
                    gradValue = std::abs(minVal);
                } else {
                    bestCol = maxLoc.x;
                    gradValue = std::abs(maxVal);
                }
            }

            if (gradValue > 5.0)
                points.emplace_back(static_cast<float>(bestCol), static_cast<float>(row));
        }
    }

    if (points.size() < 3)
        return {};

    cv::Vec4f lineParams;
    cv::fitLine(points, lineParams, cv::DIST_HUBER, 0, 0.01, 0.01);
    const double vx = lineParams[0];
    const double vy = lineParams[1];
    const double cx = lineParams[2];
    const double cy = lineParams[3];

    double x1 = 0.0;
    double y1 = 0.0;
    double x2 = 0.0;
    double y2 = 0.0;

    if (searchDir == QStringLiteral("垂直")) {
        double yLeft = cy;
        double yRight = cy;
        if (std::abs(vx) > 1e-6) {
            yLeft = cy + ((0.0 - cx) / vx) * vy;
            yRight = cy + ((static_cast<double>(rw - 1) - cx) / vx) * vy;
        }
        x1 = rx;
        y1 = ry + yLeft;
        x2 = rx + rw - 1;
        y2 = ry + yRight;
    } else {
        double xTop = cx;
        double xBottom = cx;
        if (std::abs(vy) > 1e-6) {
            xTop = cx + ((0.0 - cy) / vy) * vx;
            xBottom = cx + ((static_cast<double>(rh - 1) - cy) / vy) * vx;
        }
        x1 = rx + xTop;
        y1 = ry;
        x2 = rx + xBottom;
        y2 = ry + rh - 1;
    }

    cv::Scalar meanScalar;
    cv::Scalar stdScalar;
    cv::meanStdDev(cv::abs(grad), meanScalar, stdScalar);
    const double gradMean = meanScalar[0];
    const double gradStd = stdScalar[0];

    const int edgeT1 = qMax(5, static_cast<int>(gradMean));
    const int edgeT2 = qMax(10, static_cast<int>(gradMean + gradStd * 1.5));

    m_learnedLine = {x1, y1, x2, y2};
    m_learnedThresholds = {edgeT1, edgeT2};

    if (ok)
        *ok = true;

    return {x1, y1, x2, y2, static_cast<double>(edgeT1), static_cast<double>(edgeT2)};
}

bool LineFinderPlugin::execute()
{
    clearError();

    const cv::Mat image = inputImage(QStringLiteral("input"));
    if (image.empty()) {
        setError(QStringLiteral("没有输入图像"));
        return false;
    }

    const cv::Mat gray = ensureGray(image);
    cv::Mat output = ensureBgr(image);

    const cv::Scalar drawColor = bgrColor(paramString(QStringLiteral("draw_color"), QStringLiteral("绿色")));
    const int thickness = paramInt(QStringLiteral("line_thickness"), 2);
    const bool learnMode = paramBool(QStringLiteral("learn_mode"), false);

    double edgeT1 = paramDouble(QStringLiteral("edge_threshold1"), 30.0);
    double edgeT2 = paramDouble(QStringLiteral("edge_threshold2"), 90.0);
    const double houghT = paramDouble(QStringLiteral("hough_threshold"), 30.0);
    const double minLen = paramDouble(QStringLiteral("min_line_length"), 30.0);
    const double maxGap = paramDouble(QStringLiteral("max_line_gap"), 15.0);
    const int blurKs = oddKernelSize(paramInt(QStringLiteral("blur_ksize"), 3));

    m_detectedLines.clear();

    if (learnMode && !m_learnedThresholds.isEmpty()) {
        edgeT1 = m_learnedThresholds.value(0, static_cast<int>(edgeT1));
        edgeT2 = m_learnedThresholds.value(1, static_cast<int>(edgeT2));
    } else if (learnMode && !m_rois.isEmpty()) {
        for (const QRect &roi : m_rois)
            autoLearn(image, roi);
        if (!m_learnedThresholds.isEmpty()) {
            edgeT1 = m_learnedThresholds.value(0, static_cast<int>(edgeT1));
            edgeT2 = m_learnedThresholds.value(1, static_cast<int>(edgeT2));
        }
    }

    QList<QRect> regions = m_rois;
    if (regions.isEmpty())
        regions.append(QRect(0, 0, gray.cols, gray.rows));

    for (const QRect &region : regions) {
        const int rx = qMax(0, region.x());
        const int ry = qMax(0, region.y());
        const int rw = qMin(region.width(), gray.cols - rx);
        const int rh = qMin(region.height(), gray.rows - ry);
        if (rw <= 0 || rh <= 0)
            continue;

        const cv::Mat roiGray = gray(cv::Rect(rx, ry, rw, rh));
        cv::Mat blurred;
        cv::GaussianBlur(roiGray, blurred, cv::Size(blurKs, blurKs), 0);

        cv::Mat edges;
        cv::Canny(blurred, edges, edgeT1, edgeT2);

        std::vector<cv::Vec4i> lines;
        cv::HoughLinesP(edges, lines, 1.0, CV_PI / 180.0, cvRound(houghT), minLen, maxGap);

        if (lines.empty())
            continue;

        const double centerX = rx + rw / 2.0;
        const double centerY = ry + rh / 2.0;

        QVector<QPair<double, QVector<double>>> scored;
        for (const cv::Vec4i &line : lines) {
            const double gx1 = rx + line[0];
            const double gy1 = ry + line[1];
            const double gx2 = rx + line[2];
            const double gy2 = ry + line[3];
            const double midX = (gx1 + gx2) / 2.0;
            const double midY = (gy1 + gy2) / 2.0;
            const double dist = std::hypot(midX - centerX, midY - centerY);
            scored.append(qMakePair(dist, QVector<double>{gx1, gy1, gx2, gy2}));
        }

        if (scored.isEmpty())
            continue;

        std::sort(scored.begin(), scored.end(),
                  [](const QPair<double, QVector<double>> &a, const QPair<double, QVector<double>> &b) {
                      return a.first < b.first;
                  });

        const QVector<double> best = scored.first().second;
        m_detectedLines.append(best);
        cv::line(output, cv::Point(cvRound(best[0]), cvRound(best[1])),
                 cv::Point(cvRound(best[2]), cvRound(best[3])), drawColor, thickness);
    }

    // 绘制 ROI 框
    for (const QRect &roi : m_rois)
        cv::rectangle(output, cv::Rect(roi.x(), roi.y(), roi.width(), roi.height()),
                      cv::Scalar(0, 180, 255), 1);

    setOutput(QStringLiteral("output"), imageValue(output));
    setOutput(QStringLiteral("line_count"), m_detectedLines.size());
    setOutput(QStringLiteral("line_coords"), packLines(m_detectedLines));
    return true;
}

QDialog *LineFinderPlugin::createDialog(const cv::Mat &inputImage, QWidget *parent)
{
    return new LineFinderDialog(this, inputImage, parent);
}

QVariantMap LineFinderPlugin::extraData() const
{
    QVariantList list;
    for (const QRect &roi : m_rois)
        list.append(QVariant(QVariantList{roi.x(), roi.y(), roi.width(), roi.height()}));

    QVariantMap data;
    data.insert(QStringLiteral("rois"), list);
    return data;
}

void LineFinderPlugin::setExtraData(const QVariantMap &data)
{
    const QVariant value = data.value(QStringLiteral("rois"));
    if (!value.isValid() || value.metaType().id() != QMetaType::QVariantList)
        return;

    m_rois.clear();
    for (const QVariant &item : value.toList()) {
        if (item.metaType().id() != QMetaType::QVariantList)
            continue;
        const QVariantList inner = item.toList();
        if (inner.size() < 4)
            continue;
        m_rois.append(QRect(inner.at(0).toInt(), inner.at(1).toInt(), inner.at(2).toInt(),
                            inner.at(3).toInt()));
    }
}

} // namespace OVP
