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

// ------------------------------------------------------- WebSocketWorker

WebSocketWorker::WebSocketWorker(QObject *parent)
    : QObject(parent)
{
}

WebSocketWorker::~WebSocketWorker()
{
    closeServer();
}

void WebSocketWorker::setFrame(const cv::Mat &image)
{
    // 深拷贝后再换入缓冲区：上游后续原地修改不会影响已提交图像
    cv::Mat copy = image.clone();
    QMutexLocker locker(&m_frameMutex);
    std::swap(m_frame, copy);
    ++m_revision;
}

quint64 WebSocketWorker::frameRevision() const
{
    QMutexLocker locker(&m_frameMutex);
    return m_revision;
}

QString WebSocketWorker::lastError() const
{
    QMutexLocker locker(&m_errorMutex);
    return m_error;
}

void WebSocketWorker::setError(const QString &message)
{
    QMutexLocker locker(&m_errorMutex);
    m_error = message;
}

bool WebSocketWorker::startServer(int port, int fps, int quality, QString *error)
{
    m_quality = std::clamp(quality, 10, 100);
    const int interval = broadcastInterval(fps);
    if (error)
        error->clear();

    if (m_running && m_server && m_server->isListening() && m_listenPort == port) {
        if (m_timer && m_timer->interval() != interval)
            m_timer->setInterval(interval);
        m_interval = interval;
        return true;
    }

    closeServer();

    if (!m_server) {
        m_server = new QTcpServer(this);
        connect(m_server, &QTcpServer::newConnection, this, &WebSocketWorker::onNewConnection);
    }

    if (port < 1 || port > 65535
        || !m_server->listen(QHostAddress::Any, static_cast<quint16>(port))) {
        const QString message =
            QStringLiteral("WebSocket 启动失败: %1（端口 %2）")
                .arg(port < 1 || port > 65535 ? QStringLiteral("端口超出范围")
                                              : m_server->errorString())
                .arg(port);
        if (error)
            *error = message;
        setError(message);
        m_server->close();
        return false;
    }

    setError(QString());
    m_listenPort = port;
    m_running = true;

    if (!m_timer) {
        m_timer = new QTimer(this);
        m_timer->setTimerType(Qt::CoarseTimer);
        connect(m_timer, &QTimer::timeout, this, &WebSocketWorker::onBroadcast);
    }
    m_timer->start(interval);
    m_interval = interval;

    return true;
}

void WebSocketWorker::configure(int port, int fps, int quality)
{
    m_quality = std::clamp(quality, 10, 100);

    if (m_running && m_server && m_server->isListening() && m_listenPort == port) {
        const int interval = broadcastInterval(fps);
        if (m_timer && m_timer->interval() != interval)
            m_timer->setInterval(interval);
        m_interval = interval;
        return;
    }

    startServer(port, fps, quality, nullptr);
}

void WebSocketWorker::stopServer()
{
    setError(QString());
    closeServer();
}

void WebSocketWorker::broadcastOnce()
{
    onBroadcast();
}

void WebSocketWorker::closeServer()
{
    m_running = false;
    m_clientCount = 0;
    m_interval = 0;

    if (m_timer)
        m_timer->stop();

    if (m_server)
        m_server->close();

    for (QTcpSocket *socket : std::as_const(m_clients)) {
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

void WebSocketWorker::onNewConnection()
{
    if (!m_server)
        return;

    while (QTcpSocket *socket = m_server->nextPendingConnection()) {
        // 立即登记无数据连接，确保 stopServer 也能回收它们。
        m_handshaking.insert(socket, QByteArray());
        socket->setReadBufferSize(kMaxHandshakeBytes);
        connect(socket, &QTcpSocket::readyRead, this, &WebSocketWorker::onSocketReadyRead);
        connect(socket, &QTcpSocket::disconnected, this, &WebSocketWorker::onSocketDisconnected);
        connect(socket, &QAbstractSocket::errorOccurred, this, &WebSocketWorker::onSocketError);
        QTimer::singleShot(kHandshakeTimeoutMs, socket, [this, socket]() {
            if (m_handshaking.contains(socket))
                socket->abort();
        });
        if (socket->bytesAvailable() > 0)
            handshake(socket);
    }
}

void WebSocketWorker::onSocketReadyRead()
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

void WebSocketWorker::onSocketDisconnected()
{
    QTcpSocket *socket = qobject_cast<QTcpSocket *>(sender());
    if (!socket)
        return;

    removeClient(socket);
    socket->deleteLater();
}

void WebSocketWorker::onSocketError()
{
    QTcpSocket *socket = qobject_cast<QTcpSocket *>(sender());
    if (!socket)
        return;

    removeClient(socket);
    socket->abort();
    socket->deleteLater();
}

void WebSocketWorker::removeClient(QTcpSocket *socket)
{
    m_clients.removeAll(socket);
    m_handshaking.remove(socket);
    m_clientCount = static_cast<int>(m_clients.size());
}

bool WebSocketWorker::handshake(QTcpSocket *socket)
{
    QByteArray &buffer = m_handshaking[socket];
    buffer.append(socket->readAll());

    const qsizetype headerEnd = buffer.indexOf("\r\n\r\n");
    if ((headerEnd < 0 && buffer.size() >= kMaxHandshakeBytes)
        || headerEnd > kMaxHandshakeBytes - 4) {
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

const QByteArray &WebSocketWorker::encodedCurrentFrame()
{
    const int quality = m_quality;
    cv::Mat image;
    quint64 revision = 0;
    {
        QMutexLocker locker(&m_frameMutex);
        revision = m_revision;
        if (revision == m_encodedRevision && quality == m_encodedQuality)
            return m_encodedFrame;
        image = m_frame;
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
    setError(error);
    return m_encodedFrame;
}

QByteArray WebSocketWorker::textFrame(const QByteArray &payload)
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

void WebSocketWorker::onBroadcast()
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

// -------------------------------------------------- WebSocketServerPlugin

WebSocketServerPlugin::WebSocketServerPlugin(QObject *parent)
    : PluginBase(parent)
{
    // 后台发送器一开始就创建并移到独立线程；线程在首次启动时运行
    m_worker = new WebSocketWorker;
    m_worker->moveToThread(&m_thread);
    m_thread.setObjectName(QStringLiteral("WebSocketSender"));
}

WebSocketServerPlugin::~WebSocketServerPlugin()
{
    stopServer();

    if (m_thread.isRunning()) {
        m_thread.quit();
        m_thread.wait(3000);
    }

    delete m_worker;
    m_worker = nullptr;
}

void WebSocketServerPlugin::ensureThread()
{
    if (!m_worker || m_thread.isRunning())
        return;
    m_thread.start();
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
                  QStringLiteral("加载项目/运行流程时自动启动后台发送线程")),
        intParam(QStringLiteral("fps"), QStringLiteral("发送帧率"), 30, 1, 60, 1,
                 QStringLiteral("后台线程每秒发送的图像帧数"))
    };
}

bool WebSocketServerPlugin::execute()
{
    clearError();

    // 只更新缓冲变量：编码和发送都在后台线程，不占用流程线程
    const cv::Mat image = inputImage(QStringLiteral("input"));
    if (m_worker)
        m_worker->setFrame(image);

    setOutput(QStringLiteral("output"), imageValue(image));
    setOutput(QStringLiteral("client_count"), clientCount());

    if (paramBool(QStringLiteral("auto_start"), true)) {
        requestConfigure(paramInt(QStringLiteral("port"), 9000),
                         paramInt(QStringLiteral("fps"), 30),
                         paramInt(QStringLiteral("quality"), 80));
    }

    const QString error = m_worker ? m_worker->lastError() : QString();
    if (!error.isEmpty())
        setError(error);

    return true;
}

QVariantMap WebSocketServerPlugin::extraData() const
{
    return {{QStringLiteral("running"), isRunning()}};
}

void WebSocketServerPlugin::setExtraData(const QVariantMap &data)
{
    if (data.value(QStringLiteral("running")).toBool()
        && paramBool(QStringLiteral("auto_start"), true)) {
        QString error;
        startServer(&error);
    }
}

// ------------------------------------------------------------------ 服务控制

bool WebSocketServerPlugin::isRunning() const
{
    return m_worker && m_worker->isRunning();
}

int WebSocketServerPlugin::clientCount() const
{
    return m_worker ? m_worker->clientCount() : 0;
}

int WebSocketServerPlugin::listenPort() const
{
    return m_worker ? m_worker->listenPort() : 0;
}

int WebSocketServerPlugin::broadcastInterval() const
{
    return m_worker ? m_worker->intervalMs() : 0;
}

quint64 WebSocketServerPlugin::frameRevision() const
{
    return m_worker ? m_worker->frameRevision() : 0;
}

bool WebSocketServerPlugin::startServer(QString *error)
{
    if (!m_worker) {
        if (error)
            *error = QStringLiteral("插件已释放");
        return false;
    }

    ensureThread();

    const int port = paramInt(QStringLiteral("port"), 9000);
    const int fps = paramInt(QStringLiteral("fps"), 30);
    const int quality = paramInt(QStringLiteral("quality"), 80);

    bool ok = false;
    QString message;

    if (QThread::currentThread() == m_thread.currentThread()) {
        ok = m_worker->startServer(port, fps, quality, &message);
    } else {
        // 阻塞等待后台线程完成监听，调用方马上能拿到结果
        QMetaObject::invokeMethod(
            m_worker,
            [this, port, fps, quality, &ok, &message]() {
                ok = m_worker->startServer(port, fps, quality, &message);
            },
            Qt::BlockingQueuedConnection);
    }

    if (!ok && error)
        *error = message.isEmpty() ? QStringLiteral("WebSocket 启动失败") : message;
    return ok;
}

void WebSocketServerPlugin::stopServer()
{
    if (!m_worker)
        return;

    if (!m_thread.isRunning()) {
        m_worker->stopServer();
        return;
    }

    if (QThread::currentThread() == m_thread.currentThread()) {
        m_worker->stopServer();
        return;
    }

    QMetaObject::invokeMethod(m_worker, [this]() { m_worker->stopServer(); },
                              Qt::BlockingQueuedConnection);
}

void WebSocketServerPlugin::requestConfigure(int port, int fps, int quality)
{
    if (!m_worker)
        return;

    ensureThread();

    if (QThread::currentThread() == m_thread.currentThread()) {
        m_worker->configure(port, fps, quality);
        return;
    }

    QMetaObject::invokeMethod(
        m_worker,
        [this, port, fps, quality]() { m_worker->configure(port, fps, quality); },
        Qt::QueuedConnection);
}

bool WebSocketServerPlugin::broadcastNow()
{
    if (!m_worker || !m_thread.isRunning())
        return false;

    if (QThread::currentThread() == m_thread.currentThread()) {
        m_worker->broadcastOnce();
        return true;
    }

    QMetaObject::invokeMethod(m_worker, [this]() { m_worker->broadcastOnce(); },
                              Qt::BlockingQueuedConnection);
    return true;
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
