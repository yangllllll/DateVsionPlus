#include "flowchart/executionengine.h"

#include <QSignalSpy>
#include <QtTest>

#include <functional>
#include <memory>
#include <stdexcept>
#include <vector>

using namespace OVP;

class TestPlugin final : public PluginBase
{
public:
    QString id() const override { return QStringLiteral("test"); }
    QString name() const override { return QStringLiteral("Test"); }
    QString category() const override { return QStringLiteral("Tests"); }
    bool execute() override
    {
        ++executions;
        return action ? action(*this) : true;
    }
    void reset() override
    {
        ++resets;
        if (throwOnReset)
            throw std::runtime_error("reset failed");
        PluginBase::reset();
    }
    void fail(const QString &error) { setError(error); }

    std::function<bool(TestPlugin &)> action;
    int executions = 0;
    int resets = 0;
    bool throwOnReset = false;
};

class ExecutionEngineTest : public QObject
{
    Q_OBJECT

private slots:
    void emptyGraph();
    void deterministicTopology();
    void cyclesAndSetupInvalidation();
    void routesPortsAndResetsEachRun();
    void keepsDuplicatePortPrecedence();
    void ignoresDanglingLinksAndNullPlugins();
    void preservesSharedImageStorage();
    void queuedStopAndRestart();
    void stopDuringExecution();
    void failuresStillReportFinalProgress();
    void resetExceptionIsReported();
    void fixedGraphBenchmark_data();
    void fixedGraphBenchmark();
};

void ExecutionEngineTest::emptyGraph()
{
    ExecutionEngine engine;
    engine.setup({}, {});
    QVERIFY(engine.topologicalSort().isEmpty());
    QVERIFY(!engine.hasCycle());
    QVERIFY(engine.execute().isEmpty());
    QVERIFY(engine.lastError().isEmpty());
    QVERIFY(!engine.isRunning());
}

void ExecutionEngineTest::deterministicTopology()
{
    TestPlugin a, b, c, d, e;
    ExecutionEngine engine;
    engine.setup({{"a", &a}, {"b", &b}, {"c", &c}, {"d", &d}, {"e", &e}},
                 {{"a", "out", "d", "in"}, {"a", "out", "c", "in"},
                  {"b", "out", "c", "other"}, {"c", "out", "e", "in"},
                  {"d", "out", "e", "other"}});
    const QStringList expected{"a", "b", "d", "c", "e"};
    QCOMPARE(engine.topologicalSort(), expected);
    QCOMPARE(engine.topologicalSort(), expected);
    QVERIFY(!engine.hasCycle());
    QVERIFY(!engine.execute().contains("_error"));
}

void ExecutionEngineTest::cyclesAndSetupInvalidation()
{
    TestPlugin a, b;
    ExecutionEngine engine;
    const QMap<QString, PluginBase *> plugins{{"a", &a}, {"b", &b}};
    engine.setup(plugins, {{"a", "out", "b", "in"}, {"b", "out", "a", "in"}});
    for (int i = 0; i < 3; ++i) {
        QVERIFY(engine.hasCycle());
        QVERIFY(engine.topologicalSort().isEmpty());
        QVERIFY(engine.execute().contains("_error"));
    }
    QCOMPARE(a.executions, 0);
    QCOMPARE(b.executions, 0);
    QVERIFY(!engine.isRunning());

    engine.setup(plugins, {{"b", "out", "a", "in"}});
    QVERIFY(!engine.hasCycle());
    QCOMPARE(engine.topologicalSort(), (QStringList{"b", "a"}));
    QVERIFY(!engine.execute().contains("_error"));
    QVERIFY(engine.lastError().isEmpty());

    engine.setup({{"a", &a}}, {{"a", "out", "a", "in"}});
    QVERIFY(engine.hasCycle());
    engine.setup({}, {});
    QVERIFY(!engine.hasCycle());
    QVERIFY(engine.execute().isEmpty());
}

void ExecutionEngineTest::routesPortsAndResetsEachRun()
{
    TestPlugin source, target;
    source.setParam("value", 7);
    source.action = [](TestPlugin &plugin) {
        if (plugin.paramInt("value") >= 0)
            plugin.setOutput("number", plugin.paramInt("value"));
        plugin.setOutput("other", 99);
        return true;
    };
    target.action = [](TestPlugin &plugin) {
        plugin.setOutput("result", plugin.input("operand"));
        return true;
    };
    ExecutionEngine engine;
    engine.setup({{"source", &source}, {"target", &target}},
                 {{"source", "number", "target", "operand"}});
    engine.execute();
    QCOMPARE(engine.nodeResults("target").value("result").toInt(), 7);
    source.setParam("value", 42);
    engine.execute();
    QCOMPARE(engine.nodeResults("target").value("result").toInt(), 42);
    source.setParam("value", -1);
    engine.execute();
    QVERIFY(!target.input("operand").isValid());
    QVERIFY(!engine.nodeResults("target").value("result").isValid());
    QCOMPARE(source.resets, 3);
    QCOMPARE(target.resets, 3);

    engine.setup({{"source", &source}, {"target", &target}},
                 {{"source", "other", "target", "operand"}});
    engine.execute();
    QCOMPARE(engine.nodeResults("target").value("result").toInt(), 99);
}

void ExecutionEngineTest::keepsDuplicatePortPrecedence()
{
    TestPlugin source, target;
    source.action = [](TestPlugin &plugin) {
        plugin.setOutput("first", 1);
        plugin.setOutput("second", 2);
        return true;
    };
    ExecutionEngine engine;
    engine.setup({{"source", &source}, {"target", &target}},
                 {{"source", "first", "target", "in"},
                  {"source", "second", "target", "in"}});
    QVERIFY(!engine.execute().contains("_error"));
    QCOMPARE(target.input("in").toInt(), 1);
}

void ExecutionEngineTest::ignoresDanglingLinksAndNullPlugins()
{
    TestPlugin a, b;
    a.action = [](TestPlugin &plugin) {
        plugin.setOutput("out", 5);
        return true;
    };
    ExecutionEngine engine;
    engine.setup({{"a", &a}, {"b", &b}, {"null", nullptr}},
                 {{"missing", "out", "a", "in"}, {"a", "out", "missing", "in"},
                  {"a", "out", "b", "in"}, {"null", "out", "b", "other"}});
    QSignalSpy progress(&engine, &ExecutionEngine::progressChanged);
    QVERIFY(!engine.execute().contains("_error"));
    QCOMPARE(engine.results().size(), 2);
    QCOMPARE(b.input("in").toInt(), 5);
    QVERIFY(!b.input("other").isValid());
    QCOMPARE(progress.last().at(0).toInt(), 3);
    QCOMPARE(progress.last().at(1).toInt(), 3);
}

void ExecutionEngineTest::preservesSharedImageStorage()
{
    TestPlugin source, target;
    const cv::Mat image(8, 8, CV_8UC1, cv::Scalar(17));
    source.action = [&image](TestPlugin &plugin) {
        plugin.setOutput("image", QVariant::fromValue(image));
        return true;
    };
    ExecutionEngine engine;
    engine.setup({{"source", &source}, {"target", &target}},
                 {{"source", "image", "target", "image"}});
    engine.execute();
    QCOMPARE(target.inputImage("image").data, image.data);
    QCOMPARE(engine.nodeResults("source").value("image").value<cv::Mat>().data, image.data);
}

void ExecutionEngineTest::queuedStopAndRestart()
{
    TestPlugin plugin;
    ExecutionEngine engine;
    engine.setup({{"node", &plugin}}, {});
    engine.prepareRun();
    engine.requestStop();
    QVERIFY(engine.execute(false).isEmpty());
    QCOMPARE(plugin.executions, 0);
    QCOMPARE(plugin.resets, 0);
    QVERIFY(!engine.isRunning());

    engine.prepareRun();
    engine.execute(false);
    QCOMPARE(plugin.executions, 1);
    engine.requestStop();
    engine.execute(); // Independent callers retain the previous restart behavior.
    QCOMPARE(plugin.executions, 2);
}

void ExecutionEngineTest::stopDuringExecution()
{
    TestPlugin a, b;
    ExecutionEngine engine;
    a.action = [&engine](TestPlugin &) {
        engine.requestStop();
        return true;
    };
    engine.setup({{"a", &a}, {"b", &b}}, {{"a", "out", "b", "in"}});
    QSignalSpy progress(&engine, &ExecutionEngine::progressChanged);
    const auto results = engine.execute();
    QCOMPARE(a.executions, 1);
    QCOMPARE(b.executions, 0);
    QVERIFY(results.contains("a"));
    QVERIFY(!results.contains("b"));
    QCOMPARE(progress.size(), 1);
    QCOMPARE(progress.first().at(0).toInt(), 1);
    QCOMPARE(progress.first().at(1).toInt(), 2);
    QVERIFY(!engine.isRunning());
}

void ExecutionEngineTest::failuresStillReportFinalProgress()
{
    TestPlugin failed, thrown, unknown, passed;
    failed.action = [](TestPlugin &plugin) {
        plugin.fail("expected failure");
        return false;
    };
    thrown.action = [](TestPlugin &) -> bool { throw std::runtime_error("expected exception"); };
    unknown.action = [](TestPlugin &) -> bool { throw 1; };
    ExecutionEngine engine;
    engine.setup({{"a", &failed}, {"b", &passed}, {"y", &thrown}, {"z", &unknown}}, {});
    QSignalSpy finished(&engine, &ExecutionEngine::nodeFinished);
    QSignalSpy progress(&engine, &ExecutionEngine::progressChanged);
    const auto results = engine.execute();
    QCOMPARE(finished.size(), 4);
    QCOMPARE(finished.at(0).at(1).toInt(), 0);
    QCOMPARE(finished.at(1).at(1).toInt(), 1);
    QCOMPARE(finished.at(2).at(1).toInt(), 0);
    QCOMPARE(finished.at(3).at(1).toInt(), 0);
    QCOMPARE(results.value("a").toString(), QStringLiteral("expected failure"));
    QCOMPARE(results.value("y").toString(), QStringLiteral("expected exception"));
    QVERIFY(results.contains("_error"));
    QVERIFY(engine.nodeResults("a").isEmpty());
    QCOMPARE(progress.size(), 1);
    QCOMPARE(progress.first().at(0).toInt(), 4);
    QCOMPARE(progress.first().at(1).toInt(), 4);
    QVERIFY(!engine.isRunning());
}

void ExecutionEngineTest::resetExceptionIsReported()
{
    TestPlugin broken, downstream;
    broken.throwOnReset = true;
    ExecutionEngine engine;
    engine.setup({{"a", &broken}, {"b", &downstream}}, {{"a", "out", "b", "in"}});
    const auto results = engine.execute();
    QVERIFY(results.contains("_error"));
    QVERIFY(engine.lastError().contains("reset failed"));
    QCOMPARE(broken.executions, 0);
    QCOMPARE(downstream.executions, 0);
    QVERIFY(!engine.isRunning());
}

void ExecutionEngineTest::fixedGraphBenchmark_data()
{
    QTest::addColumn<int>("nodeCount");
    QTest::addColumn<bool>("rebuild");
    QTest::newRow("cached-128") << 128 << false;
    QTest::newRow("rebuild-128") << 128 << true;
    QTest::newRow("cached-512") << 512 << false;
    QTest::newRow("rebuild-512") << 512 << true;
}

void ExecutionEngineTest::fixedGraphBenchmark()
{
    QFETCH(int, nodeCount);
    QFETCH(bool, rebuild);
    std::vector<std::unique_ptr<TestPlugin>> owned;
    QMap<QString, PluginBase *> plugins;
    QList<LinkDef> links;
    auto nodeId = [](int index) { return QStringLiteral("node_%1").arg(index, 4, 10, QLatin1Char('0')); };
    for (int i = 0; i < nodeCount; ++i) {
        auto plugin = std::make_unique<TestPlugin>();
        plugin->action = [](TestPlugin &node) {
            node.setOutput("out", node.input("in").toInt() + 1);
            return true;
        };
        plugins.insert(nodeId(i), plugin.get());
        owned.push_back(std::move(plugin));
        if (i > 0)
            links.append({nodeId(i - 1), "out", nodeId(i), "in"});
    }
    ExecutionEngine engine;
    engine.setup(plugins, links);
    engine.execute();
    QCOMPARE(engine.nodeResults(nodeId(nodeCount - 1)).value("out").toInt(), nodeCount);
    QBENCHMARK {
        if (rebuild)
            engine.setup(plugins, links);
        engine.execute();
    }
    QCOMPARE(engine.nodeResults(nodeId(nodeCount - 1)).value("out").toInt(), nodeCount);
}

QTEST_GUILESS_MAIN(ExecutionEngineTest)
#include "executionengine_test.moc"
