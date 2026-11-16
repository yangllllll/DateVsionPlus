#include "onnxruntimebackend.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QLibrary>
#include <QMutex>
#include <QMutexLocker>

#include <cstring>
#include <vector>

#ifdef YOLO_HAVE_ONNXRUNTIME
#  include <onnxruntime_c_api.h>
#endif

namespace YOLO {
namespace {

/** 参数里指定的 DLL / 目录（两种编译方式下都要保存） */
QString g_preferredLibrary;
QMutex g_preferredMutex;

#ifdef YOLO_HAVE_ONNXRUNTIME

struct Runtime
{
    QMutex mutex;
    QLibrary *library = nullptr;
    const OrtApi *api = nullptr;
    OrtEnv *env = nullptr;
    QString libraryPath;
    QString version;
    QString error;
    bool ready = false;
    bool attempted = false;
};

Runtime &runtime()
{
    // 故意不析构：保证进程退出期间模型对象销毁时 OrtApi 仍然有效
    static Runtime *instance = new Runtime();
    return *instance;
}

QString consumeStatus(const OrtApi *api, OrtStatus *status)
{
    if (!status)
        return QString();
    const QString message = QString::fromUtf8(api->GetErrorMessage(status));
    api->ReleaseStatus(status);
    return message;
}

bool ensureRuntime(QString *error)
{
    Runtime &rt = runtime();
    QMutexLocker locker(&rt.mutex);

    if (rt.ready)
        return true;
    if (rt.attempted) {
        if (error)
            *error = rt.error;
        return false;
    }
    rt.attempted = true;

    QString preferred;
    {
        QMutexLocker preferredLocker(&g_preferredMutex);
        preferred = g_preferredLibrary;
    }

    typedef const OrtApiBase *(ORT_API_CALL * GetApiBaseFn)(void);
    QStringList attempts;

    const QStringList candidates = OrtBackend::libraryCandidates(preferred);
    for (const QString &path : candidates) {
        QLibrary *library = new QLibrary(path);
        if (!library->load()) {
            attempts.append(QStringLiteral("%1（%2）").arg(path, library->errorString()));
            delete library;
            continue;
        }

        auto getApiBase = reinterpret_cast<GetApiBaseFn>(library->resolve("OrtGetApiBase"));
        if (!getApiBase) {
            attempts.append(QStringLiteral("%1（缺少 OrtGetApiBase）").arg(path));
            library->unload();
            delete library;
            continue;
        }

        const OrtApiBase *apiBase = getApiBase();
        const OrtApi *api = apiBase ? apiBase->GetApi(ORT_API_VERSION) : nullptr;
        if (!api) {
            attempts.append(QStringLiteral("%1（ONNX Runtime 版本过旧，不支持 API %2）")
                                .arg(path)
                                .arg(ORT_API_VERSION));
            library->unload();
            delete library;
            continue;
        }

        OrtEnv *env = nullptr;
        OrtStatus *status = api->CreateEnv(ORT_LOGGING_LEVEL_WARNING, "yolodetect", &env);
        if (status) {
            attempts.append(QStringLiteral("%1（%2）").arg(path, consumeStatus(api, status)));
            library->unload();
            delete library;
            continue;
        }

        // 加载成功后库保持常驻，不做 unload
        rt.library = library;
        rt.api = api;
        rt.env = env;
        rt.libraryPath = path;
        rt.version = QString::fromUtf8(apiBase->GetVersionString());
        rt.error.clear();
        rt.ready = true;
        if (error)
            error->clear();
        return true;
    }

    rt.error = QStringLiteral(
                   "未能加载 ONNX Runtime（onnxruntime.dll）。\n"
                   "请下载 onnxruntime-win-x64（CPU）或 onnxruntime-win-x64-gpu（CUDA），"
                   "把 onnxruntime.dll 连同同目录 DLL 放到程序目录，"
                   "或在参数「ONNX Runtime 路径」中指定，或设置环境变量 ONNXRUNTIME_DIR。\n"
                   "已尝试：\n%1")
                   .arg(attempts.join(QLatin1Char('\n')));
    if (error)
        *error = rt.error;
    return false;
}

// ---- 跨版本兼容的小工具 ------------------------------------------------
// ORT 1.13 ~ 1.31 使用 SessionGetInputNameAllocated / SessionGetOutputNameAllocated；
// API 32（更新的头文件）改回 SessionGetInputName / SessionGetOutputName。
// 用重载 + SFINAE 自动选择存在的那个。

template <typename Api>
auto sessionName(const Api *api, const OrtSession *session, size_t index,
                 OrtAllocator *allocator, char **value, bool isInput, int)
    -> decltype(isInput ? api->SessionGetInputNameAllocated(session, index, allocator, value)
                        : api->SessionGetOutputNameAllocated(session, index, allocator, value))
{
    return isInput ? api->SessionGetInputNameAllocated(session, index, allocator, value)
                   : api->SessionGetOutputNameAllocated(session, index, allocator, value);
}

template <typename Api>
OrtStatus *sessionName(const Api *api, const OrtSession *session, size_t index,
                       OrtAllocator *allocator, char **value, bool isInput, long)
{
    return isInput ? api->SessionGetInputName(session, index, allocator, value)
                   : api->SessionGetOutputName(session, index, allocator, value);
}

// 新版本 OrtApi 自带 CUDA Provider 接口；老版本回退到 DLL 导出函数。
template <typename Api>
auto appendCudaProvider(const Api *api, QLibrary *, OrtSessionOptions *options, int deviceIndex,
                        bool *found, int)
    -> decltype(api->SessionOptionsAppendExecutionProvider_CUDA(options, deviceIndex))
{
    *found = true;
    return api->SessionOptionsAppendExecutionProvider_CUDA(options, deviceIndex);
}

template <typename Api>
OrtStatus *appendCudaProvider(const Api *, QLibrary *library, OrtSessionOptions *options,
                              int deviceIndex, bool *found, long)
{
    using Fn = OrtStatus *(ORT_API_CALL *)(OrtSessionOptions *, int);
    auto fn = reinterpret_cast<Fn>(
        library->resolve("OrtSessionOptionsAppendExecutionProvider_CUDA"));
    if (!fn) {
        *found = false;
        return nullptr;
    }
    *found = true;
    return fn(options, deviceIndex);
}

#endif // YOLO_HAVE_ONNXRUNTIME

} // namespace

struct OrtBackend::Impl
{
#ifdef YOLO_HAVE_ONNXRUNTIME
    OrtSession *session = nullptr;
    QString inputName;
    QString outputName;
#endif
    QString provider = QStringLiteral("cpu");
    QString lastError;
};

OrtBackend::OrtBackend()
    : m_impl(new Impl)
{
}

OrtBackend::~OrtBackend()
{
#ifdef YOLO_HAVE_ONNXRUNTIME
    if (m_impl && m_impl->session) {
        Runtime &rt = runtime();
        QMutexLocker locker(&rt.mutex);
        if (rt.api)
            rt.api->ReleaseSession(m_impl->session);
        m_impl->session = nullptr;
    }
#endif
}

// ------------------------------------------------------------- 静态接口

void OrtBackend::setPreferredLibrary(const QString &path)
{
#ifdef YOLO_HAVE_ONNXRUNTIME
    {
        Runtime &rt = runtime();
        QMutexLocker locker(&rt.mutex);
        if (rt.ready || rt.attempted)
            return;   // 已加载或已尝试过，忽略
    }
#endif
    QMutexLocker locker(&g_preferredMutex);
    g_preferredLibrary = path;
}

QStringList OrtBackend::libraryCandidates(const QString &preferredPath)
{
    QStringList result;
    auto append = [&result](const QString &value) {
        const QString trimmed = value.trimmed().remove(QLatin1Char('"'));
        if (trimmed.isEmpty())
            return;

        const QFileInfo info(trimmed);
        if (info.isDir()) {
            const QString direct = QDir(trimmed).filePath(QStringLiteral("onnxruntime.dll"));
            if (!result.contains(direct))
                result.append(direct);
            const QString inLib = QDir(trimmed).filePath(QStringLiteral("lib/onnxruntime.dll"));
            if (!result.contains(inLib))
                result.append(inLib);
        } else if (!result.contains(trimmed)) {
            result.append(trimmed);
        }
    };

    append(preferredPath);
    append(QString::fromLocal8Bit(qgetenv("ONNXRUNTIME_DIR")));

    const QString appDir = QCoreApplication::applicationDirPath();
    append(appDir);
    append(appDir + QStringLiteral("/plugins"));

    // 交给系统 PATH 搜索
    result.append(QStringLiteral("onnxruntime.dll"));
    return result;
}

bool OrtBackend::isAvailable(QString *detail)
{
#ifdef YOLO_HAVE_ONNXRUNTIME
    QString error;
    if (ensureRuntime(&error)) {
        if (detail) {
            *detail = QStringLiteral("ONNX Runtime %1（%2）")
                          .arg(runtimeVersion(), libraryPath());
        }
        return true;
    }
    if (detail)
        *detail = error;
    return false;
#else
    if (detail) {
        *detail = QStringLiteral(
            "插件编译时未找到 onnxruntime_c_api.h，未启用 ONNX Runtime 后端。\n"
            "请下载 ONNX Runtime 并设置环境变量 ONNXRUNTIME_DIR（包含 include/ 与 lib/），"
            "然后重新构建插件。");
    }
    return false;
#endif
}

QString OrtBackend::libraryPath()
{
#ifdef YOLO_HAVE_ONNXRUNTIME
    Runtime &rt = runtime();
    QMutexLocker locker(&rt.mutex);
    return rt.libraryPath;
#else
    return QString();
#endif
}

QString OrtBackend::runtimeVersion()
{
#ifdef YOLO_HAVE_ONNXRUNTIME
    Runtime &rt = runtime();
    QMutexLocker locker(&rt.mutex);
    return rt.version;
#else
    return QString();
#endif
}

// ------------------------------------------------------------- 会话管理

bool OrtBackend::open(const QString &modelPath, bool useCuda, int deviceIndex,
                      bool allowCpuFallback, QString *error)
{
    m_impl->lastError.clear();
    m_impl->provider = QStringLiteral("cpu");

#ifdef YOLO_HAVE_ONNXRUNTIME
    QString runtimeError;
    if (!ensureRuntime(&runtimeError)) {
        m_impl->lastError = runtimeError;
        if (error)
            *error = runtimeError;
        return false;
    }

    Runtime &rt = runtime();
    const OrtApi *api = rt.api;

    if (m_impl->session) {
        api->ReleaseSession(m_impl->session);
        m_impl->session = nullptr;
    }

    auto createSession = [&](bool cuda, QString *err) -> OrtSession * {
        OrtSessionOptions *options = nullptr;
        OrtStatus *status = api->CreateSessionOptions(&options);
        if (status) {
            *err = consumeStatus(api, status);
            return nullptr;
        }

        api->SetIntraOpNumThreads(options, 1);
        api->SetSessionGraphOptimizationLevel(options, ORT_ENABLE_ALL);

        if (cuda) {
            bool found = false;
            status = appendCudaProvider(api, rt.library, options, deviceIndex, &found, 0);
            if (!found) {
                api->ReleaseSessionOptions(options);
                *err = QStringLiteral(
                    "当前 onnxruntime.dll 不含 CUDA Provider（需要 GPU 版 onnxruntime，"
                    "并保证 onnxruntime_providers_cuda.dll 在同目录）");
                return nullptr;
            }
            if (status) {
                api->ReleaseSessionOptions(options);
                *err = consumeStatus(api, status);
                return nullptr;
            }
        }

        OrtSession *session = nullptr;
#ifdef _WIN32
        status = api->CreateSession(rt.env, modelPath.toStdWString().c_str(), options, &session);
#else
        status = api->CreateSession(rt.env, modelPath.toUtf8().constData(), options, &session);
#endif
        api->ReleaseSessionOptions(options);
        if (status) {
            *err = consumeStatus(api, status);
            return nullptr;
        }
        return session;
    };

    QString sessionError;
    OrtSession *session = createSession(useCuda, &sessionError);
    bool fellBack = false;
    if (!session && useCuda && allowCpuFallback) {
        QString cpuError;
        session = createSession(false, &cpuError);
        fellBack = true;
        if (!session)
            sessionError = cpuError;
    }

    if (!session) {
        m_impl->lastError = sessionError;
        if (error)
            *error = sessionError;
        return false;
    }

    OrtAllocator *allocator = nullptr;
    OrtStatus *status = api->GetAllocatorWithDefaultOptions(&allocator);
    if (status) {
        api->ReleaseSession(session);
        m_impl->lastError = consumeStatus(api, status);
        if (error)
            *error = m_impl->lastError;
        return false;
    }

    size_t inputCount = 0;
    size_t outputCount = 0;
    api->SessionGetInputCount(session, &inputCount);
    api->SessionGetOutputCount(session, &outputCount);
    if (inputCount < 1 || outputCount < 1) {
        api->ReleaseSession(session);
        m_impl->lastError = QStringLiteral("模型没有有效的输入/输出节点");
        if (error)
            *error = m_impl->lastError;
        return false;
    }

    char *rawInputName = nullptr;
    status = sessionName(api, session, 0, allocator, &rawInputName, true, 0);
    if (status) {
        api->ReleaseSession(session);
        m_impl->lastError = consumeStatus(api, status);
        if (error)
            *error = m_impl->lastError;
        return false;
    }

    char *rawOutputName = nullptr;
    status = sessionName(api, session, 0, allocator, &rawOutputName, false, 0);
    if (status) {
        api->AllocatorFree(allocator, rawInputName);
        api->ReleaseSession(session);
        m_impl->lastError = consumeStatus(api, status);
        if (error)
            *error = m_impl->lastError;
        return false;
    }

    m_impl->inputName = QString::fromUtf8(rawInputName);
    m_impl->outputName = QString::fromUtf8(rawOutputName);
    api->AllocatorFree(allocator, rawInputName);
    api->AllocatorFree(allocator, rawOutputName);

    m_impl->session = session;
    if (fellBack)
        m_impl->provider = QStringLiteral("cpu（CUDA 回退）");
    else if (useCuda)
        m_impl->provider = QStringLiteral("cuda:%1").arg(deviceIndex);
    else
        m_impl->provider = QStringLiteral("cpu");

    if (error)
        error->clear();
    return true;
#else
    Q_UNUSED(modelPath);
    Q_UNUSED(useCuda);
    Q_UNUSED(deviceIndex);
    Q_UNUSED(allowCpuFallback);

    m_impl->lastError = QStringLiteral(
        "此插件编译时未启用 ONNX Runtime（缺少 onnxruntime_c_api.h）。\n"
        "请设置环境变量 ONNXRUNTIME_DIR 指向 ONNX Runtime 根目录后重新构建。");
    if (error)
        *error = m_impl->lastError;
    return false;
#endif
}

bool OrtBackend::run(const cv::Mat &blob, cv::Mat &output, QString *error)
{
    output.release();

#ifdef YOLO_HAVE_ONNXRUNTIME
    if (!m_impl->session) {
        m_impl->lastError = QStringLiteral("ONNX Runtime 会话未创建");
        if (error)
            *error = m_impl->lastError;
        return false;
    }
    if (blob.empty() || blob.type() != CV_32F || blob.dims != 4) {
        m_impl->lastError = QStringLiteral("推理输入必须是 1x3xHxW 的 CV_32F 张量");
        if (error)
            *error = m_impl->lastError;
        return false;
    }

    const OrtApi *api = runtime().api;
    const int64_t shape[4] = {blob.size[0], blob.size[1], blob.size[2], blob.size[3]};
    const size_t byteSize = blob.total() * blob.elemSize();

    OrtMemoryInfo *memoryInfo = nullptr;
    OrtStatus *status = api->CreateCpuMemoryInfo(OrtArenaAllocator, OrtMemTypeDefault, &memoryInfo);
    if (status) {
        m_impl->lastError = consumeStatus(api, status);
        if (error)
            *error = m_impl->lastError;
        return false;
    }

    OrtValue *inputTensor = nullptr;
    status = api->CreateTensorWithDataAsOrtValue(memoryInfo, blob.data, byteSize, shape, 4,
                                                 ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT, &inputTensor);
    api->ReleaseMemoryInfo(memoryInfo);
    if (status) {
        m_impl->lastError = consumeStatus(api, status);
        if (error)
            *error = m_impl->lastError;
        return false;
    }

    const QByteArray inputName = m_impl->inputName.toUtf8();
    const QByteArray outputName = m_impl->outputName.toUtf8();
    const char *inputNames[] = {inputName.constData()};
    const char *outputNames[] = {outputName.constData()};
    const OrtValue *inputValues[] = {inputTensor};
    OrtValue *outputValues[] = {nullptr};

    status = api->Run(m_impl->session, nullptr, inputNames, inputValues, 1, outputNames, 1,
                      outputValues);
    api->ReleaseValue(inputTensor);
    if (status) {
        m_impl->lastError = consumeStatus(api, status);
        if (error)
            *error = m_impl->lastError;
        return false;
    }

    OrtValue *outputTensor = outputValues[0];
    OrtTensorTypeAndShapeInfo *info = nullptr;
    status = api->GetTensorTypeAndShape(outputTensor, &info);
    if (status) {
        api->ReleaseValue(outputTensor);
        m_impl->lastError = consumeStatus(api, status);
        if (error)
            *error = m_impl->lastError;
        return false;
    }

    size_t dimCount = 0;
    size_t elementCount = 0;
    std::vector<int64_t> shape64;
    void *rawData = nullptr;

    api->GetDimensionsCount(info, &dimCount);
    shape64.resize(dimCount);
    api->GetDimensions(info, shape64.data(), dimCount);
    api->GetTensorShapeElementCount(info, &elementCount);
    api->GetTensorMutableData(outputTensor, &rawData);
    api->ReleaseTensorTypeAndShapeInfo(info);

    if (elementCount == 0 || !rawData || dimCount < 2) {
        api->ReleaseValue(outputTensor);
        m_impl->lastError = QStringLiteral("模型输出为空或形状无效");
        if (error)
            *error = m_impl->lastError;
        return false;
    }

    // 必须在 ReleaseValue 之前复制像素
    std::vector<int> dims;
    dims.reserve(dimCount);
    for (int64_t value : shape64)
        dims.push_back(static_cast<int>(value));

    output.create(static_cast<int>(dims.size()), dims.data(), CV_32F);
    std::memcpy(output.data, rawData, elementCount * sizeof(float));
    api->ReleaseValue(outputTensor);

    if (error)
        error->clear();
    return true;
#else
    Q_UNUSED(blob);
    m_impl->lastError = QStringLiteral("此插件编译时未启用 ONNX Runtime，无法推理");
    if (error)
        *error = m_impl->lastError;
    return false;
#endif
}

QString OrtBackend::provider() const
{
    return m_impl->provider;
}

QString OrtBackend::lastError() const
{
    return m_impl->lastError;
}

} // namespace YOLO
