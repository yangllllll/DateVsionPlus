#include "executionengine.h"

#include <QQueue>

namespace OVP {

namespace {
constexpr int kProgressStep = 16;
}

ExecutionEngine::ExecutionEngine(QObject *parent)
    : QObject(parent)
{}

void ExecutionEngine::setup(const QMap<QString, PluginBase *> &plugins, const QList<LinkDef> &links)
{
    m_plugins = plugins;
    m_links = links;
    m_results.clear();
    m_lastError.clear();
    m_order.clear();
    m_orderValid = false;

    rebuildIndex();
}

void ExecutionEngine::rebuildIndex()
{
    m_incomingLinks.clear();
    for (int i = 0; i < m_links.size(); ++i) {
        const LinkDef &link = m_links.at(i);
        if (m_plugins.contains(link.targetNode))
            m_incomingLinks.insert(link.targetNode, i);
    }
}

QStringList ExecutionEngine::topologicalSort() const
{
    if (m_orderValid)
        return m_order;

    m_order.clear();

    QMap<QString, QStringList> adjacency;
    QMap<QString, int> inDegree;
    for (auto it = m_plugins.constBegin(); it != m_plugins.constEnd(); ++it) {
        adjacency.insert(it.key(), QStringList());
        inDegree.insert(it.key(), 0);
    }

    for (const LinkDef &link : m_links) {
        if (!m_plugins.contains(link.sourceNode) || !m_plugins.contains(link.targetNode))
            continue;
        adjacency[link.sourceNode].append(link.targetNode);
        inDegree[link.targetNode] += 1;
    }

    QQueue<QString> queue;
    for (auto it = inDegree.constBegin(); it != inDegree.constEnd(); ++it) {
        if (it.value() == 0)
            queue.enqueue(it.key());
    }

    m_order.reserve(m_plugins.size());
    while (!queue.isEmpty()) {
        const QString id = queue.dequeue();
        m_order.append(id);
        for (const QString &neighbor : adjacency.value(id)) {
            int &degree = inDegree[neighbor];
            if (--degree == 0)
                queue.enqueue(neighbor);
        }
    }

    m_orderValid = (m_order.size() == m_plugins.size());
    if (!m_orderValid)
        m_order.clear();

    return m_order;
}

bool ExecutionEngine::hasCycle() const
{
    return !m_plugins.isEmpty() && topologicalSort().isEmpty();
}

void ExecutionEngine::transferInputs(const QString &nodeId)
{
    PluginBase *target = m_plugins.value(nodeId, nullptr);
    if (!target)
        return;

    auto it = m_incomingLinks.constFind(nodeId);
    while (it != m_incomingLinks.constEnd() && it.key() == nodeId) {
        const LinkDef &link = m_links.at(it.value());
        PluginBase *source = m_plugins.value(link.sourceNode, nullptr);
        if (source) {
            const QVariant value = source->output(link.sourcePort);
            if (value.isValid())
                target->setInput(link.targetPort, value);
        }
        ++it;
    }
}

QMap<QString, QVariant> ExecutionEngine::execute()
{
    m_stop.store(false, std::memory_order_release);
    m_running.store(true, std::memory_order_release);
    m_results.clear();
    m_lastError.clear();

    const QStringList order = topologicalSort();
    if (order.isEmpty()) {
        m_lastError = QStringLiteral("流程图存在循环依赖，无法执行");
        m_results.insert(QStringLiteral("_error"), m_lastError);
        m_running.store(false, std::memory_order_release);
        return m_results;
    }

    for (const QString &nodeId : order) {
        PluginBase *plugin = m_plugins.value(nodeId, nullptr);
        if (plugin)
            plugin->reset();
    }

    QStringList errors;
    const int total = order.size();
    int finished = 0;

    for (const QString &nodeId : order) {
        if (m_stop.load(std::memory_order_acquire))
            break;

        PluginBase *plugin = m_plugins.value(nodeId, nullptr);
        if (!plugin) {
            ++finished;
            continue;
        }

        transferInputs(nodeId);

        bool ok = false;
        try {
            ok = plugin->execute();
        } catch (const std::exception &e) {
            const QString message = QString::fromUtf8(e.what());
            m_results.insert(nodeId, message);
            errors.append(QStringLiteral("%1: %2").arg(plugin->name(), message));
            emit nodeFinished(nodeId, 0);
            ++finished;
            continue;
        } catch (...) {
            const QString message = QStringLiteral("未知异常");
            m_results.insert(nodeId, message);
            errors.append(QStringLiteral("%1: %2").arg(plugin->name(), message));
            emit nodeFinished(nodeId, 0);
            ++finished;
            continue;
        }

        if (ok) {
            m_results.insert(nodeId, QVariant(plugin->outputs()));
        } else {
            const QString err = plugin->lastError().isEmpty() ? QStringLiteral("执行失败")
                                                              : plugin->lastError();
            m_results.insert(nodeId, err);
            errors.append(QStringLiteral("%1: %2").arg(plugin->name(), err));
        }

        emit nodeFinished(nodeId, ok ? 1 : 0);
        ++finished;

        if (finished == total || finished % kProgressStep == 0)
            emit progressChanged(finished, total);
    }

    if (!errors.isEmpty()) {
        m_lastError = errors.join(QStringLiteral("; "));
        m_results.insert(QStringLiteral("_error"), m_lastError);
    }

    m_running.store(false, std::memory_order_release);
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
