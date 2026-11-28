#ifndef WEBSOCKETSERVERPLUGIN_H
#define WEBSOCKETSERVERPLUGIN_H

// ---- 主程序 SDK ----
#include "pluginbase.h"
#include "plugininterface.h"

#include <QByteArray>
#include <QHash>
#include <QList>
#include <QMutex>
#include <QObject>
#include <QString>
#include <QThread>
#include <QVariantMap>

#include <atomic>
#include <vector>

class QDialog;
class QWidget;
class QTcpServer;
class QTcpSocket;
class QTimer;

namespace OVP {

/**
 * @brief WebSocket 后台发送器（运行在独立线程）
 *
 * 只做三件事：
 *   1) setFrame() 由流程线程调用，更新「缓冲变量」；
 *   2) 自己的线程里用定时器按发送帧率把缓冲区里的最新图像编码成
 *      JPEG + base64 并广播给所有客户端；
 *   3) 管理 QTcpServer / 客户端 socket（都归本对象所在线程所有）。
 *
 * 因此插件的 execute() 只更新缓冲变量，发包与编码完全不占用流程/UI 线程。
 */
class WebSocketWorker : public QObject
{
    Q_OBJECT

public:
    explicit WebSocketWorker(QObject *parent = nullptr);
    ~WebSocketWorker() override;

    /** 任意线程调用：更新缓冲变量（深拷贝，避免上游复用像素） */
    void setFrame(const cv::Mat &image);

    bool isRunning() const { return m_running.load(); }
    int clientCount() const { return m_clientCount.load(); }
    int listenPort() const { return m_listenPort.load(); }
    int intervalMs() const { return m_interval.load(); }
    quint64 frameRevision() const;
    QString lastError() const;

    // ---- 以下只在 worker 线程调用（通过队列/阻塞队列投递） ----
    bool startServer(int port, int fps, int quality, QString *error);
    void stopServer();
    void configure(int port, int fps, int quality);
    void broadcastOnce();

private slots:
    void onNewConnection();
    void onSocketReadyRead();
    void onSocketDisconnected();
    void onSocketError();
    void onBroadcast();

private:
    /** 完成 WebSocket 握手；数据未收全时缓存等待 */
    bool handshake(QTcpSocket *socket);
    /** 封装一次后复用到全部客户端（服务端不掩码） */
    static QByteArray textFrame(const QByteArray &payload);
    void removeClient(QTcpSocket *socket);
    /** 缓冲区 -> JPEG -> base64 -> WebSocket 帧；序号/质量未变时复用缓存 */
    const QByteArray &encodedCurrentFrame();
    void closeServer();
    void setError(const QString &message);

    // ---- 缓冲变量（流程线程写，worker 线程读） ----
    mutable QMutex m_frameMutex;
    cv::Mat m_frame;
    quint64 m_revision = 0;

    mutable QMutex m_errorMutex;
    QString m_error;

    // ---- 以下成员只在 worker 线程访问 ----
    QTcpServer *m_server = nullptr;
    QTimer *m_timer = nullptr;
    QList<QTcpSocket *> m_clients;                  ///< 已完成握手的客户端
    QHash<QTcpSocket *, QByteArray> m_handshaking;  ///< 握手中累积的 HTTP 请求
    quint64 m_encodedRevision = 0;
    int m_encodedQuality = -1;
    QByteArray m_encodedFrame;
    std::vector<uchar> m_jpegBuffer;
    int m_quality = 80;

    std::atomic_bool m_running{false};
    std::atomic_int m_clientCount{0};
    std::atomic_int m_listenPort{0};
    std::atomic_int m_interval{0};
};

/**
 * @brief WebSocket 图像流推送插件（对应 Python 版 websocket_server.py）
 *
 * 内置一个轻量 WebSocket 服务器（仅用 Qt Network 手工完成握手与帧封装，
 * 不依赖 QtWebSockets 模块）：
 *   - 启动后创建一个独立线程（WebSocketWorker），在后台按「发送帧率」持续发送；
 *   - 插件的 execute() 只更新缓冲变量中的图像并透传到 output；
 *   - 流程暂停/不执行时，后台线程仍会持续发送缓冲区里的最后一帧。
 */
class WebSocketServerPlugin : public PluginBase
{
    Q_OBJECT

public:
    explicit WebSocketServerPlugin(QObject *parent = nullptr);
    ~WebSocketServerPlugin() override;

    QString id() const override { return QStringLiteral("websocket_server"); }
    QString name() const override { return QStringLiteral("图像流推送"); }
    QString category() const override { return QStringLiteral("输入输出"); }
    QString description() const override
    {
        return QStringLiteral("后台线程持续发送缓冲区图像，WebSocket base64 图像流");
    }

    QList<PortDef> inputPorts() const override;
    QList<PortDef> outputPorts() const override;
    QList<ParamDef> inputParams() const override;

    bool execute() override;

    QVariantMap extraData() const override;
    void setExtraData(const QVariantMap &data) override;

    QDialog *createDialog(const cv::Mat &inputImage, QWidget *parent) override;

    // ---- 供状态对话框 / 测试调用 ----
    bool isRunning() const;
    int clientCount() const;
    int listenPort() const;
    /** 后台线程当前的发送间隔（毫秒） */
    int broadcastInterval() const;
    /** 按当前参数启动后台发送线程；已在监听且端口未变则直接返回 true */
    bool startServer(QString *error = nullptr);
    void stopServer();

    // ---- 离线测试辅助 ----
    /** 当前缓冲变量的版本号（每次 execute 更新缓冲区都会 +1） */
    quint64 frameRevision() const;
    /** 请求后台线程立刻广播一次（阻塞到完成） */
    bool broadcastNow();

private:
    friend class WebSocketServerPluginTest;

    void ensureThread();
    /** execute 可在流程线程调用；只投递一个配置更新给后台线程 */
    void requestConfigure(int port, int fps, int quality);

    QThread m_thread;
    WebSocketWorker *m_worker = nullptr;
};

} // namespace OVP

/**
 * @brief 插件提供者：一个 DLL 可导出多个工具
 */
class WebSocketServerPluginProvider : public QObject, public OpenVisionPluginProvider
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID OpenVisionPluginProvider_iid)
    Q_INTERFACES(OpenVisionPluginProvider)

public:
    QList<Entry> availablePlugins() const override;
};

#endif // WEBSOCKETSERVERPLUGIN_H
