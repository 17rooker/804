/*******************************************************************************
 * @File: ScheduledSendService.h
 * @Description: 定时发送服务——管理多个周期性网络数据发送任务
 *******************************************************************************/

#ifndef SCHEDULEDSENDSERVICE_H
#define SCHEDULEDSENDSERVICE_H

#include <QObject>
#include <QTimer>
#include "src/DataProcess/ScheduledTask.h"
#include "src/DataProcess/FrameDataBuilder.h"
#include "src/CustomMessage/DataInteractionManager.h"

class QThread;
class ScheduledSendWorker;


// ============================================================================
// ScheduledSendWorker  ——  仅在工作线程中运行的内部对象
//
// 线程归属：moveToThread(m_workerThread) 后，本对象的所有方法均在工作线程执行。
// 外部通过 QMetaObject::invokeMethod(..., Qt::QueuedConnection) 跨线程调用。
// m_tasks 只在工作线程访问，无需额外加锁。
// ============================================================================
class ScheduledSendWorker : public QObject
{
    Q_OBJECT

public:
    explicit ScheduledSendWorker(QObject* parent = nullptr) : QObject(parent) {}

    // ── 以下方法均在工作线程执行 ──────────────────────────────────────

    void addTaskImpl(const ScheduledTask& task)
    {
        // 同名任务已存在时先移除旧任务
        removeTaskImpl(task.taskName);

        auto* timer = new QTimer(this);  // parent = this，随本对象一同销毁
        timer->setInterval(task.intervalMs);
        timer->setSingleShot(false);  // 循环触发，单次逻辑由 fireTask 内部处理

        const QString name = task.taskName;
        connect(timer, &QTimer::timeout, this, [this, name]() {
            fireTask(name);
        });

        m_tasks[task.taskName] = TaskEntry{ task, timer };
        timer->start();
    }

    void removeTaskImpl(const QString& taskName)
    {
        auto it = m_tasks.find(taskName);
        if (it == m_tasks.end()) return;
        it->timer->stop();
        delete it->timer;
        m_tasks.erase(it);
    }

    void pauseTaskImpl(const QString& taskName)
    {
        auto it = m_tasks.find(taskName);
        if (it != m_tasks.end() && it->timer->isActive())
            it->timer->stop();
    }

    void resumeTaskImpl(const QString& taskName)
    {
        auto it = m_tasks.find(taskName);
        if (it != m_tasks.end() && !it->timer->isActive())
            it->timer->start();
    }

    void updateIntervalImpl(const QString& taskName, int newIntervalMs)
    {
        auto it = m_tasks.find(taskName);
        if (it == m_tasks.end()) return;
        it->task.intervalMs = newIntervalMs;
        it->timer->setInterval(newIntervalMs);
    }

    /** @brief clearAll  停止并销毁所有任务（stop() 时在工作线程中调用） */
    void clearAll()
    {
        for (auto& entry : m_tasks) {
            entry.timer->stop();
            delete entry.timer;
            entry.timer = nullptr;
        }
        m_tasks.clear();
    }

signals:
    void taskFired(const QString& taskName);
    void taskSendFailed(const QString& taskName, const QString& reason);

private:
    void fireTask(const QString& taskName)
    {
        auto it = m_tasks.find(taskName);
        if (it == m_tasks.end()) return;

        const ScheduledTask& task = it->task;
        STDataPrcSendMsg     msg;

        if (task.frameBuilder) {
            // ── 自定义模式：由调用方 lambda 构帧 ──────────────────────
            auto result = task.frameBuilder();
            if (!result.has_value()) return;  // 返回 nullopt 表示本次跳过
            msg = std::move(result.value());
        } else {
            // ── 简单模式：由 FrameDataBuilder 自动构帧 ────────────────
            QByteArray frame = FrameDataBuilder::build(
                task.channelId, task.frameValues, task.bigEndian);
            if (frame.isEmpty()) {
                emit taskSendFailed(taskName,
                                    QStringLiteral("FrameDataBuilder returned empty frame"));
                return;
            }
            msg.channelId   = task.channelId;
            msg.channelType = task.channelType;
            msg.eDataType   = task.eDataType;
            msg.btData      = frame;
        }

        // DataInteractionManager::sendMsg() 任意线程安全
        DataInteractionManager::getInstance().sendMsg(msg);
        emit taskFired(taskName);

        // 单次任务：发完即移除（timer 已 stop，需清理 entry）
        if (task.singleShot) {
            it->timer->stop();
            delete it->timer;
            m_tasks.erase(it);
        }
    }

    struct TaskEntry {
        ScheduledTask task;
        QTimer*       timer = nullptr;
    };

    QMap<QString, TaskEntry> m_tasks;  // 仅工作线程访问
};

/**
 * @brief ScheduledSendService  定时发送服务
 *
 * 架构：
 *   本对象驻留在调用线程（主线程），内部持有一个独立 QThread。
 *   所有 QTimer 均在工作线程中运行，timeout 时在工作线程调用
 *   DataInteractionManager::sendMsg()（该接口任意线程安全）。
 *
 * 线程安全：
 *   addTask / removeTask / pauseTask / resumeTask / updateInterval
 *   均可在任意线程调用，内部通过 QMetaObject::invokeMethod（QueuedConnection）
 *   将操作派发到工作线程执行，m_tasks 仅在工作线程访问，无需额外加锁。
 *
 * 生命周期：
 *   与 CommManager 平行，在 main.cpp 中 start() / stop()；
 *   析构时若未调用 stop() 则自动停止。
 *
 * 典型用法（main.cpp）：
 * @code
 *   ScheduledSendService sss;
 *
 *   ScheduledTask heartbeat;
 *   heartbeat.taskName    = "heartbeat";
 *   heartbeat.intervalMs  = 5000;
 *   heartbeat.channelId   = tcpChannelId;
 *   heartbeat.channelType = EChannelType::TCP;
 *   heartbeat.eDataType   = EDataType::E_Control;
 *   heartbeat.frameValues["ZLX"] = 0xF3;
 *   sss.addTask(heartbeat);
 *
 *   sss.start();
 *   comm.start();
 *   a.exec();
 *   sss.stop();
 * @endcode
 */
class ScheduledSendService : public QObject
{
    Q_OBJECT

public:
    explicit ScheduledSendService(QObject* parent = nullptr);
    ~ScheduledSendService() override;

    // ── 生命周期（主线程调用）──────────────────────────────────────────
    /** @brief start  启动工作线程，之后可调用 addTask 注册任务 */
    void start();

    /** @brief stop   停止所有定时器并退出工作线程（析构时自动调用） */
    void stop();

    // ── 任务管理（任意线程安全调用）──────────────────────────────────
    /**
     * @brief addTask  注册并立即启动定时任务
     * 若同名任务已存在，先停止旧任务再添加新任务。
     */
    void addTask(const ScheduledTask& task);

    /** @brief removeTask  停止并移除指定任务 */
    void removeTask(const QString& taskName);

    /** @brief pauseTask   暂停指定任务（不销毁，可 resume） */
    void pauseTask(const QString& taskName);

    /** @brief resumeTask  恢复已暂停的任务 */
    void resumeTask(const QString& taskName);

    /** @brief updateInterval  运行时修改发送间隔（毫秒） */
    void updateInterval(const QString& taskName, int newIntervalMs);

signals:
    /** @brief taskFired       每次定时发送触发时发出（供调试/监控） */
    void taskFired(const QString& taskName);

    /** @brief taskSendFailed  构帧失败或发送异常时发出 */
    void taskSendFailed(const QString& taskName, const QString& reason);

private:
    QThread*             m_workerThread = nullptr;
    ScheduledSendWorker* m_worker       = nullptr;
};

#endif // SCHEDULEDSENDSERVICE_H
