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

    QPixmap pixmap(140, 28);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setBrush(QColor(0, 120, 215));
    painter.setPen(Qt::NoPen);
    painter.drawRoundedRect(0, 0, 140, 28, 6, 6);
    painter.setPen(Qt::white);
    painter.setFont(QFont(QStringLiteral("Microsoft YaHei"), 9, QFont::Bold));
    painter.drawText(0, 0, 140, 28, Qt::AlignCenter, item->text(0));
    painter.end();
    drag->setPixmap(pixmap);
    drag->setHotSpot(pixmap.rect().center());

    drag->exec(Qt::CopyAction);
}
