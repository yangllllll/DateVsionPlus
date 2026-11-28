#include "websocketserverplugin.h"

#include <QElapsedTimer>
#include <QHostAddress>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTest>

#include <opencv2/imgcodecs.hpp>

#include <cmath>
#include <functional>
#include <thread>

namespace OVP {

/**
 * 回归目的：确认「缓冲区 + 独立发送线程」的行为
 *   - execute() 只更新缓冲区，编码/发送发生在后台线程；
 *   - 流程不执行时后台线程仍按帧率发送缓冲区里的最后一帧；
 *   - 上游修改图像不会影响已提交的缓冲区；
 *   - 启停、客户端计数、握手清理仍然正常。
 */
class WebSocketServerPluginTest : public QObject
{
    Q_OBJECT

    static bool waitFor(const std::function<bool()> &condition, int timeout = 4000)
    {
        QElapsedTimer timer;
        timer.start();
        while (!condition() && timer.elapsed() < timeout)
            QTest::qWait(1);
        return condition();
    }

    static int freePort()
    {
        QTcpServer reservation;
        if (!reservation.listen(QHostAddress::LocalHost, 0))
            return 0;
        return reservation.serverPort();
    }

    static bool start(WebSocketServerPlugin &plugin, int fps = 20)
    {
        plugin.setParam(QStringLiteral("auto_start"), false);
        plugin.setParam(QStringLiteral("fps"), fps);
        plugin.setParam(QStringLiteral("port"), freePort());
        return plugin.startServer();
    }

    static bool connectClient(WebSocketServerPlugin &plugin, QTcpSocket &client)
    {
        client.connectToHost(QHostAddress::LocalHost, plugin.listenPort());
        if (!waitFor([&]() { return client.state() == QAbstractSocket::ConnectedState; }))
            return false;

        client.write("GET / HTTP/1.1\r\nHost: localhost\r\nUpgrade: websocket\r\n"
                     "Connection: Upgrade\r\nSec-WebSocket-Version: 13\r\n"
                     "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n\r\n");
        QByteArray response;
        if (!waitFor([&]() {
                response += client.readAll();
                return response.contains("\r\n\r\n");
            }))
            return false;

        return plugin.clientCount() > 0 && response.startsWith("HTTP/1.1 101 ")
               && response.contains("Sec-WebSocket-Accept: s3pPLMBiTxaQ9kYGzzhZRbK+xOo=");
    }

    /** 读取一个完整的 WebSocket 文本帧（超时返回空） */
    static QByteArray readPayload(QTcpSocket &client, int timeout = 4000)
    {
        QByteArray frame;
        qsizetype headerSize = 0;
        quint64 payloadSize = 0;
        const bool complete = waitFor(
            [&]() {
                frame += client.readAll();
                if (frame.size() < 2)
                    return false;
                payloadSize = static_cast<uchar>(frame.at(1));
                headerSize = 2;
                if (payloadSize == 126) {
                    if (frame.size() < 4)
                        return false;
                    payloadSize = (static_cast<uchar>(frame.at(2)) << 8)
                                  | static_cast<uchar>(frame.at(3));
                    headerSize = 4;
                } else if (payloadSize == 127) {
                    if (frame.size() < 10)
                        return false;
                    payloadSize = 0;
                    for (int i = 2; i < 10; ++i)
                        payloadSize = (payloadSize << 8) | static_cast<uchar>(frame.at(i));
                    headerSize = 10;
                }
                return static_cast<quint64>(frame.size() - headerSize) >= payloadSize;
            },
            timeout);

        if (!complete || static_cast<uchar>(frame.at(0)) != 0x81)
            return {};
        return frame.mid(headerSize, static_cast<qsizetype>(payloadSize));
    }

    static cv::Mat decode(const QByteArray &payload)
    {
        const QByteArray jpeg = QByteArray::fromBase64(payload);
        return cv::imdecode(std::vector<uchar>(jpeg.begin(), jpeg.end()), cv::IMREAD_UNCHANGED);
    }

    /** 持续读取帧，直到满足条件或超时 */
    static QByteArray readFrameUntil(
        QTcpSocket &client,
        const std::function<bool(const QByteArray &, const cv::Mat &)> &accept,
        int timeout = 4000)
    {
        QElapsedTimer timer;
        timer.start();
        while (timer.elapsed() < timeout) {
            const QByteArray payload = readPayload(client, 1000);
            if (payload.isEmpty())
                continue;
            if (accept(payload, decode(payload)))
                return payload;
        }
        return {};
    }

private slots:
    void backgroundThreadStreamsBufferedFrame()
    {
        WebSocketServerPlugin plugin;
        QVERIFY(start(plugin));
        QVERIFY(plugin.isRunning());
        QVERIFY(plugin.listenPort() > 0);

        QTcpSocket client;
        QVERIFY(connectClient(plugin, client));
        QCOMPARE(plugin.clientCount(), 1);

        plugin.setInput(QStringLiteral("input"),
                        imageValue(cv::Mat(48, 64, CV_8UC1, cv::Scalar(90))));
        QVERIFY(plugin.execute());
        QCOMPARE(plugin.frameRevision(), quint64(1));

        const QByteArray first = readFrameUntil(client, [](const QByteArray &, const cv::Mat &image) {
            return !image.empty() && image.channels() == 1 && cv::mean(image)[0] > 89.0;
        });
        QVERIFY(!first.isEmpty());
        const cv::Mat decoded = decode(first);
        QCOMPARE(decoded.rows, 48);
        QCOMPARE(decoded.cols, 64);
        QCOMPARE(decoded.channels(), 1);

        // 不执行流程：后台线程继续发送缓冲区里的同一帧（编码缓存复用）
        const QByteArray second = readFrameUntil(client, [](const QByteArray &, const cv::Mat &image) {
            return !image.empty();
        });
        QVERIFY(!second.isEmpty());
        QCOMPARE(second, first);
        QCOMPARE(plugin.frameRevision(), quint64(1));

        plugin.stopServer();
        QVERIFY(!plugin.isRunning());
        QCOMPARE(plugin.clientCount(), 0);
        QVERIFY(waitFor([&]() { return client.state() == QAbstractSocket::UnconnectedState; }));
    }

    void executeOnlyUpdatesBuffer()
    {
        WebSocketServerPlugin plugin;
        plugin.setParam(QStringLiteral("auto_start"), true);
        plugin.setParam(QStringLiteral("fps"), 20);
        plugin.setParam(QStringLiteral("port"), freePort());
        plugin.setParam(QStringLiteral("quality"), 80);

        cv::Mat input(60, 80, CV_8UC3);
        cv::randu(input, cv::Scalar::all(0), cv::Scalar::all(255));
        const double expectedMean = cv::mean(input)[0];

        plugin.setInput(QStringLiteral("input"), imageValue(input));
        QVERIFY(plugin.execute());
        QVERIFY(waitFor([&]() { return plugin.isRunning(); }));

        QTcpSocket client;
        QVERIFY(connectClient(plugin, client));

        const QByteArray first = readFrameUntil(client, [](const QByteArray &, const cv::Mat &image) {
            return !image.empty() && image.channels() == 3;
        });
        QVERIFY(!first.isEmpty());
        QVERIFY(std::abs(cv::mean(decode(first))[0] - expectedMean) < 5.0);

        // 上游原地改成白色，再用新质量强制重新编码：
        // 缓冲区是深拷贝，取到的仍应是原来的噪声图（而不是 255）
        input.setTo(cv::Scalar::all(255));
        plugin.setParam(QStringLiteral("quality"), 30);
        QVERIFY(plugin.execute());
        QCOMPARE(plugin.frameRevision(), quint64(2));

        const QByteArray second = readFrameUntil(
            client,
            [&first, expectedMean](const QByteArray &payload, const cv::Mat &image) {
                return !image.empty() && payload.size() < first.size()
                       && std::abs(cv::mean(image)[0] - expectedMean) < 5.0;
            });
        QVERIFY(!second.isEmpty());

        plugin.stopServer();
    }

    void executeFromWorkerThreadStartsBackgroundServer()
    {
        WebSocketServerPlugin plugin;
        plugin.setParam(QStringLiteral("auto_start"), true);
        plugin.setParam(QStringLiteral("fps"), 20);
        const int port = freePort();
        plugin.setParam(QStringLiteral("port"), port);
        plugin.setInput(QStringLiteral("input"),
                        imageValue(cv::Mat(32, 32, CV_8UC3, cv::Scalar(1, 2, 3))));

        // 流程线程调用 execute：只更新缓冲区并投递启动请求，后台线程自己启动
        std::thread worker([&]() { plugin.execute(); });
        worker.join();

        QVERIFY(waitFor([&]() { return plugin.isRunning(); }));
        QCOMPARE(plugin.listenPort(), port);
        QCOMPARE(plugin.frameRevision(), quint64(1));

        QTcpSocket client;
        QVERIFY(connectClient(plugin, client));
        QCOMPARE(plugin.clientCount(), 1);

        plugin.stopServer();
    }

    void manualStopThenExecuteRestarts()
    {
        WebSocketServerPlugin plugin;
        plugin.setParam(QStringLiteral("auto_start"), true);
        plugin.setParam(QStringLiteral("fps"), 20);
        plugin.setParam(QStringLiteral("port"), freePort());
        plugin.setInput(QStringLiteral("input"),
                        imageValue(cv::Mat(16, 16, CV_8UC3, cv::Scalar(5, 6, 7))));

        std::thread first([&]() { plugin.execute(); });
        first.join();
        QVERIFY(waitFor([&]() { return plugin.isRunning(); }));

        plugin.stopServer();
        QVERIFY(!plugin.isRunning());

        // 之后的流程执行可以重新拉起后台发送线程
        std::thread second([&]() { plugin.execute(); });
        second.join();
        QVERIFY(waitFor([&]() { return plugin.isRunning(); }));

        plugin.stopServer();
    }

    void oversizedHandshakeIsRejected()
    {
        WebSocketServerPlugin plugin;
        QVERIFY(start(plugin));

        QTcpSocket client;
        client.connectToHost(QHostAddress::LocalHost, plugin.listenPort());
        QVERIFY(waitFor([&]() { return client.state() == QAbstractSocket::ConnectedState; }));

        client.write(QByteArray(20 * 1024, 'x'));
        QVERIFY(waitFor([&]() { return client.state() == QAbstractSocket::UnconnectedState; },
                        6000));
        QCOMPARE(plugin.clientCount(), 0);

        plugin.stopServer();
    }

    void frameRevisionTracksExecutes()
    {
        WebSocketServerPlugin plugin;
        plugin.setParam(QStringLiteral("auto_start"), false);
        QCOMPARE(plugin.frameRevision(), quint64(0));

        plugin.setInput(QStringLiteral("input"),
                        imageValue(cv::Mat(8, 8, CV_8UC1, cv::Scalar(1))));
        QVERIFY(plugin.execute());
        QCOMPARE(plugin.frameRevision(), quint64(1));

        QVERIFY(plugin.execute());
        QCOMPARE(plugin.frameRevision(), quint64(2));
        QCOMPARE(plugin.output(QStringLiteral("client_count")).toInt(), 0);
    }
};

} // namespace OVP

QTEST_GUILESS_MAIN(OVP::WebSocketServerPluginTest)
#include "websocketserver_tests.moc"
