/*******************************************************************************
 * @File: ScheduledSendService.cpp
 * @Description: 定时发送服务实现
 *******************************************************************************/

#include "ScheduledSendService.h"

#include <QThread>
#include <QMap>
#include <QMetaObject>


// ============================================================================
// ScheduledSendService  ——  驻留主线程的公开接口对象
// ============================================================================

ScheduledSendService::ScheduledSendService(QObject* parent)
    : QObject(parent)
{}

ScheduledSendService::~ScheduledSendService()
{
    stop();
}

void ScheduledSendService::start()
{
    if (m_workerThread) return;  // 防止重复启动

    m_worker       = new ScheduledSendWorker();       // 无 parent，手动管理生命周期
    m_workerThread = new QThread(this);               // parent = this，随本对象析构
    m_workerThread->setObjectName("ScheduledSendWorker");

    m_worker->moveToThread(m_workerThread);

    // 将工作对象的信号转发给本对象的信号（跨线程，QueuedConnection）
    connect(m_worker, &ScheduledSendWorker::taskFired,
            this,     &ScheduledSendService::taskFired,
            Qt::QueuedConnection);
    connect(m_worker, &ScheduledSendWorker::taskSendFailed,
            this,     &ScheduledSendService::taskSendFailed,
            Qt::QueuedConnection);

    m_workerThread->start();
}

void ScheduledSendService::stop()
{
    if (!m_workerThread || !m_workerThread->isRunning()) return;

    // 1. 在工作线程中停止并销毁所有定时器（阻塞等待完成）
    QMetaObject::invokeMethod(m_worker, [worker = m_worker]() {
        worker->clearAll();
    }, Qt::BlockingQueuedConnection);

    // 2. 退出工作线程事件循环
    m_workerThread->quit();
    m_workerThread->wait(3000);

    // 3. 工作线程已停止，可安全 delete（m_worker 此时无子对象、无 pending 事件）
    delete m_worker;
    m_worker = nullptr;
    // m_workerThread 由 QObject 父子关系管理，随 this 析构
}

void ScheduledSendService::addTask(const ScheduledTask& task)
{
    QMetaObject::invokeMethod(m_worker, [worker = m_worker, task]() {
        worker->addTaskImpl(task);
    }, Qt::QueuedConnection);
}

void ScheduledSendService::removeTask(const QString& taskName)
{
    QMetaObject::invokeMethod(m_worker, [worker = m_worker, taskName]() {
        worker->removeTaskImpl(taskName);
    }, Qt::QueuedConnection);
}

void ScheduledSendService::pauseTask(const QString& taskName)
{
    QMetaObject::invokeMethod(m_worker, [worker = m_worker, taskName]() {
        worker->pauseTaskImpl(taskName);
    }, Qt::QueuedConnection);
}

void ScheduledSendService::resumeTask(const QString& taskName)
{
    QMetaObject::invokeMethod(m_worker, [worker = m_worker, taskName]() {
        worker->resumeTaskImpl(taskName);
    }, Qt::QueuedConnection);
}

void ScheduledSendService::updateInterval(const QString& taskName, int newIntervalMs)
{
    QMetaObject::invokeMethod(m_worker,
                              [worker = m_worker, taskName, newIntervalMs]() {
        worker->updateIntervalImpl(taskName, newIntervalMs);
    }, Qt::QueuedConnection);
}
