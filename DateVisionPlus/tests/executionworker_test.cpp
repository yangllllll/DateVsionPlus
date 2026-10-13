#include "flowchart/executionworker.h"

#include <QSemaphore>
#include <QSignalSpy>
#include <QThread>
#include <QtTest>

#include <atomic>

namespace {
class CountingPlugin : public OVP::PluginBase
{
public:
    explicit CountingPlugin(int value = 0) : m_value(value) {}

    QString id() const override { return QStringLiteral("counting"); }
    QString name() const override { return id(); }
    QString category() const override { return QStringLiteral("test"); }

    void reset() override
    {
        ++resets;
        PluginBase::reset();
    }

    bool execute() override
    {
        ++executions;
        setOutput(QStringLiteral("output"), input(QStringLiteral("input")).toInt() + m_value);
        return true;
    }

    void setValue(int value) { m_value = value; }
    std::atomic<int> resets{0};
    std::atomic<int> executions{0};

private:
    int m_value;
};

class BlockingPlugin : public CountingPlugin
{
public:
    bool execute() override
    {
        entered.release();
        if (!resume.tryAcquire(1, 5000))
            return false;
        return CountingPlugin::execute();
    }

    QSemaphore entered;
    QSemaphore resume;
};

class WorkerThread
{
public:
    WorkerThread()
    {
        worker->moveToThread(&thread);
        QObject::connect(&thread, &QThread::finished, worker, &QObject::deleteLater);
        thread.start();
    }

    ~WorkerThread()
    {
        worker->requestStop();
        thread.quit();
        thread.wait();
    }

    bool enqueueRun()
    {
        return QMetaObject::invokeMethod(worker, &OVP::ExecutionWorker::run, Qt::QueuedConnection);
    }

    QThread thread;
    OVP::ExecutionWorker *worker = new OVP::ExecutionWorker;
};

QMap<QString, QVariant> resultsAt(const QSignalSpy &spy, int index)
{
    return qvariant_cast<QMap<QString, QVariant>>(spy.at(index).at(0));
}
}

class ExecutionWorkerTest : public QObject
{
    Q_OBJECT

private slots:
    void queuedStopIsPreservedAndNextRunResetsIt();
    void cachedGraphSeesValuesAndReplacementBindings();
    void continuousRunsSuppressPerNodeSignals();
    void stopDuringExecutionRejectsAnotherReservation();
};

void ExecutionWorkerTest::queuedStopIsPreservedAndNextRunResetsIt()
{
    CountingPlugin plugin(7);
    QSemaphore entered;
    QSemaphore resume;
    WorkerThread runner;
    auto *worker = runner.worker;
    QSignalSpy finished(worker, &OVP::ExecutionWorker::runFinished);
    QVERIFY(finished.isValid());
    worker->setGraph({{QStringLiteral("node"), &plugin}}, {});

    // Hold the event queue until the task is reserved and cancelled on the caller thread.
    QVERIFY(QMetaObject::invokeMethod(worker, [&] {
        entered.release();
        resume.tryAcquire(1, 5000);
    }, Qt::QueuedConnection));
    QVERIFY(entered.tryAcquire(1, 5000));
    QVERIFY(worker->prepareRun());
    QVERIFY(worker->isBusy());
    QVERIFY(!worker->prepareRun());
    QVERIFY(runner.enqueueRun());
    worker->requestStop();
    resume.release();

    QTRY_COMPARE(finished.count(), 1);
    QVERIFY(!worker->isBusy());
    QCOMPARE(plugin.resets.load(), 0);
    QCOMPARE(plugin.executions.load(), 0);
    QVERIFY(resultsAt(finished, 0).isEmpty());

    QVERIFY(worker->prepareRun());
    QVERIFY(runner.enqueueRun());
    QTRY_COMPARE(finished.count(), 2);
    QVERIFY(!worker->isBusy());
    QCOMPARE(plugin.executions.load(), 1);
    QCOMPARE(resultsAt(finished, 1).value(QStringLiteral("node")).toMap()
                 .value(QStringLiteral("output")).toInt(), 7);
}

void ExecutionWorkerTest::cachedGraphSeesValuesAndReplacementBindings()
{
    CountingPlugin source(7);
    CountingPlugin replacement(20);
    CountingPlugin sink(5);
    WorkerThread runner;
    auto *worker = runner.worker;
    QSignalSpy finished(worker, &OVP::ExecutionWorker::runFinished);
    const QList<OVP::LinkDef> links{{QStringLiteral("source"), QStringLiteral("output"),
                                   QStringLiteral("sink"), QStringLiteral("input")}};
    worker->setGraph({{QStringLiteral("source"), &source},
                      {QStringLiteral("sink"), &sink}}, links);
    QVERIFY(worker->prepareRun());
    QVERIFY(runner.enqueueRun());
    QTRY_COMPARE(finished.count(), 1);
    QCOMPARE(resultsAt(finished, 0).value(QStringLiteral("sink")).toMap()
                 .value(QStringLiteral("output")).toInt(), 12);

    source.setValue(9);
    QVERIFY(worker->prepareRun());
    QVERIFY(runner.enqueueRun());
    QTRY_COMPARE(finished.count(), 2);
    QCOMPARE(resultsAt(finished, 1).value(QStringLiteral("sink")).toMap()
                 .value(QStringLiteral("output")).toInt(), 14);

    // Keep the same graph IDs and edges, but replace a plugin object.
    worker->setGraph({{QStringLiteral("source"), &replacement},
                      {QStringLiteral("sink"), &sink}}, links);
    QVERIFY(worker->prepareRun());
    QVERIFY(runner.enqueueRun());
    QTRY_COMPARE(finished.count(), 3);
    QCOMPARE(resultsAt(finished, 2).value(QStringLiteral("sink")).toMap()
                 .value(QStringLiteral("output")).toInt(), 25);
    QCOMPARE(source.executions.load(), 2);
    QCOMPARE(replacement.executions.load(), 1);

    worker->setGraph({{QStringLiteral("replacement"), &replacement}}, {});
    QVERIFY(worker->prepareRun());
    QVERIFY(runner.enqueueRun());
    QTRY_COMPARE(finished.count(), 4);
    QCOMPARE(resultsAt(finished, 3).keys(), QStringList{QStringLiteral("replacement")});
    QCOMPARE(sink.executions.load(), 3);
}

void ExecutionWorkerTest::continuousRunsSuppressPerNodeSignals()
{
    CountingPlugin plugin;
    WorkerThread runner;
    auto *worker = runner.worker;
    QSignalSpy started(worker, &OVP::ExecutionWorker::runStarted);
    QSignalSpy nodes(worker, &OVP::ExecutionWorker::nodeFinished);
    QSignalSpy progress(worker, &OVP::ExecutionWorker::progressChanged);
    QSignalSpy finished(worker, &OVP::ExecutionWorker::runFinished);
    worker->setGraph({{QStringLiteral("node"), &plugin}}, {});

    QVERIFY(worker->prepareRun(false));
    QVERIFY(runner.enqueueRun());
    QTRY_COMPARE(finished.count(), 1);
    QCOMPARE(started.count(), 0);
    QCOMPARE(nodes.count(), 0);
    QCOMPARE(progress.count(), 0);
    QCOMPARE(plugin.executions.load(), 1);

    QVERIFY(worker->prepareRun(true));
    QVERIFY(runner.enqueueRun());
    QTRY_COMPARE(finished.count(), 2);
    QCOMPARE(started.count(), 1);
    QCOMPARE(nodes.count(), 1);
    QCOMPARE(progress.count(), 1);
    QCOMPARE(progress.at(0).at(0).toInt(), 1);
    QCOMPARE(progress.at(0).at(1).toInt(), 1);
}

void ExecutionWorkerTest::stopDuringExecutionRejectsAnotherReservation()
{
    BlockingPlugin first;
    CountingPlugin second;
    WorkerThread runner;
    auto *worker = runner.worker;
    QSignalSpy finished(worker, &OVP::ExecutionWorker::runFinished);
    worker->setGraph({{QStringLiteral("first"), &first},
                      {QStringLiteral("second"), &second}},
                     {{QStringLiteral("first"), QStringLiteral("output"),
                       QStringLiteral("second"), QStringLiteral("input")}});
    QVERIFY(worker->prepareRun());
    QVERIFY(runner.enqueueRun());
    QVERIFY(first.entered.tryAcquire(1, 5000));
    QVERIFY(worker->isBusy());
    QVERIFY(!worker->prepareRun(false));
    worker->requestStop();
    first.resume.release();

    QTRY_COMPARE(finished.count(), 1);
    QVERIFY(!worker->isBusy());
    QCOMPARE(first.executions.load(), 1);
    QCOMPARE(second.executions.load(), 0);
    QVERIFY(resultsAt(finished, 0).contains(QStringLiteral("first")));
    QVERIFY(!resultsAt(finished, 0).contains(QStringLiteral("second")));
}

QTEST_GUILESS_MAIN(ExecutionWorkerTest)
#include "executionworker_test.moc"
