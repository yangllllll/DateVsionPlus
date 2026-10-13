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
#include <QFontMetrics>
#include <QHBoxLayout>
#include <QKeySequence>
#include <QLabel>
#include <QMenu>
#include <QPainter>
#include <QSplitter>
#include <QStyle>
#include <QToolButton>
#include <QVBoxLayout>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMessageBox>
#include <QMetaType>
#include <QSaveFile>
#include <QScopedValueRollback>
#include <QThread>
#include <QTimer>

#include <cmath>

using namespace OVP;

namespace {
const char *kProjectFilter = "DateVisionPlus 项目文件 (*.dvp);;JSON 文件 (*.json);;所有文件 (*.*)";

/**
 * 连续运行模式下「上一轮结束」到「下一轮开始」之间的固定等待（毫秒）。
 * 0 表示不做任何等待：本轮在后台线程跑完、结果回到界面后立刻排队下一轮。
 * 单发定时器保证最多有一轮待执行；预览和结果表另按显示节拍刷新。
 */
constexpr int kContinuousIdleMs = 0;
constexpr int kPresentationIntervalMs = 33;

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
    setupVisualShell();

    reloadPlugins();

    setupWorker();

    // 单发 + 0ms：每轮执行结束后由回调重新排期，靠串行执行而非固定节拍推进
    m_continuousTimer = new QTimer(this);
    m_continuousTimer->setSingleShot(true);
    m_continuousTimer->setInterval(kContinuousIdleMs);
    connect(m_continuousTimer, &QTimer::timeout, this, &MainWindow::doExecute);

    setupConnections();
    updateWorkspaceSummary();
    ui->statusbar->showMessage(QStringLiteral("就绪"));
}

void MainWindow::setupVisualShell()
{
    // Keep a single command bar; attach actions to the window so hidden menus
    // do not disable their keyboard shortcuts.
    ui->menubar->hide();
    addActions({ui->actNew, ui->actOpen, ui->actSave, ui->actSaveAs, ui->actExit,
                ui->actRun, ui->actContinuous, ui->actStop, ui->actDelete,
                ui->actZoomIn, ui->actZoomOut, ui->actFit, ui->actRefreshPlugins,
                ui->actAbout});

    ui->mainToolBar->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    ui->mainToolBar->setIconSize(QSize(16, 16));
    ui->mainToolBar->setFloatable(false);
    ui->mainToolBar->setContextMenuPolicy(Qt::PreventContextMenu);
    const auto setIcon = [this](QAction *action, QStyle::StandardPixmap symbol, const QColor &color) {
        QPixmap pixmap = style()->standardIcon(symbol).pixmap(16, 16);
        QPainter painter(&pixmap);
        painter.setCompositionMode(QPainter::CompositionMode_SourceIn);
        painter.fillRect(pixmap.rect(), color);
        painter.end();
        action->setIcon(QIcon(pixmap));
    };
    const QColor iconColor(QStringLiteral("#b7cbd7"));
    setIcon(ui->actNew, QStyle::SP_FileIcon, iconColor);
    setIcon(ui->actOpen, QStyle::SP_DirOpenIcon, iconColor);
    setIcon(ui->actSave, QStyle::SP_DialogSaveButton, iconColor);
    setIcon(ui->actRun, QStyle::SP_MediaPlay, QColor(QStringLiteral("#273b47")));
    setIcon(ui->actContinuous, QStyle::SP_BrowserReload, iconColor);
    setIcon(ui->actStop, QStyle::SP_MediaStop, QColor(QStringLiteral("#d8a1a1")));
    setIcon(ui->actDelete, QStyle::SP_TrashIcon, iconColor);
    setIcon(ui->actRefreshPlugins, QStyle::SP_BrowserReload, iconColor);
    ui->actNew->setIconText(QStringLiteral("新建"));
    ui->actOpen->setIconText(QStringLiteral("打开"));
    ui->actSave->setIconText(QStringLiteral("保存"));
    ui->actRun->setIconText(QStringLiteral("运行  F5"));
    ui->actDelete->setIconText(QStringLiteral("删除"));
    ui->actDelete->setToolTip(QStringLiteral("删除选中节点或连线 (Del)"));
    ui->actRefreshPlugins->setToolTip(QStringLiteral("重新扫描并加载内置工具与用户插件"));
    ui->mainToolBar->widgetForAction(ui->actRun)->setObjectName(QStringLiteral("runButton"));
    ui->mainToolBar->widgetForAction(ui->actStop)->setObjectName(QStringLiteral("stopButton"));
    ui->mainToolBar->addAction(ui->actRefreshPlugins);

    auto *moreButton = new QToolButton(ui->mainToolBar);
    moreButton->setText(QStringLiteral("更多"));
    moreButton->setToolTip(QStringLiteral("另存为、关于与退出"));
    moreButton->setPopupMode(QToolButton::InstantPopup);
    auto *moreMenu = new QMenu(moreButton);
    moreMenu->addAction(ui->actSaveAs);
    moreMenu->addSeparator();
    moreMenu->addAction(ui->actAbout);
    moreMenu->addAction(ui->actExit);
    moreButton->setMenu(moreMenu);
    ui->mainToolBar->addWidget(moreButton);

    auto *commandSpacer = new QWidget(ui->mainToolBar);
    commandSpacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    ui->mainToolBar->addWidget(commandSpacer);

    const auto dockHeading = [](QDockWidget *dock, const QString &title, const QString &index) {
        auto *heading = new QWidget(dock);
        heading->setObjectName(QStringLiteral("dockHeading"));
        auto *layout = new QHBoxLayout(heading);
        layout->setContentsMargins(14, 11, 14, 11);
        auto *label = new QLabel(title, heading);
        label->setProperty("uiRole", "sectionTitle");
        auto *number = new QLabel(index, heading);
        number->setProperty("uiRole", "numeric");
        layout->addWidget(label);
        layout->addStretch();
        layout->addWidget(number);
        dock->setTitleBarWidget(heading);
    };
    dockHeading(ui->dockToolbox, QStringLiteral("视觉工具库"), QStringLiteral("01"));
    dockHeading(ui->dockProperties, QStringLiteral("节点属性"), QStringLiteral("03"));
    dockHeading(ui->dockBottom, QStringLiteral("监视与输出"), QStringLiteral("04"));
    setCorner(Qt::BottomLeftCorner, Qt::LeftDockWidgetArea);
    setCorner(Qt::BottomRightCorner, Qt::RightDockWidgetArea);
    ui->dockToolbox->setMinimumWidth(212);
    ui->dockProperties->setMinimumWidth(248);

    auto *workspaceHeader = new QWidget(ui->centralwidget);
    workspaceHeader->setObjectName(QStringLiteral("workspaceHeader"));
    auto *workspaceLayout = new QHBoxLayout(workspaceHeader);
    workspaceLayout->setContentsMargins(14, 8, 12, 8);
    workspaceLayout->setSpacing(10);
    auto *title = new QLabel(QStringLiteral("流程编排"), workspaceHeader);
    title->setObjectName(QStringLiteral("workspaceTitle"));
    workspaceLayout->addWidget(title);
    m_projectLabel = new QLabel(workspaceHeader);
    m_projectLabel->setObjectName(QStringLiteral("projectLabel"));
    m_projectLabel->setMinimumWidth(0);
    m_projectLabel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    workspaceLayout->addWidget(m_projectLabel, 1);

    // Live statistics share the command bar with the existing actions.
    auto *metrics = new QWidget(ui->mainToolBar);
    metrics->setObjectName(QStringLiteral("toolbarMetrics"));
    metrics->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    auto *metricsLayout = new QHBoxLayout(metrics);
    metricsLayout->setContentsMargins(0, 0, 0, 0);
    metricsLayout->setSpacing(0);
    const auto metric = [metricsLayout, metrics](const QString &caption,
                                                const QString &initial,
                                                const QString &unit = QString()) {
        auto *cell = new QWidget(metrics);
        cell->setProperty("uiRole", "metricCell");
        auto *layout = new QVBoxLayout(cell);
        layout->setContentsMargins(6, 0, 6, 0);
        layout->setSpacing(2);
        auto *label = new QLabel(caption, cell);
        label->setProperty("uiRole", "caption");
        auto *value = new QLabel(initial, cell);
        value->setProperty("uiRole", "metricValue");
        layout->addWidget(label);
        auto *valueLayout = new QHBoxLayout;
        valueLayout->setSpacing(4);
        valueLayout->addWidget(value);
        if (!unit.isEmpty()) {
            auto *unitLabel = new QLabel(unit, cell);
            unitLabel->setProperty("uiRole", "metricUnit");
            valueLayout->addWidget(unitLabel, 0, Qt::AlignBottom);
        }
        valueLayout->addStretch();
        layout->addLayout(valueLayout);
        metricsLayout->addWidget(cell);
        return value;
    };
    m_nodeCount = metric(QStringLiteral("流程节点"), QStringLiteral("00"));
    m_connectionCount = metric(QStringLiteral("数据连线"), QStringLiteral("00"));
    m_cycleTime = metric(QStringLiteral("最近执行"), QStringLiteral("--"), QStringLiteral("ms"));
    m_runResult = metric(QStringLiteral("执行结果"), QStringLiteral("待运行"));
    m_runResult->setObjectName(QStringLiteral("runResult"));
    // Reserve enough width for all result states without shifting the adjacent controls.
    m_runResult->setMinimumWidth(QFontMetrics(m_runResult->font()).horizontalAdvance(QStringLiteral("执行成功")));
    ui->mainToolBar->addWidget(metrics);

    auto *toolbarBrand = new QWidget(ui->mainToolBar);
    toolbarBrand->setObjectName(QStringLiteral("toolbarBrand"));
    toolbarBrand->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
    auto *brandLayout = new QHBoxLayout(toolbarBrand);
    brandLayout->setContentsMargins(12, 0, 0, 0);
    brandLayout->setSpacing(8);
    auto *brandMark = new QLabel(QStringLiteral("DV"), toolbarBrand);
    brandMark->setObjectName(QStringLiteral("toolbarBrandMark"));
    brandMark->setFixedSize(28, 28);
    brandMark->setAlignment(Qt::AlignCenter);
    auto *brandName = new QLabel(QStringLiteral("DateVision+"), toolbarBrand);
    brandName->setObjectName(QStringLiteral("toolbarBrandName"));
    brandLayout->addWidget(brandMark);
    brandLayout->addWidget(brandName);
    ui->mainToolBar->addWidget(toolbarBrand);

    for (QAction *action : {ui->actZoomOut, ui->actZoomIn, ui->actFit}) {
        action->setIconText(action == ui->actZoomOut ? QStringLiteral("−")
                               : action == ui->actZoomIn ? QStringLiteral("+") : QStringLiteral("适应"));
        auto *button = new QToolButton(workspaceHeader);
        button->setDefaultAction(action);
        button->setToolButtonStyle(Qt::ToolButtonTextOnly);
        button->setProperty("uiRole", "headerControl");
        button->setAccessibleName(action->text());
        button->setToolTip(QStringLiteral("%1 (%2)").arg(action->text(), action->shortcut().toString(QKeySequence::NativeText)));
        workspaceLayout->addWidget(button);
    }
    ui->centralLayout->insertWidget(0, workspaceHeader);
    ui->centralLayout->setStretch(1, 1);

    auto *emptyHint = new QLabel(ui->flowchartView->viewport());
    emptyHint->setObjectName(QStringLiteral("canvasEmptyHint"));
    emptyHint->setAlignment(Qt::AlignCenter);
    emptyHint->setWordWrap(true);
    emptyHint->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
    emptyHint->setAttribute(Qt::WA_TransparentForMouseEvents);
    emptyHint->setText(QStringLiteral("<span style='color:#b7d2df; font-size:20px;'>构建你的视觉检测流程</span>"
                                     "<br><br><span style='color:#8b9da9;'>从左侧拖入工具，或双击工具添加节点</span>"
                                     "<br><br><span style='color:#697f8c; font-size:11px;'>图像输入  /  图像处理  /  检测定位  /  结果输出</span>"));
    auto *hintLayout = new QVBoxLayout(ui->flowchartView->viewport());
    hintLayout->addWidget(emptyHint);
    const auto updateHint = [this, emptyHint] { emptyHint->setVisible(m_scene->nodes().isEmpty()); };
    connect(m_scene, &FlowScene::nodeAdded, this, updateHint);
    connect(m_scene, &FlowScene::nodeRemoved, this, updateHint);

    auto *footer = new QWidget(ui->centralwidget);
    footer->setObjectName(QStringLiteral("workspaceFooter"));
    auto *footerLayout = new QHBoxLayout(footer);
    footerLayout->setContentsMargins(14, 6, 14, 6);
    auto *guide = new QLabel(QStringLiteral("滚轮缩放  /  拖拽框选  /  拖动端口连线"), footer);
    guide->setProperty("uiRole", "caption");
    footerLayout->addWidget(guide);
    footerLayout->addStretch();
    auto *canvasTag = new QLabel(QStringLiteral("02  /  FLOW EDITOR"), footer);
    canvasTag->setProperty("uiRole", "caption");
    footerLayout->addWidget(canvasTag);
    ui->centralLayout->addWidget(footer);

    // Let the user size the image and log panes independently.
    auto *bottomSplitter = new QSplitter(Qt::Horizontal, ui->bottomContents);
    bottomSplitter->setChildrenCollapsible(false);
    ui->bottomLayout->removeWidget(ui->previewPanel);
    ui->bottomLayout->removeWidget(ui->bottomTabs);
    bottomSplitter->addWidget(ui->previewPanel);
    bottomSplitter->addWidget(ui->bottomTabs);
    bottomSplitter->setStretchFactor(0, 1);
    bottomSplitter->setStretchFactor(1, 1);
    bottomSplitter->setSizes({380, 440});
    ui->bottomLayout->addWidget(bottomSplitter);
    ui->bottomTabs->setTabText(0, QStringLiteral("运行输出"));
    ui->bottomTabs->setTabText(1, QStringLiteral("TCP 通信"));
    auto *statusLabel = new QLabel(QStringLiteral("LOCAL  /  本地工作站"), this);
    statusLabel->setProperty("uiRole", "caption");
    ui->statusbar->addPermanentWidget(statusLabel);

    connect(m_scene, &FlowScene::nodeAdded, this, &MainWindow::updateWorkspaceSummary);
    connect(m_scene, &FlowScene::nodeRemoved, this, [this] {
        ui->propertiesPanel->setNode(nullptr);
        updateWorkspaceSummary();
    });
    connect(m_scene, &FlowScene::connectionChanged, this, &MainWindow::updateWorkspaceSummary);
    QTimer::singleShot(0, this, [this] {
        resizeDocks({ui->dockToolbox, ui->dockProperties}, {230, 280}, Qt::Horizontal);
        resizeDocks({ui->dockBottom}, {260}, Qt::Vertical);
    });
}

void MainWindow::updateWorkspaceSummary()
{
    m_nodeCount->setText(QStringLiteral("%1").arg(m_scene->nodes().size(), 2, 10, QLatin1Char('0')));
    m_connectionCount->setText(QStringLiteral("%1").arg(m_scene->connections().size(), 2, 10, QLatin1Char('0')));
    m_projectLabel->setText(m_currentFile.isEmpty() ? QStringLiteral("未命名项目") : QFileInfo(m_currentFile).fileName());
    m_projectLabel->setToolTip(m_currentFile);
}

void MainWindow::setRunResult(const QString &text, const char *state)
{
    m_runResult->setText(text);
    m_runResult->setToolTip(QString());
    if (m_runResult->property("state").toString() != QString::fromLatin1(state)) {
        m_runResult->setProperty("state", state);
        m_runResult->style()->unpolish(m_runResult);
        m_runResult->style()->polish(m_runResult);
    }
}

MainWindow::~MainWindow()
{
    if (m_workerThread) {
        m_worker->requestStop();
        m_workerThread->quit();
        m_workerThread->wait();
        m_worker = nullptr;
    }
    delete ui;
}

void MainWindow::setupWorker()
{
    m_workerThread = new QThread(this);
    m_worker = new OVP::ExecutionWorker;
    m_worker->moveToThread(m_workerThread);
    connect(m_workerThread, &QThread::finished, m_worker, &QObject::deleteLater);

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
    connect(m_scene, &FlowScene::nodeAdded, this, [this] { m_graphDirty = true; });
    connect(m_scene, &FlowScene::nodeRemoved, this, [this] { m_graphDirty = true; });
    connect(m_scene, &FlowScene::connectionChanged, this, [this] { m_graphDirty = true; });

    connect(ui->communicationPanel, &CommunicationPanel::executeRequested, this,
            &MainWindow::onCommTrigger);
}

// ------------------------------------------------------------------ 文件操作

void MainWindow::onNew()
{
    if (!m_editableUi || m_editing)
        return;
    QScopedValueRollback<bool> editing(m_editing, true);
    const int reply = QMessageBox::question(this, QStringLiteral("新建"),
                                            QStringLiteral("确定要新建吗？当前流程图将被清除。"),
                                            QMessageBox::Yes | QMessageBox::No);
    if (reply != QMessageBox::Yes)
        return;

    m_scene->clearAll();
    m_latestResults.clear();
    m_latestOrder.clear();
    m_presentationPending = false;
    ui->previewPanel->clearImage();
    ui->outputPanel->clearLog();
    m_currentFile.clear();
    updateWorkspaceSummary();
    m_cycleTime->setText(QStringLiteral("--"));
    setRunResult(QStringLiteral("待运行"), "idle");
    m_runResult->setToolTip(QString());
    setWindowTitle(QStringLiteral("DateVisionPlus - 工业视觉检测平台"));
    ui->statusbar->showMessage(QStringLiteral("新建流程图"));
}

void MainWindow::onOpen()
{
    if (!m_editableUi || m_editing)
        return;
    QScopedValueRollback<bool> editing(m_editing, true);
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
    m_latestResults.clear();
    m_latestOrder.clear();
    m_presentationPending = false;
    ui->previewPanel->clearImage();
    ui->outputPanel->clearLog();
    ui->propertiesPanel->setNode(nullptr);

    if (root.contains(QStringLiteral("communication")))
        ui->communicationPanel->setConfig(root.value(QStringLiteral("communication")).toObject());

    m_currentFile = filePath;
    updateWorkspaceSummary();
    m_cycleTime->setText(QStringLiteral("--"));
    setRunResult(QStringLiteral("待运行"), "idle");
    m_runResult->setToolTip(QString());
    setWindowTitle(QStringLiteral("DateVisionPlus - %1").arg(QFileInfo(filePath).fileName()));
    ui->outputPanel->logSuccess(QStringLiteral("已打开: %1").arg(filePath));
    ui->statusbar->showMessage(QStringLiteral("已加载: %1").arg(QFileInfo(filePath).fileName()));
}

void MainWindow::onSave()
{
    if (!m_editableUi || m_editing)
        return;
    if (m_currentFile.isEmpty()) {
        onSaveAs();
        return;
    }
    saveToFile(m_currentFile);
}

void MainWindow::onSaveAs()
{
    if (!m_editableUi || m_editing)
        return;
    QScopedValueRollback<bool> editing(m_editing, true);
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
    updateWorkspaceSummary();
    setWindowTitle(QStringLiteral("DateVisionPlus - %1").arg(QFileInfo(filePath).fileName()));
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
        m_presentationClock.invalidate();
        m_continuousTimer->start();
        ui->outputPanel->logInfo(QStringLiteral("已开启连续运行模式（上一轮结束后立即执行下一轮）"));
        ui->statusbar->showMessage(QStringLiteral("连续运行中..."));
    } else {
        m_continuousTimer->stop();
        if (!m_executing && m_presentationPending)
            presentExecutionResults();
        ui->outputPanel->logInfo(QStringLiteral("已关闭连续运行模式"));
        ui->statusbar->showMessage(QStringLiteral("已切换为单次触发"));
    }
    updateBusyUi();
}

void MainWindow::onStop()
{
    m_stopRequested = m_executing;
    setRunResult(QStringLiteral("已停止"), "idle");
    m_worker->requestStop();
    // setChecked(false) 会触发 toggled 信号，从而调用 onToggleContinuous(false)
    if (m_continuousMode)
        ui->actContinuous->setChecked(false);
    setRunResult(QStringLiteral("已停止"), "idle");
    ui->outputPanel->logWarning(QStringLiteral("执行已停止"));
    ui->statusbar->showMessage(QStringLiteral("已停止"));
    updateBusyUi();
}

void MainWindow::onDelete()
{
    if (!m_editableUi || m_editing)
        return;
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
                       QStringLiteral("<h3>DateVisionPlus 工业视觉检测平台 v2.0.5</h3>"
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
    if (!m_editableUi || m_editing)
        return;
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
    if (!m_editableUi || m_editing)
        return;
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
    if (!m_editableUi || m_editing || !node || !node->plugin())
        return;
    QScopedValueRollback<bool> editing(m_editing, true);

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

    // 编辑预览也可能遇到环；先标记，避免递归无限增长。
    executed.insert(node->nodeId());
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
    ui->actSave->setEnabled(editable);
    ui->actSaveAs->setEnabled(editable);
    ui->actDelete->setEnabled(editable);
    ui->actRefreshPlugins->setEnabled(editable);
    ui->propertiesPanel->setEnabled(editable);
    ui->toolboxPanel->setEnabled(editable);
    ui->flowchartView->setInteractive(editable);
    ui->flowchartView->setAcceptDrops(editable);
}

void MainWindow::doExecute()
{
    // 连续运行的下一轮由执行完成回调统一排期，先取消待触发的那次，避免重复投递
    m_continuousTimer->stop();

    // 上一轮尚未结束：直接跳过本次触发，避免事件与任务堆积。
    // 必须放在采集图数据之前 —— 0ms 排期下这里的调用频率很高。
    if (m_executing || m_editing) {
        if (m_commTriggered) {
            ui->communicationPanel->setResponseData(QStringLiteral("ERROR:BUSY"));
            m_commTriggered = false;
        }
        return;
    }

    if (m_scene->nodes().isEmpty()) {
        setRunResult(QStringLiteral("无节点"), "idle");
        ui->outputPanel->logWarning(QStringLiteral("流程图为空，无法执行"));
        if (m_commTriggered) {
            ui->communicationPanel->setResponseData(QStringLiteral("ERROR:NO_NODES"));
            m_commTriggered = false;
        }
        return;
    }

    if (m_graphDirty) {
        QMap<QString, PluginBase *> plugins;
        QList<LinkDef> links;
        collectGraph(plugins, links);
        m_worker->setGraph(plugins, links);
        m_graphDirty = false;
    }
    if (!m_worker->prepareRun(!m_continuousMode))
        return;

    m_stopRequested = false;
    m_executionClock.start();
    m_executing = true;
    updateBusyUi();

    QMetaObject::invokeMethod(m_worker, &OVP::ExecutionWorker::run, Qt::QueuedConnection);
}

void MainWindow::onExecutionStarted(int total)
{
    if (!m_continuousMode)
        setRunResult(QStringLiteral("执行中"), "running");
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
    const QString error = results.value(QStringLiteral("_error")).toString();
    const bool errorChanged = error != m_latestResults.value(QStringLiteral("_error")).toString();
    const bool present = !m_continuousMode || m_stopRequested || errorChanged
                         || !m_presentationClock.isValid()
                         || m_presentationClock.elapsed() >= kPresentationIntervalMs;
    m_latestElapsedMs = m_executionClock.isValid() ? m_executionClock.elapsed() : 0;
    m_latestResults = results;
    m_latestOrder = order;
    m_presentationPending = true;
    m_executing = false;
    updateBusyUi();

    // 通信响应不能跟随显示节拍丢帧，也无需为本地运行逐轮计算格式表达式。
    if (m_commTriggered) {
        const QString delimiter = ui->communicationPanel->delimiter();
        QStringList statusParts;
        statusParts.reserve(order.size());
        for (const QString &nodeId : order) {
            const QVariant value = results.value(nodeId);
            const bool ok = value.metaType().id() == QMetaType::QVariantMap;
            statusParts.append(ok ? QStringLiteral("OK") : QStringLiteral("NG"));
        }
        const QString statusString = statusParts.isEmpty() ? QStringLiteral("NO_RESULT")
                                                          : statusParts.join(delimiter);
        const QString outputString = buildCommResponse(results);
        const QString response = outputString.isEmpty()
                                     ? statusString
                                     : QStringLiteral("%1%2%3").arg(statusString, delimiter, outputString);
        ui->communicationPanel->setResponseData(response);
        ui->outputPanel->logInfo(QStringLiteral("通信响应: %1%2")
                                     .arg(response.left(200),
                                          response.length() > 200 ? QStringLiteral("...") : QString()));
        m_commTriggered = false;
    }

    if (!error.isEmpty() && present)
        ui->outputPanel->logError(error);

    // 在后台空闲时消费当前结果，避免延迟渲染时读取插件下一轮复用的图像缓冲区。
    if (present) {
        presentExecutionResults();
    }

    if (m_continuousMode)
        m_continuousTimer->start();
}

void MainWindow::presentExecutionResults()
{
    const auto &results = m_latestResults;
    const auto &order = m_latestOrder;
    m_cycleTime->setText(QString::number(m_latestElapsedMs));
    int succeeded = 0;
    QStringList statusParts;
    statusParts.reserve(order.size());
    for (const QString &nodeId : order) {
        const QVariant value = results.value(nodeId);
        const bool ok = value.metaType().id() == QMetaType::QVariantMap;
        if (ok)
            ++succeeded;
        statusParts.append(ok ? QStringLiteral("OK") : QStringLiteral("NG"));
        if (NodeItem *node = m_scene->node(nodeId))
            node->setStatus(value.isValid() ? (ok ? 1 : 0) : -1);
    }
    if (m_stopRequested) {
        setRunResult(QStringLiteral("已停止"), "idle");
    } else if (results.contains(QStringLiteral("_error")) || succeeded != order.size()) {
        setRunResult(QStringLiteral("执行异常"), "error");
    } else {
        setRunResult(QStringLiteral("执行成功"), "success");
    }
    m_runResult->setToolTip(QStringLiteral("成功执行节点：%1 / %2").arg(succeeded).arg(order.size()));
    const QString delimiter = ui->communicationPanel->delimiter();
    const QString statusString = statusParts.isEmpty() ? QStringLiteral("NO_RESULT")
                                                       : statusParts.join(delimiter);
    if (!m_stopRequested && !results.contains(QStringLiteral("_error")))
        ui->outputPanel->logSuccess(statusString);

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

    m_presentationPending = false;
    m_presentationClock.start();
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
