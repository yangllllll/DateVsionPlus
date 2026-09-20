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

public slots:
    /** 主窗口执行完成后调用，把结果回写给触发的客户端 */
    void setResponse(const QString &data);

signals:
    void logMessage(const QString &message, const QString &color);
    void triggerReceived();
    void clientCountChanged(int count);
    void listeningChanged(bool listening);

private slots:
    void onNewConnection();
    void onSocketReadyRead();
    void onSocketDisconnected();
    void onResponseTimeout();

private:
    QTcpSocket *senderSocket() const { return m_pendingSocket; }

    QTcpServer *m_server = nullptr;
    QList<QTcpSocket *> m_clients;
    QTcpSocket *m_pendingSocket = nullptr;
    QTimer *m_timeoutTimer = nullptr;

    int m_port = 8080;
    QString m_controlWord = QStringLiteral("TRIGGER");
    QString m_response;
    bool m_waitingResponse = false;
    QByteArray m_buffer;
};

#endif // TCPSERVER_H
