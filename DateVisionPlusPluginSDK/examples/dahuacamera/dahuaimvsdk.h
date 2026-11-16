#ifndef DAHUAIMVSDK_H
#define DAHUAIMVSDK_H

/**
 * @brief 大华工业相机 MVSDK（MVSDKmd.dll）的精简封装
 *
 * 对应 Python 版：user_plugins/MVSDK/* + user_plugins/mv_camera.py
 *
 * 设计要点：
 *   1) 运行时用 QLibrary 动态加载 MVSDKmd.dll，不依赖官方 SDK 头文件 / 导入库。
 *      机器没装大华 SDK 时插件依然能被主程序加载，只在取图时报错，
 *      不会导致整个插件 DLL 加载失败。
 *   2) 结构体 / 枚举按官方 IMVApi.h（与 Python ctypes 定义一致）1:1 复刻。
 *   3) DahuaGrabber（QThread）在对话框点击「连接」后一次性 Open + StartGrabbing，
 *      线程内循环 IMV_GetFrame 并把最新帧写入内部变量（带帧序号）；
 *      流程执行只读该变量，不再有开关流开销，也不会取到过时图像。
 */

#include <QLibrary>
#include <QList>
#include <QMutex>
#include <QString>
#include <QStringList>
#include <QThread>
#include <QWaitCondition>

#include <opencv2/core.hpp>

#include <atomic>
#include <memory>

#if defined(_WIN32)
#  define DAHUA_IMV_CALL __stdcall
#else
#  define DAHUA_IMV_CALL
#endif

namespace DahuaMV {

// ---------------------------------------------------------------- 常量与枚举

/** 返回码（节选，完整列表见官方 IMVDefines.h） */
enum StatusCode {
    IMV_OK = 0,
    IMV_ERROR = -101,
    IMV_INVALID_HANDLE = -102,
    IMV_INVALID_PARAM = -103,
    IMV_INVALID_FRAME_HANDLE = -104,
    IMV_INVALID_FRAME = -105,
    IMV_INVALID_RESOURCE = -106,
    IMV_INVALID_IP = -107,
    IMV_NO_MEMORY = -108,
    IMV_INSUFFICIENT_MEMORY = -109,
    IMV_ERROR_PROPERTY_TYPE = -110,
    IMV_INVALID_ACCESS = -111,
    IMV_INVALID_RANGE = -112,
    IMV_NOT_SUPPORT = -113,
    IMV_RESTORE_STREAM = -114,
    IMV_RECONNECT_DEVICE = -115,
    IMV_NOT_AVAILABLE = -116,
    IMV_NOT_GRABBING = -117,
    IMV_NOT_CONNECTED = -118,
    IMV_TIMEOUT = -119,
    IMV_IS_CONNECTED = -120,
    IMV_IS_GRABBING = -121,
    IMV_INVOCATION_ERROR = -122,
    IMV_SYSTEM_ERROR = -123,
    IMV_OPENFILE_ERROR = -124
};

constexpr int MaxStringLength = 256;
constexpr unsigned int MaxDeviceEnumNum = 100;

/** 接口类型 */
enum InterfaceType {
    interfaceTypeAll = 0x00000000,
    interfaceTypeGige = 0x00000001,
    interfaceTypeUsb3 = 0x00000002,
    interfaceTypeCL = 0x00000004,
    interfaceTypePCIe = 0x00000008,
    interfaceInvalidType = 0xFFFFFFFF
};

/** 设备类型 */
enum CameraType {
    typeGigeCamera = 0,
    typeU3vCamera = 1,
    typeCLCamera = 2,
    typePCIeCamera = 3,
    typeUndefinedCamera = 255
};

/** 创建句柄方式 */
enum CreateHandleMode {
    modeByIndex = 0,
    modeByCameraKey = 1,
    modeByDeviceUserID = 2,
    modeByIPAddress = 3
};

/** Bayer 转 RGB 算法 */
enum BayerDemosaic {
    demosaicNearestNeighbor = 0,
    demosaicBilinear = 1,          // 官方头文件里也叫 demosaicEdgeDirected
    demosaicEdgeSensing = 2,
    demosaicNotSupport = 255
};

/** 像素格式（节选） */
enum PixelType {
    gvspPixelTypeUndefined = -1,
    gvspPixelMono8 = 0x01080001,
    gvspPixelMono10 = 0x01100003,
    gvspPixelMono12 = 0x01100005,
    gvspPixelMono16 = 0x01100007,
    gvspPixelBayGR8 = 0x01080008,
    gvspPixelBayRG8 = 0x01080009,
    gvspPixelBayGB8 = 0x0108000A,  // Python 版的 GVSP_PIXEL_BAYER_GB8
    gvspPixelBayBG8 = 0x0108000B,
    gvspPixelRGB8 = 0x02180014,
    gvspPixelBGR8 = 0x02180015
};

// ------------------------------------------------------------------ 结构体
// 布局与 Python ctypes 定义（IMVDefines.py）完全一致，勿随意增删字段。

#pragma pack(push, 8)

struct IMV_GigEInterfaceInfo
{
    char description[MaxStringLength];
    char macAddress[MaxStringLength];
    char ipAddress[MaxStringLength];
    char subnetMask[MaxStringLength];
    char defaultGateWay[MaxStringLength];
    char chReserved[5][MaxStringLength];
};

struct IMV_UsbInterfaceInfo
{
    char description[MaxStringLength];
    char vendorID[MaxStringLength];
    char deviceID[MaxStringLength];
    char subsystemID[MaxStringLength];
    char revision[MaxStringLength];
    char speed[MaxStringLength];
    char chReserved[4][MaxStringLength];
};

union IMV_InterfaceInfo
{
    IMV_GigEInterfaceInfo gigeInterfaceInfo;
    IMV_UsbInterfaceInfo usbInterfaceInfo;
};

struct IMV_GigEDeviceInfo
{
    unsigned int nIpConfigOptions;
    unsigned int nIpConfigCurrent;
    unsigned int nReserved[3];
    char macAddress[MaxStringLength];
    char ipAddress[MaxStringLength];
    char subnetMask[MaxStringLength];
    char defaultGateWay[MaxStringLength];
    char protocolVersion[MaxStringLength];
    char ipConfiguration[MaxStringLength];
    char strReserved[6][MaxStringLength];
};

struct IMV_UsbDeviceInfo
{
    bool bLowSpeedSupported;
    bool bFullSpeedSupported;
    bool bHighSpeedSupported;
    bool bSuperSpeedSupported;
    bool bDriverInstalled;
    bool boolReserved[3];
    unsigned int Reserved[4];
    char configurationValid[MaxStringLength];
    char genCPVersion[MaxStringLength];
    char u3vVersion[MaxStringLength];
    char deviceGUID[MaxStringLength];
    char familyName[MaxStringLength];
    char u3vSerialNumber[MaxStringLength];
    char speed[MaxStringLength];
    char maxPower[MaxStringLength];
    char chReserved[4][MaxStringLength];
};

union IMV_DeviceSpecificInfo
{
    IMV_GigEDeviceInfo gigeDeviceInfo;
    IMV_UsbDeviceInfo usbDeviceInfo;
};

struct IMV_DeviceInfo
{
    int nCameraType;
    int nCameraReserved[5];
    char cameraKey[MaxStringLength];
    char cameraName[MaxStringLength];
    char serialNumber[MaxStringLength];
    char vendorName[MaxStringLength];
    char modelName[MaxStringLength];
    char manufactureInfo[MaxStringLength];
    char deviceVersion[MaxStringLength];
    char cameraReserved[5][MaxStringLength];
    IMV_DeviceSpecificInfo deviceSpecificInfo;
    int nInterfaceType;
    int nInterfaceReserved[5];
    char interfaceName[MaxStringLength];
    char interfaceReserved[5][MaxStringLength];
    IMV_InterfaceInfo interfaceInfo;
};

struct IMV_DeviceList
{
    unsigned int nDevNum;
    IMV_DeviceInfo *pDevInfo;
};

struct IMV_FrameInfo
{
    unsigned long long blockId;
    unsigned int status;
    unsigned int width;
    unsigned int height;
    unsigned int size;
    int pixelFormat;
    unsigned long long timeStamp;
    unsigned int chunkCount;
    unsigned int paddingX;
    unsigned int paddingY;
    unsigned int recvFrameTime;
    unsigned int nReserved[19];
};

struct IMV_Frame
{
    void *frameHandle;
    unsigned char *pData;
    IMV_FrameInfo frameInfo;
    unsigned int nReserved[10];
};

struct IMV_PixelConvertParam
{
    unsigned int nWidth;
    unsigned int nHeight;
    int ePixelFormat;
    unsigned char *pSrcData;
    unsigned int nSrcDataLen;
    unsigned int nPaddingX;
    unsigned int nPaddingY;
    int eBayerDemosaic;
    int eDstPixelFormat;
    unsigned char *pDstBuf;
    unsigned int nDstBufSize;
    unsigned int nDstDataLen;
    unsigned int nReserved[8];
};

/** 通用字符串（官方 IMV_String） */
struct IMV_String
{
    char str[MaxStringLength];
};

/** 流统计信息（取帧超时时用于判断相机是否真的在出图） */
struct IMV_GigEStreamStatsInfo
{
    unsigned int nReserved0[10];
    unsigned int imageError;
    unsigned int lostPacketBlock;
    unsigned int nReserved1[4];
    unsigned int nReserved2[5];
    unsigned int imageReceived;
    double fps;
    double bandwidth;
    unsigned int nReserved[4];
};

struct IMV_U3VStreamStatsInfo
{
    unsigned int imageError;
    unsigned int lostPacketBlock;
    unsigned int nReserved0[10];
    unsigned int imageReceived;
    double fps;
    double bandwidth;
    unsigned int nReserved[8];
};

struct IMV_PCIEStreamStatsInfo
{
    unsigned int imageError;
    unsigned int lostPacketBlock;
    unsigned int nReserved0[10];
    unsigned int imageReceived;
    double fps;
    double bandwidth;
    unsigned int nReserved[8];
};

struct IMV_StreamStatisticsInfo
{
    int nCameraType;
    union {
        IMV_PCIEStreamStatsInfo pcieStatisticsInfo;
        IMV_U3VStreamStatsInfo u3vStatisticsInfo;
        IMV_GigEStreamStatsInfo gigeStatisticsInfo;
    };
};

#pragma pack(pop)

// ------------------------------------------------------------ 函数指针类型

typedef int(DAHUA_IMV_CALL *FnEnumDevices)(IMV_DeviceList *, unsigned int);
typedef int(DAHUA_IMV_CALL *FnCreateHandle)(void **, int, void *);
typedef int(DAHUA_IMV_CALL *FnDestroyHandle)(void *);
typedef int(DAHUA_IMV_CALL *FnOpen)(void *);
typedef int(DAHUA_IMV_CALL *FnClose)(void *);
typedef int(DAHUA_IMV_CALL *FnIsOpen)(void *);
typedef int(DAHUA_IMV_CALL *FnSetBufferCount)(void *, unsigned int);
typedef int(DAHUA_IMV_CALL *FnStartGrabbing)(void *);
typedef int(DAHUA_IMV_CALL *FnStopGrabbing)(void *);
typedef int(DAHUA_IMV_CALL *FnGetFrame)(void *, IMV_Frame *, unsigned int);
typedef int(DAHUA_IMV_CALL *FnReleaseFrame)(void *, IMV_Frame *);
typedef int(DAHUA_IMV_CALL *FnPixelConvert)(void *, IMV_PixelConvertParam *);
typedef int(DAHUA_IMV_CALL *FnSetEnumFeatureSymbol)(void *, const char *, const char *);
typedef int(DAHUA_IMV_CALL *FnSetIntFeatureValue)(void *, const char *, long long);
typedef int(DAHUA_IMV_CALL *FnSetFloatFeatureValue)(void *, const char *, double);
typedef int(DAHUA_IMV_CALL *FnSetBoolFeatureValue)(void *, const char *, bool);
typedef const char *(DAHUA_IMV_CALL *FnGetVersion)();
typedef int(DAHUA_IMV_CALL *FnGetEnumFeatureSymbol)(void *, const char *, IMV_String *);
typedef int(DAHUA_IMV_CALL *FnGetStatisticsInfo)(void *, IMV_StreamStatisticsInfo *);
typedef int(DAHUA_IMV_CALL *FnClearFrameBuffer)(void *);
typedef int(DAHUA_IMV_CALL *FnIsGrabbing)(void *);

/**
 * @brief MVSDKmd.dll 加载器（进程内单例）
 *
 * 必需符号全部解析成功才算加载成功；可选符号（IsOpen / 属性设置等）
 * 缺失时置空，调用处自动跳过，兼容不同版本的 DLL。
 */
class ImvApi
{
public:
    static ImvApi *instance();

    /** 按候选路径依次尝试加载；preferredPath 为空则走自动探测 */
    bool load(const QString &preferredPath = QString(), QString *error = nullptr);
    bool isLoaded() const { return m_loaded; }

    QString libraryPath() const { return m_libraryPath; }
    QString lastError() const { return m_lastError; }
    QString version() const;

    /** DLL 候选路径（按优先级排序） */
    static QStringList libraryCandidates(const QString &preferredPath = QString());

    FnEnumDevices enumDevices = nullptr;
    FnCreateHandle createHandle = nullptr;
    FnDestroyHandle destroyHandle = nullptr;
    FnOpen open = nullptr;
    FnClose close = nullptr;
    FnIsOpen isOpen = nullptr;
    FnSetBufferCount setBufferCount = nullptr;
    FnStartGrabbing startGrabbing = nullptr;
    FnStopGrabbing stopGrabbing = nullptr;
    FnGetFrame getFrame = nullptr;
    FnReleaseFrame releaseFrame = nullptr;
    FnPixelConvert pixelConvert = nullptr;
    FnSetEnumFeatureSymbol setEnumFeatureSymbol = nullptr;
    FnSetIntFeatureValue setIntFeatureValue = nullptr;
    FnSetFloatFeatureValue setFloatFeatureValue = nullptr;
    FnSetBoolFeatureValue setBoolFeatureValue = nullptr;
    FnGetVersion getVersion = nullptr;
    FnGetEnumFeatureSymbol getEnumFeatureSymbol = nullptr;
    FnGetStatisticsInfo getStatisticsInfo = nullptr;
    FnClearFrameBuffer clearFrameBuffer = nullptr;
    FnIsGrabbing isGrabbing = nullptr;

private:
    friend class DahuaCameraTest;
    ImvApi();
    ~ImvApi();
    ImvApi(const ImvApi &) = delete;
    ImvApi &operator=(const ImvApi &) = delete;

    bool resolveSymbols(QLibrary *library, QString *error);

    QLibrary *m_library = nullptr;
    bool m_loaded = false;
    QString m_libraryPath;
    QString m_lastError;
};

/** 枚举到的相机信息 */
struct DeviceInfo
{
    int index = -1;
    int cameraType = typeUndefinedCamera;
    QString vendorName;
    QString modelName;
    QString serialNumber;
    QString cameraName;
    QString ipAddress;

    /** 下拉框显示文本，如 "[0] DaHua A5201CU210 (SN123456)" */
    QString displayName() const;
};

/**
 * @brief 单台大华相机的连接与取帧（对应 Python 的 DahuaCamera）
 *
 * 非线程安全，按 Python 版约定：由流程执行线程或 UI 线程单独使用。
 */
class DahuaCamera
{
public:
    DahuaCamera();
    ~DahuaCamera();

    /** 枚举所有相机；error 为空表示成功。sdkPath 为空则自动探测 MVSDKmd.dll */
    static QList<DeviceInfo> enumDevices(QString *error = nullptr,
                                         const QString &sdkPath = QString());

    /** 打开设备（已打开则直接返回 true），并把触发模式配置为连续采集 */
    bool open(int index, QString *error = nullptr);
    /** 停止取流并彻底释放设备句柄 */
    void close();
    bool isOpen() const { return m_open; }
    bool isGrabbing() const { return m_grabbing; }
    int index() const { return m_index; }

    bool startGrabbing(QString *error = nullptr);
    void stopGrabbing();

    /**
     * @brief 取一帧（要求已 startGrabbing）
     * @param withDiagnostics 失败时是否附带统计/触发模式诊断（后台常驻取图时关掉，避免额外开销）
     */
    bool getFrame(cv::Mat &out, unsigned int timeoutMs = 1000, QString *error = nullptr,
                  bool withDiagnostics = true);

    /**
     * @brief 便捷取帧：未在取流时自动 Start -> Get -> Stop，
     *        已在取流（如对话框预览）时只取帧、不打断取流。
     * @note 插件已改用 DahuaGrabber 常驻取图，此接口仅保留给独立测试代码。
     */
    bool grab(cv::Mat &out, unsigned int timeoutMs = 1000, QString *error = nullptr);

    void setBufferCount(unsigned int count) { m_bufferCount = count; }
    void setPacketSize(unsigned int size) { m_packetSize = size; }

    /**
     * @brief 设置取流帧率（fps）
     *
     * <= 0 表示不限制（关闭帧率控制，按相机最大能力出图）；
     * 需在 open() 之前设置，重连后生效。
     */
    void setFrameRate(double fps) { m_frameRate = fps; }
    double frameRate() const { return m_frameRate; }

    QString lastError() const { return m_lastError; }
    /** 非致命警告（例如相机不支持帧率控制，open 仍成功） */
    QString lastWarning() const { return m_lastWarning; }

private:
    friend class DahuaCameraTest;
    void setEnumFeature(const char *name, const char *value);
    void setIntFeature(const char *name, long long value);
    int setFloatFeature(const char *name, double value);
    int setBoolFeature(const char *name, bool value);
    /** 应用取流帧率；失败只记录警告，不阻断打开相机 */
    bool applyFrameRate();
    bool fail(const QString &message, QString *error);

    /** 读取枚举属性当前值（失败返回空） */
    QString enumFeatureValue(const char *name) const;
    /** 取帧失败时的诊断信息：已收帧数 / 触发模式等 */
    QString streamDiagnostics() const;

    void *m_handle = nullptr;
    bool m_open = false;
    bool m_grabbing = false;
    int m_index = -1;
    unsigned int m_bufferCount = 4;
    unsigned int m_packetSize = 1448;
    double m_frameRate = 0.0;
    QString m_lastError;
    QString m_lastWarning;
};

/**
 * @brief 大华相机后台连续取图线程
 *
 * 生命周期与 UVC 插件一致：
 *   startGrab() -> 线程内 Open + StartGrabbing，之后循环 GetFrame，
 *                  每帧写入内部变量并累加帧序号；
 *   stopGrab()  -> 停止线程 + StopGrabbing + Close（只有这里会关流）。
 *
 * 外部（流程执行 / 对话框预览）只读最新帧变量，不直接操作相机 SDK。
 */
class DahuaGrabber : public QThread
{
public:
    explicit DahuaGrabber(QObject *parent = nullptr);
    ~DahuaGrabber() override;

    /**
     * @brief 设置打开参数（仅在未取流时生效）
     * @param frameRate 取流帧率（fps）；<= 0 表示不限制
     */
    void setOpenOptions(int index, const QString &sdkPath = QString(), double frameRate = 0.0);

    /** 打开相机 + 开始取流 + 启动后台线程；失败时 error 给出原因 */
    bool startGrab(QString *error = nullptr);
    /** 停止后台线程并关闭相机 */
    void stopGrab();
    bool isGrabbing() const;

    /** 最新一帧（拷贝出来）；一帧都没有时返回 false */
    bool lastFrame(cv::Mat &out, quint64 *seq = nullptr) const;

    /**
     * @brief 等待一帧序号大于 afterSeq 的新图
     * @param timeoutMs 最长等待时间；<= 0 表示不等待，立即返回当前帧
     * @param seq 非空时返回本次取到的帧序号
     */
    bool waitFrame(cv::Mat &out, int timeoutMs, quint64 afterSeq = 0,
                   QString *error = nullptr, quint64 *seq = nullptr);

    quint64 frameSeq() const;
    /** 后台线程实测帧率 */
    double measuredFps() const;
    int frameWidth() const;
    int frameHeight() const;
    int cameraIndex() const;

    /** 后台线程的最后一次错误（取帧失败 / 相机断开等） */
    QString error() const;
    /** 非致命警告（例如相机不支持设置帧率） */
    QString warning() const;

protected:
    void run() override;

private:
    friend class DahuaGrabberTest;
    void prepareFrame(std::shared_ptr<cv::Mat> &frame);
    void publishFrame(std::shared_ptr<cv::Mat> &frame);

    mutable QMutex m_mutex;
    QWaitCondition m_cond;

    DahuaCamera m_camera;
    int m_index = 0;
    QString m_sdkPath;
    double m_frameRate = 0.0;
    std::shared_ptr<cv::Mat> m_latest;
    quint64 m_seq = 0;
    double m_fps = 0.0;
    int m_width = 0;
    int m_height = 0;
    bool m_started = false;
    QString m_error;
    QString m_warning;

    std::atomic<bool> m_stop{false};
};

/** 错误码转可读文本 */
QString statusText(int code);

} // namespace DahuaMV

#endif // DAHUAIMVSDK_H
