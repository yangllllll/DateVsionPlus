#include "snap7client.h"
#include "mock.h"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QLibrary>
#include <QTextStream>

#include <algorithm>
#include <limits>

namespace {
int failures = 0;
int checks = 0;
#define CHECK(condition) do { \
    ++checks; \
    if (!(condition)) { \
        ++failures; \
        QTextStream(stderr) << "FAIL line " << __LINE__ << ": " << #condition << Qt::endl; \
    } \
} while (false)

using VoidFn = void(SNAP7_CALL *)();
using IntFn = void(SNAP7_CALL *)(int);
using StatsFn = MockStats(SNAP7_CALL *)();

void benchmark(StatsFn stats, VoidFn reset)
{
    reset();
    Snap7::Snap7Client client;
    CHECK(client.connectTo(QStringLiteral("mock-only"), 0, 1));
    constexpr int iterations = 1000000;
    qint64 times[5] = {};
    for (qint64 &elapsed : times) {
        QElapsedTimer timer;
        timer.start();
        for (int i = 0; i < iterations; ++i) {
            client.writeByte(Snap7::AreaMK, 0, 100, static_cast<quint8>(i & 1));
            quint8 value;
            client.readByte(Snap7::AreaMK, 0, 100, &value);
        }
        elapsed = timer.nsecsElapsed();
    }
    std::sort(std::begin(times), std::end(times));
    CHECK(stats().writes == 5 * iterations);
    CHECK(stats().reads == 5 * iterations);
    QTextStream(stdout) << "mock median writeByte + readByte: "
                        << static_cast<double>(times[2]) / iterations
                        << " ns/pair; " << stats().writes << " writes, "
                        << stats().reads << " reads (5 rounds)" << Qt::endl;
}
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    const QStringList args = app.arguments();
    if (args.size() < 2 || !QFileInfo::exists(args.at(1))) {
        QTextStream(stderr) << "Usage: plc_tests <absolute mock library path> [--benchmark-only]" << Qt::endl;
        return 2;
    }
    // Prove that this is our inert test library before allowing the wrapper to load it.
    QLibrary mock(args.at(1));
    const auto reset = reinterpret_cast<VoidFn>(mock.resolve("MockReset"));
    const auto stats = reinterpret_cast<StatsFn>(mock.resolve("MockStatsSnapshot"));
    const auto failCreate = reinterpret_cast<VoidFn>(mock.resolve("MockFailNextCreate"));
    const auto statusError = reinterpret_cast<IntFn>(mock.resolve("MockStatusError"));
    const auto ioError = reinterpret_cast<IntFn>(mock.resolve("MockIOError"));
    const auto connectError = reinterpret_cast<IntFn>(mock.resolve("MockConnectError"));
    if (!reset || !stats || !failCreate || !statusError || !ioError || !connectError)
        return 2;
    QString error;
    CHECK(Snap7::Snap7Api::instance()->load(args.at(1), &error));
    if (failures)
        return 1;
    if (args.contains(QStringLiteral("--benchmark-only"))) {
        benchmark(stats, reset);
        return failures ? 1 : 0;
    }

    reset();
    {
        Snap7::Snap7Client client;
        failCreate();
        CHECK(!client.connectTo(QStringLiteral("mock-only"), 0, 1, &error));
        CHECK(stats().connects == 0);
        CHECK(client.connectTo(QStringLiteral("mock-only"), 0, 1, &error));
        CHECK(stats().creates == 2);
        CHECK(stats().connects == 1);
        CHECK(stats().statusQueries == 1);
    }
    CHECK(stats().destroys == 1);

    reset();
    {
        Snap7::Snap7Client client;
        CHECK(client.connectTo(QStringLiteral("mock-only"), 0, 1));
        CHECK(client.connectTo(QStringLiteral("mock-only"), 0, 1));
        CHECK(stats().connects == 1);
        CHECK(stats().statusQueries == 2);
        CHECK(client.connectTo(QStringLiteral("mock-only"), 1, 1));
        CHECK(stats().connects == 2 && stats().disconnects == 1);
        CHECK(stats().rack == 1);
        CHECK(client.connectTo(QStringLiteral("mock-only"), 1, 2));
        CHECK(stats().connects == 3 && stats().disconnects == 2);
        CHECK(stats().slot == 2);
        CHECK(client.connectTo(QStringLiteral("another-mock"), 1, 2));
        CHECK(stats().connects == 4 && stats().statusQueries == 5);
        statusError(1);
        CHECK(!client.isConnected());
        statusError(0);
        connectError(1);
        CHECK(!client.connectTo(QStringLiteral("another-mock"), 1, 2));
        CHECK(client.address().isEmpty());
        connectError(0);
        CHECK(client.connectTo(QStringLiteral("another-mock"), 1, 2));
    }

    reset();
    {
        Snap7::Snap7Client client;
        CHECK(client.connectTo(QStringLiteral("mock-only"), 0, 1));
        for (int i = 0; i < 1000; ++i)
            CHECK(client.writeByte(Snap7::AreaMK, 0, 100, 1));
        CHECK(stats().writes == 1000); // Do not suppress identical cyclic writes.
        CHECK(stats().amount == 1 && stats().value == 1);
        quint8 value = 0;
        CHECK(client.readByte(Snap7::AreaMK, 0, 100, &value));
        CHECK(value == 0x5A);
        // Snap7 Amount is an element count; WLBit supports only one element.
        // https://snap7.sourceforge.net/sharp7.html (ReadArea buffer size table)
        const struct { int wordLen; int bytes; } cases[] = {
            {0x01, 1}, {0x02, 1}, {0x03, 1}, {0x04, 2}, {0x05, 2}, {0x06, 4}, {0x07, 4},
            {0x08, 4}, {0x1C, 2}, {0x1D, 2}
        };
        for (const auto &test : cases) {
            const int amount = test.wordLen == Snap7::WLBit ? 1 : 3;
            QByteArray data;
            CHECK(client.readArea(Snap7::AreaDB, 1, 0, amount, test.wordLen, &data));
            CHECK(data == QByteArray(amount * test.bytes, '\x5A'));
            CHECK(stats().amount == amount);
            CHECK(client.writeArea(Snap7::AreaDB, 1, 0, test.wordLen, data));
            CHECK(stats().amount == amount);
        }
        for (int area : {Snap7::AreaCT, Snap7::AreaTM}) {
            QByteArray data;
            CHECK(client.readArea(area, 0, 0, 3, Snap7::WLByte, &data));
            CHECK(data == QByteArray(6, '\x5A'));
            CHECK(client.writeArea(area, 0, 0, Snap7::WLByte, data));
            CHECK(stats().amount == 3);
            CHECK(!client.readByte(area, 0, 0, &value, &error));
            CHECK(!client.writeByte(area, 0, 0, 1, &error));
        }
        const int writes = stats().writes;
        CHECK(!client.writeArea(Snap7::AreaDB, 1, 0, Snap7::WLWord, QByteArray(3, 'x'), &error));
        CHECK(!client.writeArea(Snap7::AreaDB, 1, 0, Snap7::WLByte, QByteArray(), &error));
        CHECK(!client.writeArea(Snap7::AreaDB, 1, 0, 999, QByteArray(4, 'x'), &error));
        CHECK(stats().writes == writes);
        const int reads = stats().reads;
        QByteArray data("unchanged");
        CHECK(!client.readArea(Snap7::AreaDB, 1, 0, -1, Snap7::WLByte, &data, &error));
        CHECK(!client.readArea(Snap7::AreaDB, 1, 0, 0, Snap7::WLByte, &data, &error));
        CHECK(!client.readArea(Snap7::AreaDB, 1, 0, std::numeric_limits<int>::max(), Snap7::WLDWord, &data, &error));
        CHECK(!client.readArea(Snap7::AreaDB, 1, 0, 1, 999, &data, &error));
        CHECK(stats().reads == reads);
        CHECK(data == QByteArray("unchanged"));
        ioError(1);
        CHECK(!client.readArea(Snap7::AreaDB, 1, 0, 2, Snap7::WLWord, &data, &error));
        CHECK(data == QByteArray("unchanged"));
        value = 7;
        CHECK(!client.readByte(Snap7::AreaMK, 0, 100, &value, &error));
        CHECK(value == 7);
        CHECK(!client.writeByte(Snap7::AreaMK, 0, 100, 1, &error));
        ioError(0);
    }
    benchmark(stats, reset);
    QTextStream(stdout) << checks << " checks, " << failures << " failures" << Qt::endl;
    return failures ? 1 : 0;
}
