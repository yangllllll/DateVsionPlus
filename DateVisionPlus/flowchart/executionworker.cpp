#include "executionworker.h"

#include <utility>

namespace OVP {

ExecutionWorker::ExecutionWorker(QObject *parent)
    : QObject(parent)
    , m_engine(this)
{
    connect(&m_engine, &ExecutionEngine::nodeFinished, this,
            [this](const QString &nodeId, int status) {
                if (m_reportProgress)
                    emit nodeFinished(nodeId, status);
            });
    connect(&m_engine, &ExecutionEngine::progressChanged, this,
            [this](int finished, int total) {
                if (m_reportProgress)
                    emit progressChanged(finished, total);
            });
}

void ExecutionWorker::setGraph(const QMap<QString, PluginBase *> &plugins,
                               const QList<LinkDef> &links)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_plugins = plugins;
    m_links = links;
    m_graphDirty = true;
}

bool ExecutionWorker::prepareRun(bool reportProgress)
{
    bool expected = false;
    if (!m_busy.compare_exchange_strong(expected, true, std::memory_order_acq_rel))
        return false;
    m_reportProgress = reportProgress;
    m_engine.prepareRun();
    return true;
}

void ExecutionWorker::run()
{
    // 先把图数据拷出来再解锁：避免执行期间 GUI 线程调用 setGraph() 被长时间阻塞
    QMap<QString, PluginBase *> plugins;
    QList<LinkDef> links;
    bool graphDirty = false;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        graphDirty = m_graphDirty;
        if (graphDirty) {
            plugins = m_plugins;
            links = m_links;
            m_graphDirty = false;
        }
    }

    if (graphDirty)
        m_engine.setup(plugins, links);
    if (m_reportProgress)
        emit runStarted(m_engine.topologicalSort().size());

    const QMap<QString, QVariant> results = m_engine.execute(false);
    const QStringList order = m_engine.topologicalSort();

    m_busy.store(false, std::memory_order_release);
    emit runFinished(results, order);
}

} // namespace OVP
