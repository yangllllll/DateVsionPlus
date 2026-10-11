#include "toolboxpanel.h"
#include "plugintreewidget.h"
#include "ui_toolboxpanel.h"

#include "../core/pluginmanager.h"

#include <QFont>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QTreeWidgetItem>

ToolboxPanel::ToolboxPanel(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::ToolboxPanel)
{
    ui->setupUi(this);
    m_tree = ui->tree;
    ui->lblTitle->hide();
    ui->mainLayout->setContentsMargins(10, 10, 10, 10);
    ui->mainLayout->setSpacing(8);

    m_search = new QLineEdit(this);
    m_search->setObjectName(QStringLiteral("toolSearch"));
    m_search->setPlaceholderText(QStringLiteral("搜索工具名称 / 类型"));
    m_search->setClearButtonEnabled(true);
    m_search->setMinimumWidth(0);
    ui->mainLayout->insertWidget(0, m_search);

    auto *summary = new QWidget(this);
    auto *summaryLayout = new QHBoxLayout(summary);
    summaryLayout->setContentsMargins(2, 2, 2, 2);
    auto *caption = new QLabel(QStringLiteral("可用工具"), summary);
    caption->setProperty("uiRole", "caption");
    m_count = new QLabel(summary);
    m_count->setProperty("uiRole", "counter");
    summaryLayout->addWidget(caption);
    summaryLayout->addStretch();
    summaryLayout->addWidget(m_count);
    ui->mainLayout->insertWidget(1, summary);
    m_tree->setIndentation(12);
    m_tree->setUniformRowHeights(true);
    m_tree->setAnimated(false);
    m_tree->setFrameShape(QFrame::NoFrame);

    auto *hint = new QLabel(QStringLiteral("拖入画布 / 双击添加"), this);
    hint->setProperty("uiRole", "muted");
    hint->setAlignment(Qt::AlignCenter);
    ui->mainLayout->addWidget(hint);
    connect(m_search, &QLineEdit::textChanged, this, &ToolboxPanel::filterPlugins);

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
        const QStringList pluginIds = manager.pluginsInCategory(category);
        auto *categoryItem = new QTreeWidgetItem(m_tree);
        categoryItem->setText(0, QStringLiteral("%1  /  %2").arg(category).arg(pluginIds.size(), 2, 10, QLatin1Char('0')));
        categoryItem->setData(0, Qt::UserRole + 1, category);
        categoryItem->setFlags(categoryItem->flags() & ~Qt::ItemIsDragEnabled);
        QFont categoryFont = m_tree->font();
        categoryFont.setBold(true);
        categoryItem->setFont(0, categoryFont);
        categoryItem->setForeground(0, QColor(QStringLiteral("#b7d2df")));
        categoryItem->setSizeHint(0, QSize(0, 34));

        for (const QString &pluginId : pluginIds) {
            const OVP::PluginManager::Info info = manager.info(pluginId);
            auto *item = new QTreeWidgetItem(categoryItem);
            item->setText(0, info.name);
            item->setToolTip(0, info.description);
            item->setData(0, Qt::UserRole, pluginId);
            item->setSizeHint(0, QSize(0, 32));
            item->setFlags(item->flags() | Qt::ItemIsDragEnabled);
        }
        m_tree->expandItem(categoryItem);
    }
    filterPlugins(m_search->text());
}

void ToolboxPanel::filterPlugins(const QString &query)
{
    const QString text = query.trimmed();
    int visibleCount = 0;
    int totalCount = 0;
    for (int i = 0; i < m_tree->topLevelItemCount(); ++i) {
        auto *category = m_tree->topLevelItem(i);
        const bool categoryMatches = category->data(0, Qt::UserRole + 1).toString().contains(text, Qt::CaseInsensitive);
        int matches = 0;
        for (int j = 0; j < category->childCount(); ++j) {
            auto *item = category->child(j);
            const bool visible = text.isEmpty() || categoryMatches
                                 || item->text(0).contains(text, Qt::CaseInsensitive)
                                 || item->toolTip(0).contains(text, Qt::CaseInsensitive)
                                 || item->data(0, Qt::UserRole).toString().contains(text, Qt::CaseInsensitive);
            item->setHidden(!visible);
            matches += visible ? 1 : 0;
            ++totalCount;
        }
        category->setHidden(matches == 0);
        if (!text.isEmpty())
            category->setExpanded(true);
        visibleCount += matches;
    }
    m_count->setText(text.isEmpty()
                         ? QStringLiteral("%1").arg(totalCount, 2, 10, QLatin1Char('0'))
                         : QStringLiteral("%1 / %2").arg(visibleCount).arg(totalCount));
}
