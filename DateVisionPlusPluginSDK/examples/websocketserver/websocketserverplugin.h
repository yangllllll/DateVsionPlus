#ifndef WEBSOCKETSERVERPLUGIN_H
#define WEBSOCKETSERVERPLUGIN_H

// ---- 主程序 SDK ----
#include "pluginbase.h"
#include "plugininterface.h"

#include <QHash>
#include <QList>
#include <QMutex>
#include <QObject>
#include <QString>
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
 * @brief WebSocket 图像流推送插件（对应 Python 版 websocket_server.py）
 *
 * 内置一个轻量 WebSocket 服务器（仅用 Qt Network 手工完成握手与帧封装，
 * 不依赖 QtWebSockets 模块）：
 *   - 输入图像由 execute() 保存下来；
 *   - 后台定时器按「发送帧率」把图像编码成 JPEG + base64，广播给所有已连接客户端；
 *   - 图像原样透传到 output 端口，client_count 输出当前客户端数。
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
        return QStringLiteral("启动 WebSocket 服务器，将图像流以 base64 发送到所有订阅客户端");
    }

    QList<PortDef> inputPorts() const override;
    QList<PortDef> outputPorts() const override;
    QList<ParamDef> inputParams() const override;

    bool execute() override;

    QVariantMap extraData() const override;
    void setExtraData(const QVariantMap &data) override;

    QDialog *createDialog(const cv::Mat &inputImage, QWidget *parent) override;

    // ---- 供状态对话框调用 ----
    bool isRunning() const { return m_running.load(); }
    int clientCount() const { return m_clientCount.load(); }
    int listenPort() const { return m_listenPort.load(); }
    /** 在插件所属线程按当前参数启动服务；已在监听且端口未变则直接返回 true */
    bool startServer(QString *error = nullptr);
    void stopServer();

private slots:
    void onNewConnection();
    void onSocketReadyRead();
    void onSocketDisconnected();
    void onSocketError();
    void onBroadcast();

private:
    friend class WebSocketServerPluginTest;

    /** 完成 WebSocket 握手；数据未收全时缓存等待 */
    bool handshake(QTcpSocket *socket);
    /** 封装一次后复用到全部客户端（服务端不掩码） */
    static QByteArray textFrame(const QByteArray &payload);
    void removeClient(QTcpSocket *socket);
    /** 当前帧 -> JPEG -> base64 -> WebSocket 帧，输入/质量未变时复用缓存 */
    const QByteArray &encodedCurrentFrame();
    /** execute 可由流程工作线程调用；只投递一个待处理的配置更新 */
    void requestRestart(int port, int fps);
    bool startServerWithSettings(int port, int fps, QString *error);
    void closeServer();

    QTcpServer *m_server = nullptr;
    QTimer *m_broadcastTimer = nullptr;
    QList<QTcpSocket *> m_clients;                  ///< 已完成握手的客户端
    QHash<QTcpSocket *, QByteArray> m_handshaking;  ///< 握手中累积的 HTTP 请求
    // 下面的邮箱由 execute 工作线程和网络所属线程共享。
    QMutex m_frameMutex;
    cv::Mat m_currentFrame;                         ///< 最新不可变快照，编码时仅短暂加锁取引用
    quint64 m_frameRevision = 0;
    quint64 m_controlGeneration = 0;
    bool m_restartQueued = false;
    int m_requestedPort = 9000;
    int m_requestedFps = 30;
    QString m_serverError;
    QString m_encodingError;

    // 编码和所有 socket/timer 操作仅在 QObject 所属线程执行。
    quint64 m_encodedRevision = 0;
    int m_encodedQuality = -1;
    QByteArray m_encodedFrame;
    std::vector<uchar> m_jpegBuffer;
    std::atomic_bool m_running{false};
    std::atomic_int m_clientCount{0};
    std::atomic_int m_listenPort{0};
    std::atomic_int m_broadcastInterval{0};
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
