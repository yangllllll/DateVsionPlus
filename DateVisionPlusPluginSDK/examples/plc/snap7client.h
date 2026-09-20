#ifndef SNAP7CLIENT_H
#define SNAP7CLIENT_H

/**
 * @brief snap7.dll 的精简 C 接口封装（对应 Python 的 snap7.client.Client）
 *
 * 对应 Python 版：user_plugins/plc.py 中的  self._plc = snap7.client.Client()
 *
 * 设计要点（与大华相机插件一致）：
 *   1) 运行时用 QLibrary 动态加载 snap7.dll，不依赖 snap7 的头文件 / 导入库，
 *      因此 MSVC 与 MinGW 套件都能构建；机器上没有 snap7.dll 时，本插件 DLL
 *      依然能被主程序加载，只在真正连接 PLC 时给出明确错误。
 *   2) 常量与函数签名按 snap7.h（snap7-full-1.4.2/release/Wrappers/c-cpp/snap7.h）
 *      1:1 复刻：S7Object = uintptr_t，Windows 下调用约定 __stdcall。
 *   3) 只保留本项目用到的接口：创建/销毁、连接/断开、读写存储区、错误文本。
 *
 * 运行期 DLL 查找顺序（见 Snap7Api::libraryCandidates）：
 *   显式路径 -> 环境变量 SNAP7_PATH -> 主程序目录 -> 主程序目录/plugins
 *   -> 主程序目录/snap7 -> 系统 PATH
 */

#include <QLibrary>
#include <QString>
#include <QStringList>

#include <cstdint>

#if defined(Q_OS_WIN)
#  define SNAP7_CALL __stdcall
#else
#  define SNAP7_CALL
#endif

namespace Snap7 {

/** snap7.h: typedef uintptr_t S7Object */
using S7Object = std::uintptr_t;

/** Area ID（snap7.h 常量值） */
enum Area {
    AreaPE = 0x81, ///< 输入映像区 I
    AreaPA = 0x82, ///< 输出映像区 Q
    AreaMK = 0x83, ///< 位存储区 M（Python 版 snap7.Area.MK）
    AreaDB = 0x84, ///< 数据块 DB
    AreaCT = 0x1C, ///< 计数器
    AreaTM = 0x1D  ///< 定时器
};

/** Word Length（snap7.h 常量值） */
enum WordLen {
    WLBit = 0x01,
    WLByte = 0x02, ///< Python write_area 的默认单位
    WLWord = 0x04,
    WLDWord = 0x06,
    WLReal = 0x08
};

// ------------------------------------------------------------ 函数指针类型

typedef S7Object(SNAP7_CALL *FnCliCreate)();
typedef void(SNAP7_CALL *FnCliDestroy)(S7Object *);
typedef int(SNAP7_CALL *FnCliConnect)(S7Object);
typedef int(SNAP7_CALL *FnCliConnectTo)(S7Object, const char *, int, int);
typedef int(SNAP7_CALL *FnCliDisconnect)(S7Object);
typedef int(SNAP7_CALL *FnCliGetConnected)(S7Object, int *);
typedef int(SNAP7_CALL *FnCliReadArea)(S7Object, int, int, int, int, int, void *);
typedef int(SNAP7_CALL *FnCliWriteArea)(S7Object, int, int, int, int, int, void *);
typedef int(SNAP7_CALL *FnCliGetLastError)(S7Object, int *);
typedef int(SNAP7_CALL *FnCliErrorText)(int, char *, int);

/**
 * @brief snap7.dll 加载器（进程内单例）
 *
 * 必需符号全部解析成功才算加载成功；可选符号（Cli_Connect / Cli_ReadArea /
 * Cli_GetLastError）缺失时置空，调用处自动降级，兼容不同版本的 DLL。
 */
class Snap7Api
{
public:
    static Snap7Api *instance();

    /** 按候选路径依次尝试加载；preferredPath 为空则走自动探测 */
    bool load(const QString &preferredPath = QString(), QString *error = nullptr);
    bool isLoaded() const { return m_loaded; }

    QString libraryPath() const { return m_libraryPath; }
    QString lastError() const { return m_lastError; }

    /** DLL 候选路径（按优先级排序，已去重） */
    static QStringList libraryCandidates(const QString &preferredPath = QString());

    FnCliCreate create = nullptr;
    FnCliDestroy destroy = nullptr;
    FnCliConnect connect = nullptr;
    FnCliConnectTo connectTo = nullptr;
    FnCliDisconnect disconnect = nullptr;
    FnCliGetConnected getConnected = nullptr;
    FnCliReadArea readArea = nullptr;
    FnCliWriteArea writeArea = nullptr;
    FnCliGetLastError getLastError = nullptr;
    FnCliErrorText errorText = nullptr;

private:
    Snap7Api();
    ~Snap7Api();
    Snap7Api(const Snap7Api &) = delete;
    Snap7Api &operator=(const Snap7Api &) = delete;

    bool resolveSymbols(QLibrary *library, QString *error);

    QLibrary *m_library = nullptr;
    bool m_loaded = false;
    QString m_libraryPath;
    QString m_lastError;
};

/**
 * @brief 单个 S7 客户端连接（对应 Python 的 snap7.client.Client）
 *
 * 对应 Python 版用法：
 *   client.connect(ip, 0, 1) / client.get_connected() /
 *   client.write_area(Area.MK, 0, 100, bytearray([1])) / client.disconnect()
 *
 * 非线程安全，按 Python 版约定由流程执行线程单独使用。
 */
class Snap7Client
{
public:
    Snap7Client();
    ~Snap7Client();

    /** 已连接则返回 true；地址变化时自动重连 */
    bool connectTo(const QString &address, int rack, int slot, QString *error = nullptr);
    void disconnect();
    bool isConnected() const;

    /** 写入存储区（对应 write_area） */
    bool writeArea(int area, int dbNumber, int start, int wordLen, const QByteArray &data,
                   QString *error = nullptr);
    /** 写 1 个字节（Python 版行为：MK 区起始字节写 0/1） */
    bool writeByte(int area, int dbNumber, int start, quint8 value, QString *error = nullptr);
    /** 读回存储区，用于调试 / 对话框展示 */
    bool readArea(int area, int dbNumber, int start, int amount, int wordLen, QByteArray *out,
                  QString *error = nullptr);
    bool readByte(int area, int dbNumber, int start, quint8 *value, QString *error = nullptr);

    /** 错误码 -> snap7 官方错误文本 */
    static QString errorText(int code);
    QString lastError() const { return m_lastError; }
    /** 当前连接地址（未连接为空） */
    QString address() const { return m_address; }

private:
    bool ensureCreated(QString *error);
    /** 错误码 + 上下文 -> 可读文本，并写入 m_lastError */
    QString describe(int code, const QString &context) const;
    bool fail(const QString &message, QString *error);

    S7Object m_client = 0;
    bool m_created = false;
    QString m_address;
    QString m_lastError;
};

/** 区域名（MK/DB/PA/PE...）-> Area ID，未知则默认 MK */
int areaFromName(const QString &name);

} // namespace Snap7

#endif // SNAP7CLIENT_H
