#ifndef TOOLBOXPANEL_H
#define TOOLBOXPANEL_H

#include <QWidget>

class PluginTreeWidget;

namespace Ui {
class ToolboxPanel;
}

/** 工具箱面板：按分类展示可用插件，支持拖拽 / 双击添加 */
class ToolboxPanel : public QWidget
{
    Q_OBJECT

public:
    explicit ToolboxPanel(QWidget *parent = nullptr);
    ~ToolboxPanel() override;

public slots:
    void refresh();

signals:
    void pluginDoubleClicked(const QString &pluginId);

private:
    void loadPlugins();

    Ui::ToolboxPanel *ui = nullptr;
    PluginTreeWidget *m_tree = nullptr;
};

#endif // TOOLBOXPANEL_H
