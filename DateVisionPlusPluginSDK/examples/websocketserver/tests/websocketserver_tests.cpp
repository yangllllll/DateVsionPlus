#include "websocketserverplugin.h"

#include <QElapsedTimer>
#include <QHostAddress>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTest>
#include <QThread>
#include <QTimer>

#include <opencv2/imgcodecs.hpp>

#include <atomic>
#include <functional>
#include <thread>

namespace OVP {

class WebSocketServerPluginTest : public QObject
{
    Q_OBJECT

    static bool waitFor(const std::function<bool()> &condition, int timeout = 3000)
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

    static bool start(WebSocketServerPlugin &plugin)
    {
        plugin.setParam(QStringLiteral("auto_start"), false);
        plugin.setParam(QStringLiteral("port"), freePort());
        if (!plugin.startServer())
            return false;
        plugin.m_broadcastTimer->stop();
        return true;
    }

    static bool connectClient(WebSocketServerPlugin &plugin, QTcpSocket &client)
    {
        const int previousCount = plugin.clientCount();
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
        return plugin.clientCount() == previousCount + 1 &&
               response.startsWith("HTTP/1.1 101 ") &&
               response.contains("Sec-WebSocket-Accept: s3pPLMBiTxaQ9kYGzzhZRbK+xOo=");
    }

    static QByteArray readPayload(QTcpSocket &client)
    {
        QByteArray frame;
        qsizetype headerSize = 0;
        quint64 payloadSize = 0;
        const bool complete = waitFor([&]() {
            frame += client.readAll();
            if (frame.size() < 2)
                return false;
            payloadSize = static_cast<uchar>(frame.at(1));
            headerSize = 2;
            if (payloadSize == 126) {
                if (frame.size() < 4)
                    return false;
                payloadSize = (static_cast<uchar>(frame.at(2)) << 8) |
                              static_cast<uchar>(frame.at(3));
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
        });
        if (!complete || static_cast<uchar>(frame.at(0)) != 0x81)
            return {};
        return frame.mid(headerSize, static_cast<qsizetype>(payloadSize));
    }

    static cv::Mat decode(const QByteArray &payload)
    {
        const QByteArray jpeg = QByteArray::fromBase64(payload);
        return cv::imdecode(std::vector<uchar>(jpeg.begin(), jpeg.end()), cv::IMREAD_UNCHANGED);
    }

private slots:
    void workerExecutionUsesOwnerThread()
    {
        WebSocketServerPlugin plugin;
        plugin.setParam(QStringLiteral("port"), freePort());
        plugin.setInput(QStringLiteral("input"), imageValue(cv::Mat(64, 64, CV_8UC3, cv::Scalar(20, 40, 80))));
        std::atomic_bool success{true};
        std::thread worker([&]() {
            for (int i = 0; i < 100; ++i)
                success = plugin.execute() && success;
        });
        worker.join();
        QVERIFY(success);
        QVERIFY(!plugin.isRunning()); // No event processing while the worker runs.
        QVERIFY(plugin.m_restartQueued);
        QVERIFY(waitFor([&]() { return plugin.isRunning(); }));
        QCOMPARE(plugin.m_server->thread(), plugin.thread());
        QCOMPARE(plugin.m_broadcastTimer->thread(), plugin.thread());
        QVERIFY(!plugin.m_restartQueued);

        QTcpSocket client;
        plugin.m_broadcastTimer->stop();
        QVERIFY(connectClient(plugin, client));
        std::atomic_bool finished{false};
        std::thread concurrentWorker([&]() {
            for (int i = 0; i < 200; ++i) {
                plugin.execute();
                std::this_thread::yield();
            }
            finished = true;
        });
        while (!finished) {
            plugin.onBroadcast();
            QCoreApplication::processEvents();
            client.readAll();
        }
        concurrentWorker.join();
        QCOMPARE(plugin.m_frameRevision, quint64(300));
        QCOMPARE(plugin.output(QStringLiteral("client_count")).toInt(), 1);
    }

    void manualStopCancelsPendingStart()
    {
        WebSocketServerPlugin plugin;
        plugin.setParam(QStringLiteral("port"), freePort());
        std::thread worker([&]() { plugin.execute(); });
        worker.join();
        QVERIFY(plugin.m_restartQueued);
        plugin.stopServer();
        QCoreApplication::processEvents();
        QVERIFY(!plugin.isRunning());
        QVERIFY(!plugin.m_restartQueued);

        // A later execution may auto-start again; old requests cannot cancel it.
        std::thread laterWorker([&]() { plugin.execute(); });
        laterWorker.join();
        QVERIFY(waitFor([&]() { return plugin.isRunning(); }));
    }

    void snapshotCacheAndQualityInvalidation()
    {
        WebSocketServerPlugin plugin;
        QVERIFY(start(plugin));
        QTcpSocket client;
        QVERIFY(connectClient(plugin, client));

        cv::Mat input(240, 320, CV_8UC3);
        cv::randu(input, cv::Scalar::all(0), cv::Scalar::all(255));
        const cv::Mat original = input.clone();
        plugin.setInput(QStringLiteral("input"), imageValue(input));
        QVERIFY(plugin.execute());
        input.setTo(cv::Scalar::all(0));
        plugin.onBroadcast();
        const QByteArray first = readPayload(client);
        QVERIFY(!first.isEmpty());
        const cv::Mat decoded = decode(first);
        QCOMPARE(decoded.rows, original.rows);
        QCOMPARE(decoded.cols, original.cols);
        QVERIFY(cv::mean(decoded)[0] > 100); // The producer mutation did not alter the snapshot.
        const char *cachedData = plugin.m_encodedFrame.constData();
        const quint64 revision = plugin.m_encodedRevision;

        plugin.onBroadcast();
        QCOMPARE(readPayload(client), first); // Paused workflows keep transmitting their final image.
        QVERIFY(plugin.m_encodedFrame.constData() == cachedData);
        QCOMPARE(plugin.m_encodedRevision, revision);

        plugin.setParam(QStringLiteral("quality"), 20);
        plugin.onBroadcast();
        const QByteArray lowerQuality = readPayload(client);
        QVERIFY(!lowerQuality.isEmpty());
        QVERIFY(lowerQuality.size() < first.size());
        QCOMPARE(plugin.m_encodedQuality, 20);
        QCOMPARE(plugin.m_encodedRevision, revision);

        QVERIFY(plugin.execute()); // A new black snapshot must replace cached noise.
        plugin.onBroadcast();
        QCOMPARE(cv::mean(decode(readPayload(client)))[0], 0.0);
        QVERIFY(plugin.m_encodedRevision > revision);
    }

    void manualStartOverridesPendingConfiguration()
    {
        WebSocketServerPlugin plugin;
        const int automaticPort = freePort();
        int manualPort = freePort();
        while (manualPort == automaticPort)
            manualPort = freePort();
        plugin.setParam(QStringLiteral("port"), automaticPort);
        std::thread worker([&]() { plugin.execute(); });
        worker.join();
        QVERIFY(plugin.m_restartQueued);
        plugin.setParam(QStringLiteral("port"), manualPort);
        QVERIFY(plugin.startServer());
        QCOMPARE(plugin.listenPort(), manualPort);
        QCoreApplication::processEvents();
        QCOMPARE(plugin.listenPort(), manualPort);
        QVERIFY(plugin.isRunning());
        QVERIFY(!plugin.m_restartQueued);
        plugin.m_broadcastTimer->stop();
        QTcpSocket client;
        QVERIFY(connectClient(plugin, client));
    }

    void noClientsAndBackpressureSkipEncoding()
    {
        WebSocketServerPlugin plugin;
        QVERIFY(start(plugin));
        plugin.setInput(QStringLiteral("input"), imageValue(cv::Mat(64, 64, CV_8UC3, cv::Scalar::all(128))));
        QVERIFY(plugin.execute());
        plugin.onBroadcast();
        QVERIFY(plugin.m_encodedFrame.isEmpty());
        QCOMPARE(plugin.m_encodedRevision, quint64(0));

        QTcpSocket slowClient;
        QVERIFY(connectClient(plugin, slowClient));
        QTcpSocket *serverSocket = plugin.m_clients.first();
        // With no event loop turn between calls, bytesToWrite deterministically holds one frame.
        plugin.onBroadcast();
        const qint64 queued = serverSocket->bytesToWrite();
        QVERIFY(queued > 0);
        const quint64 encodedRevision = plugin.m_encodedRevision;
        QVERIFY(plugin.execute());
        for (int i = 0; i < 100; ++i)
            plugin.onBroadcast();
        QCOMPARE(serverSocket->bytesToWrite(), queued);
        QCOMPARE(plugin.m_encodedRevision, encodedRevision);
        QVERIFY(!readPayload(slowClient).isEmpty());

        // An unblocked peer still receives while the slow client's queue stays bounded.
        QTcpSocket fastClient;
        QVERIFY(connectClient(plugin, fastClient));
        serverSocket->write(QByteArray(1024, 'x'));
        const qint64 slowQueued = serverSocket->bytesToWrite();
        QVERIFY(slowQueued > 0);
        plugin.onBroadcast();
        QCOMPARE(serverSocket->bytesToWrite(), slowQueued);
        QVERIFY(!readPayload(fastClient).isEmpty());
        QCOMPARE(plugin.m_encodedRevision, plugin.m_frameRevision);
    }

    void grayAlphaEmptyAndUnsupportedImages()
    {
        WebSocketServerPlugin plugin;
        QVERIFY(start(plugin));
        QTcpSocket client;
        QVERIFY(connectClient(plugin, client));
        plugin.setInput(QStringLiteral("input"), imageValue(cv::Mat(16, 24, CV_8UC1, cv::Scalar(90))));
        QVERIFY(plugin.execute());
        plugin.onBroadcast();
        const cv::Mat gray = decode(readPayload(client));
        QCOMPARE(gray.channels(), 1);
        QCOMPARE(cv::mean(gray)[0], 90.0);

        plugin.setInput(QStringLiteral("input"), imageValue(cv::Mat(16, 24, CV_8UC4, cv::Scalar(10, 20, 30, 255))));
        QVERIFY(plugin.execute());
        plugin.onBroadcast();
        QCOMPARE(decode(readPayload(client)).channels(), 3);

        plugin.setInput(QStringLiteral("input"), imageValue(cv::Mat()));
        QVERIFY(plugin.execute());
        plugin.onBroadcast();
        QVERIFY(plugin.m_encodedFrame.isEmpty());
        QCOMPARE(client.bytesAvailable(), qint64(0));

        plugin.setInput(QStringLiteral("input"), imageValue(cv::Mat(16, 24, CV_8UC2)));
        QVERIFY(plugin.execute());
        plugin.onBroadcast(); // cv::Exception must not escape the Qt timer callback.
        QVERIFY(plugin.m_encodedFrame.isEmpty());
        QVERIFY(!plugin.m_encodingError.isEmpty());
        const quint64 failedRevision = plugin.m_encodedRevision;
        plugin.onBroadcast();
        QCOMPARE(plugin.m_encodedRevision, failedRevision);
    }

    void idleAndPartialHandshakesCloseOnStop()
    {
        WebSocketServerPlugin plugin;
        QVERIFY(start(plugin));
        QTcpSocket idle;
        QTcpSocket partial;
        idle.connectToHost(QHostAddress::LocalHost, plugin.listenPort());
        partial.connectToHost(QHostAddress::LocalHost, plugin.listenPort());
        QVERIFY(waitFor([&]() { return plugin.m_handshaking.size() == 2; }));
        partial.write("GET / HTTP/1.1\r\nSec-WebSocket-Key:");
        QVERIFY(waitFor([&]() {
            for (const QByteArray &request : plugin.m_handshaking)
                if (!request.isEmpty())
                    return true;
            return false;
        }));
        QCOMPARE(plugin.clientCount(), 0);
        plugin.stopServer();
        QVERIFY(waitFor([&]() {
            return idle.state() == QAbstractSocket::UnconnectedState &&
                   partial.state() == QAbstractSocket::UnconnectedState;
        }));
        QVERIFY(plugin.m_handshaking.isEmpty());
    }

    void oversizedHandshakeIsRejected()
    {
        WebSocketServerPlugin plugin;
        QVERIFY(start(plugin));
        QTcpSocket client;
        client.connectToHost(QHostAddress::LocalHost, plugin.listenPort());
        QVERIFY(waitFor([&]() { return !plugin.m_handshaking.isEmpty(); }));
        client.write(QByteArray(20 * 1024, 'x'));
        QVERIFY(waitFor([&]() { return client.state() == QAbstractSocket::UnconnectedState; }));
        QVERIFY(plugin.m_handshaking.isEmpty());
        QCOMPARE(plugin.clientCount(), 0);
    }

    void fragmentedHandshakeCompletes()
    {
        WebSocketServerPlugin plugin;
        QVERIFY(start(plugin));
        QTcpSocket client;
        client.connectToHost(QHostAddress::LocalHost, plugin.listenPort());
        QVERIFY(waitFor([&]() { return !plugin.m_handshaking.isEmpty(); }));
        client.write("GET / HTTP/1.1\r\nSec-WebSocket-K");
        QVERIFY(waitFor([&]() { return !plugin.m_handshaking.constBegin().value().isEmpty(); }));
        QCOMPARE(plugin.clientCount(), 0);
        client.write("ey: dGhlIHNhbXBsZSBub25jZQ==\r\n\r\n");
        QVERIFY(waitFor([&]() { return plugin.clientCount() == 1; }));
        QVERIFY(waitFor([&]() { return client.bytesAvailable() > 0; }));
        QVERIFY(client.readAll().contains("Sec-WebSocket-Accept: s3pPLMBiTxaQ9kYGzzhZRbK+xOo="));
        QVERIFY(plugin.m_handshaking.isEmpty());
    }

    void frameRateUpdateKeepsExistingClients()
    {
        WebSocketServerPlugin plugin;
        QVERIFY(start(plugin));
        QTcpSocket client;
        QVERIFY(connectClient(plugin, client));
        plugin.setParam(QStringLiteral("auto_start"), true);
        plugin.setParam(QStringLiteral("fps"), 20);
        std::thread worker([&]() { plugin.execute(); });
        worker.join();
        QVERIFY(waitFor([&]() { return plugin.m_broadcastTimer->interval() == 50; }));
        QCOMPARE(plugin.clientCount(), 1);
        QCOMPARE(client.state(), QAbstractSocket::ConnectedState);
    }

    void frameLengthBoundaries()
    {
        for (const int length : {0, 125, 126, 65535, 65536}) {
            const QByteArray payload(length, 'x');
            const QByteArray frame = WebSocketServerPlugin::textFrame(payload);
            const int headerSize = length < 126 ? 2 : (length <= 65535 ? 4 : 10);
            QCOMPARE(frame.size(), qsizetype(length + headerSize));
            QCOMPARE(static_cast<uchar>(frame.at(0)), uchar(0x81));
            QCOMPARE(frame.mid(headerSize), payload);
            quint64 declaredLength = static_cast<uchar>(frame.at(1));
            if (headerSize > 2) {
                declaredLength = 0;
                for (int i = 2; i < headerSize; ++i)
                    declaredLength = (declaredLength << 8) | static_cast<uchar>(frame.at(i));
            }
            QCOMPARE(declaredLength, quint64(length));
        }
    }

    void cachedEncodingBenchmark()
    {
        WebSocketServerPlugin plugin;
        plugin.setParam(QStringLiteral("auto_start"), false);
        cv::Mat image(1080, 1920, CV_8UC3);
        cv::randu(image, cv::Scalar::all(0), cv::Scalar::all(255));
        plugin.setInput(QStringLiteral("input"), imageValue(image));
        QVERIFY(plugin.execute());
        QElapsedTimer timer;
        timer.start();
        QVERIFY(!plugin.encodedCurrentFrame().isEmpty());
        const qint64 firstNs = timer.nsecsElapsed();
        timer.restart();
        for (int i = 0; i < 1000; ++i)
            QVERIFY(!plugin.encodedCurrentFrame().isEmpty());
        qInfo("1080p JPEG+base64+framing: first %.3f ms; cached lookup mean %.3f us (1000 calls)",
              firstNs / 1e6, timer.nsecsElapsed() / 1e6);
    }
};

} // namespace OVP

QTEST_GUILESS_MAIN(OVP::WebSocketServerPluginTest)
#include "websocketserver_tests.moc"
