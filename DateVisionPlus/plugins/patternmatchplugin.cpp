#include "patternmatchplugin.h"

#include "../core/cvutils.h"
#include "../core/plugintypes.h"
#include "../dialogs/patternmatchdialog.h"

#include <QByteArray>
#include <QDialog>

#include "../core/opencvcompat.h"
#include <opencv2/imgcodecs.hpp>

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

namespace OVP {

PatternMatchPlugin::PatternMatchPlugin(QObject *parent)
    : PluginBase(parent)
{
}

QList<PortDef> PatternMatchPlugin::inputPorts() const
{
    return {port(QStringLiteral("input"), PortType::Image, QStringLiteral("输入图像"))};
}

QList<PortDef> PatternMatchPlugin::outputPorts() const
{
    return {port(QStringLiteral("output"), PortType::Image, QStringLiteral("匹配结果图像")),
            port(QStringLiteral("match_count"), PortType::Number, QStringLiteral("匹配数量")),
            port(QStringLiteral("match_positions"), PortType::Any, QStringLiteral("匹配位置列表"))};
}

QList<ParamDef> PatternMatchPlugin::inputParams() const
{
    return {
        choiceParam(QStringLiteral("method"), QStringLiteral("匹配方法"),
                    QStringLiteral("CCOEFF_NORMED"),
                    {QStringLiteral("CCOEFF"), QStringLiteral("CCOEFF_NORMED"),
                     QStringLiteral("CCORR"), QStringLiteral("CCORR_NORMED"),
                     QStringLiteral("SQDIFF"), QStringLiteral("SQDIFF_NORMED")},
                    QStringLiteral("模板匹配方法")),
        floatParam(QStringLiteral("threshold"), QStringLiteral("匹配阈值"), 0.8, 0.0, 1.0, 0.01,
                   QStringLiteral("匹配置信度阈值")),
        intParam(QStringLiteral("max_matches"), QStringLiteral("最大匹配数"), 10, 1, 100, 1,
                 QStringLiteral("最多返回的匹配数")),
        choiceParam(QStringLiteral("draw_color"), QStringLiteral("标记颜色"), QStringLiteral("绿色"),
                    {QStringLiteral("绿色"), QStringLiteral("红色"), QStringLiteral("蓝色"),
                     QStringLiteral("黄色"), QStringLiteral("青色"), QStringLiteral("白色")},
                    QStringLiteral("绘制匹配框的颜色")),
        intParam(QStringLiteral("line_thickness"), QStringLiteral("线宽"), 2, 1, 5, 1,
                 QStringLiteral("标注线宽"))
    };
}

cv::Mat PatternMatchPlugin::trainTemplate(const cv::Mat &image, const QRect &roi)
{
    if (image.empty() || roi.width() < 5 || roi.height() < 5)
        return cv::Mat();

    const int rx = qMax(0, roi.x());
    const int ry = qMax(0, roi.y());
    const int rw = qMin(roi.width(), image.cols - rx);
    const int rh = qMin(roi.height(), image.rows - ry);
    if (rw < 5 || rh < 5)
        return cv::Mat();

    const cv::Mat cropped = image(cv::Rect(rx, ry, rw, rh));
    const cv::Mat gray = ensureGray(cropped).clone();

    m_templateRoi = QRect(rx, ry, rw, rh);
    m_templateImage = gray;
    return gray;
}

bool PatternMatchPlugin::execute()
{
    clearError();

    const cv::Mat image = inputImage(QStringLiteral("input"));
    if (image.empty()) {
        setError(QStringLiteral("没有输入图像"));
        return false;
    }

    if (m_templateImage.empty()) {
        setError(QStringLiteral("没有模板图像，请先双击节点框选ROI训练模板"));
        return false;
    }

    const cv::Mat gray = ensureGray(image);
    const cv::Mat &templ = m_templateImage;

    if (templ.rows > gray.rows || templ.cols > gray.cols) {
        setOutput(QStringLiteral("output"), imageValue(ensureBgr(image)));
        setOutput(QStringLiteral("match_count"), 0);
        setOutput(QStringLiteral("match_positions"), packPoints({}));
        setError(QStringLiteral("模板尺寸大于输入图像"));
        return false;
    }

    const cv::Scalar drawColor = bgrColor(paramString(QStringLiteral("draw_color"), QStringLiteral("绿色")));
    const int thickness = paramInt(QStringLiteral("line_thickness"), 2);

    const QString methodName = paramString(QStringLiteral("method"), QStringLiteral("CCOEFF_NORMED"));
    int method = cv::TM_CCOEFF_NORMED;
    if (methodName == QStringLiteral("CCOEFF")) method = cv::TM_CCOEFF;
    else if (methodName == QStringLiteral("CCORR")) method = cv::TM_CCORR;
    else if (methodName == QStringLiteral("CCORR_NORMED")) method = cv::TM_CCORR_NORMED;
    else if (methodName == QStringLiteral("SQDIFF")) method = cv::TM_SQDIFF;
    else if (methodName == QStringLiteral("SQDIFF_NORMED")) method = cv::TM_SQDIFF_NORMED;

    const double threshold = paramDouble(QStringLiteral("threshold"), 0.8);
    const int maxMatches = paramInt(QStringLiteral("max_matches"), 10);

    cv::Mat result;
    cv::matchTemplate(gray, templ, result, method);

    const int h = templ.rows;
    const int w = templ.cols;
    const bool isSqDiff = (method == cv::TM_SQDIFF || method == cv::TM_SQDIFF_NORMED);

    // 收集候选位置
    struct Candidate
    {
        int x;
        int y;
        float score;
    };
    std::vector<Candidate> candidates;

    for (int y = 0; y < result.rows; ++y) {
        const float *row = result.ptr<float>(y);
        for (int x = 0; x < result.cols; ++x) {
            const float score = row[x];
            if (isSqDiff ? (score <= static_cast<float>(threshold))
                         : (score >= static_cast<float>(threshold))) {
                candidates.push_back({x, y, score});
            }
        }
    }

    std::sort(candidates.begin(), candidates.end(),
              [isSqDiff](const Candidate &a, const Candidate &b) {
                  return isSqDiff ? (a.score < b.score) : (a.score > b.score);
              });

    // 非极大值抑制
    QList<QPointF> positions;
    for (const Candidate &c : candidates) {
        if (positions.size() >= maxMatches)
            break;
        bool overlap = false;
        for (const QPointF &p : positions) {
            if (std::abs(c.x - p.x()) < w && std::abs(c.y - p.y()) < h) {
                overlap = true;
                break;
            }
        }
        if (!overlap)
            positions.append(QPointF(c.x, c.y));
    }

    cv::Mat output = ensureBgr(image);
    for (int i = 0; i < positions.size(); ++i) {
        const QPointF &p = positions.at(i);
        const cv::Rect rect(cvRound(p.x()), cvRound(p.y()), w, h);
        cv::rectangle(output, rect, drawColor, thickness);
        cv::putText(output, std::to_string(i + 1),
                    cv::Point(rect.x, std::max(rect.y - 5, 0)), cv::FONT_HERSHEY_SIMPLEX, 0.5,
                    drawColor, 1);
    }

    setOutput(QStringLiteral("output"), imageValue(output));
    setOutput(QStringLiteral("match_count"), positions.size());
    setOutput(QStringLiteral("match_positions"), packPoints(positions));

    if (positions.isEmpty()) {
        setError(QStringLiteral("未匹配到任何目标"));
        return false;
    }
    return true;
}

QDialog *PatternMatchPlugin::createDialog(const cv::Mat &inputImage, QWidget *parent)
{
    return new PatternMatchDialog(this, inputImage, parent);
}

QVariantMap PatternMatchPlugin::extraData() const
{
    QVariantMap data;
    if (m_templateRoi.isValid()) {
        data.insert(QStringLiteral("template_roi"),
                    QVariant(QVariantList{m_templateRoi.x(), m_templateRoi.y(),
                                          m_templateRoi.width(), m_templateRoi.height()}));
    }

    if (!m_templateImage.empty()) {
        std::vector<uchar> buffer;
        cv::imencode(".png", m_templateImage, buffer);
        const QByteArray bytes(reinterpret_cast<const char *>(buffer.data()),
                               static_cast<qsizetype>(buffer.size()));
        data.insert(QStringLiteral("template_base64"), QString::fromLatin1(bytes.toBase64()));
    }
    return data;
}

void PatternMatchPlugin::setExtraData(const QVariantMap &data)
{
    const QVariant roiValue = data.value(QStringLiteral("template_roi"));
    if (roiValue.isValid() && roiValue.metaType().id() == QMetaType::QVariantList) {
        const QVariantList list = roiValue.toList();
        if (list.size() >= 4) {
            m_templateRoi = QRect(list.at(0).toInt(), list.at(1).toInt(), list.at(2).toInt(),
                                  list.at(3).toInt());
        }
    }

    const QVariant base64 = data.value(QStringLiteral("template_base64"));
    if (base64.isValid()) {
        const QByteArray bytes = QByteArray::fromBase64(base64.toString().toLatin1());
        const std::vector<uchar> buffer(bytes.begin(), bytes.end());
        if (!buffer.empty()) {
            const cv::Mat decoded = cv::imdecode(buffer, cv::IMREAD_GRAYSCALE);
            if (!decoded.empty())
                m_templateImage = decoded;
        }
    }
}

} // namespace OVP
