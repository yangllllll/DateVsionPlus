#include "connectionitem.h"
#include "portitem.h"

#include <QPainter>
#include <QPen>
#include <QStyleOptionGraphicsItem>

namespace OVP {

ConnectionItem::ConnectionItem(PortItem *source, PortItem *target)
    : QGraphicsObject(nullptr)
    , m_source(source)
    , m_target(target)
    , m_isTemp(target == nullptr)
{
    setFlag(ItemIsSelectable, true);
    setZValue(0.0);
    updatePath();
}

QRectF ConnectionItem::boundingRect() const
{
    return m_path.boundingRect().adjusted(-5.0, -5.0, 5.0, 5.0);
}

QPainterPath ConnectionItem::shape() const
{
    QPainterPathStroker stroker;
    stroker.setWidth(6.0);
    return stroker.createStroke(m_path);
}

void ConnectionItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option,
                           QWidget *widget)
{
    Q_UNUSED(option);
    Q_UNUSED(widget);
    painter->setRenderHint(QPainter::Antialiasing);
    painter->setBrush(Qt::NoBrush);
    const bool selected = !m_isTemp && isSelected();
    if (!m_isTemp) {
        painter->setPen(QPen(QColor(10, 16, 22), selected ? 4.5 : 3.5,
                             Qt::SolidLine, Qt::SquareCap));
        painter->drawPath(m_path);
    }
    const QColor lineColor = m_isTemp ? QColor(158, 181, 196, 185)
        : selected ? QColor(211, 231, 240) : QColor(125, 156, 176);
    painter->setPen(QPen(lineColor, selected ? 2.2 : 1.6,
                         m_isTemp ? Qt::DashLine : Qt::SolidLine, Qt::SquareCap));
    painter->drawPath(m_path);
}

void ConnectionItem::setTempEnd(const QPointF &pos)
{
    m_tempEnd = pos;
    updatePath();
}

void ConnectionItem::finalize(PortItem *target)
{
    m_target = target;
    m_isTemp = false;
    updatePath();
}

void ConnectionItem::updatePath()
{
    if (!m_source)
        return;

    const QPointF start = m_source->centerInScene();
    setPos(0.0, 0.0);

    QPointF end;
    if (m_isTemp)
        end = m_tempEnd;
    else if (m_target)
        end = m_target->centerInScene();
    else
        return;

    const qreal dx = qAbs(end.x() - start.x()) * 0.5;
    QPainterPath path;
    path.moveTo(start);
    path.cubicTo(QPointF(start.x() + dx, start.y()), QPointF(end.x() - dx, end.y()), end);
    m_path = path;

    prepareGeometryChange();
    update();
}

} // namespace OVP
