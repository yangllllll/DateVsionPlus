#include "portitem.h"
#include "nodeitem.h"

#include <QBrush>
#include <QCursor>
#include <QGraphicsSceneMouseEvent>
#include <QPainter>
#include <QPen>

namespace OVP {

PortItem::PortItem(const QString &name, PortType type, bool isInput, NodeItem *node)
    : QGraphicsObject(nullptr)
    , m_name(name)
    , m_type(type)
    , m_isInput(isInput)
    , m_node(node)
{
    setParentItem(node);
    setAcceptHoverEvents(true);
    setCursor(Qt::CrossCursor);
    setZValue(1.0);
}

QRectF PortItem::boundingRect() const
{
    const qreal m = PortRadius + 2.0;
    return QRectF(-m, -m, m * 2.0, m * 2.0);
}

void PortItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    Q_UNUSED(option);
    Q_UNUSED(widget);
    painter->setRenderHint(QPainter::Antialiasing);

    const QColor color = portTypeColor(m_type);
    const QRectF rect(-PortRadius, -PortRadius, PortRadius * 2.0, PortRadius * 2.0);

    if (!connectedPorts.isEmpty()) {
        painter->setBrush(QBrush(color));
        painter->setPen(QPen(color.darker(130), 1.5));
    } else {
        painter->setBrush(QBrush(color.lighter(160)));
        painter->setPen(QPen(color, 1.5));
    }
    painter->drawEllipse(rect);

    if (isUnderMouse()) {
        painter->setBrush(QBrush(color.lighter(180)));
        painter->setPen(QPen(Qt::white, 2.0));
        painter->drawEllipse(rect);
    }
}

QPointF PortItem::centerInScene() const
{
    return mapToScene(QPointF(0.0, 0.0));
}

bool PortItem::isConnectableWith(const PortItem *other) const
{
    if (other == nullptr || other == this)
        return false;
    if (other->node() == m_node)
        return false;
    if (m_isInput == other->isInput())
        return false;
    if (m_type != other->portType() && m_type != PortType::Any
        && other->portType() != PortType::Any) {
        return false;
    }
    return true;
}

void PortItem::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        emit dragStarted(this);
        event->accept();
        return;
    }
    QGraphicsObject::mousePressEvent(event);
}

} // namespace OVP
