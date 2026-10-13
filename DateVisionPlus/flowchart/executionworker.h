#ifndef EXECUTIONWORKER_H
#define EXECUTIONWORKER_H

#include "executionengine.h"

#include <QMap>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariant>

#include <atomic>
#include <mutex>

namespace OVP {

/**
 * @brief 后台执行器：把流程图的计算过程移出 GUI 线程
 *
 * 用法：对象本身 moveToThread 到工作线程，GUI 线程在空闲期调用 setGraph()
 * 更新图数据，prepareRun() 成功后通过（队列连接的）run() 槽触发一次执行。
 * 图不变时无需重复 setGraph()；每次 prepareRun() 只能投递一个 run()。
 * 结果通过 runFinished() 信号回到 GUI 线程。
 */
class ExecutionWorker : public QObject
{
    Q_OBJECT

public:
    explicit ExecutionWorker(QObject *parent = nullptr);

    /** 由 GUI 线程在空闲期调用；内部仅做一次短暂加锁，不会阻塞 */
    void setGraph(const QMap<QString, PluginBase *> &plugins, const QList<LinkDef> &links);

    /** 在投递 run() 前调用；预留本轮执行，并在排队之前清除上轮停止状态。 */
    bool prepareRun(bool reportProgress = true);

    /** 中止当前执行；可从任意线程调用 */
    void requestStop() { m_engine.requestStop(); }

    bool isBusy() const { return m_busy.load(std::memory_order_acquire); }

public slots:
    void run();

signals:
    void runStarted(int total);
    void nodeFinished(const QString &nodeId, int status);
    void progressChanged(int finished, int total);
    void runFinished(const QMap<QString, QVariant> &results, const QStringList &order);

private:
    mutable std::mutex m_mutex;
    ExecutionEngine m_engine;
    QMap<QString, PluginBase *> m_plugins;
    QList<LinkDef> m_links;
    bool m_graphDirty = true;
    bool m_reportProgress = true;
    std::atomic<bool> m_busy{false};
};

} // namespace OVP

#endif // EXECUTIONWORKER_H
