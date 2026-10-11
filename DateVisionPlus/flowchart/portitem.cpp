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
    setToolTip(m_name);
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

    const QColor typeColor = portTypeColor(m_type);
    // Retain the type hue while pulling it into the subdued instrument palette.
    const QColor color((typeColor.red() * 2 + 168 * 3) / 5,
                       (typeColor.green() * 2 + 187 * 3) / 5,
                       (typeColor.blue() * 2 + 200 * 3) / 5);
    const bool hovered = isUnderMouse();
    const QRectF rect(-PortRadius + 1.0, -PortRadius + 1.0,
                      (PortRadius - 1.0) * 2.0, (PortRadius - 1.0) * 2.0);

    painter->setBrush(QColor(14, 22, 29));
    painter->setPen(QPen(hovered ? QColor(231, 241, 247) : color, hovered ? 2.0 : 1.3));
    painter->drawRect(rect);
    if (!connectedPorts.isEmpty() || hovered) {
        painter->setPen(Qt::NoPen);
        painter->setBrush(hovered ? QColor(220, 236, 244) : color);
        painter->drawRect(QRectF(-2.5, -2.5, 5.0, 5.0));
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
