#ifndef NODEITEM_H
#define NODEITEM_H

#include "../core/pluginbase.h"

#include <QGraphicsObject>
#include <QList>
#include <QString>

namespace OVP {

class PortItem;

/** 流程图节点（一个插件实例的可视化载体） */
class NodeItem : public QGraphicsObject
{
    Q_OBJECT

public:
    NodeItem(PluginBase *plugin, const QString &nodeId, QGraphicsItem *parent = nullptr);
    ~NodeItem() override;

    QRectF boundingRect() const override;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option,
               QWidget *widget = nullptr) override;

    QString nodeId() const { return m_nodeId; }
    QString pluginId() const { return m_plugin ? m_plugin->id() : QString(); }
    QString pluginName() const { return m_plugin ? m_plugin->name() : QString(); }
    PluginBase *plugin() const { return m_plugin; }

    QList<PortItem *> inputPortsList() const { return m_inputPorts; }
    QList<PortItem *> outputPortsList() const { return m_outputPorts; }

    /** 执行状态：-1 未执行, 0 NG, 1 OK */
    int status() const { return m_status; }
    void setStatus(int status);

signals:
    void nodeSelected(NodeItem *node);
    void nodeMoved(NodeItem *node);
    void nodeDoubleClicked(NodeItem *node);

protected:
    QVariant itemChange(GraphicsItemChange change, const QVariant &value) override;
    void mousePressEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event) override;

private:
    void buildPorts();
    void updateGeometry();

    PluginBase *m_plugin = nullptr;
    QString m_nodeId;
    QList<PortItem *> m_inputPorts;
    QList<PortItem *> m_outputPorts;

    qreal m_width = 140.0;
    qreal m_height = 60.0;
    qreal m_headerHeight = 28.0;
    qreal m_portRowHeight = 22.0;
    qreal m_portSpacing = 4.0;
    int m_status = -1;
};

} // namespace OVP

#endif // NODEITEM_H
