#ifndef WEBSOCKETSERVERPLUGIN_H
#define WEBSOCKETSERVERPLUGIN_H

// ---- 主程序 SDK ----
#include "pluginbase.h"
#include "plugininterface.h"

#include <QHash>
#include <QList>
#include <QObject>
#include <QString>
#include <QVariantMap>

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
    bool isRunning() const { return m_running; }
    int clientCount() const { return m_clients.size(); }
    int listenPort() const { return m_listenPort; }
    /** 按当前参数启动服务；已在监听且端口未变则直接返回 true */
    bool startServer(QString *error = nullptr);
    void stopServer();

private slots:
    void onNewConnection();
    void onSocketReadyRead();
    void onSocketDisconnected();
    void onSocketError();
    void onBroadcast();

private:
    /** 完成 WebSocket 握手；数据未收全时缓存等待 */
    bool handshake(QTcpSocket *socket);
    /** 发送一个 WebSocket 文本帧（服务端不掩码） */
    void sendTextFrame(QTcpSocket *socket, const QByteArray &payload) const;
    void removeClient(QTcpSocket *socket);
    /** 当前帧 -> JPEG -> base64 */
    QByteArray encodeCurrentFrame() const;
    /** 端口/帧率变化或尚未启动时（重新）启动服务 */
    void restartIfNeeded();

    QTcpServer *m_server = nullptr;
    QTimer *m_broadcastTimer = nullptr;
    QList<QTcpSocket *> m_clients;                  ///< 已完成握手的客户端
    QHash<QTcpSocket *, QByteArray> m_handshaking;  ///< 握手中累积的 HTTP 请求
    cv::Mat m_currentFrame;                         ///< 最近一次输入图像（深拷贝）
    bool m_running = false;
    int m_listenPort = 0;
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
