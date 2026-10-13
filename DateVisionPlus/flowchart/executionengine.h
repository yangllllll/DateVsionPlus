#ifndef EXECUTIONENGINE_H
#define EXECUTIONENGINE_H

#include "../core/pluginbase.h"

#include <QList>
#include <QMap>
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

    /** 拓扑排序结果；存在环时返回空列表。无环与有环结果均缓存到下次 setup() */
    QStringList topologicalSort() const;
    bool hasCycle() const;

    /** 在投递后台任务前调用，防止排队期间收到的停止请求被 execute() 清除 */
    void prepareRun() { m_stop.store(false, std::memory_order_release); }
    QMap<QString, QVariant> execute(bool resetStop = true);

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
    struct InputBinding
    {
        PluginBase *source = nullptr;
        QString sourcePort;
        QString targetPort;
    };
    struct NodePlan
    {
        QString id;
        PluginBase *plugin = nullptr;
        QList<InputBinding> inputs;
        QList<int> outgoing;
        int inDegree = 0;
    };

    /** setup() 解析节点和端口绑定；执行热路径只访问编译后的索引与指针 */
    void rebuildIndex(const QMap<QString, PluginBase *> &plugins, const QList<LinkDef> &links);
    void transferInputs(const NodePlan &node);

    QList<NodePlan> m_nodes;

    mutable QStringList m_order;
    mutable QList<int> m_executionOrder;
    mutable bool m_orderValid = false;
    mutable bool m_hasCycle = false;

    QMap<QString, QVariant> m_results;
    QString m_lastError;

    std::atomic<bool> m_stop{false};
    std::atomic<bool> m_running{false};
};

} // namespace OVP

#endif // EXECUTIONENGINE_H
