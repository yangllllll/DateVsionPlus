#include "randompickplugin.h"
#include "randompickdialog.h"

#include "calibdata.h"
#include "detectionlist.h"

#include <QVariantList>

#include <opencv2/imgproc.hpp>

#include <algorithm>

namespace OVP {

static PluginBase *createRandomPickPlugin(QObject *parent)
{
    return new RandomPickPlugin(parent);
}

RandomPickPlugin::RandomPickPlugin(QObject *parent)
    : PluginBase(parent)
{
}

QList<PortDef> RandomPickPlugin::inputPorts() const
{
    return {
        port(QStringLiteral("input"), PortType::Image, QStringLiteral("图像（可选，用于标注显示）")),
        port(QStringLiteral("detections"), PortType::Any, QStringLiteral("YOLO 检测结果")),
    };
}

QList<PortDef> RandomPickPlugin::outputPorts() const
{
    return {
        port(QStringLiteral("output"), PortType::Image, QStringLiteral("标注图像")),
        port(QStringLiteral("robot_x"), PortType::Number, QStringLiteral("抓取点机器人 X")),
        port(QStringLiteral("robot_y"), PortType::Number, QStringLiteral("抓取点机器人 Y")),
        port(QStringLiteral("robot_point"), PortType::Any, QStringLiteral("机器人坐标 [x, y]")),
        port(QStringLiteral("pixel_x"), PortType::Number, QStringLiteral("抓取点图像 X")),
        port(QStringLiteral("pixel_y"), PortType::Number, QStringLiteral("抓取点图像 Y")),
        port(QStringLiteral("class_id"), PortType::Number, QStringLiteral("目标类别 id")),
        port(QStringLiteral("class_name"), PortType::String, QStringLiteral("目标类别名")),
        port(QStringLiteral("confidence"), PortType::Number, QStringLiteral("目标置信度")),
        port(QStringLiteral("pick_index"), PortType::Number,
             QStringLiteral("实际选取的目标序号（0 起，按置信度降序）")),
    };
}

QList<ParamDef> RandomPickPlugin::inputParams() const
{
    return {
        fileParam(QStringLiteral("calib_file"), QStringLiteral("标定文件"), QString(),
                  QStringLiteral("N点标定保存的 .ncal 文件（对话框中选择）")),
        intParam(QStringLiteral("pick_index"), QStringLiteral("抓取目标序号"), 0, 0, 99, 1,
                 QStringLiteral("0 = 第一个（置信度最高）；超出目标数量时本次执行失败")),
        floatParam(QStringLiteral("min_score"), QStringLiteral("最小置信度"), 0.0, 0.0, 1.0, 0.01,
                   QStringLiteral("低于该置信度的目标不参与抓取")),
        stringParam(QStringLiteral("class_filter"), QStringLiteral("类别过滤"), QString(),
                    QStringLiteral("只抓取指定类别，填类别 id 或名称，逗号分隔；留空表示全部")),
        boolParam(QStringLiteral("draw"), QStringLiteral("绘制抓取点"), true,
                  QStringLiteral("在输出图像上标注所选目标")),
        intParam(QStringLiteral("line_thickness"), QStringLiteral("线宽"), 2, 1, 5, 1,
                 QStringLiteral("标注线宽")),
    };
}

bool RandomPickPlugin::execute()
{
    clearError();

    const QString calibPath = paramString(QStringLiteral("calib_file")).trimmed();
    if (calibPath.isEmpty()) {
        setError(QStringLiteral("未选择标定文件：请先在参数或对话框中选择 N点标定 保存的 .ncal 文件"));
        return false;
    }

    QString error;
    const std::shared_ptr<const CALIB::CalibData> calib = CALIB::loadCached(calibPath, &error);
    if (!calib) {
        setError(error);
        return false;
    }
    if (!calib->valid) {
        setError(QStringLiteral("标定文件无效（未包含有效的变换矩阵）：%1").arg(calibPath));
        return false;
    }

    const QVariant detValue = input(QStringLiteral("detections"));
    if (!detValue.isValid()) {
        setError(QStringLiteral("没有检测结果输入，请连接 YOLO目标识别的 detections 输出"));
        return false;
    }

    QList<DETECT::Item> items;
    if (!DETECT::parse(detValue, items, &error)) {
        setError(error);
        return false;
    }

    const float minScore = static_cast<float>(paramDouble(QStringLiteral("min_score"), 0.0));
    const QString classFilter = paramString(QStringLiteral("class_filter"));

    QList<DETECT::Item> candidates;
    candidates.reserve(items.size());
    for (const DETECT::Item &item : items) {
        if (item.confidence < minScore)
            continue;
        if (!DETECT::matchesFilter(item, classFilter))
            continue;
        candidates.append(item);
    }

    if (candidates.isEmpty()) {
        setError(QStringLiteral("没有满足条件的抓取目标（检测到 %1 个目标）").arg(items.size()));
        return false;
    }

    // 统一按置信度降序，保证「第一个」就是最可信的目标
    std::sort(candidates.begin(), candidates.end(),
              [](const DETECT::Item &a, const DETECT::Item &b) {
                  return a.confidence > b.confidence;
              });

    const int pickIndex = paramInt(QStringLiteral("pick_index"), 0);
    if (pickIndex < 0 || pickIndex >= candidates.size()) {
        setError(QStringLiteral("抓取目标序号 %1 超出范围（当前可抓取 %2 个）")
                     .arg(pickIndex)
                     .arg(candidates.size()));
        return false;
    }

    const DETECT::Item target = candidates.at(pickIndex);
    const QPointF robot = calib->apply(target.center);

    m_lastCalibrationFile = calibPath;

    setOutput(QStringLiteral("robot_x"), robot.x());
    setOutput(QStringLiteral("robot_y"), robot.y());
    setOutput(QStringLiteral("robot_point"), QVariantList{robot.x(), robot.y()});
    setOutput(QStringLiteral("pixel_x"), target.center.x());
    setOutput(QStringLiteral("pixel_y"), target.center.y());
    setOutput(QStringLiteral("class_id"), target.classId);
    setOutput(QStringLiteral("class_name"), target.className);
    setOutput(QStringLiteral("confidence"), static_cast<double>(target.confidence));
    setOutput(QStringLiteral("pick_index"), pickIndex);

    const cv::Mat image = inputImage(QStringLiteral("input"));
    if (!image.empty()) {
        cv::Mat annotated = image.clone();

        if (paramBool(QStringLiteral("draw"), true)) {
            const int thickness = paramInt(QStringLiteral("line_thickness"), 2);
            const cv::Scalar gray(170, 170, 170);
            const cv::Scalar green(0, 255, 0);

            for (const DETECT::Item &item : items) {
                cv::circle(annotated,
                           cv::Point(cvRound(item.center.x()), cvRound(item.center.y())),
                           6, gray, 2, cv::LINE_AA);
            }

            const cv::Point center(cvRound(target.center.x()), cvRound(target.center.y()));
            cv::drawMarker(annotated, center, green, cv::MARKER_CROSS, 26, thickness, cv::LINE_AA);
            cv::circle(annotated, center, 12, green, thickness, cv::LINE_AA);

            const std::string label = QStringLiteral("PICK %1  (%2, %3)")
                                          .arg(target.className.isEmpty()
                                                   ? QString::number(target.classId)
                                                   : target.className)
                                          .arg(robot.x(), 0, 'f', 2)
                                          .arg(robot.y(), 0, 'f', 2)
                                          .toStdString();
            cv::putText(annotated, label, center + cv::Point(14, -14),
                        cv::FONT_HERSHEY_SIMPLEX, 0.6, green, 2, cv::LINE_AA);
        }

        setOutput(QStringLiteral("output"), imageValue(annotated));
    }

    return true;
}

QDialog *RandomPickPlugin::createDialog(const cv::Mat &inputImage, QWidget *parent)
{
    return new RandomPickDialog(this, inputImage, parent);
}

} // namespace OVP

// ------------------------------------------------------------------ DLL 导出

QList<OpenVisionPluginProvider::Entry> RandomPickPluginProvider::availablePlugins() const
{
    return {OpenVisionPluginProvider::Entry{OVP::createRandomPickPlugin}};
}
