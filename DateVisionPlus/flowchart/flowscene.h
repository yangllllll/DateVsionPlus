#ifndef FLOWSCENE_H
#define FLOWSCENE_H

#include <QGraphicsScene>
#include <QJsonObject>
#include <QList>
#include <QMap>
#include <QPointF>
#include <QString>

namespace OVP {

class NodeItem;
class PortItem;
class ConnectionItem;

/** 流程图场景：管理节点、连线与序列化 */
class FlowScene : public QGraphicsScene
{
    Q_OBJECT

public:
    explicit FlowScene(QObject *parent = nullptr);

    NodeItem *addPluginNode(const QString &pluginId, const QPointF &pos = QPointF());
    NodeItem *addNode(const QString &pluginId, const QString &nodeId, const QPointF &pos);
    void removeNode(const QString &nodeId);
    void removeSelected();
    void clearAll();

    QMap<QString, NodeItem *> nodes() const { return m_nodes; }
    QList<ConnectionItem *> connections() const { return m_connections; }

    QJsonObject toJson() const;
    void fromJson(const QJsonObject &object);

signals:
    void nodeAdded(NodeItem *node);
    void nodeRemoved(const QString &nodeId);
    void nodeSelected(NodeItem *node);
    void nodeDeselected();
    void connectionChanged();

private slots:
    void onPortDragStarted(PortItem *port);
    void onNodeMoved(NodeItem *node);
    void onNodeSelection(NodeItem *node);

protected:
    void mousePressEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseMoveEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override;

private:
    void drawGrid();
    void finalizeConnection(PortItem *source, PortItem *target);
    void cancelTempConnection();
    void removeConnection(ConnectionItem *connection);

    QMap<QString, NodeItem *> m_nodes;
    QList<ConnectionItem *> m_connections;
    QMap<QString, int> m_nodeCounter;

    PortItem *m_draggingPort = nullptr;
    ConnectionItem *m_tempConnection = nullptr;
};

} // namespace OVP

#endif // FLOWSCENE_H
