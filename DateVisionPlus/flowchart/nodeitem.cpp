#include "nodeitem.h"
#include "portitem.h"

#include <QFont>
#include <QFontMetricsF>
#include <QGraphicsSceneMouseEvent>
#include <QPainter>
#include <QPen>
#include <QStyleOptionGraphicsItem>

namespace OVP {

NodeItem::NodeItem(PluginBase *plugin, const QString &nodeId, QGraphicsItem *parent)
    : QGraphicsObject(parent)
    , m_plugin(plugin)
    , m_nodeId(nodeId)
{
    setFlag(ItemIsMovable, true);
    setFlag(ItemIsSelectable, true);
    setFlag(ItemSendsGeometryChanges, true);
    setZValue(2.0);
    setAcceptHoverEvents(true);
    setToolTip(m_nodeId);

    buildPorts();
    updateGeometry();
}

NodeItem::~NodeItem() = default;

void NodeItem::buildPorts()
{
    if (!m_plugin)
        return;

    for (const PortDef &def : m_plugin->inputPorts())
        m_inputPorts.append(new PortItem(def.name, def.type, true, this));

    for (const PortDef &def : m_plugin->outputPorts())
        m_outputPorts.append(new PortItem(def.name, def.type, false, this));
}

void NodeItem::updateGeometry()
{
    const int maxPorts = qMax(qMax(m_inputPorts.size(), m_outputPorts.size()), 1);
    m_height = m_headerHeight + maxPorts * m_portRowHeight + m_portSpacing * 2.0;

    for (int i = 0; i < m_inputPorts.size(); ++i) {
        const qreal y = m_headerHeight + m_portSpacing + i * m_portRowHeight
                        + m_portRowHeight / 2.0;
        m_inputPorts.at(i)->setPos(0.0, y);
    }
    for (int i = 0; i < m_outputPorts.size(); ++i) {
        const qreal y = m_headerHeight + m_portSpacing + i * m_portRowHeight
                        + m_portRowHeight / 2.0;
        m_outputPorts.at(i)->setPos(m_width, y);
    }
}

QRectF NodeItem::boundingRect() const
{
    return QRectF(0.0, 0.0, m_width, m_height);
}

void NodeItem::setStatus(int status)
{
    if (m_status == status)
        return;
    m_status = status;
    update();
}

void NodeItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    Q_UNUSED(option);
    Q_UNUSED(widget);
    painter->setRenderHint(QPainter::Antialiasing);

    const bool selected = isSelected();
    const QColor borderColor = selected ? QColor(177, 201, 214) : QColor(65, 82, 96);
    const QColor headerColor = selected ? QColor(37, 54, 67) : QColor(27, 37, 47);
    const QRectF frame(0.75, 0.75, m_width - 1.5, m_height - 1.5);

    // All decoration stays inside the item bounds, keeping redraws precise.
    painter->fillRect(frame, QColor(19, 27, 35));
    painter->fillRect(QRectF(0.75, 0.75, m_width - 1.5, m_headerHeight - 0.75), headerColor);
    painter->setBrush(Qt::NoBrush);
    painter->setPen(QPen(borderColor, selected ? 1.5 : 1.0));
    painter->drawRect(frame);
    painter->setPen(QPen(QColor(53, 69, 82), 1.0));
    painter->drawLine(QPointF(1.0, m_headerHeight), QPointF(m_width - 1.0, m_headerHeight));

    // Small registration corners echo the industrial instrument panels.
    painter->setPen(QPen(selected ? QColor(215, 230, 238) : QColor(118, 146, 163), 1.5));
    painter->drawLine(QPointF(0.75, 9.0), QPointF(0.75, 0.75));
    painter->drawLine(QPointF(0.75, 0.75), QPointF(9.0, 0.75));
    painter->drawLine(QPointF(m_width - 9.0, m_height - 0.75),
                      QPointF(m_width - 0.75, m_height - 0.75));
    painter->drawLine(QPointF(m_width - 0.75, m_height - 0.75),
                      QPointF(m_width - 0.75, m_height - 9.0));

    // 标题文字
    painter->setPen(QColor(224, 233, 239));
    painter->setFont(QFont(QStringLiteral("Microsoft YaHei"), 9, QFont::Bold));
    const qreal titleWidth = m_width - 34.0;
    const QString title = QFontMetricsF(painter->font()).elidedText(m_nodeId, Qt::ElideRight,
                                                                  titleWidth);
    painter->drawText(QRectF(10.0, 0.0, titleWidth, m_headerHeight),
                      Qt::AlignVCenter | Qt::AlignLeft, title);

    // 状态指示灯
    const QColor statusColor = m_status < 0 ? QColor(103, 124, 140)
        : m_status == 1 ? QColor(129, 183, 161) : QColor(218, 139, 129);
    painter->setPen(QPen(statusColor, 1.0));
    painter->setBrush(m_status < 0 ? QBrush(Qt::NoBrush) : QBrush(statusColor));
    painter->drawRect(QRectF(m_width - 17.5, m_headerHeight / 2.0 - 3.5, 7.0, 7.0));

    // 端口名
    painter->setFont(QFont(QStringLiteral("Microsoft YaHei"), 8));
    painter->setPen(QColor(173, 190, 203));
    const QFontMetricsF portMetrics(painter->font());
    const qreal portTextWidth = m_width / 2.0 - 18.0;

    for (int i = 0; i < m_inputPorts.size(); ++i) {
        const qreal y = m_headerHeight + m_portSpacing + i * m_portRowHeight + m_portRowHeight / 2.0;
        painter->drawText(QRectF(14.0, y - 9.0, m_width / 2.0 - 18.0, 18.0),
                          Qt::AlignVCenter | Qt::AlignLeft,
                          portMetrics.elidedText(m_inputPorts.at(i)->portName(),
                                                 Qt::ElideRight, portTextWidth));
    }
    for (int i = 0; i < m_outputPorts.size(); ++i) {
        const qreal y = m_headerHeight + m_portSpacing + i * m_portRowHeight + m_portRowHeight / 2.0;
        painter->drawText(QRectF(m_width / 2.0 + 4.0, y - 9.0, m_width / 2.0 - 18.0, 18.0),
                          Qt::AlignVCenter | Qt::AlignRight,
                          portMetrics.elidedText(m_outputPorts.at(i)->portName(),
                                                 Qt::ElideLeft, portTextWidth));
    }
}

QVariant NodeItem::itemChange(GraphicsItemChange change, const QVariant &value)
{
    if (change == ItemPositionHasChanged)
        emit nodeMoved(this);
    return QGraphicsObject::itemChange(change, value);
}

void NodeItem::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    QGraphicsObject::mousePressEvent(event);
    if (event->button() == Qt::LeftButton)
        emit nodeSelected(this);
}

void NodeItem::mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event)
{
    emit nodeDoubleClicked(this);
    QGraphicsObject::mouseDoubleClickEvent(event);
}

} // namespace OVP
