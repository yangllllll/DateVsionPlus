#include "yolodetector.h"

#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QSet>

#include <opencv2/core/cuda.hpp>
#include <opencv2/imgproc.hpp>

#include <algorithm>
#include <cmath>
#include <vector>

namespace YOLO {
namespace {

constexpr int kPadValue = 114;   // letterbox 填充灰度

/** letterbox 变换参数 */
struct Letterbox
{
    float ratio = 1.0f;
    float padX = 0.0f;
    float padY = 0.0f;
};

/** 保持长宽比缩放到 size x size，四周填充灰边 */
cv::Mat letterbox(const cv::Mat &src, int size, Letterbox &info)
{
    const int w = src.cols;
    const int h = src.rows;

    info.ratio = std::min(static_cast<float>(size) / static_cast<float>(w),
                          static_cast<float>(size) / static_cast<float>(h));

    const int newW = std::max(1, static_cast<int>(std::lround(w * info.ratio)));
    const int newH = std::max(1, static_cast<int>(std::lround(h * info.ratio)));

    cv::Mat resized;
    if (newW == w && newH == h)
        resized = src;
    else
        cv::resize(src, resized, cv::Size(newW, newH), 0, 0, cv::INTER_LINEAR);

    const int dx = (size - newW) / 2;
    const int dy = (size - newH) / 2;
    info.padX = static_cast<float>(dx);
    info.padY = static_cast<float>(dy);

    cv::Mat canvas(size, size, CV_8UC3, cv::Scalar(kPadValue, kPadValue, kPadValue));
    resized.copyTo(canvas(cv::Rect(dx, dy, newW, newH)));
    return canvas;
}

/** 输出张量的形状描述，用于报错 */
QString shapeText(const cv::Mat &mat)
{
    QStringList dims;
    for (int i = 0; i < mat.dims; ++i)
        dims.append(QString::number(mat.size[i]));
    if (mat.dims == 2)
        return QStringLiteral("[%1, %2]").arg(mat.rows).arg(mat.cols);
    return QStringLiteral("[%1]").arg(dims.join(QStringLiteral(", ")));
}

/** 按类别分别做 NMS，返回保留目标在 detections 中的下标 */
std::vector<int> nmsPerClass(const QList<Detection> &detections, float conf, float iou)
{
    QHash<int, std::vector<int>> groups;
    for (int i = 0; i < detections.size(); ++i)
        groups[detections[i].classId].push_back(i);

    std::vector<int> kept;
    kept.reserve(static_cast<size_t>(detections.size()));

    for (auto it = groups.constBegin(); it != groups.constEnd(); ++it) {
        std::vector<cv::Rect> boxes;
        std::vector<float> scores;
        boxes.reserve(it.value().size());
        scores.reserve(it.value().size());
        for (int index : it.value()) {
            boxes.push_back(detections[index].box);
            scores.push_back(detections[index].confidence);
        }

        std::vector<int> indices;
        cv::dnn::NMSBoxes(boxes, scores, conf, iou, indices);
        for (int index : indices)
            kept.push_back(it.value()[static_cast<size_t>(index)]);
    }
    return kept;
}

/**
 * @brief 解析网络输出
 *
 * 统一按「行 = 候选框，列 = 属性」处理：
 *   - YOLOv8/11: [1, 4+nc, N] -> channelsFirst
 *   - YOLOv5   : [1, N, 5+nc]
 *   - 已 NMS   : [1, N, 6] -> x1 y1 x2 y2 score class
 */
bool parseOutput(const cv::Mat &output, const DetectOptions &options, const Letterbox &lb,
                 const cv::Size &srcSize, const QStringList &names,
                 QList<Detection> &detections, QString *error)
{
    detections.clear();

    cv::Mat out = output;
    if (out.type() != CV_32F)
        out.convertTo(out, CV_32F);
    if (!out.isContinuous())
        out = out.clone();

    int d1 = 0;
    int d2 = 0;
    if (out.dims == 2) {
        d1 = out.rows;
        d2 = out.cols;
    } else if (out.dims == 3) {
        d1 = out.size[1];
        d2 = out.size[2];
    } else if (out.dims == 4) {
        d1 = out.size[2];
        d2 = out.size[3];
    } else {
        if (error)
            *error = QStringLiteral("不支持的模型输出维度：%1").arg(shapeText(out));
        return false;
    }

    if (d1 <= 0 || d2 <= 0 || (d1 < 6 && d2 < 6)) {
        if (error)
            *error = QStringLiteral("模型输出形状无效：%1").arg(shapeText(out));
        return false;
    }

    OutputFormat format = options.format;
    if (format == OutputFormat::Auto) {
        // 端到端导出（NMS 后）通常是 [1, 300, 6]；YOLOv5 单类别为 [1, 25200, 6]，用行数区分
        if (d2 == 6 && d1 <= 1000)
            format = OutputFormat::Nms;
        else if (d1 < d2)
            format = OutputFormat::YoloV8;
        else
            format = OutputFormat::YoloV5;
    }

    const bool channelsFirst = (d1 < d2);   // [1, C, N]
    const int rows = channelsFirst ? d2 : d1;
    const int attrs = channelsFirst ? d1 : d2;

    const float *data = out.ptr<float>(0);
    auto at = [&](int row, int attr) -> float {
        return channelsFirst ? data[static_cast<size_t>(attr) * rows + row]
                             : data[static_cast<size_t>(row) * attrs + attr];
    };

    QSet<int> allowed;
    for (int id : options.classFilter)
        allowed.insert(id);
    const bool filterEnabled = !allowed.isEmpty();

    const int classCount = (format == OutputFormat::YoloV5) ? attrs - 5 : attrs - 4;
    if (format != OutputFormat::Nms && classCount <= 0) {
        if (error) {
            *error = QStringLiteral("模型输出属性数不足（%1），与所选输出格式不匹配：%2；"
                                    "请检查「输出格式」参数")
                         .arg(attrs)
                         .arg(shapeText(out));
        }
        return false;
    }

    const float maxX = static_cast<float>(srcSize.width - 1);
    const float maxY = static_cast<float>(srcSize.height - 1);
    const float conf = options.confThreshold;

    auto clampValue = [](float value, float low, float high) {
        return std::min(std::max(value, low), high);
    };

    for (int row = 0; row < rows; ++row) {
        int classId = -1;
        float score = 0.0f;
        float x1 = 0.0f;
        float y1 = 0.0f;
        float x2 = 0.0f;
        float y2 = 0.0f;

        if (format == OutputFormat::Nms) {
            if (attrs < 6)
                break;
            score = at(row, 4);
            if (score < conf)
                continue;
            classId = static_cast<int>(std::lround(at(row, 5)));
            if (filterEnabled && !allowed.contains(classId))
                continue;
            x1 = at(row, 0);
            y1 = at(row, 1);
            x2 = at(row, 2);
            y2 = at(row, 3);
        } else if (format == OutputFormat::YoloV8) {
            for (int c = 4; c < attrs; ++c) {
                const int id = c - 4;
                if (filterEnabled && !allowed.contains(id))
                    continue;
                const float value = at(row, c);
                if (value > score) {
                    score = value;
                    classId = id;
                }
            }
            if (classId < 0 || score < conf)
                continue;

            const float cx = at(row, 0);
            const float cy = at(row, 1);
            const float w = at(row, 2);
            const float h = at(row, 3);
            x1 = cx - w * 0.5f;
            y1 = cy - h * 0.5f;
            x2 = cx + w * 0.5f;
            y2 = cy + h * 0.5f;
        } else { // YOLOv5
            const float objectness = at(row, 4);
            if (objectness < conf)
                continue;
            for (int c = 5; c < attrs; ++c) {
                const int id = c - 5;
                if (filterEnabled && !allowed.contains(id))
                    continue;
                const float value = at(row, c);
                if (value > score) {
                    score = value;
                    classId = id;
                }
            }
            if (classId < 0)
                continue;
            score *= objectness;
            if (score < conf)
                continue;

            const float cx = at(row, 0);
            const float cy = at(row, 1);
            const float w = at(row, 2);
            const float h = at(row, 3);
            x1 = cx - w * 0.5f;
            y1 = cy - h * 0.5f;
            x2 = cx + w * 0.5f;
            y2 = cy + h * 0.5f;
        }

        // letterbox 逆变换回原图坐标
        x1 = clampValue((x1 - lb.padX) / lb.ratio, 0.0f, maxX);
        y1 = clampValue((y1 - lb.padY) / lb.ratio, 0.0f, maxY);
        x2 = clampValue((x2 - lb.padX) / lb.ratio, 0.0f, maxX);
        y2 = clampValue((y2 - lb.padY) / lb.ratio, 0.0f, maxY);

        const int ix1 = static_cast<int>(std::lround(x1));
        const int iy1 = static_cast<int>(std::lround(y1));
        const int ix2 = static_cast<int>(std::lround(x2));
        const int iy2 = static_cast<int>(std::lround(y2));
        if (ix2 <= ix1 || iy2 <= iy1)
            continue;

        Detection detection;
        detection.classId = classId;
        detection.className = names.value(classId, QString::number(classId));
        detection.confidence = score;
        detection.box = cv::Rect(cv::Point(ix1, iy1), cv::Point(ix2, iy2));
        detection.center = cv::Point((ix1 + ix2) / 2, (iy1 + iy2) / 2);
        detections.append(detection);
    }

    if (format != OutputFormat::Nms && !detections.isEmpty()) {
        const std::vector<int> kept = nmsPerClass(detections, conf, options.iouThreshold);
        QList<Detection> filtered;
        filtered.reserve(static_cast<int>(kept.size()));
        for (int index : kept)
            filtered.append(detections[index]);
        detections = filtered;
    }

    std::sort(detections.begin(), detections.end(),
              [](const Detection &a, const Detection &b) {
                  return a.confidence > b.confidence;
              });

    if (options.maxDet > 0 && detections.size() > options.maxDet)
        detections = detections.mid(0, options.maxDet);

    return true;
}

bool isAscii(const QString &text)
{
    for (const QChar c : text) {
        if (c.unicode() > 127)
            return false;
    }
    return true;
}

} // namespace

// ------------------------------------------------------------------ Detector

QString Detector::resolveModelPath(const QString &path)
{
    const QString trimmed = path.trimmed().remove(QLatin1Char('"'));
    if (trimmed.isEmpty())
        return QString();

    const QFileInfo info(trimmed);
    if (info.isAbsolute() || info.exists())
        return trimmed;

    // 相对路径：当前目录找不到时回退到程序目录
    const QString appCandidate =
        QDir(QCoreApplication::applicationDirPath()).filePath(trimmed);
    if (QFileInfo::exists(appCandidate))
        return appCandidate;

    return trimmed;
}

int Detector::cudaDeviceCount()
{
    static int cached = -1;
    if (cached < 0) {
        int count = 0;
        try {
            count = cv::cuda::getCudaEnabledDeviceCount();
        } catch (...) {
            count = 0;
        }
        cached = count > 0 ? count : 0;
    }
    return cached;
}

DeviceSelection Detector::parseDevice(const QString &text)
{
    DeviceSelection selection;
    const QString name = text.trimmed();

    if (name.compare(QStringLiteral("CPU"), Qt::CaseInsensitive) == 0) {
        selection.cuda = false;
        selection.automatic = false;
        return selection;
    }

    if (name.startsWith(QStringLiteral("CUDA"), Qt::CaseInsensitive)) {
        selection.cuda = true;
        selection.automatic = false;
        const int colon = name.indexOf(QLatin1Char(':'));
        if (colon >= 0)
            selection.deviceIndex = qMax(0, name.mid(colon + 1).toInt());
        return selection;
    }

    // 自动：先尝试 CUDA，失败时允许回退 CPU
    selection.cuda = true;
    selection.automatic = true;
    selection.deviceIndex = 0;
    return selection;
}

QStringList Detector::loadClassNames(const QString &modelPath, const QString &namesPath)
{
    QString path = namesPath.trimmed().remove(QLatin1Char('"'));

    if (path.isEmpty()) {
        const QFileInfo modelInfo(modelPath);
        const QString dir = modelInfo.absolutePath();
        const QString base = modelInfo.completeBaseName();
        const QStringList candidates = {
            QDir(dir).filePath(base + QStringLiteral(".names.txt")),
            QDir(dir).filePath(base + QStringLiteral(".txt")),
            QDir(dir).filePath(QStringLiteral("classes.txt")),
            QDir(dir).filePath(QStringLiteral("labels.txt")),
            QDir(dir).filePath(QStringLiteral("names.txt")),
        };
        for (const QString &candidate : candidates) {
            if (QFileInfo::exists(candidate)) {
                path = candidate;
                break;
            }
        }
    }

    if (path.isEmpty() || !QFileInfo::exists(path))
        return QStringList();

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return QStringList();

    const QString content = QString::fromUtf8(file.readAll());
    file.close();

    // 一行一个类别名；也兼容逗号分隔
    QStringList tokens;
    if (content.contains(QLatin1Char('\n')))
        tokens = content.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    else
        tokens = content.split(QLatin1Char(','), Qt::SkipEmptyParts);

    QStringList names;
    for (QString token : tokens) {
        token = token.trimmed();
        if (!token.isEmpty())
            names.append(token);
    }
    return names;
}

std::shared_ptr<Detector> Detector::acquire(const QString &modelPath, const QString &classNamesPath,
                                            QString *error)
{
    static QMutex cacheMutex;
    static QHash<QString, std::shared_ptr<Detector>> cache;

    const QString resolved = resolveModelPath(modelPath);
    if (resolved.isEmpty()) {
        if (error)
            *error = QStringLiteral("模型路径为空，请先选择 YOLO 模型文件");
        return nullptr;
    }

    const QFileInfo info(resolved);
    const QString modelFile = info.exists() ? info.absoluteFilePath() : resolved;
    // 类别名称文件也参与 key，避免切换 names 后仍用旧类别名
    const QString key = modelFile + QLatin1Char('|') + classNamesPath.trimmed();

    {
        QMutexLocker locker(&cacheMutex);
        const auto it = cache.constFind(key);
        if (it != cache.constEnd())
            return it.value();
    }

    std::shared_ptr<Detector> detector(new Detector);
    if (!detector->load(modelFile, classNamesPath, error))
        return nullptr;

    {
        QMutexLocker locker(&cacheMutex);
        cache.insert(key, detector);
    }
    return detector;
}

bool Detector::load(const QString &path, const QString &classNamesPath, QString *error)
{
    const QString filePath = path.trimmed().remove(QLatin1Char('"'));
    const QFileInfo info(filePath);

    if (filePath.isEmpty() || info.suffix().compare(QStringLiteral("onnx"), Qt::CaseInsensitive) != 0) {
        if (error) {
            *error = QStringLiteral(
                         "C++ 版通过 OpenCV DNN 推理，仅支持 .onnx 模型（当前：%1）。\n"
                         "请先导出模型，例如：yolo export model=yolo11n.pt format=onnx")
                         .arg(filePath.isEmpty() ? QStringLiteral("空路径") : filePath);
        }
        return false;
    }

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        if (error)
            *error = QStringLiteral("无法打开模型文件：%1\n(%2)").arg(filePath, file.errorString());
        return false;
    }
    const QByteArray data = file.readAll();
    file.close();

    if (data.isEmpty()) {
        if (error)
            *error = QStringLiteral("模型文件为空：%1").arg(filePath);
        return false;
    }

    try {
        // 用内存加载，避免 Windows 中文/空格路径 fopen 失败
        const auto *begin = reinterpret_cast<const uchar *>(data.constData());
        std::vector<uchar> buffer(begin, begin + data.size());
        m_net = cv::dnn::readNetFromONNX(buffer);
    } catch (const cv::Exception &e) {
        if (error)
            *error = QStringLiteral("加载 ONNX 模型失败：%1").arg(QString::fromUtf8(e.what()));
        return false;
    }

    if (m_net.empty()) {
        if (error)
            *error = QStringLiteral("模型加载后为空：%1").arg(filePath);
        return false;
    }

    m_net.setPreferableBackend(cv::dnn::DNN_BACKEND_OPENCV);
    m_net.setPreferableTarget(cv::dnn::DNN_TARGET_CPU);

    m_modelPath = filePath;
    m_classNames = loadClassNames(filePath, classNamesPath);
    m_loaded = true;
    return true;
}

bool Detector::detect(const cv::Mat &bgr, const DetectOptions &options,
                      QList<Detection> &detections, QString *error)
{
    detections.clear();
    {
        QMutexLocker locker(&m_mutex);
        m_lastInferenceMs = 0.0;
    }

    if (!m_loaded) {
        if (error)
            *error = QStringLiteral("模型尚未加载");
        return false;
    }
    if (bgr.empty()) {
        if (error)
            *error = QStringLiteral("输入图像为空");
        return false;
    }

    cv::Mat src;
    if (bgr.channels() == 1)
        cv::cvtColor(bgr, src, cv::COLOR_GRAY2BGR);
    else if (bgr.channels() == 4)
        cv::cvtColor(bgr, src, cv::COLOR_BGRA2BGR);
    else if (bgr.channels() == 3)
        src = bgr;
    else {
        if (error)
            *error = QStringLiteral("不支持的图像通道数：%1").arg(bgr.channels());
        return false;
    }

    int imgsz = options.imgsz > 0 ? options.imgsz : 640;
    imgsz = std::max(32, (imgsz / 32) * 32);

    Letterbox lb;
    const cv::Mat padded = letterbox(src, imgsz, lb);
    const cv::Mat blob = cv::dnn::blobFromImage(padded, 1.0 / 255.0, cv::Size(imgsz, imgsz),
                                                cv::Scalar(), true, false, CV_32F);

    QMutexLocker locker(&m_mutex);

    // ---- 选择推理后端 ----
    bool useOrt = false;
    if (options.backend != Backend::OpenCvDnn) {
        QString ortDetail;
        const bool ortAvailable = OrtBackend::isAvailable(&ortDetail);
        if (options.backend == Backend::OnnxRuntime) {
            if (!ortAvailable) {
                if (error)
                    *error = QStringLiteral("ONNX Runtime 不可用：\n%1").arg(ortDetail);
                return false;
            }
            useOrt = true;
        } else {
            useOrt = ortAvailable;   // 自动
        }
    }

    QElapsedTimer timer;
    timer.start();

    cv::Mat output;

    if (useOrt) {
        // 设备/回退策略变化时重建会话
        const int deviceKey = (options.useCuda ? options.cudaDevice : -1) * 2
                              + (options.deviceAuto ? 1 : 0);
        if (!m_ort || m_ortDevice != deviceKey) {
            m_ort.reset(new OrtBackend);
            QString ortError;
            if (!m_ort->open(m_modelPath, options.useCuda, options.cudaDevice,
                             options.deviceAuto, &ortError)) {
                m_ort.reset();
                m_ortDevice = -100;
                if (error)
                    *error = QStringLiteral("ONNX Runtime 会话创建失败：\n%1").arg(ortError);
                return false;
            }
            m_ortDevice = deviceKey;
        }

        if (!m_ort->run(blob, output, error))
            return false;
        m_lastDevice = QStringLiteral("onnxruntime/%1").arg(m_ort->provider());
    } else {
        // ---- OpenCV DNN ----
        const int cudaCount = cudaDeviceCount();
        if (options.useCuda && !options.deviceAuto
            && (cudaCount <= 0 || options.cudaDevice >= cudaCount)) {
            if (error) {
                *error = QStringLiteral(
                             "OpenCV DNN 后端没有可用的 CUDA（当前 OpenCV 未编译 CUDA，"
                             "或 CUDA:%1 不存在）。\n"
                             "可改用「ONNX Runtime」后端，或把推理设备设为 CPU。")
                             .arg(options.cudaDevice);
            }
            return false;
        }

        const bool wantCuda = options.useCuda && cudaCount > 0 && options.cudaDevice >= 0
                              && options.cudaDevice < cudaCount;

        auto configure = [&](bool cuda) {
            if (cuda) {
                cv::cuda::setDevice(options.cudaDevice);
                m_net.setPreferableBackend(cv::dnn::DNN_BACKEND_CUDA);
                m_net.setPreferableTarget(options.half ? cv::dnn::DNN_TARGET_CUDA_FP16
                                                       : cv::dnn::DNN_TARGET_CUDA);
            } else {
                m_net.setPreferableBackend(cv::dnn::DNN_BACKEND_OPENCV);
                m_net.setPreferableTarget(cv::dnn::DNN_TARGET_CPU);
            }
        };

        bool usedCuda = wantCuda;
        try {
            configure(usedCuda);
            m_net.setInput(blob);
            output = m_net.forward();
        } catch (const cv::Exception &e) {
            if (!usedCuda) {
                if (error)
                    *error = QStringLiteral("推理失败：%1").arg(QString::fromUtf8(e.what()));
                return false;
            }
            // CUDA 初始化失败（显存不足等）时回退 CPU
            usedCuda = false;
            try {
                configure(false);
                m_net.setInput(blob);
                output = m_net.forward();
            } catch (const cv::Exception &fallback) {
                if (error)
                    *error = QStringLiteral("推理失败：%1").arg(QString::fromUtf8(fallback.what()));
                return false;
            }
        }

        m_lastDevice = usedCuda ? QStringLiteral("cuda:%1").arg(options.cudaDevice)
                                : QStringLiteral("cpu");
        if (wantCuda && !usedCuda)
            m_lastDevice = QStringLiteral("cpu（CUDA 回退）");
    }

    m_lastInferenceMs = timer.nsecsElapsed() / 1.0e6;

    return parseOutput(output, options, lb, src.size(), m_classNames, detections, error);
}

// ------------------------------------------------------------------- 绘制

cv::Mat drawDetections(const cv::Mat &bgr, const QList<Detection> &detections,
                       bool showLabel, int thickness)
{
    static const cv::Scalar palette[] = {
        cv::Scalar(0, 255, 0),   cv::Scalar(0, 128, 255), cv::Scalar(255, 128, 0),
        cv::Scalar(255, 0, 255), cv::Scalar(0, 255, 255), cv::Scalar(255, 0, 0),
        cv::Scalar(180, 255, 0), cv::Scalar(0, 64, 255),  cv::Scalar(255, 255, 0),
        cv::Scalar(128, 0, 255),
    };
    constexpr int kPaletteSize = static_cast<int>(sizeof(palette) / sizeof(palette[0]));

    cv::Mat output = bgr.clone();
    const int line = std::max(1, thickness);

    for (const Detection &det : detections) {
        const int colorIndex = ((det.classId % kPaletteSize) + kPaletteSize) % kPaletteSize;
        const cv::Scalar color = palette[colorIndex];
        cv::rectangle(output, det.box, color, line, cv::LINE_AA);

        if (!showLabel)
            continue;

        QString name = det.className;
        if (!isAscii(name))
            name = QStringLiteral("#%1").arg(det.classId);   // Hershey 字体无法绘制中文
        const QString label = QStringLiteral("%1 %2").arg(name).arg(det.confidence, 0, 'f', 2);

        int baseline = 0;
        const cv::Size textSize = cv::getTextSize(label.toStdString(), cv::FONT_HERSHEY_SIMPLEX,
                                                  0.5, 1, &baseline);
        const int labelHeight = textSize.height + 6;

        int boxTop = det.box.y;
        int boxBottom = det.box.y;
        int textY = det.box.y;
        if (det.box.y >= labelHeight) {
            boxTop = det.box.y - labelHeight;
            textY = det.box.y - 4;
        } else {
            // 贴近图像上边缘时把标签画在框内
            boxBottom = det.box.y + labelHeight;
            textY = det.box.y + textSize.height + 2;
        }

        const cv::Rect labelRect(det.box.x, boxTop, textSize.width + 6, boxBottom - boxTop);
        if (labelRect.width > 0 && labelRect.height > 0)
            cv::rectangle(output, labelRect, color, cv::FILLED);
        cv::putText(output, label.toStdString(), cv::Point(det.box.x + 3, textY),
                    cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(0, 0, 0), 1, cv::LINE_AA);
    }

    return output;
}

} // namespace YOLO
