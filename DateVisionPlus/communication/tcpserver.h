#ifndef TCPSERVER_H
#define TCPSERVER_H

#include <QByteArray>
#include <QList>
#include <QObject>
#include <QString>

class QTcpServer;
class QTcpSocket;
class QTimer;

/** TCP 服务端：接收控制字触发检测，并回传结果字符串 */
class TcpServer : public QObject
{
    Q_OBJECT

public:
    explicit TcpServer(QObject *parent = nullptr);
    ~TcpServer() override;

    void configure(int port, const QString &controlWord);
    bool startServer();
    void stopServer();
    bool isListening() const;

    /** 是否存在已连接的客户端（只有存在时才需要回发结果） */
    bool hasClient() const;

    /**
     * 丢弃当前待回发的触发请求：停止超时计时与延迟发送，且不再回包。
     * 用于「执行过程中收到触发，直接忽略」的场景。
     */
    void discardPendingResponse();

    /**
     * 结果发送延迟（毫秒）。
     * 部分客户端（PLC 等）发出触发后需要时间切换到接收状态，
     * 结果返回太快会被丢弃；此时把延迟调大即可。
     */
    void setSendDelay(int ms) { m_sendDelayMs = qMax(0, ms); }
    int sendDelay() const { return m_sendDelayMs; }

public slots:
    /** 主窗口执行完成后调用，把结果回写给触发的客户端 */
    void setResponse(const QString &data);

signals:
    void logMessage(const QString &message, const QString &color);
    void triggerReceived();
    void clientCountChanged(int count);
    void listeningChanged(bool listening);
    /** 等待结果超时（已回 ERROR:TIMEOUT），该请求随之作废 */
    void responseTimedOut();

private slots:
    void onNewConnection();
    void onSocketReadyRead();
    void onSocketDisconnected();
    void onResponseTimeout();
    void sendPendingResponse();

private:
    QTcpSocket *senderSocket() const { return m_pendingSocket; }
    /** 最近一个仍处于连接状态的客户端（无触发来源时作为回发目标） */
    QTcpSocket *lastConnectedSocket() const;
    /** 处理一次控制字触发（一个数据包里可能连续出现多次） */
    void handleTrigger(QTcpSocket *socket);

    QTcpServer *m_server = nullptr;
    QList<QTcpSocket *> m_clients;
    QTcpSocket *m_pendingSocket = nullptr;
    QTimer *m_timeoutTimer = nullptr;
    QTimer *m_sendTimer = nullptr;
    QString m_pendingData;
    int m_sendDelayMs = 0;

    int m_port = 8080;
    QString m_controlWord = QStringLiteral("TRIGGER");
    QString m_response;
    bool m_waitingResponse = false;
    QByteArray m_buffer;
};

#endif // TCPSERVER_H
