#include "executionengine.h"

#include <QHash>

#include <exception>
#include <utility>

namespace OVP {

namespace {
constexpr int kProgressStep = 16;

class RunningGuard
{
public:
    explicit RunningGuard(std::atomic<bool> &running) : m_running(running)
    {
        m_running.store(true, std::memory_order_release);
    }
    ~RunningGuard() { m_running.store(false, std::memory_order_release); }

private:
    std::atomic<bool> &m_running;
};
}

ExecutionEngine::ExecutionEngine(QObject *parent)
    : QObject(parent)
{}

void ExecutionEngine::setup(const QMap<QString, PluginBase *> &plugins, const QList<LinkDef> &links)
{
    m_results.clear();
    m_lastError.clear();
    m_order.clear();
    m_executionOrder.clear();
    m_orderValid = false;
    m_hasCycle = false;

    rebuildIndex(plugins, links);
}

void ExecutionEngine::rebuildIndex(const QMap<QString, PluginBase *> &plugins,
                                   const QList<LinkDef> &links)
{
    m_nodes.clear();
    m_nodes.reserve(plugins.size());
    QHash<QString, int> indices;
    indices.reserve(plugins.size());
    // 保留 QMap 的节点顺序，使无依赖节点的执行顺序与之前一致。
    for (auto it = plugins.constBegin(); it != plugins.constEnd(); ++it) {
        indices.insert(it.key(), m_nodes.size());
        NodePlan node;
        node.id = it.key();
        node.plugin = it.value();
        m_nodes.append(std::move(node));
    }

    for (const LinkDef &link : links) {
        const auto source = indices.constFind(link.sourceNode);
        const auto target = indices.constFind(link.targetNode);
        if (source == indices.constEnd() || target == indices.constEnd())
            continue;
        NodePlan &sourceNode = m_nodes[source.value()];
        NodePlan &targetNode = m_nodes[target.value()];
        sourceNode.outgoing.append(target.value());
        ++targetNode.inDegree;
        if (sourceNode.plugin)
            targetNode.inputs.append({sourceNode.plugin, link.sourcePort, link.targetPort});
    }
}

QStringList ExecutionEngine::topologicalSort() const
{
    if (m_orderValid)
        return m_order;

    m_order.clear();

    QList<int> inDegree;
    inDegree.reserve(m_nodes.size());
    m_executionOrder.clear();
    m_executionOrder.reserve(m_nodes.size());
    for (int i = 0; i < m_nodes.size(); ++i) {
        const int degree = m_nodes.at(i).inDegree;
        inDegree.append(degree);
        if (degree == 0)
            m_executionOrder.append(i);
    }

    // 索引队列同时保存执行顺序；不再为每条边查询/修改 QString 键映射。
    for (int cursor = 0; cursor < m_executionOrder.size(); ++cursor) {
        const NodePlan &node = m_nodes.at(m_executionOrder.at(cursor));
        for (int neighbor : node.outgoing) {
            if (--inDegree[neighbor] == 0)
                m_executionOrder.append(neighbor);
        }
    }

    m_hasCycle = (m_executionOrder.size() != m_nodes.size());
    m_orderValid = true;
    if (m_hasCycle) {
        m_executionOrder.clear();
    } else {
        m_order.reserve(m_nodes.size());
        for (int index : std::as_const(m_executionOrder))
            m_order.append(m_nodes.at(index).id);
    }

    return m_order;
}

bool ExecutionEngine::hasCycle() const
{
    topologicalSort();
    return m_hasCycle;
}

void ExecutionEngine::transferInputs(const NodePlan &node)
{
    // 原 QMultiHash 在同一目标节点下按逆插入顺序遍历，保留重复端口的覆盖规则。
    for (auto it = node.inputs.crbegin(); it != node.inputs.crend(); ++it) {
        const QVariant value = it->source->output(it->sourcePort);
        if (value.isValid())
            node.plugin->setInput(it->targetPort, value);
    }
}

QMap<QString, QVariant> ExecutionEngine::execute(bool resetStop)
{
    if (resetStop)
        prepareRun();
    const RunningGuard runningGuard(m_running);
    m_results.clear();
    m_lastError.clear();

    topologicalSort();
    if (m_hasCycle) {
        m_lastError = QStringLiteral("流程图存在循环依赖，无法执行");
        m_results.insert(QStringLiteral("_error"), m_lastError);
        return m_results;
    }

    for (int index : std::as_const(m_executionOrder)) {
        if (m_stop.load(std::memory_order_acquire))
            return m_results;
        const NodePlan &node = m_nodes.at(index);
        if (!node.plugin)
            continue;
        try {
            node.plugin->reset();
        } catch (const std::exception &e) {
            m_lastError = QStringLiteral("%1: %2").arg(node.plugin->name(), QString::fromUtf8(e.what()));
        } catch (...) {
            m_lastError = QStringLiteral("%1: 重置时发生未知异常").arg(node.plugin->name());
        }
        if (!m_lastError.isEmpty()) {
            m_results.insert(node.id, m_lastError);
            m_results.insert(QStringLiteral("_error"), m_lastError);
            emit nodeFinished(node.id, 0);
            return m_results;
        }
    }

    QStringList errors;
    const int total = m_executionOrder.size();
    int finished = 0;
    int lastReported = 0;

    for (int index : std::as_const(m_executionOrder)) {
        if (m_stop.load(std::memory_order_acquire))
            break;

        const NodePlan &node = m_nodes.at(index);
        PluginBase *plugin = node.plugin;
        if (plugin) {
            bool ok = false;
            QString error;
            try {
                transferInputs(node);
                ok = plugin->execute();
                if (!ok)
                    error = plugin->lastError().isEmpty() ? QStringLiteral("执行失败")
                                                          : plugin->lastError();
            } catch (const std::exception &e) {
                error = QString::fromUtf8(e.what());
            } catch (...) {
                error = QStringLiteral("未知异常");
            }

            if (ok) {
                m_results.insert(node.id, QVariant(plugin->outputs()));
            } else {
                m_results.insert(node.id, error);
                errors.append(QStringLiteral("%1: %2").arg(plugin->name(), error));
            }
            emit nodeFinished(node.id, ok ? 1 : 0);
        }

        ++finished;

        if (finished == total || finished % kProgressStep == 0) {
            emit progressChanged(finished, total);
            lastReported = finished;
        }
    }
    if (finished != lastReported)
        emit progressChanged(finished, total);

    if (!errors.isEmpty()) {
        m_lastError = errors.join(QStringLiteral("; "));
        m_results.insert(QStringLiteral("_error"), m_lastError);
    }

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
