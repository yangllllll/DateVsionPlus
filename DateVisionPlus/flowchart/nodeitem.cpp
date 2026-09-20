#include "nodeitem.h"
#include "portitem.h"

#include <QFont>
#include <QGraphicsSceneMouseEvent>
#include <QPainter>
#include <QPainterPath>
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

    QColor headerColor(60, 60, 65);
    QColor bodyColor(35, 35, 38);
    QColor borderColor(80, 80, 85);
    qreal borderWidth = 1.0;

    if (isSelected()) {
        headerColor = QColor(0, 120, 215);
        bodyColor = QColor(45, 45, 50);
        borderColor = QColor(0, 160, 255);
        borderWidth = 2.0;
    }

    // 阴影
    QPainterPath shadow;
    shadow.addRoundedRect(QRectF(3.0, 3.0, m_width, m_height), 6.0, 6.0);
    painter->fillPath(shadow, QColor(0, 0, 0, 60));

    // 主体
    QPainterPath body;
    body.addRoundedRect(QRectF(0.0, 0.0, m_width, m_height), 6.0, 6.0);
    painter->setPen(QPen(borderColor, borderWidth));
    painter->setBrush(bodyColor);
    painter->drawPath(body);

    // 标题栏（圆角顶部）
    QPainterPath header;
    header.moveTo(6.0, m_headerHeight);
    header.lineTo(0.0, m_headerHeight);
    header.lineTo(0.0, 6.0);
    header.arcTo(0.0, 0.0, 12.0, 12.0, 180.0, -90.0);
    header.lineTo(m_width - 6.0, 0.0);
    header.arcTo(m_width - 12.0, 0.0, 12.0, 12.0, 90.0, -90.0);
    header.lineTo(m_width, m_headerHeight);
    header.closeSubpath();
    painter->setPen(Qt::NoPen);
    painter->setBrush(headerColor);
    painter->drawPath(header);

    // 标题文字
    painter->setPen(Qt::white);
    painter->setFont(QFont(QStringLiteral("Microsoft YaHei"), 9, QFont::Bold));
    painter->drawText(QRectF(6.0, 0.0, m_width - 24.0, m_headerHeight),
                      Qt::AlignVCenter | Qt::AlignLeft, m_nodeId.left(12));

    // 状态指示灯
    if (m_status >= 0) {
        const QColor dot = (m_status == 1) ? QColor(76, 175, 80) : QColor(244, 67, 54);
        painter->setPen(Qt::NoPen);
        painter->setBrush(dot);
        painter->drawEllipse(QPointF(m_width - 14.0, m_headerHeight / 2.0), 5.0, 5.0);
    }

    // 端口名
    painter->setFont(QFont(QStringLiteral("Microsoft YaHei"), 8));
    painter->setPen(QColor(200, 200, 200));

    for (int i = 0; i < m_inputPorts.size(); ++i) {
        const qreal y = m_headerHeight + m_portSpacing + i * m_portRowHeight + m_portRowHeight / 2.0;
        painter->drawText(QRectF(14.0, y - 9.0, m_width / 2.0 - 18.0, 18.0),
                          Qt::AlignVCenter | Qt::AlignLeft, m_inputPorts.at(i)->portName());
    }
    for (int i = 0; i < m_outputPorts.size(); ++i) {
        const qreal y = m_headerHeight + m_portSpacing + i * m_portRowHeight + m_portRowHeight / 2.0;
        painter->drawText(QRectF(m_width / 2.0 + 4.0, y - 9.0, m_width / 2.0 - 18.0, 18.0),
                          Qt::AlignVCenter | Qt::AlignRight, m_outputPorts.at(i)->portName());
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
