#ifndef EXECUTIONENGINE_H
#define EXECUTIONENGINE_H

#include <QList>
#include <QMap>
#include <QString>
#include <QStringList>
#include <QVariant>

namespace OVP {

class NodeItem;
class ConnectionItem;

/**
 * @brief 执行引擎：按拓扑顺序执行流程图
 *
 * execute() 返回 nodeId -> QVariant 的映射，
 * 其中 QVariant 为 QVariantMap（成功时的输出端口数据）或 QString（错误信息）。
 */
class ExecutionEngine
{
public:
    void setup(const QMap<QString, NodeItem *> &nodes, const QList<ConnectionItem *> &connections);

    /** 拓扑排序结果；存在环时返回空列表 */
    QStringList topologicalSort() const;
    bool hasCycle() const;

    QMap<QString, QVariant> execute();

    QVariantMap nodeResults(const QString &nodeId) const;
    QString lastError() const { return m_lastError; }

    void requestStop() { m_running = false; }
    bool isRunning() const { return m_running; }

private:
    void transferData();

    QMap<QString, NodeItem *> m_nodes;
    QList<ConnectionItem *> m_connections;
    QMap<QString, QVariant> m_results;
    QString m_lastError;
    bool m_running = false;
};

} // namespace OVP

#endif // EXECUTIONENGINE_H
