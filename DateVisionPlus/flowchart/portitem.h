#ifndef PORTITEM_H
#define PORTITEM_H

#include "../core/plugintypes.h"

#include <QGraphicsObject>
#include <QList>

namespace OVP {

class NodeItem;

/** 节点上的输入 / 输出端口图形项 */
class PortItem : public QGraphicsObject
{
    Q_OBJECT

public:
    static constexpr qreal PortRadius = 6.0;

    PortItem(const QString &name, PortType type, bool isInput, NodeItem *node);

    QRectF boundingRect() const override;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option,
               QWidget *widget = nullptr) override;

    QString portName() const { return m_name; }
    PortType portType() const { return m_type; }
    bool isInput() const { return m_isInput; }
    NodeItem *node() const { return m_node; }

    QPointF centerInScene() const;
    bool isConnectableWith(const PortItem *other) const;

    /** 已连接的对端端口列表 */
    QList<PortItem *> connectedPorts;

signals:
    void dragStarted(PortItem *port);

protected:
    void mousePressEvent(QGraphicsSceneMouseEvent *event) override;

private:
    QString m_name;
    PortType m_type = PortType::Any;
    bool m_isInput = true;
    NodeItem *m_node = nullptr;
};

} // namespace OVP

#endif // PORTITEM_H
