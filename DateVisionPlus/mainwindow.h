#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "flowchart/executionengine.h"

#include <QMainWindow>
#include <QMap>
#include <QSet>
#include <QString>
#include <QTimer>
#include <QVariant>

#include <opencv2/core.hpp>

namespace OVP {
class FlowScene;
class NodeItem;
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

private:
    void setupConnections();
    /** 重新加载内置插件与用户插件 DLL，并把加载结果写入输出面板 */
    void reloadPlugins();
    void saveToFile(const QString &filePath);
    void addNodeToCenter(const QString &pluginId);

    /** 递归执行上游节点，取得该节点的输入图像 */
    cv::Mat inputImageForNode(OVP::NodeItem *node);
    void executeUpstream(OVP::NodeItem *node, QSet<QString> &executed);

    QString buildCommResponse(const QMap<QString, QVariant> &results);
    QString evalFormatLine(const QString &line, const QMap<QString, QVariant> &results);
    QString nodeOutputValue(const QString &nodeId, const QString &portName,
                            const QMap<QString, QVariant> &results);

    Ui::MainWindow *ui = nullptr;
    OVP::FlowScene *m_scene = nullptr;
    OVP::ExecutionEngine m_engine;
    QTimer *m_continuousTimer = nullptr;
    bool m_continuousMode = false;
    bool m_commTriggered = false;
    QString m_currentFile;
};

#endif // MAINWINDOW_H
