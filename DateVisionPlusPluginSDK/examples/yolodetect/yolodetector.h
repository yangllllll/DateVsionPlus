#ifndef YOLODETECTOR_H
#define YOLODETECTOR_H

/**
 * @brief YOLO 目标检测（OpenCV DNN）
 *
 * 对应 Python 版 yolo_detect.py 的 ultralytics 封装，C++ 版改用 OpenCV DNN：
 *   - 模型：ONNX（cv::dnn::readNetFromONNX），CPU 推理；OpenCV 带 CUDA 时可走 GPU
 *   - 输出格式：自动识别 YOLOv8/v11（[1, 4+nc, N]）、YOLOv5（[1, N, 5+nc]）
 *     以及导出时已做 NMS 的端到端模型（[1, N, 6]）
 *   - 预处理：letterbox（保持比例 + 灰边），推理后把框映射回原图坐标
 *   - 模型按绝对路径缓存，多个插件实例 / 多次执行共用同一个 Net
 *
 * 线程安全：detect() 内部对 Net 加锁，可被多个线程调用（串行执行）。
 */

#include "onnxruntimebackend.h"

#include <QList>
#include <QMutex>
#include <QString>
#include <QStringList>
#include <QtGlobal>

#include <opencv2/core.hpp>
#include <opencv2/dnn.hpp>

#include <memory>

namespace YOLO {

/** 单个检测目标 */
struct Detection
{
    int classId = -1;
    QString className;
    float confidence = 0.0f;
    cv::Rect box;            //!< 原图像素坐标（已裁剪到图像范围内）
    cv::Point center;        //!< 框中心点
};

/** 模型输出格式 */
enum class OutputFormat {
    Auto = 0,   //!< 按输出张量形状自动判断
    YoloV8,     //!< [1, 4+nc, N]（YOLOv8 / YOLO11 等，cx cy w h + 类别分数）
    YoloV5,     //!< [1, N, 5+nc]（cx cy w h + objectness + 类别分数）
    Nms         //!< [1, N, 6]（x1 y1 x2 y2 score class，导出时已 NMS）
};

/** 推理后端 */
enum class Backend {
    Auto = 0,       //!< 有 ONNX Runtime 时优先使用，否则 OpenCV DNN
    OpenCvDnn,
    OnnxRuntime
};

/** 推理参数 */
struct DetectOptions
{
    int imgsz = 640;
    float confThreshold = 0.25f;
    float iouThreshold = 0.45f;
    int maxDet = 300;
    QList<int> classFilter;                  //!< 空 = 不过滤
    Backend backend = Backend::Auto;
    bool useCuda = false;
    int cudaDevice = 0;
    bool deviceAuto = false;                 //!< 设备为「自动」：CUDA 失败时允许回退 CPU
    bool half = false;                       //!< 仅 CUDA 有效：FP16（OpenCV DNN 后端）
    OutputFormat format = OutputFormat::Auto;
};

/** 设备选择结果 */
struct DeviceSelection
{
    bool cuda = false;
    int deviceIndex = 0;
    bool automatic = true;
};

/**
 * @brief 模型 + 推理器（进程内按模型路径缓存）
 */
class Detector
{
public:
    /** 取（或首次加载）指定模型的检测器；失败返回空指针并写入 error */
    static std::shared_ptr<Detector> acquire(const QString &modelPath,
                                             const QString &classNamesPath = QString(),
                                             QString *error = nullptr);

    /** 相对路径依次按 当前目录 -> 程序目录 解析 */
    static QString resolveModelPath(const QString &path);

    /**
     * @brief 解析界面设备选项（CPU / CUDA:n / 自动）
     *
     * 只做解析，不检查可用性：OpenCV DNN 与 ONNX Runtime 的 CUDA 能力不同，
     * 具体的可用性/回退在 detect() 里按实际后端判断。
     */
    static DeviceSelection parseDevice(const QString &text);

    /** 本机可用的 CUDA 设备数（OpenCV 未编译 CUDA 时为 0） */
    static int cudaDeviceCount();

    bool detect(const cv::Mat &bgr, const DetectOptions &options,
                QList<Detection> &detections, QString *error = nullptr);

    QString modelPath() const { return m_modelPath; }
    QStringList classNames() const { return m_classNames; }
    double lastInferenceMs() const
    {
        QMutexLocker locker(&m_mutex);
        return m_lastInferenceMs;
    }
    /** 本次推理实际使用的设备（CUDA 初始化失败时会自动回退 CPU） */
    QString lastDevice() const
    {
        QMutexLocker locker(&m_mutex);
        return m_lastDevice;
    }

private:
    Detector() = default;
    bool load(const QString &path, const QString &classNamesPath, QString *error);
    static QStringList loadClassNames(const QString &modelPath, const QString &namesPath);

    mutable QMutex m_mutex;
    cv::dnn::Net m_net;
    std::unique_ptr<OrtBackend> m_ort;
    int m_ortDevice = -100;   //!< -100 = 未创建；-1 = CPU；>=0 = CUDA 设备号
    QString m_modelPath;
    QStringList m_classNames;
    double m_lastInferenceMs = 0.0;
    QString m_lastDevice;
    bool m_loaded = false;
};

/** 在图像上绘制检测框与标签（源图为 BGR） */
cv::Mat drawDetections(const cv::Mat &bgr, const QList<Detection> &detections,
                       bool showLabel = true, int thickness = 2);

} // namespace YOLO

#endif // YOLODETECTOR_H
