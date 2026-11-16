#include "yolodetectplugin.h"
#include "yolodetectdialog.h"

#include <QHash>
#include <QVariantList>
#include <QVariantMap>

namespace OVP {
namespace {

YOLO::Backend backendFromText(const QString &text)
{
    if (text.contains(QStringLiteral("ONNX"), Qt::CaseInsensitive))
        return YOLO::Backend::OnnxRuntime;
    if (text.contains(QStringLiteral("OpenCV"), Qt::CaseInsensitive))
        return YOLO::Backend::OpenCvDnn;
    return YOLO::Backend::Auto;
}

YOLO::OutputFormat formatFromText(const QString &text)
{
    if (text.contains(QStringLiteral("NMS"), Qt::CaseInsensitive))
        return YOLO::OutputFormat::Nms;
    if (text.contains(QStringLiteral("v5"), Qt::CaseInsensitive))
        return YOLO::OutputFormat::YoloV5;
    if (text.contains(QStringLiteral("v8"), Qt::CaseInsensitive)
        || text.contains(QStringLiteral("11"))) {
        return YOLO::OutputFormat::YoloV8;
    }
    return YOLO::OutputFormat::Auto;
}

/** 解析类别过滤：支持 id 或类别名，逗号分隔 */
bool parseClassFilter(const QString &text, const QStringList &names, QList<int> &ids,
                      QString *error)
{
    ids.clear();

    QString trimmed = text.trimmed();
    if (trimmed.isEmpty())
        return true;
    trimmed.replace(QStringLiteral("，"), QStringLiteral(","));

    QHash<QString, int> nameToId;
    for (int i = 0; i < names.size(); ++i)
        nameToId.insert(names.at(i).trimmed().toLower(), i);

    const QStringList tokens = trimmed.split(QLatin1Char(','), Qt::SkipEmptyParts);
    for (QString token : tokens) {
        token = token.trimmed();
        if (token.isEmpty())
            continue;

        bool ok = false;
        const int id = token.toInt(&ok);
        if (ok) {
            ids.append(id);
            continue;
        }

        const auto it = nameToId.constFind(token.toLower());
        if (it != nameToId.constEnd()) {
            ids.append(it.value());
            continue;
        }

        if (error) {
            *error = QStringLiteral("未知的类别过滤项：%1\n"
                                    "可填类别 id，或提供「类别名称文件」后填类别名")
                        .arg(token);
        }
        return false;
    }
    return true;
}

/** 检测结果打包为 QVariantList<QVariantMap> */
QVariant packDetections(const QList<YOLO::Detection> &detections)
{
    QVariantList list;
    list.reserve(detections.size());

    for (const YOLO::Detection &det : detections) {
        const int x1 = det.box.x;
        const int y1 = det.box.y;
        const int x2 = det.box.x + det.box.width;
        const int y2 = det.box.y + det.box.height;

        QVariantMap map;
        map.insert(QStringLiteral("class_id"), det.classId);
        map.insert(QStringLiteral("class_name"), det.className);
        map.insert(QStringLiteral("confidence"), static_cast<double>(det.confidence));
        map.insert(QStringLiteral("xyxy"), QVariantList{x1, y1, x2, y2});
        map.insert(QStringLiteral("center"), QVariantList{det.center.x, det.center.y});
        // 扁平字段，方便下游直接取值
        map.insert(QStringLiteral("x1"), x1);
        map.insert(QStringLiteral("y1"), y1);
        map.insert(QStringLiteral("x2"), x2);
        map.insert(QStringLiteral("y2"), y2);
        map.insert(QStringLiteral("center_x"), det.center.x);
        map.insert(QStringLiteral("center_y"), det.center.y);
        list.append(map);
    }
    return list;
}

} // namespace

static PluginBase *createYoloDetectPlugin(QObject *parent)
{
    return new YoloDetectPlugin(parent);
}

// ------------------------------------------------------------ YoloDetectPlugin

YoloDetectPlugin::YoloDetectPlugin(QObject *parent)
    : PluginBase(parent)
{
}

QList<PortDef> YoloDetectPlugin::inputPorts() const
{
    return {port(QStringLiteral("input"), PortType::Image, QStringLiteral("输入图像"))};
}

QList<PortDef> YoloDetectPlugin::outputPorts() const
{
    return {
        port(QStringLiteral("output"), PortType::Image, QStringLiteral("标注图像")),
        port(QStringLiteral("det_count"), PortType::Number, QStringLiteral("目标数量")),
        port(QStringLiteral("detections"), PortType::Any,
             QStringLiteral("检测结果 [{class_id,class_name,confidence,xyxy,center,...},...]")),
        port(QStringLiteral("top_class"), PortType::String, QStringLiteral("最高置信度类别")),
        port(QStringLiteral("top_score"), PortType::Number, QStringLiteral("最高置信度")),
        port(QStringLiteral("device_used"), PortType::String, QStringLiteral("实际推理设备")),
    };
}

QList<ParamDef> YoloDetectPlugin::inputParams() const
{
    return {
        fileParam(QStringLiteral("model_path"), QStringLiteral("模型文件"), QString(),
                  QStringLiteral("YOLO ONNX 模型（.onnx）。.pt 需先导出："
                                 "yolo export model=yolo11n.pt format=onnx")),
        choiceParam(QStringLiteral("device"), QStringLiteral("推理设备"), QStringLiteral("自动"),
                    {QStringLiteral("自动"), QStringLiteral("CPU"), QStringLiteral("CUDA:0"),
                     QStringLiteral("CUDA:1"), QStringLiteral("CUDA:2"), QStringLiteral("CUDA:3")},
                    QStringLiteral("自动：OpenCV 带 CUDA 且检测到 GPU 时使用 CUDA，否则 CPU")),
        choiceParam(QStringLiteral("backend"), QStringLiteral("推理后端"), QStringLiteral("自动"),
                    {QStringLiteral("自动"), QStringLiteral("OpenCV DNN"),
                     QStringLiteral("ONNX Runtime")},
                    QStringLiteral("自动：可用 ONNX Runtime 时优先使用（支持 GPU），"
                                   "否则 OpenCV DNN")),
        stringParam(QStringLiteral("ort_path"), QStringLiteral("ONNX Runtime 路径"), QString(),
                    QStringLiteral("onnxruntime.dll 或其所在目录（GPU 版才支持 CUDA）；"
                                   "留空自动探测，也可用环境变量 ONNXRUNTIME_DIR")),
        intParam(QStringLiteral("imgsz"), QStringLiteral("推理尺寸"), 640, 32, 2048, 32,
                 QStringLiteral("推理输入尺寸（需为 32 的倍数）")),
        floatParam(QStringLiteral("conf"), QStringLiteral("置信度阈值"), 0.25, 0.0, 1.0, 0.01,
                   QStringLiteral("低于该置信度的目标会被过滤")),
        floatParam(QStringLiteral("iou"), QStringLiteral("NMS IoU阈值"), 0.45, 0.0, 1.0, 0.01,
                   QStringLiteral("非极大值抑制的 IoU 阈值")),
        intParam(QStringLiteral("max_det"), QStringLiteral("最大目标数"), 300, 1, 1000, 1,
                 QStringLiteral("单张图最多保留的目标数量")),
        stringParam(QStringLiteral("class_filter"), QStringLiteral("类别过滤"), QString(),
                    QStringLiteral("只保留指定类别，填类别 id 或名称，逗号分隔，如：0,2 或 person,car；"
                                   "留空表示全部")),
        stringParam(QStringLiteral("class_names"), QStringLiteral("类别名称文件"), QString(),
                    QStringLiteral("每行一个类别名；留空则在模型同目录查找 "
                                   "<模型名>.names.txt / classes.txt / labels.txt / names.txt")),
        choiceParam(QStringLiteral("output_format"), QStringLiteral("输出格式"),
                    QStringLiteral("自动"),
                    {QStringLiteral("自动"), QStringLiteral("YOLOv8/v11"),
                     QStringLiteral("YOLOv5"), QStringLiteral("已NMS")},
                    QStringLiteral("自动按输出张量形状判断；识别结果异常时请手动指定")),
        boolParam(QStringLiteral("half"), QStringLiteral("半精度推理(FP16)"), false,
                  QStringLiteral("仅 OpenCV DNN + CUDA 有效；ONNX Runtime 后端忽略此项")),
        boolParam(QStringLiteral("empty_as_ng"), QStringLiteral("空结果判NG"), true,
                  QStringLiteral("勾选后，未检测到目标时节点返回失败(NG)")),
        boolParam(QStringLiteral("draw"), QStringLiteral("绘制结果"), true,
                  QStringLiteral("是否在输出图像上绘制检测框")),
        boolParam(QStringLiteral("show_label"), QStringLiteral("显示类别标签"), true,
                  QStringLiteral("是否绘制类别名与置信度")),
        intParam(QStringLiteral("line_thickness"), QStringLiteral("线宽"), 2, 1, 5, 1,
                 QStringLiteral("检测框线宽")),
    };
}

bool YoloDetectPlugin::execute()
{
    clearError();
    m_lastDetections.clear();
    m_lastInferenceMs = 0.0;

    const cv::Mat src = inputImage(QStringLiteral("input"));
    if (src.empty()) {
        setError(QStringLiteral("没有输入图像"));
        return false;
    }

    const QString modelPath = paramString(QStringLiteral("model_path")).trimmed();
    if (modelPath.isEmpty()) {
        setError(QStringLiteral("请先设置 YOLO 模型文件"));
        return false;
    }

    QString error;

    // ONNX Runtime 路径；在首次加载 DLL 之前设置才生效
    YOLO::OrtBackend::setPreferredLibrary(paramString(QStringLiteral("ort_path")));

    const YOLO::DeviceSelection device =
        YOLO::Detector::parseDevice(paramString(QStringLiteral("device"), QStringLiteral("自动")));

    const std::shared_ptr<YOLO::Detector> detector =
        YOLO::Detector::acquire(modelPath, paramString(QStringLiteral("class_names")), &error);
    if (!detector) {
        setError(error);
        return false;
    }

    YOLO::DetectOptions options;
    options.imgsz = paramInt(QStringLiteral("imgsz"), 640);
    options.confThreshold = static_cast<float>(paramDouble(QStringLiteral("conf"), 0.25));
    options.iouThreshold = static_cast<float>(paramDouble(QStringLiteral("iou"), 0.45));
    options.maxDet = paramInt(QStringLiteral("max_det"), 300);
    options.backend = backendFromText(paramString(QStringLiteral("backend"),
                                                  QStringLiteral("自动")));
    options.useCuda = device.cuda;
    options.cudaDevice = device.deviceIndex;
    options.deviceAuto = device.automatic;
    options.half = paramBool(QStringLiteral("half"), false) && device.cuda;
    options.format = formatFromText(paramString(QStringLiteral("output_format"),
                                                QStringLiteral("自动")));

    if (!parseClassFilter(paramString(QStringLiteral("class_filter")), detector->classNames(),
                          options.classFilter, &error)) {
        setError(error);
        return false;
    }

    QList<YOLO::Detection> detections;
    if (!detector->detect(src, options, detections, &error)) {
        setError(error);
        return false;
    }

    m_lastDetections = detections;
    m_deviceUsed = detector->lastDevice();
    m_lastInferenceMs = detector->lastInferenceMs();

    const bool showLabel = paramBool(QStringLiteral("show_label"), true);
    const int thickness = paramInt(QStringLiteral("line_thickness"), 2);
    const cv::Mat annotated = paramBool(QStringLiteral("draw"), true)
                                  ? YOLO::drawDetections(src, detections, showLabel, thickness)
                                  : src.clone();

    setOutput(QStringLiteral("output"), imageValue(annotated));
    setOutput(QStringLiteral("det_count"), static_cast<int>(detections.size()));
    setOutput(QStringLiteral("detections"), packDetections(detections));
    setOutput(QStringLiteral("top_class"),
              detections.isEmpty() ? QString() : detections.first().className);
    setOutput(QStringLiteral("top_score"),
              detections.isEmpty() ? 0.0 : static_cast<double>(detections.first().confidence));
    setOutput(QStringLiteral("device_used"), m_deviceUsed);

    if (detections.isEmpty() && paramBool(QStringLiteral("empty_as_ng"), true)) {
        setError(QStringLiteral("未检测到任何目标"));
        return false;
    }

    return true;
}

QDialog *YoloDetectPlugin::createDialog(const cv::Mat &inputImage, QWidget *parent)
{
    return new YoloDetectDialog(this, inputImage, parent);
}

} // namespace OVP

// ------------------------------------------------------------------ DLL 导出

QList<OpenVisionPluginProvider::Entry> YoloDetectPluginProvider::availablePlugins() const
{
    return {OpenVisionPluginProvider::Entry{OVP::createYoloDetectPlugin}};
}
