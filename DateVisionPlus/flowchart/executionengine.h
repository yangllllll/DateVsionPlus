#ifndef EXECUTIONENGINE_H
#define EXECUTIONENGINE_H

#include "../core/pluginbase.h"

#include <QList>
#include <QMap>
#include <QMultiHash>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariant>

#include <atomic>

namespace OVP {

/**
 * @brief 运行期连线描述
 *
 * 只保留节点 id 与端口名，不引用任何 QGraphicsItem，
 * 因此整张图可以在后台线程安全地参与运算。
 */
struct LinkDef
{
    QString sourceNode;
    QString sourcePort;
    QString targetNode;
    QString targetPort;
};

/**
 * @brief 执行引擎：按拓扑顺序执行流程图
 *
 * 本类与界面完全解耦（只依赖 PluginBase），可以在任意线程运行。
 * 节点执行结果通过 nodeFinished() / progressChanged() 信号上报，
 * 由 GUI 线程负责刷新指示灯。
 *
 * execute() 返回 nodeId -> QVariant 的映射，
 * 其中 QVariant 为 QVariantMap（成功时的输出端口数据）或 QString（错误信息）。
 */
class ExecutionEngine : public QObject
{
    Q_OBJECT

public:
    explicit ExecutionEngine(QObject *parent = nullptr);

    void setup(const QMap<QString, PluginBase *> &plugins, const QList<LinkDef> &links);

    /** 拓扑排序结果；存在环时返回空列表。结果会被缓存，重复调用无开销 */
    QStringList topologicalSort() const;
    bool hasCycle() const;

    QMap<QString, QVariant> execute();

    const QMap<QString, QVariant> &results() const { return m_results; }
    QVariantMap nodeResults(const QString &nodeId) const;
    QString lastError() const { return m_lastError; }

    /** 可从任意线程调用 */
    void requestStop() { m_stop.store(true, std::memory_order_release); }
    bool isRunning() const { return m_running.load(std::memory_order_acquire); }

signals:
    /** status: 1 成功，0 失败 */
    void nodeFinished(const QString &nodeId, int status);
    /** 进度上报按节点数节流，避免高频刷新界面 */
    void progressChanged(int finished, int total);

private:
    void rebuildIndex();
    /** 只把指向 nodeId 的连线数据搬进该节点，复杂度与入度成正比 */
    void transferInputs(const QString &nodeId);

    QMap<QString, PluginBase *> m_plugins;
    QList<LinkDef> m_links;
    /** 目标节点 -> 连线下标，避免每执行一个节点都全量扫描连线 */
    QMultiHash<QString, int> m_incomingLinks;

    mutable QStringList m_order;
    mutable bool m_orderValid = false;

    QMap<QString, QVariant> m_results;
    QString m_lastError;

    std::atomic<bool> m_stop{false};
    std::atomic<bool> m_running{false};
};

} // namespace OVP

#endif // EXECUTIONENGINE_H
