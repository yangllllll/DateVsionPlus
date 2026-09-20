#include "websocketserverplugin.h"

#include "opencvcompat.h"

#include <QCryptographicHash>
#include <QDialog>
#include <QHBoxLayout>
#include <QHostAddress>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>
#include <QVBoxLayout>

#include <opencv2/imgcodecs.hpp>

#include <algorithm>
#include <vector>

namespace OVP {

static PluginBase *createWebSocketServerPlugin(QObject *parent)
{
    return new WebSocketServerPlugin(parent);
}

namespace {

/** WebSocket 握手用的 GUID（RFC 6455） */
const QByteArray kWebSocketMagic = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";

int broadcastInterval(int fps)
{
    return std::max(1, 1000 / std::max(1, fps));
}

} // namespace

// -------------------------------------------------- WebSocketServerPlugin

WebSocketServerPlugin::WebSocketServerPlugin(QObject *parent)
    : PluginBase(parent)
{
}

WebSocketServerPlugin::~WebSocketServerPlugin()
{
    stopServer();
}

QList<PortDef> WebSocketServerPlugin::inputPorts() const
{
    return {port(QStringLiteral("input"), PortType::Image, QStringLiteral("输入图像"))};
}

QList<PortDef> WebSocketServerPlugin::outputPorts() const
{
    return {port(QStringLiteral("output"), PortType::Image, QStringLiteral("原始图像(透传)")),
            port(QStringLiteral("client_count"), PortType::Number, QStringLiteral("当前客户端数"))};
}

QList<ParamDef> WebSocketServerPlugin::inputParams() const
{
    return {
        intParam(QStringLiteral("port"), QStringLiteral("监听端口"), 9000, 1024, 65535, 1,
                 QStringLiteral("WebSocket 服务端口")),
        intParam(QStringLiteral("quality"), QStringLiteral("JPEG质量"), 80, 10, 100, 5,
                 QStringLiteral("base64 编码图片的 JPEG 质量")),
        boolParam(QStringLiteral("auto_start"), QStringLiteral("自动启动"), true,
                  QStringLiteral("加载项目时自动启动服务")),
        intParam(QStringLiteral("fps"), QStringLiteral("发送帧率"), 30, 1, 60, 1,
                 QStringLiteral("每秒发送的图像帧数"))
    };
}

bool WebSocketServerPlugin::execute()
{
    clearError();

    // 保存当前图像供广播线程使用（对应 Python: self._current_frame = img.copy()）
    const cv::Mat image = inputImage(QStringLiteral("input"));
    m_currentFrame = image.empty() ? cv::Mat() : image.clone();

    // 透传图像 + 客户端数量
    setOutput(QStringLiteral("output"), imageValue(image));
    setOutput(QStringLiteral("client_count"), clientCount());

    if (paramBool(QStringLiteral("auto_start"), true))
        restartIfNeeded();

    return true;
}

QVariantMap WebSocketServerPlugin::extraData() const
{
    return {{QStringLiteral("running"), m_running}};
}

void WebSocketServerPlugin::setExtraData(const QVariantMap &data)
{
    if (data.value(QStringLiteral("running")).toBool() && paramBool(QStringLiteral("auto_start"), true)) {
        QString error;
        startServer(&error);
    }
}

// ------------------------------------------------------------------ 服务控制

bool WebSocketServerPlugin::startServer(QString *error)
{
    const int port = paramInt(QStringLiteral("port"), 9000);

    if (m_running && m_server && m_server->isListening() && m_listenPort == port)
        return true;

    stopServer(); // 端口变化或上次监听失败，重新绑定

    if (!m_server) {
        m_server = new QTcpServer(this);
        connect(m_server, &QTcpServer::newConnection, this, &WebSocketServerPlugin::onNewConnection);
    }

    if (!m_server->listen(QHostAddress::Any, static_cast<quint16>(port))) {
        const QString message =
            QStringLiteral("WebSocket 启动失败: %1（端口 %2）").arg(m_server->errorString()).arg(port);
        if (error)
            *error = message;
        setError(message);
        m_server->close();
        return false;
    }

    m_listenPort = port;
    m_running = true;

    if (!m_broadcastTimer) {
        m_broadcastTimer = new QTimer(this);
        m_broadcastTimer->setTimerType(Qt::CoarseTimer);
        connect(m_broadcastTimer, &QTimer::timeout, this, &WebSocketServerPlugin::onBroadcast);
    }
    m_broadcastTimer->start(broadcastInterval(paramInt(QStringLiteral("fps"), 30)));

    return true;
}

void WebSocketServerPlugin::stopServer()
{
    m_running = false;

    if (m_broadcastTimer)
        m_broadcastTimer->stop();

    if (m_server)
        m_server->close();

    for (QTcpSocket *socket : m_clients) {
        socket->disconnect(this);
        socket->close();
        socket->deleteLater();
    }
    m_clients.clear();

    for (QTcpSocket *socket : m_handshaking.keys()) {
        socket->disconnect(this);
        socket->close();
        socket->deleteLater();
    }
    m_handshaking.clear();
}

void WebSocketServerPlugin::restartIfNeeded()
{
    const int port = paramInt(QStringLiteral("port"), 9000);
    const int interval = broadcastInterval(paramInt(QStringLiteral("fps"), 30));

    if (m_running && m_listenPort == port) {
        if (m_broadcastTimer && m_broadcastTimer->interval() != interval)
            m_broadcastTimer->setInterval(interval);
        return;
    }

    if (!paramBool(QStringLiteral("auto_start"), true))
        return;

    QString error;
    if (!startServer(&error))
        setError(error);
}

// ------------------------------------------------------------------ 连接管理

void WebSocketServerPlugin::onNewConnection()
{
    while (QTcpSocket *socket = m_server->nextPendingConnection()) {
        connect(socket, &QTcpSocket::readyRead, this, &WebSocketServerPlugin::onSocketReadyRead);
        connect(socket, &QTcpSocket::disconnected, this, &WebSocketServerPlugin::onSocketDisconnected);
        connect(socket, &QAbstractSocket::errorOccurred, this, &WebSocketServerPlugin::onSocketError);
    }
}

void WebSocketServerPlugin::onSocketReadyRead()
{
    QTcpSocket *socket = qobject_cast<QTcpSocket *>(sender());
    if (!socket)
        return;

    if (!m_clients.contains(socket)) {
        handshake(socket);
        return;
    }

    // 已握手：忽略数据帧，收到关闭帧则断开
    const QByteArray data = socket->readAll();
    if (!data.isEmpty() && (static_cast<unsigned char>(data.at(0)) & 0x0F) == 0x8)
        socket->disconnectFromHost();
}

void WebSocketServerPlugin::onSocketDisconnected()
{
    QTcpSocket *socket = qobject_cast<QTcpSocket *>(sender());
    if (!socket)
        return;

    removeClient(socket);
    socket->deleteLater();
}

void WebSocketServerPlugin::onSocketError()
{
    QTcpSocket *socket = qobject_cast<QTcpSocket *>(sender());
    if (!socket)
        return;

    removeClient(socket);
    if (socket->state() != QAbstractSocket::ConnectedState)
        socket->deleteLater();
}

void WebSocketServerPlugin::removeClient(QTcpSocket *socket)
{
    m_clients.removeAll(socket);
    m_handshaking.remove(socket);
}

bool WebSocketServerPlugin::handshake(QTcpSocket *socket)
{
    QByteArray buffer = m_handshaking.value(socket);
    buffer.append(socket->readAll());

    const int headerEnd = buffer.indexOf("\r\n\r\n");
    if (headerEnd < 0) { // 请求头未收全，等下一次 readyRead
        m_handshaking.insert(socket, buffer);
        return false;
    }

    const QByteArray header = buffer.left(headerEnd);
    m_handshaking.remove(socket);

    QByteArray key;
    for (const QByteArray &line : header.split('\n')) {
        const QByteArray trimmed = line.trimmed();
        if (trimmed.toLower().startsWith("sec-websocket-key:")) {
            key = trimmed.mid(18).trimmed();
            break;
        }
    }

    if (key.isEmpty()) {
        socket->write("HTTP/1.1 400 Bad Request\r\n\r\n");
        socket->disconnectFromHost();
        return false;
    }

    const QByteArray accept =
        QCryptographicHash::hash(key + kWebSocketMagic, QCryptographicHash::Sha1).toBase64();

    const QByteArray response = "HTTP/1.1 101 Switching Protocols\r\n"
                                "Upgrade: websocket\r\n"
                                "Connection: Upgrade\r\n"
                                "Sec-WebSocket-Accept: " +
                                accept + "\r\n\r\n";
    socket->write(response);
    m_clients.append(socket);

    return true;
}

// -------------------------------------------------------------------- 广播

QByteArray WebSocketServerPlugin::encodeCurrentFrame() const
{
    if (m_currentFrame.empty())
        return QByteArray();

    cv::Mat image;
    if (m_currentFrame.channels() == 1)
        cv::cvtColor(m_currentFrame, image, cv::COLOR_GRAY2BGR);
    else if (m_currentFrame.channels() == 4)
        cv::cvtColor(m_currentFrame, image, cv::COLOR_BGRA2BGR);
    else
        image = m_currentFrame;

    const int quality = paramInt(QStringLiteral("quality"), 80);
    std::vector<uchar> buffer;
    const std::vector<int> params = {cv::IMWRITE_JPEG_QUALITY, quality};
    if (!cv::imencode(".jpg", image, buffer, params))
        return QByteArray();

    return QByteArray(reinterpret_cast<const char *>(buffer.data()), static_cast<int>(buffer.size()))
        .toBase64();
}

void WebSocketServerPlugin::sendTextFrame(QTcpSocket *socket, const QByteArray &payload) const
{
    QByteArray frame;
    frame.reserve(payload.size() + 10);
    frame.append(static_cast<char>(0x81)); // FIN + 文本帧

    const quint64 length = static_cast<quint64>(payload.size());
    if (length < 126) {
        frame.append(static_cast<char>(length));
    } else if (length <= 0xFFFFull) {
        frame.append(static_cast<char>(126));
        frame.append(static_cast<char>((length >> 8) & 0xFF));
        frame.append(static_cast<char>(length & 0xFF));
    } else {
        frame.append(static_cast<char>(127));
        for (int shift = 56; shift >= 0; shift -= 8)
            frame.append(static_cast<char>((length >> shift) & 0xFF));
    }

    frame.append(payload);
    socket->write(frame);
}

void WebSocketServerPlugin::onBroadcast()
{
    if (m_clients.isEmpty())
        return;

    const QByteArray payload = encodeCurrentFrame();
    if (payload.isEmpty())
        return;

    const QList<QTcpSocket *> sockets = m_clients; // 拷贝一份，发送过程中可能被移除
    for (QTcpSocket *socket : sockets) {
        if (socket->state() != QAbstractSocket::ConnectedState)
            continue;
        sendTextFrame(socket, payload);
    }
}

} // namespace OVP

// ------------------------------------------------------------ 状态对话框

namespace {

/**
 * @brief 简单的服务状态对话框：显示状态 / 端口 / 客户端数，可手动启停
 */
class WebSocketStatusDialog : public QDialog
{
public:
    explicit WebSocketStatusDialog(OVP::WebSocketServerPlugin *plugin, QWidget *parent = nullptr)
        : QDialog(parent)
        , m_plugin(plugin)
    {
        setWindowTitle(QStringLiteral("图像流推送"));
        resize(320, 140);

        auto *layout = new QVBoxLayout(this);
        m_statusLabel = new QLabel(this);
        m_statusLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        layout->addWidget(m_statusLabel);

        auto *buttonLayout = new QHBoxLayout();
        auto *startButton = new QPushButton(QStringLiteral("启动服务"), this);
        auto *stopButton = new QPushButton(QStringLiteral("停止服务"), this);
        buttonLayout->addWidget(startButton);
        buttonLayout->addWidget(stopButton);
        layout->addLayout(buttonLayout);

        auto *closeButton = new QPushButton(QStringLiteral("关闭"), this);
        layout->addWidget(closeButton);

        connect(startButton, &QPushButton::clicked, this, [this]() {
            QString error;
            if (!m_plugin->startServer(&error))
                QMessageBox::warning(this, QStringLiteral("图像流推送"), error);
            refresh();
        });
        connect(stopButton, &QPushButton::clicked, this, [this]() {
            m_plugin->stopServer();
            refresh();
        });
        connect(closeButton, &QPushButton::clicked, this, &QDialog::accept);

        auto *timer = new QTimer(this);
        connect(timer, &QTimer::timeout, this, [this]() { refresh(); });
        timer->start(500);

        refresh();
    }

private:
    void refresh()
    {
        m_statusLabel->setText(QStringLiteral("状态: %1\n监听端口: %2\n客户端数: %3")
                                   .arg(m_plugin->isRunning() ? QStringLiteral("运行中")
                                                              : QStringLiteral("已停止"))
                                   .arg(m_plugin->listenPort())
                                   .arg(m_plugin->clientCount()));
    }

    OVP::WebSocketServerPlugin *m_plugin = nullptr;
    QLabel *m_statusLabel = nullptr;
};

} // namespace

namespace OVP {

QDialog *WebSocketServerPlugin::createDialog(const cv::Mat &inputImage, QWidget *parent)
{
    Q_UNUSED(inputImage);
    return new WebSocketStatusDialog(this, parent);
}

} // namespace OVP

// ------------------------------------------------------------------ DLL 导出

QList<OpenVisionPluginProvider::Entry> WebSocketServerPluginProvider::availablePlugins() const
{
    return {OpenVisionPluginProvider::Entry{OVP::createWebSocketServerPlugin}};
}
