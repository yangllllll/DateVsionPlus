#include "plugintreewidget.h"

#include "../core/plugintypes.h"

#include <QDrag>
#include <QFont>
#include <QMimeData>
#include <QPainter>
#include <QPixmap>

PluginTreeWidget::PluginTreeWidget(QWidget *parent)
    : QTreeWidget(parent)
{
}

void PluginTreeWidget::startDrag(Qt::DropActions supportedActions)
{
    Q_UNUSED(supportedActions);
    QTreeWidgetItem *item = currentItem();
    if (!item)
        return;

    const QString pluginId = item->data(0, Qt::UserRole).toString();
    if (pluginId.isEmpty())
        return;

    auto *drag = new QDrag(this);
    auto *mime = new QMimeData();
    mime->setData(OVP::pluginIdMimeType(), pluginId.toUtf8());
    drag->setMimeData(mime);

    QPixmap pixmap(168, 34);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setBrush(QColor(QStringLiteral("#1c2830")));
    painter.setPen(QColor(QStringLiteral("#b7d2df")));
    painter.drawRect(pixmap.rect().adjusted(0, 0, -1, -1));
    painter.fillRect(0, 0, 3, pixmap.height(), QColor(QStringLiteral("#b7d2df")));
    painter.setPen(QColor(QStringLiteral("#e2ebf0")));
    painter.setFont(QFont(QStringLiteral("Microsoft YaHei"), 9, QFont::Bold));
    painter.drawText(pixmap.rect(), Qt::AlignCenter, item->text(0));
    painter.end();
    drag->setPixmap(pixmap);
    drag->setHotSpot(pixmap.rect().center());

    drag->exec(Qt::CopyAction);
}
