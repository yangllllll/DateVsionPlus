#include "executionengine.h"
#include "connectionitem.h"
#include "nodeitem.h"
#include "portitem.h"

#include "../core/pluginbase.h"

#include <QQueue>

namespace OVP {

void ExecutionEngine::setup(const QMap<QString, NodeItem *> &nodes,
                            const QList<ConnectionItem *> &connections)
{
    m_nodes = nodes;
    m_connections = connections;
    m_results.clear();
}

QStringList ExecutionEngine::topologicalSort() const
{
    QMap<QString, QStringList> adjacency;
    for (auto it = m_nodes.constBegin(); it != m_nodes.constEnd(); ++it)
        adjacency.insert(it.key(), QStringList());

    for (ConnectionItem *conn : m_connections) {
        if (!conn->sourcePort() || !conn->targetPort())
            continue;
        const QString srcId = conn->sourcePort()->node()->nodeId();
        const QString tgtId = conn->targetPort()->node()->nodeId();
        if (adjacency.contains(srcId) && m_nodes.contains(tgtId))
            adjacency[srcId].append(tgtId);
    }

    QMap<QString, int> inDegree;
    for (auto it = m_nodes.constBegin(); it != m_nodes.constEnd(); ++it)
        inDegree.insert(it.key(), 0);

    for (auto it = adjacency.constBegin(); it != adjacency.constEnd(); ++it) {
        for (const QString &target : it.value()) {
            if (inDegree.contains(target))
                inDegree[target] += 1;
        }
    }

    QQueue<QString> queue;
    for (auto it = inDegree.constBegin(); it != inDegree.constEnd(); ++it) {
        if (it.value() == 0)
            queue.enqueue(it.key());
    }

    QStringList order;
    while (!queue.isEmpty()) {
        const QString id = queue.dequeue();
        order.append(id);
        for (const QString &neighbor : adjacency.value(id)) {
            int &degree = inDegree[neighbor];
            degree -= 1;
            if (degree == 0)
                queue.enqueue(neighbor);
        }
    }

    if (order.size() != m_nodes.size())
        return QStringList();

    return order;
}

bool ExecutionEngine::hasCycle() const
{
    return !m_nodes.isEmpty() && topologicalSort().isEmpty();
}

void ExecutionEngine::transferData()
{
    for (ConnectionItem *conn : m_connections) {
        if (!conn->sourcePort() || !conn->targetPort())
            continue;
        NodeItem *srcNode = conn->sourcePort()->node();
        NodeItem *tgtNode = conn->targetPort()->node();
        if (!srcNode || !tgtNode || !srcNode->plugin() || !tgtNode->plugin())
            continue;

        const QVariant value = srcNode->plugin()->output(conn->sourcePort()->portName());
        if (value.isValid())
            tgtNode->plugin()->setInput(conn->targetPort()->portName(), value);
    }
}

QMap<QString, QVariant> ExecutionEngine::execute()
{
    m_running = true;
    m_results.clear();
    m_lastError.clear();

    const QStringList order = topologicalSort();
    if (order.isEmpty()) {
        m_lastError = QStringLiteral("流程图存在循环依赖，无法执行");
        m_results.insert(QStringLiteral("_error"), m_lastError);
        m_running = false;
        return m_results;
    }

    for (const QString &nodeId : order) {
        NodeItem *node = m_nodes.value(nodeId, nullptr);
        if (!node)
            continue;
        if (node->plugin())
            node->plugin()->reset();
        node->setStatus(-1);
    }

    QStringList errors;
    for (const QString &nodeId : order) {
        if (!m_running)
            break;

        NodeItem *node = m_nodes.value(nodeId, nullptr);
        if (!node || !node->plugin())
            continue;

        transferData();

        bool ok = false;
        try {
            ok = node->plugin()->execute();
        } catch (const std::exception &e) {
            const QString message = QString::fromUtf8(e.what());
            m_results.insert(nodeId, message);
            errors.append(QStringLiteral("%1: %2").arg(node->pluginName(), message));
            node->setStatus(0);
            continue;
        } catch (...) {
            const QString message = QStringLiteral("未知异常");
            m_results.insert(nodeId, message);
            errors.append(QStringLiteral("%1: %2").arg(node->pluginName(), message));
            node->setStatus(0);
            continue;
        }

        if (ok) {
            m_results.insert(nodeId, QVariant(node->plugin()->outputs()));
            node->setStatus(1);
        } else {
            const QString err = node->plugin()->lastError().isEmpty()
                                    ? QStringLiteral("执行失败")
                                    : node->plugin()->lastError();
            m_results.insert(nodeId, err);
            errors.append(QStringLiteral("%1: %2").arg(node->pluginName(), err));
            node->setStatus(0);
        }
    }

    if (!errors.isEmpty()) {
        m_lastError = errors.join(QStringLiteral("; "));
        m_results.insert(QStringLiteral("_error"), m_lastError);
    }

    m_running = false;
    return m_results;
}

QVariantMap ExecutionEngine::nodeResults(const QString &nodeId) const
{
    const QVariant value = m_results.value(nodeId);
    if (value.isValid() && value.metaType().id() == QMetaType::QVariantMap)
        return value.toMap();
    return QVariantMap();
}

} // namespace OVP
