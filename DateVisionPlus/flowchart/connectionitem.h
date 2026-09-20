#ifndef CONNECTIONITEM_H
#define CONNECTIONITEM_H

#include <QGraphicsObject>
#include <QPainterPath>
#include <QPointF>

namespace OVP {

class PortItem;

/** 节点之间的贝塞尔连接线 */
class ConnectionItem : public QGraphicsObject
{
    Q_OBJECT

public:
    ConnectionItem(PortItem *source, PortItem *target = nullptr);

    QRectF boundingRect() const override;
    QPainterPath shape() const override;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option,
               QWidget *widget = nullptr) override;

    void setTempEnd(const QPointF &pos);
    void finalize(PortItem *target);
    void updatePath();

    PortItem *sourcePort() const { return m_source; }
    PortItem *targetPort() const { return m_target; }
    bool isTemp() const { return m_isTemp; }

private:
    PortItem *m_source = nullptr;
    PortItem *m_target = nullptr;
    QPointF m_tempEnd;
    bool m_isTemp = true;
    QPainterPath m_path;
};

} // namespace OVP

#endif // CONNECTIONITEM_H
