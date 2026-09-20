#ifndef PLUGINTREEWIDGET_H
#define PLUGINTREEWIDGET_H

#include <QTreeWidget>

/** 工具箱树控件：支持把插件 ID 拖拽到流程图画布 */
class PluginTreeWidget : public QTreeWidget
{
    Q_OBJECT

public:
    explicit PluginTreeWidget(QWidget *parent = nullptr);

signals:
    void pluginDragRequested(const QString &pluginId);

protected:
    void startDrag(Qt::DropActions supportedActions) override;
};

#endif // PLUGINTREEWIDGET_H
