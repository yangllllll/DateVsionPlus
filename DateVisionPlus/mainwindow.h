#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "flowchart/executionworker.h"

#include <QList>
#include <QMainWindow>
#include <QMap>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QTimer>
#include <QVariant>

#include <opencv2/core.hpp>

class QThread;

namespace OVP {
class FlowScene;
class NodeItem;
class PluginBase;
struct LinkDef;
}

namespace Ui {
class MainWindow;
}

/** 主窗口：工具栏 / 菜单 / 停靠面板 / 执行调度 / 项目存取 */
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

private slots:
    void onNew();
    void onOpen();
    void onSave();
    void onSaveAs();
    void onRun();
    void onToggleContinuous(bool checked);
    void onStop();
    void onDelete();
    void onZoomIn();
    void onZoomOut();
    void onFit();
    void onAbout();
    void onRefreshPlugins();

    void onNodeAdded(OVP::NodeItem *node);
    void onNodeSelected(OVP::NodeItem *node);
    void onNodeDeselected();
    void onNodeDoubleClicked(OVP::NodeItem *node);

    void onCommTrigger();
    void doExecute();

    // ---- 后台执行回调（均由 GUI 线程执行）----
    void onExecutionStarted(int total);
    void onNodeStatusChanged(const QString &nodeId, int status);
    void onExecutionProgress(int finished, int total);
    void onExecutionFinished(const QMap<QString, QVariant> &results, const QStringList &order);

private:
    void setupConnections();
    /** 创建后台执行线程，使流程计算不再阻塞界面 */
    void setupWorker();
    /** 重新加载内置插件与用户插件 DLL，并把加载结果写入输出面板 */
    void reloadPlugins();
    void saveToFile(const QString &filePath);
    void addNodeToCenter(const QString &pluginId);

    /** 递归执行上游节点，取得该节点的输入图像 */
    cv::Mat inputImageForNode(OVP::NodeItem *node);
    void executeUpstream(OVP::NodeItem *node, QSet<QString> &executed);

    /** 从场景中抽取与界面无关的执行用图数据 */
    void collectGraph(QMap<QString, OVP::PluginBase *> &plugins, QList<OVP::LinkDef> &links) const;
    /** 执行期间禁用会破坏图结构的编辑操作，避免后台线程持有悬空指针 */
    void updateBusyUi();

    QString buildCommResponse(const QMap<QString, QVariant> &results);
    QString evalFormatLine(const QString &line, const QMap<QString, QVariant> &results);
    QString nodeOutputValue(const QString &nodeId, const QString &portName,
                            const QMap<QString, QVariant> &results);

    Ui::MainWindow *ui = nullptr;
    OVP::FlowScene *m_scene = nullptr;

    QThread *m_workerThread = nullptr;
    OVP::ExecutionWorker *m_worker = nullptr;
    bool m_executing = false;

    QTimer *m_continuousTimer = nullptr;
    bool m_continuousMode = false;
    /** 当前编辑类控件是否可用，用于避免重复调用 setEnabled() */
    bool m_editableUi = true;
    bool m_commTriggered = false;
    QString m_currentFile;
};

#endif // MAINWINDOW_H
