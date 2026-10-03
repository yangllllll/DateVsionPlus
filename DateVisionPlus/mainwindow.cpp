#include "mainwindow.h"
#include "ui_mainwindow.h"

#include "core/expressionevaluator.h"
#include "core/pluginmanager.h"
#include "core/plugintypes.h"
#include "dialogs/linefinderdialog.h"
#include "dialogs/patternmatchdialog.h"
#include "flowchart/connectionitem.h"
#include "flowchart/executionworker.h"
#include "flowchart/flowscene.h"
#include "flowchart/flowview.h"
#include "flowchart/nodeitem.h"
#include "flowchart/portitem.h"
#include "core/pluginbase.h"
#include "panels/communicationpanel.h"
#include "panels/outputpanel.h"
#include "panels/previewpanel.h"
#include "panels/propertiespanel.h"
#include "panels/toolboxpanel.h"
#include "plugins/linefinderplugin.h"
#include "plugins/patternmatchplugin.h"

#include <QDialog>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMessageBox>
#include <QMetaType>
#include <QSaveFile>
#include <QThread>
#include <QTimer>

#include <cmath>

using namespace OVP;

namespace {
const char *kProjectFilter = "DateVisionPlus 项目文件 (*.dvp);;JSON 文件 (*.json);;所有文件 (*.*)";

/**
 * 连续运行模式下「上一轮结束」到「下一轮开始」之间的固定等待（毫秒）。
 * 0 表示不做任何等待：本轮在后台线程跑完、结果回到界面后立刻排队下一轮。
 * 该定时器是单发的（由执行完成回调重新排期），所以 0ms 也不会空转拖垮界面：
 * Qt 只在事件队列没有其他待处理事件时才投递 timeout。
 */
constexpr int kContinuousIdleMs = 0;

QVariantMap mapValue(const QVariant &value)
{
    if (value.isValid() && value.metaType().id() == QMetaType::QVariantMap)
        return value.toMap();
    return QVariantMap();
}
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    m_scene = ui->flowchartView->flowScene();

    reloadPlugins();

    setupWorker();

    // 单发 + 0ms：每轮执行结束后由回调重新排期，靠串行执行而非固定节拍推进
    m_continuousTimer = new QTimer(this);
    m_continuousTimer->setSingleShot(true);
    m_continuousTimer->setInterval(kContinuousIdleMs);
    connect(m_continuousTimer, &QTimer::timeout, this, &MainWindow::doExecute);

    setupConnections();
    ui->statusbar->showMessage(QStringLiteral("就绪"));
}

MainWindow::~MainWindow()
{
    if (m_workerThread) {
        m_worker->requestStop();
        m_workerThread->quit();
        m_workerThread->wait();
        delete m_worker;
        m_worker = nullptr;
    }
    delete ui;
}

void MainWindow::setupWorker()
{
    m_workerThread = new QThread(this);
    m_worker = new OVP::ExecutionWorker;
    m_worker->moveToThread(m_workerThread);

    connect(m_worker, &OVP::ExecutionWorker::runStarted, this, &MainWindow::onExecutionStarted);
    connect(m_worker, &OVP::ExecutionWorker::nodeFinished, this, &MainWindow::onNodeStatusChanged);
    connect(m_worker, &OVP::ExecutionWorker::progressChanged, this, &MainWindow::onExecutionProgress);
    connect(m_worker, &OVP::ExecutionWorker::runFinished, this, &MainWindow::onExecutionFinished);

    m_workerThread->start();
}

void MainWindow::setupConnections()
{
    connect(ui->actNew, &QAction::triggered, this, &MainWindow::onNew);
    connect(ui->actOpen, &QAction::triggered, this, &MainWindow::onOpen);
    connect(ui->actSave, &QAction::triggered, this, &MainWindow::onSave);
    connect(ui->actSaveAs, &QAction::triggered, this, &MainWindow::onSaveAs);
    connect(ui->actExit, &QAction::triggered, this, &MainWindow::close);
    connect(ui->actRun, &QAction::triggered, this, &MainWindow::onRun);
    connect(ui->actContinuous, &QAction::toggled, this, &MainWindow::onToggleContinuous);
    connect(ui->actStop, &QAction::triggered, this, &MainWindow::onStop);
    connect(ui->actDelete, &QAction::triggered, this, &MainWindow::onDelete);
    connect(ui->actZoomIn, &QAction::triggered, this, &MainWindow::onZoomIn);
    connect(ui->actZoomOut, &QAction::triggered, this, &MainWindow::onZoomOut);
    connect(ui->actFit, &QAction::triggered, this, &MainWindow::onFit);
    connect(ui->actAbout, &QAction::triggered, this, &MainWindow::onAbout);
    connect(ui->actRefreshPlugins, &QAction::triggered, this, &MainWindow::onRefreshPlugins);

    connect(ui->toolboxPanel, &ToolboxPanel::pluginDoubleClicked, this, &MainWindow::addNodeToCenter);

    connect(m_scene, &FlowScene::nodeAdded, this, &MainWindow::onNodeAdded);
    connect(m_scene, &FlowScene::nodeSelected, this, &MainWindow::onNodeSelected);
    connect(m_scene, &FlowScene::nodeDeselected, this, &MainWindow::onNodeDeselected);

    connect(ui->communicationPanel, &CommunicationPanel::executeRequested, this,
            &MainWindow::onCommTrigger);
}

// ------------------------------------------------------------------ 文件操作

void MainWindow::onNew()
{
    const int reply = QMessageBox::question(this, QStringLiteral("新建"),
                                            QStringLiteral("确定要新建吗？当前流程图将被清除。"),
                                            QMessageBox::Yes | QMessageBox::No);
    if (reply != QMessageBox::Yes)
        return;

    m_scene->clearAll();
    ui->previewPanel->clearImage();
    ui->outputPanel->clearLog();
    m_currentFile.clear();
    setWindowTitle(QStringLiteral("DateVisionPlus - 工业视觉检测平台"));
    ui->statusbar->showMessage(QStringLiteral("新建流程图"));
}

void MainWindow::onOpen()
{
    const QString filePath = QFileDialog::getOpenFileName(
        this, QStringLiteral("打开项目"), QString(), QString::fromUtf8(kProjectFilter));
    if (filePath.isEmpty())
        return;

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        QMessageBox::critical(this, QStringLiteral("打开失败"),
                              QStringLiteral("无法读取文件：\n%1").arg(filePath));
        return;
    }

    const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
    file.close();

    if (!document.isObject()) {
        QMessageBox::critical(this, QStringLiteral("打开失败"), QStringLiteral("项目文件格式不正确"));
        return;
    }

    const QJsonObject root = document.object();
    m_scene->fromJson(root);
    ui->previewPanel->clearImage();
    ui->outputPanel->clearLog();
    ui->propertiesPanel->setNode(nullptr);

    if (root.contains(QStringLiteral("communication")))
        ui->communicationPanel->setConfig(root.value(QStringLiteral("communication")).toObject());

    m_currentFile = filePath;
    setWindowTitle(QStringLiteral("OpenVisionPlus - %1").arg(QFileInfo(filePath).fileName()));
    ui->outputPanel->logSuccess(QStringLiteral("已打开: %1").arg(filePath));
    ui->statusbar->showMessage(QStringLiteral("已加载: %1").arg(QFileInfo(filePath).fileName()));
}

void MainWindow::onSave()
{
    if (m_currentFile.isEmpty()) {
        onSaveAs();
        return;
    }
    saveToFile(m_currentFile);
}

void MainWindow::onSaveAs()
{
    const QString filePath = QFileDialog::getSaveFileName(
        this, QStringLiteral("保存项目"), QStringLiteral("untitled.dvp"),
        QString::fromUtf8(kProjectFilter));
    if (filePath.isEmpty())
        return;
    saveToFile(filePath);
}

void MainWindow::saveToFile(const QString &filePath)
{
    QJsonObject root = m_scene->toJson();
    root.insert(QStringLiteral("communication"), ui->communicationPanel->config());

    QSaveFile file(filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        QMessageBox::critical(this, QStringLiteral("保存失败"),
                              QStringLiteral("无法写入文件：\n%1").arg(filePath));
        return;
    }
    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    if (!file.commit()) {
        QMessageBox::critical(this, QStringLiteral("保存失败"), QStringLiteral("写入文件失败"));
        return;
    }

    m_currentFile = filePath;
    setWindowTitle(QStringLiteral("OpenVisionPlus - %1").arg(QFileInfo(filePath).fileName()));
    ui->outputPanel->logSuccess(QStringLiteral("已保存: %1").arg(filePath));
    ui->statusbar->showMessage(QStringLiteral("已保存: %1").arg(QFileInfo(filePath).fileName()));
}

// ------------------------------------------------------------------ 运行控制

void MainWindow::onRun()
{
    ui->outputPanel->logInfo(QStringLiteral("开始执行流程图..."));
    doExecute();
}

void MainWindow::onToggleContinuous(bool checked)
{
    m_continuousMode = checked;

    if (checked) {
        m_continuousTimer->start();
        ui->outputPanel->logInfo(QStringLiteral("已开启连续运行模式（上一轮结束后立即执行下一轮）"));
        ui->statusbar->showMessage(QStringLiteral("连续运行中..."));
    } else {
        m_continuousTimer->stop();
        ui->outputPanel->logInfo(QStringLiteral("已关闭连续运行模式"));
        ui->statusbar->showMessage(QStringLiteral("已切换为单次触发"));
    }
    updateBusyUi();
}

void MainWindow::onStop()
{
    m_worker->requestStop();
    // setChecked(false) 会触发 toggled 信号，从而调用 onToggleContinuous(false)
    if (m_continuousMode)
        ui->actContinuous->setChecked(false);
    ui->outputPanel->logWarning(QStringLiteral("执行已停止"));
    ui->statusbar->showMessage(QStringLiteral("已停止"));
    updateBusyUi();
}

void MainWindow::onDelete()
{
    m_scene->removeSelected();
    ui->propertiesPanel->setNode(nullptr);
}

void MainWindow::onZoomIn()
{
    ui->flowchartView->zoomIn();
}

void MainWindow::onZoomOut()
{
    ui->flowchartView->zoomOut();
}

void MainWindow::onFit()
{
    ui->flowchartView->zoomFit();
}

void MainWindow::onAbout()
{
    QMessageBox::about(this, QStringLiteral("关于 DateVisionPlus"),
                       QStringLiteral("<h3>DateVisionPlus 工业视觉检测平台 v2.0.0</h3>"
                                      "<p>基于 Qt6 + OpenCV (C++) 构建</p>"
                                      "<p>开源免费，无授权限制</p>"
                                      "<hr>"
                                      "<p><b>开发者：</b>杨佳祺</p>"
                                      "<p><b>联系电话：</b>15803820398</p>"
                                      "<p><b>联系邮箱：</b>yangjiaqi@datekj.top</p>"
                                      "<p><b>版权所有：</b>郑州德塔工业自动化</p>"));
}

void MainWindow::onRefreshPlugins()
{
    reloadPlugins();
}

void MainWindow::reloadPlugins()
{
    PluginManager &manager = PluginManager::instance();
    manager.reloadAll();
    ui->toolboxPanel->refresh();

    // 输出用户插件 DLL 加载明细
    const QList<PluginManager::UserPluginRecord> records = manager.userPluginRecords();
    int okCount = 0;
    for (const PluginManager::UserPluginRecord &record : records) {
        const QString fileName = QFileInfo(record.filePath).fileName();
        if (record.loaded) {
            ++okCount;
            ui->outputPanel->logSuccess(QStringLiteral("用户插件 [%1] %2: %3")
                                            .arg(fileName, record.message,
                                                 record.pluginIds.join(QStringLiteral(", "))));
        } else {
            ui->outputPanel->logWarning(QStringLiteral("跳过动态库 [%1]: %2").arg(fileName, record.message));
        }
    }

    if (records.isEmpty()) {
        ui->outputPanel->logInfo(QStringLiteral("未在插件目录发现用户插件动态库: %1")
                                     .arg(manager.pluginDirs().join(QStringLiteral("; "))));
    } else {
        ui->outputPanel->logInfo(QStringLiteral("用户插件加载完成: %1/%2 个动态库可用")
                                     .arg(okCount)
                                     .arg(records.size()));
    }
}

void MainWindow::onCommTrigger()
{
    ui->outputPanel->logInfo(QStringLiteral("TCP通信触发检测..."));
    // 执行已改为异步，标志位在 onExecutionFinished() 中复位
    m_commTriggered = true;
    doExecute();
}

// ------------------------------------------------------------------ 节点交互

void MainWindow::addNodeToCenter(const QString &pluginId)
{
    const QPointF center = ui->flowchartView->mapToScene(
        ui->flowchartView->viewport()->rect().center());
    const int offset = m_scene->nodes().size() * 30;
    m_scene->addPluginNode(pluginId, QPointF(center.x() - 70 + offset, center.y() - 50 + offset));
    ui->statusbar->showMessage(QStringLiteral("已添加工具: %1").arg(pluginId));
}

void MainWindow::onNodeAdded(NodeItem *node)
{
    ui->outputPanel->logInfo(QStringLiteral("添加节点: %1 [%2]").arg(node->pluginName(), node->nodeId()));
    connect(node, &NodeItem::nodeDoubleClicked, this, &MainWindow::onNodeDoubleClicked);
}

void MainWindow::onNodeSelected(NodeItem *node)
{
    ui->propertiesPanel->setNode(node);
}

void MainWindow::onNodeDeselected()
{
    ui->propertiesPanel->setNode(nullptr);
}

void MainWindow::onNodeDoubleClicked(NodeItem *node)
{
    if (!node || !node->plugin())
        return;

    // 对话框由插件自己创建（内置工具与用户插件 DLL 走同一套机制）
    const cv::Mat image = inputImageForNode(node);
    QDialog *dialog = node->plugin()->createDialog(image, this);
    if (!dialog) {
        ui->outputPanel->logInfo(QStringLiteral("节点 [%1] 没有专用编辑界面").arg(node->pluginName()));
        return;
    }

    dialog->setAttribute(Qt::WA_DeleteOnClose);
    if (dialog->exec() == QDialog::Accepted) {
        ui->outputPanel->logInfo(QStringLiteral("节点 [%1] 编辑完成").arg(node->pluginName()));
        ui->statusbar->showMessage(QStringLiteral("%1 参数已保存").arg(node->pluginName()));
    }
}

void MainWindow::executeUpstream(NodeItem *node, QSet<QString> &executed)
{
    if (!node || !node->plugin() || executed.contains(node->nodeId()))
        return;

    node->plugin()->reset();

    for (ConnectionItem *conn : m_scene->connections()) {
        if (!conn->targetPort() || !conn->sourcePort())
            continue;
        if (conn->targetPort()->node() != node)
            continue;

        NodeItem *source = conn->sourcePort()->node();
        if (!source || !source->plugin())
            continue;

        executeUpstream(source, executed);
        const QVariant value = source->plugin()->output(conn->sourcePort()->portName());
        if (value.isValid())
            node->plugin()->setInput(conn->targetPort()->portName(), value);
    }

    node->plugin()->execute();
    executed.insert(node->nodeId());
}

cv::Mat MainWindow::inputImageForNode(NodeItem *node)
{
    QSet<QString> executed;
    for (ConnectionItem *conn : m_scene->connections()) {
        if (!conn->targetPort() || !conn->sourcePort())
            continue;
        if (conn->targetPort()->node() != node)
            continue;

        NodeItem *source = conn->sourcePort()->node();
        if (!source || !source->plugin())
            continue;

        executeUpstream(source, executed);
        const QVariantMap outputs = source->plugin()->outputs();
        for (auto it = outputs.constBegin(); it != outputs.constEnd(); ++it) {
            const cv::Mat image = toMat(it.value());
            if (!image.empty())
                return image;
        }
    }
    return cv::Mat();
}

// ------------------------------------------------------------------ 执行

void MainWindow::collectGraph(QMap<QString, PluginBase *> &plugins, QList<LinkDef> &links) const
{
    const QMap<QString, NodeItem *> nodes = m_scene->nodes();
    plugins.clear();
    links.clear();

    for (auto it = nodes.constBegin(); it != nodes.constEnd(); ++it) {
        if (PluginBase *plugin = it.value()->plugin())
            plugins.insert(it.key(), plugin);
    }

    for (ConnectionItem *conn : m_scene->connections()) {
        if (!conn->sourcePort() || !conn->targetPort())
            continue;
        if (!conn->sourcePort()->node() || !conn->targetPort()->node())
            continue;

        LinkDef link;
        link.sourceNode = conn->sourcePort()->node()->nodeId();
        link.sourcePort = conn->sourcePort()->portName();
        link.targetNode = conn->targetPort()->node()->nodeId();
        link.targetPort = conn->targetPort()->portName();
        links.append(link);
    }
}

void MainWindow::updateBusyUi()
{
    const bool editable = !m_executing && !m_continuousMode;

    // 只在状态真正变化时写一遍属性：setEnabled() 会递归作用于整棵子控件树，
    // 连续运行每轮都调用会白白开销两遍遍历（初始状态与 .ui 一致，即 enabled）
    if (editable == m_editableUi)
        return;
    m_editableUi = editable;

    ui->actRun->setEnabled(editable);
    ui->actNew->setEnabled(editable);
    ui->actOpen->setEnabled(editable);
    ui->actDelete->setEnabled(editable);
    ui->actRefreshPlugins->setEnabled(editable);
    ui->propertiesPanel->setEnabled(editable);
}

void MainWindow::doExecute()
{
    // 连续运行的下一轮由执行完成回调统一排期，先取消待触发的那次，避免重复投递
    m_continuousTimer->stop();

    // 上一轮尚未结束：直接跳过本次触发，避免事件与任务堆积。
    // 必须放在采集图数据之前 —— 0ms 排期下这里的调用频率很高。
    if (m_executing) {
        if (m_commTriggered) {
            ui->communicationPanel->setResponseData(QStringLiteral("ERROR:BUSY"));
            m_commTriggered = false;
        }
        return;
    }

    QMap<QString, PluginBase *> plugins;
    QList<LinkDef> links;
    collectGraph(plugins, links);

    if (plugins.isEmpty()) {
        ui->outputPanel->logWarning(QStringLiteral("流程图为空，无法执行"));
        if (m_commTriggered) {
            ui->communicationPanel->setResponseData(QStringLiteral("ERROR:NO_NODES"));
            m_commTriggered = false;
        }
        return;
    }

    m_executing = true;
    updateBusyUi();

    m_worker->setGraph(plugins, links);
    QMetaObject::invokeMethod(m_worker, &OVP::ExecutionWorker::run, Qt::QueuedConnection);
}

void MainWindow::onExecutionStarted(int total)
{
    // 统一清除指示灯，避免每个节点单独触发一次重绘（已废弃）
    const QMap<QString, NodeItem *> nodes = m_scene->nodes();
  //  for (auto it = nodes.constBegin(); it != nodes.constEnd(); ++it)
  //      it.value()->setStatus(-1);

    ui->statusbar->showMessage(QStringLiteral("执行中... 共 %1 个节点").arg(total));
}

void MainWindow::onNodeStatusChanged(const QString &nodeId, int status)
{
    if (NodeItem *node = m_scene->node(nodeId))
        node->setStatus(status);
}

void MainWindow::onExecutionProgress(int finished, int total)
{
    ui->statusbar->showMessage(QStringLiteral("执行中... %1/%2").arg(finished).arg(total));
}

void MainWindow::onExecutionFinished(const QMap<QString, QVariant> &results,
                                     const QStringList &order)
{
    m_executing = false;
    updateBusyUi();

    const QString delimiter = ui->communicationPanel->delimiter();

    QStringList statusParts;
    statusParts.reserve(order.size());
    for (const QString &nodeId : order) {
        const QVariant value = results.value(nodeId);
        const bool ok = value.isValid() && value.metaType().id() == QMetaType::QVariantMap;
        statusParts.append(ok ? QStringLiteral("OK") : QStringLiteral("NG"));
    }
    const QString statusString = statusParts.isEmpty() ? QStringLiteral("NO_RESULT")
                                                       : statusParts.join(delimiter);

    // 输出内容
    const QString outputString = buildCommResponse(results);
    const QString response = outputString.isEmpty()
                                 ? statusString
                                 : QStringLiteral("%1%2%3").arg(statusString, delimiter, outputString);

    if (m_commTriggered) {
        ui->communicationPanel->setResponseData(response);
        ui->outputPanel->logInfo(QStringLiteral("通信响应: %1%2")
                                     .arg(response.left(200),
                                          response.length() > 200 ? QStringLiteral("...") : QString()));
        m_commTriggered = false;
    } else if (!results.contains(QStringLiteral("_error"))) {
        ui->outputPanel->logSuccess(statusString);
    }

    if (results.contains(QStringLiteral("_error")))
        ui->outputPanel->logError(results.value(QStringLiteral("_error")).toString());

    // 预览最后一个有图像输出的节点
    for (int i = order.size() - 1; i >= 0; --i) {
        const QVariantMap nodeOutputs = mapValue(results.value(order.at(i)));
        bool found = false;
        for (auto it = nodeOutputs.constBegin(); it != nodeOutputs.constEnd(); ++it) {
            const cv::Mat image = toMat(it.value());
            if (!image.empty()) {
                ui->previewPanel->setImage(image);
                found = true;
                break;
            }
        }
        if (found)
            break;
    }

    ui->outputPanel->updateResults(results);
    ui->statusbar->showMessage(QStringLiteral("执行完成：%1").arg(statusString));

    // 连续运行：本轮收尾后立刻排队下一轮，不再固定等待 100ms
    if (m_continuousMode)
        m_continuousTimer->start();
}

QString MainWindow::buildCommResponse(const QMap<QString, QVariant> &results)
{
    const QString format = ui->communicationPanel->outputFormat().trimmed();
    if (format.isEmpty())
        return QString();

    const QString delimiter = ui->communicationPanel->delimiter();

    QStringList outputParts;
    const QStringList lines = format.split(QLatin1Char('\n'));
    for (const QString &rawLine : lines) {
        const QString line = rawLine.trimmed();
        if (line.isEmpty())
            continue;
        outputParts.append(evalFormatLine(line, results));
    }

    return outputParts.join(delimiter);
}

QString MainWindow::evalFormatLine(const QString &line, const QMap<QString, QVariant> &results)
{
    const QString substituted = substituteReferences(line, [this, &results](const QString &ref) {
        const QStringList parts = ref.split(QLatin1Char('.'));
        if (parts.size() >= 2)
            return nodeOutputValue(parts.at(0).trimmed(), parts.at(1).trimmed(), results);
        return QStringLiteral("N/A");
    });

    double value = 0.0;
    if (evaluateArithmetic(substituted, value)) {
        if (std::abs(value - std::round(value)) < 1e-9)
            return QString::number(static_cast<qlonglong>(std::round(value)));
        return QString::number(value, 'f', 6);
    }
    return substituted;
}

QString MainWindow::nodeOutputValue(const QString &nodeId, const QString &portName,
                                    const QMap<QString, QVariant> &results)
{
    const QVariant nodeValue = results.value(nodeId);
    if (!nodeValue.isValid() || nodeValue.metaType().id() != QMetaType::QVariantMap)
        return QStringLiteral("N/A");

    const QVariant value = nodeValue.toMap().value(portName);
    if (!value.isValid())
        return QStringLiteral("N/A");

    switch (value.metaType().id()) {
    case QMetaType::Int:
    case QMetaType::UInt:
    case QMetaType::LongLong:
        return QString::number(value.toLongLong());
    case QMetaType::Double:
    case QMetaType::Float:
        return QString::number(value.toDouble(), 'f', 6);
    case QMetaType::Bool:
        return value.toBool() ? QStringLiteral("True") : QStringLiteral("False");
    case QMetaType::QVariantList:
        return QString::number(value.toList().size());
    default:
        return value.toString();
    }
}
