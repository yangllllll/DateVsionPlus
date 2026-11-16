#ifndef ONNXRUNTIMEBACKEND_H
#define ONNXRUNTIMEBACKEND_H

/**
 * @brief ONNX Runtime 推理后端（运行期动态加载 onnxruntime.dll）
 *
 * 依赖方式：
 *   - 编译期：只需要 ONNX Runtime 的 `onnxruntime_c_api.h`（不需要导入库），
 *     由 yolodetect.pro 探测 ONNXRUNTIME_DIR；没找到头文件时插件仍能编译，
 *     只是没有这个后端（退化为 OpenCV DNN）。
 *   - 运行期：用 QLibrary 加载 `onnxruntime.dll`，解析 `OrtGetApiBase`。
 *     CPU 版 DLL 只能 CPU；GPU 版（CUDA/TensorRT 包）带 CUDA Provider。
 *
 * 模型：ONNX（与 OpenCV DNN 共用同一份预处理 / 后处理代码）。
 */

#include <QString>
#include <QStringList>

#include <opencv2/core.hpp>

#include <memory>

namespace YOLO {

class OrtBackend
{
public:
    OrtBackend();
    ~OrtBackend();

    OrtBackend(const OrtBackend &) = delete;
    OrtBackend &operator=(const OrtBackend &) = delete;

    /** 编译期是否带 ONNX Runtime 支持 + 运行期 DLL 能否加载 */
    static bool isAvailable(QString *detail = nullptr);

    /** 指定 onnxruntime.dll 或其所在目录（仅在运行时尚未加载时生效） */
    static void setPreferredLibrary(const QString &path);

    static QString libraryPath();
    static QString runtimeVersion();
    /** DLL 查找顺序：参数路径 -> ONNXRUNTIME_DIR -> 程序目录/plugins -> PATH */
    static QStringList libraryCandidates(const QString &preferredPath = QString());

    /**
     * @brief 创建会话
     * @param useCuda 是否使用 CUDA Provider
     * @param deviceIndex CUDA 设备号
     * @param allowCpuFallback CUDA 失败时是否自动回退 CPU
     */
    bool open(const QString &modelPath, bool useCuda, int deviceIndex, bool allowCpuFallback,
              QString *error = nullptr);

    /** 推理；blob 为 1x3xHxW CV_32F，output 为网络原始输出（N 维 float） */
    bool run(const cv::Mat &blob, cv::Mat &output, QString *error = nullptr);

    /** 实际使用的执行设备："cuda:0" / "cpu" / "cpu（CUDA 回退）" */
    QString provider() const;
    QString lastError() const;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace YOLO

#endif // ONNXRUNTIMEBACKEND_H
