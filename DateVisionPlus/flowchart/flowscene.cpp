#include "flowscene.h"
#include "connectionitem.h"
#include "nodeitem.h"
#include "portitem.h"

#include "../core/pluginmanager.h"

#include <QGraphicsSceneMouseEvent>
#include <QJsonArray>
#include <QLineF>
#include <QPainter>
#include <QPen>
#include <QRectF>
#include <QVarLengthArray>

#include <cmath>

namespace OVP {
namespace {
constexpr int kGridSize = 20;
}

FlowScene::FlowScene(QObject *parent)
    : QGraphicsScene(parent)
{
    setSceneRect(-5000.0, -5000.0, 10000.0, 10000.0);
    setBackgroundBrush(QColor(30, 30, 32));
}

void FlowScene::drawBackground(QPainter *painter, const QRectF &rect)
{
    painter->fillRect(rect, QColor(30, 30, 32));

    const qreal left = std::floor(rect.left() / kGridSize) * kGridSize;
    const qreal top = std::floor(rect.top() / kGridSize) * kGridSize;
    const qreal right = rect.right();
    const qreal bottom = rect.bottom();

    QVarLengthArray<QLineF, 256> lines;
    for (qreal x = left; x <= right; x += kGridSize)
        lines.append(QLineF(x, top, x, bottom));
    for (qreal y = top; y <= bottom; y += kGridSize)
        lines.append(QLineF(left, y, right, y));

    painter->setPen(QPen(QColor(45, 45, 48), 0.5));
    painter->drawLines(lines.data(), lines.size());
}

NodeItem *FlowScene::addPluginNode(const QString &pluginId, const QPointF &pos)
{
    const PluginManager::Info info = PluginManager::instance().info(pluginId);
    const QString baseName = info.name.isEmpty() ? pluginId : info.name;
    const int index = m_nodeCounter.value(baseName, 0) + 1;
    m_nodeCounter.insert(baseName, index);
    const QString nodeId = QStringLiteral("%1_%2").arg(baseName).arg(index);
    return addNode(pluginId, nodeId, pos);
}

NodeItem *FlowScene::addNode(const QString &pluginId, const QString &nodeId, const QPointF &pos)
{
    PluginBase *plugin = PluginManager::instance().create(pluginId, nullptr);
    if (!plugin)
        return nullptr;

    auto *node = new NodeItem(plugin, nodeId);
    plugin->setParent(node);
    node->setPos(pos);

    connect(node, &NodeItem::nodeSelected, this, &FlowScene::onNodeSelection);
    connect(node, &NodeItem::nodeMoved, this, &FlowScene::onNodeMoved);

    const QList<PortItem *> ports = node->inputPortsList() + node->outputPortsList();
    for (PortItem *port : ports)
        connect(port, &PortItem::dragStarted, this, &FlowScene::onPortDragStarted);

    addItem(node);
    m_nodes.insert(nodeId, node);
    emit nodeAdded(node);
    return node;
}

void FlowScene::removeNode(const QString &nodeId)
{
    NodeItem *node = m_nodes.value(nodeId, nullptr);
    if (!node)
        return;

    const QList<ConnectionItem *> related = m_connections;
    for (ConnectionItem *conn : related) {
        if (!m_connections.contains(conn))
            continue;
        if (conn->sourcePort() && conn->sourcePort()->node() == node)
            removeConnection(conn);
        else if (conn->targetPort() && conn->targetPort()->node() == node)
            removeConnection(conn);
    }

    removeItem(node);
    m_nodes.remove(nodeId);
    emit nodeRemoved(nodeId);
    delete node;
}

void FlowScene::removeSelected()
{
    const QList<QGraphicsItem *> items = selectedItems();

    for (QGraphicsItem *item : items) {
        if (auto *node = dynamic_cast<NodeItem *>(item)) {
            if (m_nodes.contains(node->nodeId()))
                removeNode(node->nodeId());
        }
    }
    for (QGraphicsItem *item : items) {
        if (auto *conn = dynamic_cast<ConnectionItem *>(item)) {
            if (m_connections.contains(conn))
                removeConnection(conn);
        }
    }
}

void FlowScene::clearAll()
{
    const QList<ConnectionItem *> conns = m_connections;
    for (ConnectionItem *conn : conns) {
        if (m_connections.contains(conn))
            removeConnection(conn);
    }
    m_connections.clear();

    const QStringList ids = m_nodes.keys();
    for (const QString &id : ids)
        removeNode(id);
    m_nodes.clear();
    m_nodeCounter.clear();
}

QJsonObject FlowScene::toJson() const
{
    QJsonArray nodesArray;
    for (auto it = m_nodes.constBegin(); it != m_nodes.constEnd(); ++it) {
        NodeItem *node = it.value();
        QJsonObject nodeObject;
        nodeObject.insert(QStringLiteral("id"), node->nodeId());
        nodeObject.insert(QStringLiteral("plugin_id"), node->pluginId());
        nodeObject.insert(QStringLiteral("x"), node->pos().x());
        nodeObject.insert(QStringLiteral("y"), node->pos().y());

        QJsonObject paramsObject;
        if (node->plugin()) {
            const QVariantMap params = node->plugin()->params();
            for (auto p = params.constBegin(); p != params.constEnd(); ++p)
                paramsObject.insert(p.key(), QJsonValue::fromVariant(p.value()));
        }
        nodeObject.insert(QStringLiteral("params"), paramsObject);

        QJsonObject extraObject;
        if (node->plugin()) {
            const QVariantMap extra = node->plugin()->extraData();
            for (auto e = extra.constBegin(); e != extra.constEnd(); ++e)
                extraObject.insert(e.key(), QJsonValue::fromVariant(e.value()));
        }
        nodeObject.insert(QStringLiteral("extra"), extraObject);

        nodesArray.append(nodeObject);
    }

    QJsonArray connectionsArray;
    for (ConnectionItem *conn : m_connections) {
        if (!conn->sourcePort() || !conn->targetPort())
            continue;
        QJsonObject connObject;
        connObject.insert(QStringLiteral("source_node"), conn->sourcePort()->node()->nodeId());
        connObject.insert(QStringLiteral("source_port"), conn->sourcePort()->portName());
        connObject.insert(QStringLiteral("target_node"), conn->targetPort()->node()->nodeId());
        connObject.insert(QStringLiteral("target_port"), conn->targetPort()->portName());
        connectionsArray.append(connObject);
    }

    QJsonObject root;
    root.insert(QStringLiteral("version"), QStringLiteral("2.0"));
    root.insert(QStringLiteral("nodes"), nodesArray);
    root.insert(QStringLiteral("connections"), connectionsArray);
    return root;
}

void FlowScene::fromJson(const QJsonObject &object)
{
    clearAll();

    const QJsonArray nodesArray = object.value(QStringLiteral("nodes")).toArray();
    for (const QJsonValue &value : nodesArray) {
        const QJsonObject nodeObject = value.toObject();
        const QString pluginId = nodeObject.value(QStringLiteral("plugin_id")).toString();
        const QString nodeId = nodeObject.value(QStringLiteral("id")).toString();
        if (pluginId.isEmpty() || nodeId.isEmpty())
            continue;

        const QPointF pos(nodeObject.value(QStringLiteral("x")).toDouble(0.0),
                          nodeObject.value(QStringLiteral("y")).toDouble(0.0));
        NodeItem *node = addNode(pluginId, nodeId, pos);
        if (!node || !node->plugin())
            continue;

        const QJsonObject paramsObject = nodeObject.value(QStringLiteral("params")).toObject();
        QVariantMap params;
        for (auto it = paramsObject.constBegin(); it != paramsObject.constEnd(); ++it)
            params.insert(it.key(), it.value().toVariant());
        node->plugin()->setParams(params);

        const QJsonObject extraObject = nodeObject.value(QStringLiteral("extra")).toObject();
        QVariantMap extra;
        for (auto it = extraObject.constBegin(); it != extraObject.constEnd(); ++it)
            extra.insert(it.key(), it.value().toVariant());
        if (!extra.isEmpty())
            node->plugin()->setExtraData(extra);

        // 同步节点编号计数器
        const int underscore = nodeId.lastIndexOf(QLatin1Char('_'));
        if (underscore > 0) {
            const QString namePart = nodeId.left(underscore);
            bool ok = false;
            const int num = nodeId.mid(underscore + 1).toInt(&ok);
            if (ok)
                m_nodeCounter.insert(namePart, qMax(m_nodeCounter.value(namePart, 0), num));
        }
    }

    const QJsonArray connectionsArray = object.value(QStringLiteral("connections")).toArray();
    for (const QJsonValue &value : connectionsArray) {
        const QJsonObject connObject = value.toObject();
        NodeItem *srcNode = m_nodes.value(connObject.value(QStringLiteral("source_node")).toString(), nullptr);
        NodeItem *tgtNode = m_nodes.value(connObject.value(QStringLiteral("target_node")).toString(), nullptr);
        if (!srcNode || !tgtNode)
            continue;

        const QString srcPortName = connObject.value(QStringLiteral("source_port")).toString();
        const QString tgtPortName = connObject.value(QStringLiteral("target_port")).toString();

        PortItem *srcPort = nullptr;
        for (PortItem *p : srcNode->outputPortsList()) {
            if (p->portName() == srcPortName) {
                srcPort = p;
                break;
            }
        }
        PortItem *tgtPort = nullptr;
        for (PortItem *p : tgtNode->inputPortsList()) {
            if (p->portName() == tgtPortName) {
                tgtPort = p;
                break;
            }
        }
        if (!srcPort || !tgtPort)
            continue;

        auto *conn = new ConnectionItem(srcPort, tgtPort);
        srcPort->connectedPorts.append(tgtPort);
        tgtPort->connectedPorts.append(srcPort);
        m_connections.append(conn);
        addItem(conn);
    }

    for (ConnectionItem *conn : m_connections)
        conn->updatePath();

    emit connectionChanged();
}

void FlowScene::onNodeSelection(NodeItem *node)
{
    emit nodeSelected(node);
}

void FlowScene::onNodeMoved(NodeItem *node)
{
    for (ConnectionItem *conn : m_connections) {
        if ((conn->sourcePort() && conn->sourcePort()->node() == node)
            || (conn->targetPort() && conn->targetPort()->node() == node)) {
            conn->updatePath();
        }
    }
}

void FlowScene::onPortDragStarted(PortItem *port)
{
    m_draggingPort = port;
    m_tempConnection = new ConnectionItem(port, nullptr);
    m_tempConnection->setTempEnd(port->centerInScene());
    addItem(m_tempConnection);
}

void FlowScene::finalizeConnection(PortItem *source, PortItem *target)
{
    for (ConnectionItem *conn : m_connections) {
        if (conn->sourcePort() == source && conn->targetPort() == target) {
            cancelTempConnection();
            return;
        }
    }

    if (!m_tempConnection) {
        m_tempConnection = new ConnectionItem(source, nullptr);
        addItem(m_tempConnection);
    }

    if (target->isInput() && !source->isInput()) {
        m_tempConnection->finalize(target);
        source->connectedPorts.append(target);
        target->connectedPorts.append(source);
        m_connections.append(m_tempConnection);
        m_tempConnection = nullptr;
        emit connectionChanged();
    } else if (source->isInput() && !target->isInput()) {
        m_tempConnection->finalize(source);
        target->connectedPorts.append(source);
        source->connectedPorts.append(target);
        m_connections.append(m_tempConnection);
        m_tempConnection = nullptr;
        emit connectionChanged();
    } else {
        cancelTempConnection();
    }
}

void FlowScene::cancelTempConnection()
{
    if (m_tempConnection) {
        removeItem(m_tempConnection);
        delete m_tempConnection;
        m_tempConnection = nullptr;
    }
}

void FlowScene::removeConnection(ConnectionItem *connection)
{
    if (!connection)
        return;

    PortItem *src = connection->sourcePort();
    PortItem *tgt = connection->targetPort();
    if (src && tgt) {
        src->connectedPorts.removeAll(tgt);
        tgt->connectedPorts.removeAll(src);
    }
    removeItem(connection);
    m_connections.removeAll(connection);
    delete connection;
    emit connectionChanged();
}

void FlowScene::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        QGraphicsItem *item = itemAt(event->scenePos(), QTransform());
        if (!item) {
            clearSelection();
            emit nodeDeselected();
        }
    }
    QGraphicsScene::mousePressEvent(event);
}

void FlowScene::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    if (m_tempConnection)
        m_tempConnection->setTempEnd(event->scenePos());
    QGraphicsScene::mouseMoveEvent(event);
}

void FlowScene::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    if (m_draggingPort && m_tempConnection) {
        QGraphicsItem *item = itemAt(event->scenePos(), QTransform());
        auto *targetPort = dynamic_cast<PortItem *>(item);
        if (targetPort && m_draggingPort->isConnectableWith(targetPort))
            finalizeConnection(m_draggingPort, targetPort);
        else
            cancelTempConnection();
    }

    m_draggingPort = nullptr;
    m_tempConnection = nullptr;
    QGraphicsScene::mouseReleaseEvent(event);
}

} // namespace OVP
