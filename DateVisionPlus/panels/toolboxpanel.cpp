#include "toolboxpanel.h"
#include "plugintreewidget.h"
#include "ui_toolboxpanel.h"

#include "../core/pluginmanager.h"

#include <QFont>
#include <QTreeWidgetItem>

ToolboxPanel::ToolboxPanel(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::ToolboxPanel)
{
    ui->setupUi(this);
    m_tree = ui->tree;

    connect(m_tree, &QTreeWidget::itemDoubleClicked, this,
            [this](QTreeWidgetItem *item, int column) {
                Q_UNUSED(column);
                const QString pluginId = item->data(0, Qt::UserRole).toString();
                if (!pluginId.isEmpty())
                    emit pluginDoubleClicked(pluginId);
            });

    loadPlugins();
}

ToolboxPanel::~ToolboxPanel()
{
    delete ui;
}

void ToolboxPanel::refresh()
{
    m_tree->clear();
    OVP::PluginManager::instance().reloadAll();
    loadPlugins();
}

void ToolboxPanel::loadPlugins()
{
    OVP::PluginManager &manager = OVP::PluginManager::instance();

    for (const QString &category : manager.categories()) {
        auto *categoryItem = new QTreeWidgetItem(m_tree);
        categoryItem->setText(0, category);
        categoryItem->setFlags(categoryItem->flags() & ~Qt::ItemIsDragEnabled);
        categoryItem->setFont(0, QFont(QStringLiteral("Microsoft YaHei"), 10, QFont::Bold));
        categoryItem->setForeground(0, QColor(200, 200, 200));

        for (const QString &pluginId : manager.pluginsInCategory(category)) {
            const OVP::PluginManager::Info info = manager.info(pluginId);
            auto *item = new QTreeWidgetItem(categoryItem);
            item->setText(0, info.name);
            item->setToolTip(0, info.description);
            item->setData(0, Qt::UserRole, pluginId);
            item->setFont(0, QFont(QStringLiteral("Microsoft YaHei"), 9));
            item->setFlags(item->flags() | Qt::ItemIsDragEnabled);
        }
        m_tree->expandItem(categoryItem);
    }
}
