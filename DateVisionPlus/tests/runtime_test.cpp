#include "mainwindow.h"
#include "core/pluginmanager.h"
#include "flowchart/flowscene.h"
#include "flowchart/flowview.h"
#include "flowchart/nodeitem.h"
#include "panels/communicationpanel.h"
#include "panels/outputpanel.h"
#include "panels/toolboxpanel.h"
#include "communication/tcpserver.h"

#include <QAction>
#include <QDialog>
#include <QTableWidget>
#include <QTcpServer>
#include <QTcpSocket>
#include <QtTest>

#include <atomic>

namespace {
class ProbePlugin : public OVP::PluginBase
{
public:
    using PluginBase::PluginBase;
    QString id() const override { return QStringLiteral("runtime_probe"); }
    QString name() const override { return id(); }
    QString category() const override { return QStringLiteral("test"); }
    bool execute() override
    {
        ++calls;
        setOutput(QStringLiteral("value"), paramInt(QStringLiteral("value"), 42));
        return true;
    }
    QDialog *createDialog(const cv::Mat &, QWidget *parent) override
    {
        ++dialogs;
        auto *dialog = new QDialog(parent);
        QTimer::singleShot(0, dialog, [parent, dialog] {
            QMetaObject::invokeMethod(parent, "onCommTrigger", Qt::DirectConnection);
            dialog->accept();
        });
        return dialog;
    }
    static std::atomic<int> calls;
    static int dialogs;
};
std::atomic<int> ProbePlugin::calls{0};
int ProbePlugin::dialogs = 0;

OVP::NodeItem *addProbe(MainWindow &window, const QString &id)
{
    OVP::PluginManager::instance().registerFactory(&OVP::createPluginInstance<ProbePlugin>);
    return window.findChild<FlowView *>()->flowScene()->addNode(
        QStringLiteral("runtime_probe"), id, QPointF());
}

QTableWidget *resultTable(MainWindow &window)
{
    return window.findChild<OutputPanel *>()->findChild<QTableWidget *>();
}
}

class RuntimeTest : public QObject
{
    Q_OBJECT
private slots:
    void init()
    {
        ProbePlugin::calls = 0;
        ProbePlugin::dialogs = 0;
        // Tests must not load user DLLs or access camera/PLC hardware.
        OVP::PluginManager::instance().setPluginDirs({});
    }

    void busyGuardsAndGraphInvalidation()
    {
        MainWindow window;
        auto *node = addProbe(window, QStringLiteral("probe"));
        QVERIFY(node);
        auto *view = window.findChild<FlowView *>();
        auto *run = window.findChild<QAction *>(QStringLiteral("actRun"));
        auto *save = window.findChild<QAction *>(QStringLiteral("actSave"));
        QVERIFY(run && save);
        node->setSelected(true);

        run->trigger();
        QVERIFY(!run->isEnabled());
        QVERIFY(!save->isEnabled());
        QVERIFY(!view->isInteractive());
        QVERIFY(!view->acceptDrops());
        QVERIFY(!window.findChild<ToolboxPanel *>()->isEnabled());
        QVERIFY(QMetaObject::invokeMethod(&window, "onDelete", Qt::DirectConnection));
        QVERIFY(QMetaObject::invokeMethod(&window, "onNodeDoubleClicked", Qt::DirectConnection,
                                         Q_ARG(OVP::NodeItem *, node)));
        QCOMPARE(view->flowScene()->nodes().size(), 1);
        QCOMPARE(ProbePlugin::dialogs, 0);
        QTRY_VERIFY(run->isEnabled());
        QCOMPARE(node->status(), 1);
        QCOMPARE(resultTable(window)->item(0, 2)->text(), QStringLiteral("42"));
        QVERIFY(view->isInteractive() && view->acceptDrops() && save->isEnabled());

        // Parameter changes are visible without rebuilding the graph.
        node->plugin()->setParam(QStringLiteral("value"), 43);
        run->trigger();
        QTRY_VERIFY(run->isEnabled());
        QCOMPARE(resultTable(window)->item(0, 2)->text(), QStringLiteral("43"));

        auto *second = addProbe(window, QStringLiteral("second"));
        QVERIFY(second);
        second->plugin()->setParam(QStringLiteral("value"), 7);
        run->trigger();
        QTRY_VERIFY(run->isEnabled());
        QCOMPARE(resultTable(window)->rowCount(), 2);
        QCOMPARE(resultTable(window)->item(1, 2)->text(), QStringLiteral("7"));

        view->flowScene()->removeNode(QStringLiteral("probe"));
        run->trigger();
        QTRY_VERIFY(run->isEnabled());
        QCOMPARE(resultTable(window)->rowCount(), 1);
        QCOMPARE(resultTable(window)->item(0, 0)->text(), QStringLiteral("second"));
    }

    void modalEditorRejectsRemoteExecution()
    {
        MainWindow window;
        auto *node = addProbe(window, QStringLiteral("probe"));
        QVERIFY(node);
        QVERIFY(QMetaObject::invokeMethod(&window, "onNodeDoubleClicked", Qt::DirectConnection,
                                         Q_ARG(OVP::NodeItem *, node)));
        QCoreApplication::processEvents();
        QCOMPARE(ProbePlugin::dialogs, 1);
        QCOMPARE(ProbePlugin::calls.load(), 0);
        QVERIFY(window.findChild<QAction *>(QStringLiteral("actRun"))->isEnabled());
    }

    void continuousStopPresentsFinalResult()
    {
        MainWindow window;
        QVERIFY(addProbe(window, QStringLiteral("probe")));
        auto *continuous = window.findChild<QAction *>(QStringLiteral("actContinuous"));
        auto *run = window.findChild<QAction *>(QStringLiteral("actRun"));
        continuous->setChecked(true);
        QTRY_VERIFY(ProbePlugin::calls.load() >= 5);
        QVERIFY(QMetaObject::invokeMethod(&window, "onStop", Qt::DirectConnection));
        QTRY_VERIFY(run->isEnabled());
        QVERIFY(!continuous->isChecked());
        const int finishedCalls = ProbePlugin::calls.load();
        QTest::qWait(50);
        QCOMPARE(ProbePlugin::calls.load(), finishedCalls);
        // Stop is cooperative; if it arrived before the queued run started,
        // that run correctly has no outputs. The next single run still works.
        run->trigger();
        QTRY_VERIFY(run->isEnabled());
        QCOMPARE(resultTable(window)->item(0, 2)->text(), QStringLiteral("42"));
    }

    void remoteTriggerKeepsFormattedResponse()
    {
        MainWindow window;
        QVERIFY(addProbe(window, QStringLiteral("probe")));
        auto *panel = window.findChild<CommunicationPanel *>();
        panel->setConfig({{QStringLiteral("output_format"), QStringLiteral("{probe.value} * 2")},
                          {QStringLiteral("delimiter"), QStringLiteral(",")}});
        auto *server = panel->findChild<TcpServer *>();
        QVERIFY(server);
        server->configure(0, QStringLiteral("TRIGGER"));
        QVERIFY(server->startServer());
        QTcpSocket client;
        client.connectToHost(QHostAddress::LocalHost, server->findChild<QTcpServer *>()->serverPort());
        QTRY_COMPARE(client.state(), QAbstractSocket::ConnectedState);
        client.write("TRIGGER");
        QTRY_VERIFY(client.canReadLine());
        QCOMPARE(client.readLine(), QByteArray("OK,84\n"));
        client.disconnectFromHost();
        QTRY_COMPARE(client.state(), QAbstractSocket::UnconnectedState);
        QCoreApplication::processEvents();
        server->stopServer();
    }
};

QTEST_MAIN(RuntimeTest)
#include "runtime_test.moc"
