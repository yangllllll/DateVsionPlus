#include "websocketserverplugin.h"

#include "opencvcompat.h"

#include <QCryptographicHash>
#include <QDialog>
#include <QHBoxLayout>
#include <QHostAddress>
#include <QLabel>
#include <QMessageBox>
#include <QMutexLocker>
#include <QPointer>
#include <QPushButton>
#include <QTcpServer>
#include <QTcpSocket>
#include <QThread>
#include <QTimer>
#include <QVBoxLayout>

#include <opencv2/imgcodecs.hpp>

#include <algorithm>
#include <utility>
#include <vector>

namespace OVP {

static PluginBase *createWebSocketServerPlugin(QObject *parent)
{
    return new WebSocketServerPlugin(parent);
}

namespace {

/** WebSocket 握手用的 GUID（RFC 6455） */
const QByteArray kWebSocketMagic = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
constexpr qsizetype kMaxHandshakeBytes = 16 * 1024;
constexpr int kHandshakeTimeoutMs = 5000;

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

    // 流程可在工作线程执行，网络对象仍属于 UI 线程。先复制再换入邮箱，
    // 不在大图拷贝/编码期间持锁，也不让上游后续原地修改影响已提交图像。
    const cv::Mat image = inputImage(QStringLiteral("input"));
    cv::Mat snapshot = image.clone();
    {
        QMutexLocker locker(&m_frameMutex);
        std::swap(m_currentFrame, snapshot);
        ++m_frameRevision;
    }

    // 透传图像 + 客户端数量
    setOutput(QStringLiteral("output"), imageValue(image));
    setOutput(QStringLiteral("client_count"), clientCount());

    if (paramBool(QStringLiteral("auto_start"), true)) {
        requestRestart(paramInt(QStringLiteral("port"), 9000),
                       paramInt(QStringLiteral("fps"), 30));
    }

    {
        QMutexLocker locker(&m_frameMutex);
        // 网络回调只写自己的错误邮箱，不与 execute 竞争 PluginBase::lastError。
        if (!m_serverError.isEmpty())
            setError(m_serverError);
        else if (!m_encodingError.isEmpty())
            setError(m_encodingError);
    }

    return true;
}

QVariantMap WebSocketServerPlugin::extraData() const
{
    return {{QStringLiteral("running"), isRunning()}};
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
    if (QThread::currentThread() != thread()) {
        if (error)
            *error = QStringLiteral("请在插件所属线程启动 WebSocket 服务");
        return false;
    }
    {
        QMutexLocker locker(&m_frameMutex);
        // 手动启动采用当前参数，先前排队的自动启动不能覆盖本次选择。
        ++m_controlGeneration;
        m_restartQueued = false;
    }
    return startServerWithSettings(paramInt(QStringLiteral("port"), 9000),
                                   paramInt(QStringLiteral("fps"), 30), error);
}

bool WebSocketServerPlugin::startServerWithSettings(int port, int fps, QString *error)
{
    const int interval = broadcastInterval(fps);
    if (error)
        error->clear();

    if (m_running && m_server && m_server->isListening() && m_listenPort == port) {
        if (m_broadcastTimer->interval() != interval)
            m_broadcastTimer->setInterval(interval);
        m_broadcastInterval = interval;
        return true;
    }

    closeServer(); // 保留后续已排队的配置更新，只重新绑定网络对象

    if (!m_server) {
        m_server = new QTcpServer(this);
        connect(m_server, &QTcpServer::newConnection, this, &WebSocketServerPlugin::onNewConnection);
    }

    if (port < 1 || port > 65535 ||
        !m_server->listen(QHostAddress::Any, static_cast<quint16>(port))) {
        const QString message =
            QStringLiteral("WebSocket 启动失败: %1（端口 %2）")
                .arg(port < 1 || port > 65535 ? QStringLiteral("端口超出范围")
                                           : m_server->errorString())
                .arg(port);
        if (error)
            *error = message;
        {
            QMutexLocker locker(&m_frameMutex);
            m_serverError = message;
        }
        m_server->close();
        return false;
    }

    {
        QMutexLocker locker(&m_frameMutex);
        m_serverError.clear();
    }

    m_listenPort = port;
    m_running = true;

    if (!m_broadcastTimer) {
        m_broadcastTimer = new QTimer(this);
        m_broadcastTimer->setTimerType(Qt::CoarseTimer);
        connect(m_broadcastTimer, &QTimer::timeout, this, &WebSocketServerPlugin::onBroadcast);
    }
    m_broadcastTimer->start(interval);
    m_broadcastInterval = interval;

    return true;
}

void WebSocketServerPlugin::stopServer()
{
    if (QThread::currentThread() != thread()) {
        QMetaObject::invokeMethod(this, &WebSocketServerPlugin::stopServer, Qt::QueuedConnection);
        return;
    }

    {
        QMutexLocker locker(&m_frameMutex);
        // 手动停止之后，先前 execute 排队的自动启动不应让服务重新启动。
        ++m_controlGeneration;
        m_restartQueued = false;
        m_serverError.clear();
    }
    closeServer();
}

void WebSocketServerPlugin::closeServer()
{
    m_running = false;
    m_clientCount = 0;

    if (m_broadcastTimer)
        m_broadcastTimer->stop();

    if (m_server)
        m_server->close();

    for (QTcpSocket *socket : m_clients) {
        socket->disconnect(this);
        socket->abort();
        socket->deleteLater();
    }
    m_clients.clear();

    for (QTcpSocket *socket : m_handshaking.keys()) {
        socket->disconnect(this);
        socket->abort();
        socket->deleteLater();
    }
    m_handshaking.clear();
}

void WebSocketServerPlugin::requestRestart(int port, int fps)
{
    quint64 generation;
    {
        QMutexLocker locker(&m_frameMutex);
        m_requestedPort = port;
        m_requestedFps = fps;
        if (m_restartQueued)
            return;
        if (m_running && m_listenPort == port && m_broadcastInterval == broadcastInterval(fps))
            return;
        generation = m_controlGeneration;
        m_restartQueued = true;
    }

    auto apply = [this, generation]() {
        int port;
        int fps;
        {
            QMutexLocker locker(&m_frameMutex);
            if (generation != m_controlGeneration)
                return;
            m_restartQueued = false;
            port = m_requestedPort;
            fps = m_requestedFps;
        }
        startServerWithSettings(port, fps, nullptr);
    };

    if (QThread::currentThread() == thread())
        apply();
    else
        QMetaObject::invokeMethod(this, std::move(apply), Qt::QueuedConnection);
}

// ------------------------------------------------------------------ 连接管理

void WebSocketServerPlugin::onNewConnection()
{
    while (QTcpSocket *socket = m_server->nextPendingConnection()) {
        // 立即登记无数据连接，确保 stopServer 也能回收它们。
        m_handshaking.insert(socket, QByteArray());
        socket->setReadBufferSize(kMaxHandshakeBytes);
        connect(socket, &QTcpSocket::readyRead, this, &WebSocketServerPlugin::onSocketReadyRead);
        connect(socket, &QTcpSocket::disconnected, this, &WebSocketServerPlugin::onSocketDisconnected);
        connect(socket, &QAbstractSocket::errorOccurred, this, &WebSocketServerPlugin::onSocketError);
        QTimer::singleShot(kHandshakeTimeoutMs, socket, [this, socket]() {
            if (m_handshaking.contains(socket))
                socket->abort();
        });
        if (socket->bytesAvailable() > 0)
            handshake(socket);
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
    socket->abort();
    socket->deleteLater();
}

void WebSocketServerPlugin::removeClient(QTcpSocket *socket)
{
    m_clients.removeAll(socket);
    m_handshaking.remove(socket);
    m_clientCount = static_cast<int>(m_clients.size());
}

bool WebSocketServerPlugin::handshake(QTcpSocket *socket)
{
    QByteArray &buffer = m_handshaking[socket];
    buffer.append(socket->readAll());

    const qsizetype headerEnd = buffer.indexOf("\r\n\r\n");
    if ((headerEnd < 0 && buffer.size() >= kMaxHandshakeBytes) ||
        headerEnd > kMaxHandshakeBytes - 4) {
        socket->abort();
        return false;
    }
    if (headerEnd < 0) { // 请求头未收全，等下一次 readyRead
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
    m_clientCount = static_cast<int>(m_clients.size());

    return true;
}

// -------------------------------------------------------------------- 广播

const QByteArray &WebSocketServerPlugin::encodedCurrentFrame()
{
    const int quality = std::clamp(paramInt(QStringLiteral("quality"), 80), 10, 100);
    cv::Mat image;
    quint64 revision;
    {
        QMutexLocker locker(&m_frameMutex);
        revision = m_frameRevision;
        if (revision == m_encodedRevision && quality == m_encodedQuality)
            return m_encodedFrame;
        image = m_currentFrame;
    }

    m_encodedRevision = revision;
    m_encodedQuality = quality;
    m_encodedFrame.clear();
    QString error;
    if (!image.empty()) {
        try {
            // JPEG 原生支持灰度，无须先复制扩展到三通道。
            if (image.channels() == 4)
                cv::cvtColor(image, image, cv::COLOR_BGRA2BGR);
            const std::vector<int> params = {cv::IMWRITE_JPEG_QUALITY, quality};
            if (cv::imencode(".jpg", image, m_jpegBuffer, params)) {
                const QByteArray payload = QByteArray::fromRawData(
                    reinterpret_cast<const char *>(m_jpegBuffer.data()),
                    static_cast<qsizetype>(m_jpegBuffer.size())).toBase64();
                m_encodedFrame = textFrame(payload);
            } else {
                error = QStringLiteral("WebSocket JPEG 编码失败");
            }
        } catch (const cv::Exception &exception) {
            error = QStringLiteral("WebSocket JPEG 编码失败: %1")
                        .arg(QString::fromUtf8(exception.what()));
        }
    }
    {
        QMutexLocker locker(&m_frameMutex);
        m_encodingError = error;
    }
    return m_encodedFrame;
}

QByteArray WebSocketServerPlugin::textFrame(const QByteArray &payload)
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
    return frame;
}

void WebSocketServerPlugin::onBroadcast()
{
    if (m_clients.isEmpty())
        return;

    // 实时流只发送最新帧，慢客户端最多积压一个应用帧。
    // 全部客户端拥堵时跳过编码，防止无收益的 JPEG/base64 工作。
    QList<QTcpSocket *> ready;
    for (QTcpSocket *socket : std::as_const(m_clients)) {
        if (socket->state() == QAbstractSocket::ConnectedState && socket->bytesToWrite() == 0)
            ready.append(socket);
    }
    if (ready.isEmpty())
        return;

    const QByteArray &frame = encodedCurrentFrame();
    if (frame.isEmpty())
        return;

    for (QTcpSocket *socket : std::as_const(ready))
        socket->write(frame);
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
            if (!m_plugin)
                return;
            QString error;
            if (!m_plugin->startServer(&error))
                QMessageBox::warning(this, QStringLiteral("图像流推送"), error);
            refresh();
        });
        connect(stopButton, &QPushButton::clicked, this, [this]() {
            if (!m_plugin)
                return;
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
        if (!m_plugin) {
            m_statusLabel->setText(QStringLiteral("插件已移除"));
            return;
        }
        m_statusLabel->setText(QStringLiteral("状态: %1\n监听端口: %2\n客户端数: %3")
                                   .arg(m_plugin->isRunning() ? QStringLiteral("运行中")
                                                              : QStringLiteral("已停止"))
                                   .arg(m_plugin->listenPort())
                                   .arg(m_plugin->clientCount()));
    }

    QPointer<OVP::WebSocketServerPlugin> m_plugin;
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
