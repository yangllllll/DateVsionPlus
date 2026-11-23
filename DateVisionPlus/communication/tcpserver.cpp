#include "tcpserver.h"

#include <QHostAddress>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>

#include <utility>

TcpServer::TcpServer(QObject *parent)
    : QObject(parent)
{
    m_server = new QTcpServer(this);
    connect(m_server, &QTcpServer::newConnection, this, &TcpServer::onNewConnection);

    m_timeoutTimer = new QTimer(this);
    m_timeoutTimer->setSingleShot(true);
    m_timeoutTimer->setInterval(10000);
    connect(m_timeoutTimer, &QTimer::timeout, this, &TcpServer::onResponseTimeout);

    // 结果发送延迟（见 setSendDelay）
    m_sendTimer = new QTimer(this);
    m_sendTimer->setSingleShot(true);
    connect(m_sendTimer, &QTimer::timeout, this, &TcpServer::sendPendingResponse);
}

TcpServer::~TcpServer()
{
    stopServer();
}

void TcpServer::configure(int port, const QString &controlWord)
{
    m_port = port;
    m_controlWord = controlWord;
}

bool TcpServer::startServer()
{
    if (m_server->isListening())
        return true;

    if (!m_server->listen(QHostAddress::Any, static_cast<quint16>(m_port))) {
        emit logMessage(QStringLiteral("TCP服务启动失败: %1").arg(m_server->errorString()),
                        QStringLiteral("#f44336"));
        return false;
    }

    emit logMessage(QStringLiteral("TCP服务已启动，监听端口 %1").arg(m_port),
                    QStringLiteral("#4caf50"));
    emit listeningChanged(true);
    return true;
}

void TcpServer::stopServer()
{
    m_timeoutTimer->stop();
    m_sendTimer->stop();
    m_waitingResponse = false;
    m_pendingSocket = nullptr;
    m_pendingData.clear();

    for (QTcpSocket *socket : m_clients) {
        socket->disconnectFromHost();
        socket->deleteLater();
    }
    m_clients.clear();

    if (m_server->isListening()) {
        m_server->close();
        emit logMessage(QStringLiteral("TCP服务已停止"), QStringLiteral("#ff9800"));
        emit listeningChanged(false);
    }
    emit clientCountChanged(0);
}

bool TcpServer::isListening() const
{
    return m_server->isListening();
}

bool TcpServer::hasClient() const
{
    return lastConnectedSocket() != nullptr;
}

void TcpServer::discardPendingResponse()
{
    m_timeoutTimer->stop();
    m_sendTimer->stop();
    m_waitingResponse = false;
    m_pendingSocket = nullptr;
    m_pendingData.clear();
}

QTcpSocket *TcpServer::lastConnectedSocket() const
{
    for (int i = m_clients.size() - 1; i >= 0; --i) {
        if (m_clients.at(i)->state() == QAbstractSocket::ConnectedState)
            return m_clients.at(i);
    }
    return nullptr;
}

void TcpServer::setResponse(const QString &data)
{
    m_timeoutTimer->stop();
    m_waitingResponse = false;

    m_pendingData = data;
    if (m_sendDelayMs > 0) {
        // 延迟发送：给客户端留出切换到接收状态的时间
        m_sendTimer->start(m_sendDelayMs);
        return;
    }
    sendPendingResponse();
}

void TcpServer::sendPendingResponse()
{
    m_sendTimer->stop();

    // 优先回给触发本次检测的客户端；若是界面/连续执行触发的，则回给当前连接的客户端
    QTcpSocket *socket = m_pendingSocket;
    m_pendingSocket = nullptr;
    const QString data = std::exchange(m_pendingData, QString());
    if (!socket || socket->state() != QAbstractSocket::ConnectedState)
        socket = lastConnectedSocket();
    if (!socket)
        return;

    socket->write((data + QStringLiteral("\n")).toUtf8());
    socket->flush();
    emit logMessage(QStringLiteral("响应: %1%2")
                        .arg(data.left(100), data.length() > 100 ? QStringLiteral("...") : QString()),
                    QStringLiteral("#4caf50"));
}

void TcpServer::onNewConnection()
{
    while (m_server->hasPendingConnections()) {
        QTcpSocket *socket = m_server->nextPendingConnection();
        if (!socket)
            break;

        m_clients.append(socket);
        emit clientCountChanged(m_clients.size());
        emit logMessage(
            QStringLiteral("客户端连接: %1:%2").arg(socket->peerAddress().toString()).arg(socket->peerPort()),
            QStringLiteral("#2196f3"));

        connect(socket, &QTcpSocket::readyRead, this, &TcpServer::onSocketReadyRead);
        connect(socket, &QTcpSocket::disconnected, this, &TcpServer::onSocketDisconnected);
    }
}

void TcpServer::onSocketReadyRead()
{
    auto *socket = qobject_cast<QTcpSocket *>(sender());
    if (!socket)
        return;

    m_buffer.append(socket->readAll());

    const QByteArray controlBytes = m_controlWord.toUtf8();
    if (controlBytes.isEmpty())
        return;

    const int index = m_buffer.indexOf(controlBytes);
    if (index < 0)
        return;

    m_buffer.remove(0, index + controlBytes.length());

    // 同一批数据里出现多个控制字时只按「一次触发」处理：
    // 把剩余的重复控制字一并丢弃，否则它们会滞留在缓冲区，
    // 等下一批数据到达时被误当成新的触发。
    m_buffer.replace(controlBytes, QByteArray());

    handleTrigger(socket);
}

void TcpServer::handleTrigger(QTcpSocket *socket)
{
    emit logMessage(QStringLiteral("收到控制字 '%1'，触发检测").arg(m_controlWord),
                    QStringLiteral("#ce93d8"));

    // 上一轮结果可能还在延迟队列里：先发出去，避免它误用到本次触发的 socket
    if (m_sendTimer->isActive())
        sendPendingResponse();

    m_response.clear();
    m_waitingResponse = true;
    m_pendingSocket = socket;
    m_timeoutTimer->start();

    emit triggerReceived();
}

void TcpServer::onSocketDisconnected()
{
    auto *socket = qobject_cast<QTcpSocket *>(sender());
    if (!socket)
        return;

    m_clients.removeAll(socket);
    emit clientCountChanged(m_clients.size());
    emit logMessage(QStringLiteral("客户端断开连接"), QStringLiteral("#ff9800"));

    if (m_pendingSocket == socket) {
        m_pendingSocket = nullptr;
        m_waitingResponse = false;
        m_timeoutTimer->stop();
    }
    socket->deleteLater();
}

void TcpServer::onResponseTimeout()
{
    m_waitingResponse = false;
    QTcpSocket *socket = m_pendingSocket;
    m_pendingSocket = nullptr;
    if (!socket)
        return;

    socket->write("ERROR:TIMEOUT\n");
    socket->flush();
    emit logMessage(QStringLiteral("等待检测结果超时"), QStringLiteral("#f44336"));

    // 通知主窗口作废这条请求：本轮即使随后跑完，也不再补发第二个包
    emit responseTimedOut();
}
