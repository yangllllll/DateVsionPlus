#include "panels/outputpanel.h"
#include "core/plugintypes.h"

#include <QAbstractItemModel>
#include <QSignalSpy>
#include <QTableWidget>
#include <QTextEdit>
#include <QTimer>
#include <QtTest>

namespace {
QMap<QString, QVariant> makeResults(int nodeCount, int portCount, int offset = 0)
{
    QMap<QString, QVariant> results;
    for (int node = 0; node < nodeCount; ++node) {
        QVariantMap ports;
        for (int port = 0; port < portCount; ++port)
            ports.insert(QStringLiteral("port_%1").arg(port, 3, 10, QLatin1Char('0')),
                         node * portCount + port + offset);
        results.insert(QStringLiteral("node_%1").arg(node, 3, 10, QLatin1Char('0')), ports);
    }
    return results;
}
}

class OutputPanelTest : public QObject
{
    Q_OBJECT

private slots:
    void unchangedResultsAvoidModelReset();
    void resultsHandleChangedValuesAndShape();
    void sustainedLoggingFlushesBeforeProducerStops();
    void clearDiscardsPendingLogs();
    void benchmarkUnchangedResults();
    void benchmarkChangingResults();
};

void OutputPanelTest::unchangedResultsAvoidModelReset()
{
    OutputPanel panel;
    const auto results = makeResults(3, 2);
    panel.updateResults(results);
    auto *table = panel.findChild<QTableWidget *>(QStringLiteral("tblResults"));
    QVERIFY(table);
    QCOMPARE(table->rowCount(), 6);
    auto *firstItem = table->item(0, 0);
    table->selectRow(2);
    QSignalSpy resetSpy(table->model(), &QAbstractItemModel::modelReset);
    QSignalSpy changedSpy(table->model(), &QAbstractItemModel::dataChanged);

    panel.updateResults(results);

    QCOMPARE(resetSpy.count(), 0);
    QCOMPARE(changedSpy.count(), 0);
    QCOMPARE(table->item(0, 0), firstItem);
    QCOMPARE(table->selectionModel()->selectedRows().size(), 1);
    QCOMPARE(table->selectionModel()->selectedRows().first().row(), 2);
}

void OutputPanelTest::resultsHandleChangedValuesAndShape()
{
    OutputPanel panel;
    auto *table = panel.findChild<QTableWidget *>(QStringLiteral("tblResults"));
    QVERIFY(table);
    panel.updateResults(makeResults(2, 2));
    panel.updateResults(makeResults(2, 2, 10));
    QCOMPARE(table->rowCount(), 4);
    QCOMPARE(table->item(3, 2)->text(), QStringLiteral("13"));

    QMap<QString, QVariant> results;
    results.insert(QStringLiteral("_metadata"), QVariantMap{{QStringLiteral("ignored"), 1}});
    results.insert(QStringLiteral("invalid"), QStringLiteral("not a port map"));
    results.insert(QStringLiteral("camera"), QVariantMap{
        {QStringLiteral("image"), OVP::imageValue(cv::Mat::zeros(2, 3, CV_8UC1))},
        {QStringLiteral("ok"), true}});
    panel.updateResults(results);
    QCOMPARE(table->rowCount(), 2);
    QCOMPARE(table->item(0, 0)->text(), QStringLiteral("camera"));
    QCOMPARE(table->item(0, 1)->text(), QStringLiteral("image"));
    QCOMPARE(table->item(0, 2)->text(), QStringLiteral("Mat 3x2"));
    QCOMPARE(table->item(1, 2)->text(), QStringLiteral("True"));

    panel.updateResults({});
    QCOMPARE(table->rowCount(), 0);
    panel.updateResults(makeResults(3, 2));
    QCOMPARE(table->rowCount(), 6);
    QCOMPARE(table->item(5, 2)->text(), QStringLiteral("5"));
}

void OutputPanelTest::sustainedLoggingFlushesBeforeProducerStops()
{
    OutputPanel panel;
    auto *log = panel.findChild<QTextEdit *>(QStringLiteral("txtLog"));
    QVERIFY(log);
    panel.log(QStringLiteral("first record"));
    QTimer producer;
    producer.setInterval(20);
    connect(&producer, &QTimer::timeout, &panel, [&panel] {
        panel.log(QStringLiteral("following record"));
    });
    producer.start();

    // A continuous producer must not keep postponing the first flush.
    QTRY_VERIFY_WITH_TIMEOUT(log->toPlainText().contains(QStringLiteral("first record")), 500);
    QVERIFY(producer.isActive());
}

void OutputPanelTest::clearDiscardsPendingLogs()
{
    OutputPanel panel;
    auto *log = panel.findChild<QTextEdit *>(QStringLiteral("txtLog"));
    QVERIFY(log);
    panel.log(QStringLiteral("discarded record"));
    panel.clearLog();
    QTest::qWait(120);
    QVERIFY(log->toPlainText().isEmpty());

    panel.log(QStringLiteral("new record"));
    QTRY_COMPARE(log->toPlainText(), QStringLiteral("new record\n"));
}

void OutputPanelTest::benchmarkUnchangedResults()
{
    OutputPanel panel;
    const auto results = makeResults(100, 10);
    panel.updateResults(results);
    QBENCHMARK {
        panel.updateResults(results);
    }
}

void OutputPanelTest::benchmarkChangingResults()
{
    OutputPanel panel;
    const auto first = makeResults(100, 10);
    const auto second = makeResults(100, 10, 1);
    panel.updateResults(first);
    bool alternate = false;
    QBENCHMARK {
        panel.updateResults(alternate ? first : second);
        alternate = !alternate;
    }
}

QTEST_MAIN(OutputPanelTest)
#include "outputpanel_test.moc"
