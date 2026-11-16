#include "dahuaimvsdk.h"

#include <QCoreApplication>
#include <QDeadlineTimer>
#include <QDir>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QScopeGuard>

#include <cstring>
#include <limits>
#include <utility>

#if defined(Q_OS_WIN)
#  include <windows.h>
#endif

/** MVSDK 动态库文件名 */
static const char *const kLibraryName = "MVSDKmd.dll";

namespace DahuaMV {

namespace {

/** 定长 char 数组 -> QString（与 Python 端 chr(c) for c != 0 的解码一致） */
QString fixedString(const char *text, int maxLength = MaxStringLength)
{
    int length = 0;
    while (length < maxLength && text[length] != '\0')
        ++length;
    return QString::fromUtf8(text, length).trimmed();
}

/** 写回错误文本（供静态成员函数使用） */
void assignError(QString *error, const QString &message)
{
    if (error)
        *error = message;
}

/**
 * @brief 规范化用户填写的路径
 *
 * 允许填 DLL 完整路径，也允许只填 SDK 安装目录（自动补 MVSDKmd.dll）；
 * 目录本身不会被当作库文件去加载。
 */
QStringList expandCandidate(const QString &path)
{
    if (path.isEmpty())
        return QStringList();

    const QFileInfo info(path);
    const bool looksLikeLibrary = path.endsWith(QLatin1String(".dll"), Qt::CaseInsensitive);
    if (looksLikeLibrary && !info.isDir())
        return QStringList(path);

    const QString withName = QDir(path).filePath(QLatin1String(kLibraryName));
    if (info.isDir())
        return QStringList(withName);

    // 路径不存在时两种形式都试，便于定位到底是路径写错还是库缺失
    return QStringList{ withName, path };
}

#if defined(Q_OS_WIN)
/**
 * @brief 把 DLL 所在目录加入搜索路径
 *
 * MVSDKmd.dll 依赖 GenApi / log4cpp / Qt5 等同目录 DLL，
 * 只把单个 DLL 拷到别处时会因依赖缺失而加载失败。
 */
void addDllSearchDirectory(const QString &filePath)
{
    const QFileInfo info(filePath);
    if (!info.isFile())
        return;
    SetDllDirectoryW(reinterpret_cast<const wchar_t *>(
        QDir::toNativeSeparators(info.absolutePath()).utf16()));
}
#endif

} // namespace

QString statusText(int code)
{
    switch (code) {
    case IMV_OK:
        return QStringLiteral("成功");
    case IMV_INVALID_HANDLE:
        return QStringLiteral("无效句柄");
    case IMV_INVALID_PARAM:
        return QStringLiteral("参数错误");
    case IMV_INVALID_FRAME:
        return QStringLiteral("无效帧");
    case IMV_INVALID_RESOURCE:
        return QStringLiteral("资源无效");
    case IMV_INVALID_IP:
        return QStringLiteral("设备与主机 IP 网段不匹配");
    case IMV_NO_MEMORY:
        return QStringLiteral("内存不足");
    case IMV_INSUFFICIENT_MEMORY:
        return QStringLiteral("传入内存不足");
    case IMV_INVALID_ACCESS:
        return QStringLiteral("属性不可访问");
    case IMV_INVALID_RANGE:
        return QStringLiteral("属性值超出范围");
    case IMV_NOT_SUPPORT:
        return QStringLiteral("设备不支持该功能");
    case IMV_RESTORE_STREAM:
        return QStringLiteral("取图恢复中");
    case IMV_RECONNECT_DEVICE:
        return QStringLiteral("重连恢复中");
    case IMV_NOT_AVAILABLE:
        return QStringLiteral("连接不可达");
    case IMV_NOT_GRABBING:
        return QStringLiteral("相机已停止取图");
    case IMV_NOT_CONNECTED:
        return QStringLiteral("设备未连接");
    case IMV_TIMEOUT:
        return QStringLiteral("超时");
    case IMV_IS_CONNECTED:
        return QStringLiteral("设备已连接");
    case IMV_IS_GRABBING:
        return QStringLiteral("设备正在取图");
    case IMV_INVOCATION_ERROR:
        return QStringLiteral("调用时序错误");
    case IMV_SYSTEM_ERROR:
        return QStringLiteral("系统接口返回错误");
    case IMV_OPENFILE_ERROR:
        return QStringLiteral("打开文件失败");
    default:
        return QStringLiteral("未知错误");
    }
}

// ------------------------------------------------------------------- ImvApi

ImvApi::ImvApi() = default;

ImvApi::~ImvApi()
{
    delete m_library;
    m_library = nullptr;
}

ImvApi *ImvApi::instance()
{
    static ImvApi api;
    return &api;
}

QStringList ImvApi::libraryCandidates(const QString &preferredPath)
{
    QStringList paths;
    auto append = [&paths](const QString &path) {
        for (const QString &candidate : expandCandidate(path)) {
            if (!paths.contains(candidate))
                paths.append(candidate);
        }
    };

    append(preferredPath);
    append(QString::fromLocal8Bit(qgetenv("DAHUA_MVSDK_PATH")));

    const QString appDir = QCoreApplication::applicationDirPath();
    append(appDir + QLatin1Char('/') + QLatin1String(kLibraryName));
    append(appDir + QStringLiteral("/MVSDK/") + QLatin1String(kLibraryName));
    append(appDir + QStringLiteral("/plugins/") + QLatin1String(kLibraryName));

    // Python 版使用的默认安装目录（大华 MV Viewer）
    append(QStringLiteral("D:/MVviewer/MV Viewer/Application/x64"));
    append(QStringLiteral("C:/Program Files/Dahua/MVviewer/MV Viewer/Application/x64"));

    // 交给系统 PATH / 当前目录搜索
    append(QLatin1String(kLibraryName));

    return paths;
}

bool ImvApi::resolveSymbols(QLibrary *library, QString *error)
{
    QStringList missing;

#define DAHUA_RESOLVE_REQUIRED(fn, symbol)                                                        \
    do {                                                                                           \
        fn = reinterpret_cast<decltype(fn)>(library->resolve(symbol));                             \
        if (!fn)                                                                                   \
            missing.append(QString::fromLatin1(symbol));                                           \
    } while (false)

#define DAHUA_RESOLVE_OPTIONAL(fn, symbol)                                                        \
    do {                                                                                           \
        fn = reinterpret_cast<decltype(fn)>(library->resolve(symbol));                             \
    } while (false)

    // 官方 SDK 的导出名统一带 IMV_ 前缀（IMVApi.h）
    DAHUA_RESOLVE_REQUIRED(enumDevices, "IMV_EnumDevices");
    DAHUA_RESOLVE_REQUIRED(createHandle, "IMV_CreateHandle");
    DAHUA_RESOLVE_REQUIRED(destroyHandle, "IMV_DestroyHandle");
    DAHUA_RESOLVE_REQUIRED(open, "IMV_Open");
    DAHUA_RESOLVE_REQUIRED(close, "IMV_Close");
    DAHUA_RESOLVE_REQUIRED(startGrabbing, "IMV_StartGrabbing");
    DAHUA_RESOLVE_REQUIRED(stopGrabbing, "IMV_StopGrabbing");
    DAHUA_RESOLVE_REQUIRED(getFrame, "IMV_GetFrame");
    DAHUA_RESOLVE_REQUIRED(releaseFrame, "IMV_ReleaseFrame");
    DAHUA_RESOLVE_REQUIRED(pixelConvert, "IMV_PixelConvert");

    DAHUA_RESOLVE_OPTIONAL(isOpen, "IMV_IsOpen");
    DAHUA_RESOLVE_OPTIONAL(setBufferCount, "IMV_SetBufferCount");
    DAHUA_RESOLVE_OPTIONAL(setEnumFeatureSymbol, "IMV_SetEnumFeatureSymbol");
    DAHUA_RESOLVE_OPTIONAL(setIntFeatureValue, "IMV_SetIntFeatureValue");
    DAHUA_RESOLVE_OPTIONAL(setFloatFeatureValue, "IMV_SetFloatFeatureValue");
    DAHUA_RESOLVE_OPTIONAL(setBoolFeatureValue, "IMV_SetBoolFeatureValue");
    DAHUA_RESOLVE_OPTIONAL(getVersion, "IMV_GetVersion");
    DAHUA_RESOLVE_OPTIONAL(getEnumFeatureSymbol, "IMV_GetEnumFeatureSymbol");
    DAHUA_RESOLVE_OPTIONAL(getStatisticsInfo, "IMV_GetStatisticsInfo");
    DAHUA_RESOLVE_OPTIONAL(clearFrameBuffer, "IMV_ClearFrameBuffer");
    DAHUA_RESOLVE_OPTIONAL(isGrabbing, "IMV_IsGrabbing");

#undef DAHUA_RESOLVE_REQUIRED
#undef DAHUA_RESOLVE_OPTIONAL

    if (!missing.isEmpty()) {
        if (error)
            *error = QStringLiteral("缺少接口: %1").arg(missing.join(QStringLiteral(", ")));
        return false;
    }
    return true;
}

bool ImvApi::load(const QString &preferredPath, QString *error)
{
    if (m_loaded)
        return true;

    QStringList attempts;
    for (const QString &path : libraryCandidates(preferredPath)) {
#if defined(Q_OS_WIN)
        addDllSearchDirectory(path);
#endif
        QLibrary *library = new QLibrary(path);
        if (!library->load()) {
            attempts.append(QStringLiteral("%1（%2）").arg(path, library->errorString()));
            delete library;
            continue;
        }

        QString resolveError;
        if (!resolveSymbols(library, &resolveError)) {
            attempts.append(QStringLiteral("%1（%2）").arg(path, resolveError));
            delete library;
            continue;
        }

        delete m_library;
        m_library = library;
        m_libraryPath = path;
        m_loaded = true;
        m_lastError.clear();
        if (error)
            error->clear();
        return true;
    }

    m_lastError = QStringLiteral("未能加载大华 MVSDK（%1）。已尝试：\n%2\n"
                                 "请安装大华 MV Viewer / 相机 SDK，或在参数「MVSDK 路径」中填写 "
                                 "%1 的完整路径（也可只填 SDK 所在目录），"
                                 "或设置环境变量 DAHUA_MVSDK_PATH。\n"
                                 "注意：%1 依赖同目录的 GenApi / log4cpp / Qt5 等 DLL，"
                                 "单独拷贝一个 DLL 会加载失败。")
                      .arg(QLatin1String(kLibraryName), attempts.join(QStringLiteral("\n")));
    if (error)
        *error = m_lastError;
    return false;
}

QString ImvApi::version() const
{
    if (!m_loaded || !getVersion)
        return QString();
    const char *text = getVersion();
    return text ? QString::fromUtf8(text) : QString();
}

// --------------------------------------------------------------- DeviceInfo

QString DeviceInfo::displayName() const
{
    QString name = QStringLiteral("[%1]").arg(index);
    const QStringList parts{ vendorName, modelName };
    const QString model = parts.join(QLatin1Char(' ')).trimmed();
    if (!model.isEmpty())
        name += QLatin1Char(' ') + model;
    if (!serialNumber.isEmpty())
        name += QStringLiteral(" (%1)").arg(serialNumber);
    if (!ipAddress.isEmpty())
        name += QStringLiteral(" %1").arg(ipAddress);
    return name;
}

// -------------------------------------------------------------- DahuaCamera

DahuaCamera::DahuaCamera() = default;

DahuaCamera::~DahuaCamera()
{
    close();
}

bool DahuaCamera::fail(const QString &message, QString *error)
{
    m_lastError = message;
    if (error)
        *error = message;
    return false;
}

QList<DeviceInfo> DahuaCamera::enumDevices(QString *error, const QString &sdkPath)
{
    QList<DeviceInfo> devices;

    ImvApi *api = ImvApi::instance();
    if (!api->isLoaded() && !api->load(sdkPath, error))
        return devices;

    IMV_DeviceList list;
    std::memset(&list, 0, sizeof(list));

    const int ret = api->enumDevices(&list, interfaceTypeAll);
    if (ret != IMV_OK) {
        assignError(error, QStringLiteral("枚举相机失败: %1（%2）").arg(statusText(ret)).arg(ret));
        return devices;
    }

    if (list.nDevNum == 0 || !list.pDevInfo) {
        assignError(error, QStringLiteral("未发现相机（find no device）"));
        return devices;
    }

    const unsigned int count = qMin(list.nDevNum, MaxDeviceEnumNum);
    for (unsigned int i = 0; i < count; ++i) {
        const IMV_DeviceInfo &info = list.pDevInfo[i];
        DeviceInfo device;
        device.index = static_cast<int>(i);
        device.cameraType = info.nCameraType;
        device.vendorName = fixedString(info.vendorName);
        device.modelName = fixedString(info.modelName);
        device.serialNumber = fixedString(info.serialNumber);
        device.cameraName = fixedString(info.cameraName);
        if (info.nCameraType == typeGigeCamera)
            device.ipAddress = fixedString(info.deviceSpecificInfo.gigeDeviceInfo.ipAddress);
        devices.append(device);
    }

    return devices;
}

bool DahuaCamera::open(int index, QString *error)
{
    if (m_open)
        return true;

    ImvApi *api = ImvApi::instance();
    if (!api->isLoaded() && !api->load(QString(), error))
        return false;

    void *handle = nullptr;
    unsigned int deviceIndex = static_cast<unsigned int>(qMax(0, index));
    int ret = api->createHandle(&handle, modeByIndex, &deviceIndex);
    if (ret != IMV_OK)
        return fail(QStringLiteral("创建句柄失败: %1（%2）").arg(statusText(ret)).arg(ret), error);

    m_handle = handle;
    ret = api->open(m_handle);
    if (ret != IMV_OK) {
        api->destroyHandle(m_handle);
        m_handle = nullptr;
        return fail(QStringLiteral("打开相机失败: %1（%2）").arg(statusText(ret)).arg(ret), error);
    }

    m_open = true;
    m_index = index;

    // ---- 一次性配置（与 Python 版一致）----
    if (api->setBufferCount)
        api->setBufferCount(m_handle, m_bufferCount);

    // 连续采集：部分相机默认是单帧模式，不设置会一直取不到图
    setEnumFeature("AcquisitionMode", "Continuous");

    // 触发相关：先关 TriggerMode，再设 Source / Selector（顺序不能变）
    setEnumFeature("TriggerMode", "Off");

    // 回读确认：仍处于触发模式说明设置没生效，再设一次（GigE/U3V 偶发）
    const QString triggerMode = enumFeatureValue("TriggerMode");
    if (!triggerMode.isEmpty() && triggerMode.compare(QLatin1String("Off"), Qt::CaseInsensitive) != 0)
        setEnumFeature("TriggerMode", "Off");

    setEnumFeature("TriggerSource", "Software");
    setEnumFeature("TriggerSelector", "FrameStart");

    // 下面两个仅 GigE 有效，U3V 会失败，忽略即可
    setIntFeature("GevSCPSPacketSize", static_cast<long long>(m_packetSize));
    setIntFeature("GevStreamChannelSelector", 0);

    // 取流帧率（失败只记录警告，不影响相机打开；fps <= 0 时关闭帧率控制）
    m_lastWarning.clear();
    applyFrameRate();

    return true;
}

/**
 * @brief 应用取流帧率
 *
 * 帧率控制在不同型号上可能是 AcquisitionFrameRateEnable 或
 * AcquisitionFrameRateControlEnable，这里两个都试；
 * 相机不支持时只记录警告，open() 仍返回成功。
 */
bool DahuaCamera::applyFrameRate()
{
    ImvApi *api = ImvApi::instance();
    if (!m_handle)
        return false;

    auto setRateControlEnabled = [&](bool enabled) {
        if (setBoolFeature("AcquisitionFrameRateEnable", enabled) == IMV_OK)
            return;
        setBoolFeature("AcquisitionFrameRateControlEnable", enabled);
    };

    if (m_frameRate <= 0.0) {
        // 0 = 不限制：关闭帧率控制，按相机最大能力出图
        setRateControlEnabled(false);
        return true;
    }

    setRateControlEnabled(true);

    if (!api->setFloatFeatureValue) {
        m_lastWarning = QStringLiteral("当前 MVSDK 不支持浮点属性设置，取流帧率未生效");
        return false;
    }

    const int ret = setFloatFeature("AcquisitionFrameRate", m_frameRate);
    if (ret != IMV_OK) {
        m_lastWarning = QStringLiteral("设置取流帧率 %1 fps 失败：%2（%3），"
                                       "相机继续按自身帧率出图")
                            .arg(m_frameRate, 0, 'f', 1)
                            .arg(statusText(ret))
                            .arg(ret);
        return false;
    }

    return true;
}

void DahuaCamera::close()
{
    ImvApi *api = ImvApi::instance();
    if (!m_handle) {
        m_open = false;
        m_grabbing = false;
        return;
    }

    if (m_grabbing && api->stopGrabbing)
        api->stopGrabbing(m_handle);
    m_grabbing = false;

    if (m_open && api->close)
        api->close(m_handle);
    m_open = false;

    if (api->destroyHandle)
        api->destroyHandle(m_handle);
    m_handle = nullptr;
}

bool DahuaCamera::startGrabbing(QString *error)
{
    if (m_grabbing)
        return true;
    if (!m_open)
        return fail(QStringLiteral("相机未连接"), error);

    ImvApi *api = ImvApi::instance();
    const int ret = api->startGrabbing(m_handle);
    if (ret != IMV_OK)
        return fail(QStringLiteral("开始取流失败: %1（%2）").arg(statusText(ret)).arg(ret), error);

    m_grabbing = true;
    return true;
}

void DahuaCamera::stopGrabbing()
{
    if (!m_grabbing || !m_handle)
        return;
    ImvApi *api = ImvApi::instance();
    api->stopGrabbing(m_handle);
    m_grabbing = false;
}

bool DahuaCamera::getFrame(cv::Mat &out, unsigned int timeoutMs, QString *error,
                           bool withDiagnostics)
{
    if (!m_open)
        return fail(QStringLiteral("相机未连接"), error);
    if (!m_grabbing)
        return fail(QStringLiteral("相机未在取流"), error);

    ImvApi *api = ImvApi::instance();

    IMV_Frame frame;
    std::memset(&frame, 0, sizeof(frame));

    // 首帧（尤其 GigE 刚 StartGrabbing 时）可能较慢，超时就多试几次
    constexpr int kMaxFrameAttempts = 3;
    int ret = IMV_OK;
    for (int attempt = 0; attempt < kMaxFrameAttempts; ++attempt) {
        ret = api->getFrame(m_handle, &frame, timeoutMs);
        if (ret == IMV_OK)
            break;
        if (ret != IMV_TIMEOUT) // 非超时错误（未取流/未连接等）不用重试
            break;
    }

    if (ret != IMV_OK) {
        QString detail = QStringLiteral("获取帧失败: %1（%2），超时 %3 ms")
                             .arg(statusText(ret)).arg(ret).arg(timeoutMs);
        if (withDiagnostics) {
            const QString diagnostics = streamDiagnostics();
            if (!diagnostics.isEmpty())
                detail += QStringLiteral("；%1").arg(diagnostics);
        }
        return fail(detail, error);
    }

    const auto releaseFrame = qScopeGuard([&] {
        if (api->releaseFrame)
            api->releaseFrame(m_handle, &frame);
    });

    bool ok = false;
    QString convertError;

    const IMV_FrameInfo &info = frame.frameInfo;
    const int width = static_cast<int>(info.width);
    const int height = static_cast<int>(info.height);

    if (frame.pData && width > 0 && height > 0) {
        // Bayer GB8：Python 版直接返回原始单通道数据（该型号相机按灰度使用）
        if (info.pixelFormat == gvspPixelBayGB8) {
            const size_t rowStride = static_cast<size_t>(width) + info.paddingX;
            const size_t requiredBytes = rowStride * (height - 1) + width;
            if (requiredBytes > info.size) {
                convertError = QStringLiteral("帧数据长度不足");
            } else {
                out = cv::Mat(height, width, CV_8UC1, frame.pData, rowStride).clone();
                ok = true;
            }
        } else {
            // 其它格式统一转成 BGR8 输出
            const size_t dstSize = static_cast<size_t>(width) * height * 3;
            if (dstSize > (std::numeric_limits<unsigned int>::max)())
                return fail(QStringLiteral("帧尺寸超出像素转换接口范围"), error);
            // SDK 直接写入最终图像，省去转换缓存到 Mat 的整帧复制。
            cv::Mat converted(height, width, CV_8UC3);

            IMV_PixelConvertParam param;
            std::memset(&param, 0, sizeof(param));
            param.nWidth = info.width;
            param.nHeight = info.height;
            param.ePixelFormat = info.pixelFormat;
            param.pSrcData = frame.pData;
            param.nSrcDataLen = info.size;
            param.nPaddingX = info.paddingX;
            param.nPaddingY = info.paddingY;
            param.eBayerDemosaic = demosaicBilinear;
            param.eDstPixelFormat = gvspPixelBGR8;
            param.pDstBuf = converted.data;
            param.nDstBufSize = static_cast<unsigned int>(dstSize);

            const int convertRet = api->pixelConvert(m_handle, &param);
            if (convertRet != IMV_OK) {
                convertError = QStringLiteral("像素格式转换失败: %1（%2）")
                                   .arg(statusText(convertRet)).arg(convertRet);
            } else {
                out = std::move(converted);
                ok = true;
            }
        }
    } else {
        convertError = QStringLiteral("帧数据为空");
    }

    if (!ok)
        return fail(convertError.isEmpty() ? QStringLiteral("取帧失败") : convertError, error);

    return true;
}

bool DahuaCamera::grab(cv::Mat &out, unsigned int timeoutMs, QString *error)
{
    // 已在取流（对话框预览）时只取帧，不打断
    const bool alreadyGrabbing = m_grabbing;
    if (!alreadyGrabbing && !startGrabbing(error))
        return false;

    const auto stopTemporaryStream = qScopeGuard([&] {
        if (!alreadyGrabbing)
            stopGrabbing();
    });
    return getFrame(out, timeoutMs, error);
}

void DahuaCamera::setEnumFeature(const char *name, const char *value)
{
    ImvApi *api = ImvApi::instance();
    if (m_handle && api->setEnumFeatureSymbol)
        api->setEnumFeatureSymbol(m_handle, name, value);
}

void DahuaCamera::setIntFeature(const char *name, long long value)
{
    ImvApi *api = ImvApi::instance();
    if (m_handle && api->setIntFeatureValue)
        api->setIntFeatureValue(m_handle, name, value);
}

int DahuaCamera::setFloatFeature(const char *name, double value)
{
    ImvApi *api = ImvApi::instance();
    if (!m_handle || !api->setFloatFeatureValue)
        return IMV_NOT_SUPPORT;
    return api->setFloatFeatureValue(m_handle, name, value);
}

int DahuaCamera::setBoolFeature(const char *name, bool value)
{
    ImvApi *api = ImvApi::instance();
    if (!m_handle || !api->setBoolFeatureValue)
        return IMV_NOT_SUPPORT;
    return api->setBoolFeatureValue(m_handle, name, value);
}

QString DahuaCamera::enumFeatureValue(const char *name) const
{
    ImvApi *api = ImvApi::instance();
    if (!m_handle || !api->getEnumFeatureSymbol)
        return QString();

    IMV_String value;
    std::memset(&value, 0, sizeof(value));
    if (api->getEnumFeatureSymbol(m_handle, name, &value) != IMV_OK)
        return QString();

    return fixedString(value.str);
}

QString DahuaCamera::streamDiagnostics() const
{
    ImvApi *api = ImvApi::instance();
    QStringList items;

    if (m_handle && api->getStatisticsInfo) {
        IMV_StreamStatisticsInfo stats;
        std::memset(&stats, 0, sizeof(stats));
        if (api->getStatisticsInfo(m_handle, &stats) == IMV_OK) {
            unsigned int received = 0;
            unsigned int errored = 0;
            switch (stats.nCameraType) {
            case typeGigeCamera:
                received = stats.gigeStatisticsInfo.imageReceived;
                errored = stats.gigeStatisticsInfo.imageError;
                break;
            case typeU3vCamera:
                received = stats.u3vStatisticsInfo.imageReceived;
                errored = stats.u3vStatisticsInfo.imageError;
                break;
            case typePCIeCamera:
                received = stats.pcieStatisticsInfo.imageReceived;
                errored = stats.pcieStatisticsInfo.imageError;
                break;
            default:
                break;
            }
            items.append(QStringLiteral("已收帧=%1，错误帧=%2").arg(received).arg(errored));
        }
    }

    const QString triggerMode = enumFeatureValue("TriggerMode");
    if (!triggerMode.isEmpty())
        items.append(QStringLiteral("TriggerMode=%1").arg(triggerMode));

    return items.join(QStringLiteral("，"));
}

// -------------------------------------------------------------- DahuaGrabber

namespace {

constexpr int kGrabTimeoutMs = 1000;         //!< 后台线程每次取帧的超时
constexpr int kMaxConsecutiveFailures = 10;  //!< 连续失败次数，超过则认为相机已断开
constexpr int kOpenWaitMs = 8000;            //!< startGrab 等待相机打开的最长时间

} // namespace

DahuaGrabber::DahuaGrabber(QObject *parent)
    : QThread(parent)
{
}

DahuaGrabber::~DahuaGrabber()
{
    stopGrab();
}

void DahuaGrabber::setOpenOptions(int index, const QString &sdkPath, double frameRate)
{
    QMutexLocker locker(&m_mutex);
    m_index = qMax(0, index);
    m_sdkPath = sdkPath;
    m_frameRate = frameRate;
}

bool DahuaGrabber::startGrab(QString *error)
{
    if (isGrabbing())
        return true;
    if (isRunning())
        stopGrab();

    {
        QMutexLocker locker(&m_mutex);
        m_error.clear();
        m_warning.clear();
        m_started = false;
        m_seq = 0;
        m_fps = 0.0;
        m_latest.reset();
    }

    m_stop.store(false);
    start();

    // 等相机真正打开，让调用方立刻拿到明确结果
    QDeadlineTimer deadline(kOpenWaitMs);
    QMutexLocker locker(&m_mutex);
    while (!m_started) {
        if (!m_cond.wait(&m_mutex, deadline))
            break;
    }

    if (!m_error.isEmpty()) {
        if (error)
            *error = m_error;
        return false;
    }
    if (!m_started) {
        if (error)
            *error = QStringLiteral("打开相机超时");
        return false;
    }
    return true;
}

void DahuaGrabber::stopGrab()
{
    if (!isRunning())
        return;

    m_stop.store(true);
    {
        QMutexLocker locker(&m_mutex);
        m_cond.wakeAll();
    }
    wait(6000);
}

bool DahuaGrabber::isGrabbing() const
{
    return isRunning() && !m_stop.load();
}

bool DahuaGrabber::lastFrame(cv::Mat &out, quint64 *seq) const
{
    std::shared_ptr<const cv::Mat> snapshot;
    {
        QMutexLocker locker(&m_mutex);
        if (!m_latest || m_latest->empty())
            return false;

        snapshot = m_latest;
        if (seq)
            *seq = m_seq;
    }
    // 发布后的帧不再修改，引用计数保证解锁后仍有效；输出独立副本供下游修改。
    out = snapshot->clone();
    return true;
}

bool DahuaGrabber::waitFrame(cv::Mat &out, int timeoutMs, quint64 afterSeq,
                             QString *error, quint64 *seq)
{
    QDeadlineTimer deadline(timeoutMs);
    QMutexLocker locker(&m_mutex);

    while (m_seq <= afterSeq && m_error.isEmpty() && !m_stop.load()) {
        if (timeoutMs <= 0)
            break;
        if (!m_cond.wait(&m_mutex, deadline))
            break;
    }

    if (!m_error.isEmpty() && m_seq == 0) {
        if (error)
            *error = m_error;
        return false;
    }
    if (m_seq == 0) {
        if (error)
            *error = QStringLiteral("相机未输出图像");
        return false;
    }
    if (m_seq <= afterSeq) {
        if (error)
            *error = QStringLiteral("等待新帧超时（%1 ms）").arg(timeoutMs);
        return false;
    }

    std::shared_ptr<const cv::Mat> snapshot = m_latest;
    if (seq)
        *seq = m_seq;
    locker.unlock();
    out = snapshot->clone();
    return true;
}

void DahuaGrabber::prepareFrame(std::shared_ptr<cv::Mat> &frame)
{
    // 帧已移出最新帧槽，当前无新读者；仅在旧读者全部释放后复用缓冲区。
    QMutexLocker locker(&m_mutex);
    if (!frame || frame.use_count() != 1)
        frame = std::make_shared<cv::Mat>();
}

void DahuaGrabber::publishFrame(std::shared_ptr<cv::Mat> &frame)
{
    // 只交换所有权，不在采集锁内复制图像。
    QMutexLocker locker(&m_mutex);
    m_latest.swap(frame);
    ++m_seq;
    m_width = m_latest->cols;
    m_height = m_latest->rows;
    m_cond.wakeAll();
}

quint64 DahuaGrabber::frameSeq() const
{
    QMutexLocker locker(&m_mutex);
    return m_seq;
}

double DahuaGrabber::measuredFps() const
{
    QMutexLocker locker(&m_mutex);
    return m_fps;
}

int DahuaGrabber::frameWidth() const
{
    QMutexLocker locker(&m_mutex);
    return m_width;
}

int DahuaGrabber::frameHeight() const
{
    QMutexLocker locker(&m_mutex);
    return m_height;
}

int DahuaGrabber::cameraIndex() const
{
    QMutexLocker locker(&m_mutex);
    return m_index;
}

QString DahuaGrabber::error() const
{
    QMutexLocker locker(&m_mutex);
    return m_error;
}

QString DahuaGrabber::warning() const
{
    QMutexLocker locker(&m_mutex);
    return m_warning;
}

void DahuaGrabber::run()
{
    int index = 0;
    QString sdkPath;
    double frameRate = 0.0;
    {
        QMutexLocker locker(&m_mutex);
        index = m_index;
        sdkPath = m_sdkPath;
        frameRate = m_frameRate;
    }

    // —— 长连接：Open / StartGrabbing 只做一次，之后只在 stopGrab 时关闭 ——
    QString error;
    ImvApi *api = ImvApi::instance();
    if (!api->isLoaded() && !api->load(sdkPath, &error)) {
        QMutexLocker locker(&m_mutex);
        m_error = error;
        m_started = true;
        m_cond.wakeAll();
        return;
    }

    m_camera.setFrameRate(frameRate);

    if (!m_camera.open(index, &error)) {
        QMutexLocker locker(&m_mutex);
        m_error = error;
        m_started = true;
        m_cond.wakeAll();
        return;
    }
    if (!m_camera.startGrabbing(&error)) {
        m_camera.close();
        QMutexLocker locker(&m_mutex);
        m_error = error;
        m_started = true;
        m_cond.wakeAll();
        return;
    }

    {
        QMutexLocker locker(&m_mutex);
        m_started = true;
        m_error.clear();
        m_warning = m_camera.lastWarning();   // 帧率设置等非致命提示
        m_cond.wakeAll();
    }

    std::shared_ptr<cv::Mat> frame;
    int failures = 0;
    quint64 counted = 0;
    QElapsedTimer fpsTimer;
    fpsTimer.start();

    while (!m_stop.load(std::memory_order_relaxed)) {
        prepareFrame(frame);
        if (!m_camera.getFrame(*frame, kGrabTimeoutMs, &error, false)) {
            if (m_stop.load())
                break;
            if (++failures >= kMaxConsecutiveFailures) {
                QMutexLocker locker(&m_mutex);
                m_error = error;
                m_cond.wakeAll();
                break;
            }
            continue;
        }
        failures = 0;

        // 交换采集/最新帧缓冲区；prepareFrame 保证下一次 getFrame 不覆盖读者的像素。
        publishFrame(frame);

        if (++counted >= 30) {
            const double elapsed = fpsTimer.elapsed() / 1000.0;
            if (elapsed > 0.0) {
                QMutexLocker locker(&m_mutex);
                m_fps = counted / elapsed;
            }
            counted = 0;
            fpsTimer.restart();
        }
    }

    m_camera.stopGrabbing();
    m_camera.close();
}

} // namespace DahuaMV
