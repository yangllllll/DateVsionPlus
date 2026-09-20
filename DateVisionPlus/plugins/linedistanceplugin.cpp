#include "linedistanceplugin.h"

#include "../core/cvutils.h"
#include "../core/plugintypes.h"

#include <QPointF>

#include "../core/opencvcompat.h"

#include <cmath>

namespace OVP {

LineDistancePlugin::LineDistancePlugin(QObject *parent)
    : PluginBase(parent)
{
}

QList<PortDef> LineDistancePlugin::inputPorts() const
{
    return {port(QStringLiteral("lines1"), PortType::Any, QStringLiteral("线坐标集合1")),
            port(QStringLiteral("lines2"), PortType::Any, QStringLiteral("线坐标集合2")),
            port(QStringLiteral("input"), PortType::Image, QStringLiteral("参考图像(可选)"))};
}

QList<PortDef> LineDistancePlugin::outputPorts() const
{
    return {port(QStringLiteral("output"), PortType::Image, QStringLiteral("标注图像")),
            port(QStringLiteral("horizontal_dist"), PortType::Number, QStringLiteral("水平距离")),
            port(QStringLiteral("vertical_dist"), PortType::Number, QStringLiteral("垂直距离")),
            port(QStringLiteral("all_distances"), PortType::Any, QStringLiteral("所有距离详情")),
            port(QStringLiteral("pair_count"), PortType::Number, QStringLiteral("匹配对数"))};
}

QList<ParamDef> LineDistancePlugin::inputParams() const
{
    return {
        choiceParam(QStringLiteral("draw_color"), QStringLiteral("标注颜色"), QStringLiteral("绿色"),
                    {QStringLiteral("绿色"), QStringLiteral("红色"), QStringLiteral("蓝色"),
                     QStringLiteral("黄色"), QStringLiteral("青色"), QStringLiteral("白色")},
                    QStringLiteral("标注线的颜色")),
        intParam(QStringLiteral("line_thickness"), QStringLiteral("线宽"), 2, 1, 5, 1,
                 QStringLiteral("标注线宽")),
        floatParam(QStringLiteral("font_scale"), QStringLiteral("字体大小"), 0.6, 0.3, 2.0, 0.1,
                   QStringLiteral("标注文字大小"))
    };
}

bool LineDistancePlugin::execute()
{
    clearError();

    const QVariant lines1Value = input(QStringLiteral("lines1"));
    const QVariant lines2Value = input(QStringLiteral("lines2"));
    const cv::Mat image = inputImage(QStringLiteral("input"));

    if (!lines1Value.isValid() || !lines2Value.isValid()) {
        setError(QStringLiteral("缺少线坐标输入，请连接两个线查找工具的输出"));
        return false;
    }

    const QList<QVector<double>> lines1 = unpackLines(lines1Value);
    const QList<QVector<double>> lines2 = unpackLines(lines2Value);

    if (lines1.isEmpty() || lines2.isEmpty()) {
        setError(QStringLiteral("线坐标为空，请先运行线查找工具"));
        setOutput(QStringLiteral("output"), imageValue(image));
        setOutput(QStringLiteral("horizontal_dist"), 0.0);
        setOutput(QStringLiteral("vertical_dist"), 0.0);
        setOutput(QStringLiteral("all_distances"), packLines({}));
        setOutput(QStringLiteral("pair_count"), 0);
        return false;
    }

    const cv::Scalar drawColor = bgrColor(paramString(QStringLiteral("draw_color"), QStringLiteral("绿色")));
    const int thickness = paramInt(QStringLiteral("line_thickness"), 2);
    const double fontScale = paramDouble(QStringLiteral("font_scale"), 0.6);

    m_distances.clear();

    cv::Mat output;
    if (!image.empty()) {
        output = ensureBgr(image);
    } else {
        double maxX = 0.0;
        double maxY = 0.0;
        for (const QVector<double> &l : lines1 + lines2) {
            maxX = qMax(maxX, qMax(l.value(0), l.value(2)));
            maxY = qMax(maxY, qMax(l.value(1), l.value(3)));
        }
        const int rows = qMax(static_cast<int>(maxY) + 50, 200);
        const int cols = qMax(static_cast<int>(maxX) + 50, 200);
        output = cv::Mat(rows, cols, CV_8UC3, cv::Scalar(30, 30, 30));
    }

    double totalH = 0.0;
    double totalV = 0.0;
    int count = 0;

    for (int i = 0; i < lines1.size(); ++i) {
        const QVector<double> &l1 = lines1.at(i);
        const double midX1 = (l1.value(0) + l1.value(2)) / 2.0;
        const double midY1 = (l1.value(1) + l1.value(3)) / 2.0;

        for (int j = 0; j < lines2.size(); ++j) {
            const QVector<double> &l2 = lines2.at(j);
            const double midX2 = (l2.value(0) + l2.value(2)) / 2.0;
            const double midY2 = (l2.value(1) + l2.value(3)) / 2.0;

            const double hDist = std::abs(midX2 - midX1);
            const double vDist = std::abs(midY2 - midY1);

            m_distances.append({hDist, vDist});
            totalH += hDist;
            totalV += vDist;
            ++count;

            cv::line(output, cv::Point(cvRound(l1.value(0)), cvRound(l1.value(1))),
                     cv::Point(cvRound(l1.value(2)), cvRound(l1.value(3))), cv::Scalar(0, 180, 255),
                     thickness);
            cv::line(output, cv::Point(cvRound(l2.value(0)), cvRound(l2.value(1))),
                     cv::Point(cvRound(l2.value(2)), cvRound(l2.value(3))), drawColor, thickness);

            const cv::Point midA(cvRound(midX1), cvRound(midY1));
            const cv::Point midB(cvRound(midX2), cvRound(midY2));
            cv::line(output, midA, midB, cv::Scalar(255, 255, 255), 1, cv::LINE_AA);

            const cv::Point textPos(midA.x + 10, midA.y - 10);
            cv::putText(output, QStringLiteral("dH=%1").arg(hDist, 0, 'f', 1).toStdString(), textPos,
                        cv::FONT_HERSHEY_SIMPLEX, fontScale, cv::Scalar(255, 255, 255), 1);
            cv::putText(output, QStringLiteral("dV=%1").arg(vDist, 0, 'f', 1).toStdString(),
                        cv::Point(textPos.x, textPos.y + static_cast<int>(20 * fontScale)),
                        cv::FONT_HERSHEY_SIMPLEX, fontScale, cv::Scalar(255, 255, 255), 1);
        }
    }

    const double avgH = count > 0 ? (totalH / count) : 0.0;
    const double avgV = count > 0 ? (totalV / count) : 0.0;

    setOutput(QStringLiteral("output"), imageValue(output));
    setOutput(QStringLiteral("horizontal_dist"), avgH);
    setOutput(QStringLiteral("vertical_dist"), avgV);
    setOutput(QStringLiteral("all_distances"), packLines(m_distances));
    setOutput(QStringLiteral("pair_count"), count);
    return true;
}

} // namespace OVP
