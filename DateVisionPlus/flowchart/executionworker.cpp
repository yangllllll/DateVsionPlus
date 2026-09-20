#include "executionworker.h"

#include <utility>

namespace OVP {

ExecutionWorker::ExecutionWorker(QObject *parent)
    : QObject(parent)
    , m_engine(this)
{
    connect(&m_engine, &ExecutionEngine::nodeFinished,
            this, &ExecutionWorker::nodeFinished);
    connect(&m_engine, &ExecutionEngine::progressChanged,
            this, &ExecutionWorker::progressChanged);
}

void ExecutionWorker::setGraph(const QMap<QString, PluginBase *> &plugins,
                               const QList<LinkDef> &links)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_plugins = plugins;
    m_links = links;
}

void ExecutionWorker::run()
{
    // 先把图数据拷出来再解锁：避免执行期间 GUI 线程调用 setGraph() 被长时间阻塞
    QMap<QString, PluginBase *> plugins;
    QList<LinkDef> links;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        plugins = m_plugins;
        links = m_links;
    }

    m_busy.store(true, std::memory_order_release);

    m_engine.setup(plugins, links);
    const int total = m_engine.topologicalSort().size();
    emit runStarted(total);

    const QMap<QString, QVariant> results = m_engine.execute();
    const QStringList order = m_engine.topologicalSort();

    emit runFinished(results, order);

    m_busy.store(false, std::memory_order_release);
}

} // namespace OVP
