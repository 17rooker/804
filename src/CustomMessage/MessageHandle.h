#ifndef MESSAGEHANDLE_H
#define MESSAGEHANDLE_H


#include "src/Common/ConstDefine.h"

#include <QObject>
#include <QEvent>
#include <QMap>
#include <QVector>
#include <QReadWriteLock>

class IMessage;
class IEvent;

/**
 * @brief MessageHandle  事件订阅分发器
 *
 * 运行在 Qt 主线程，通过 QObject::event() 接收
 * QCoreApplication::postEvent() 投递的 IEvent，
 * 并路由给已订阅的 IMessage 实现。
 *
 * 线程安全：subMessage / unSubMessage 可从任意线程调用（内部读写锁保护）。
 */
class  MessageHandle : public QObject {
    Q_OBJECT

public:
    explicit MessageHandle(QObject* parent = nullptr);
    ~MessageHandle() override = default;

    /**
     * @brief subMessage    订阅指定子类型消息
     * @param pMsg          订阅者（需实现 IMessage）
     * @param subDataType   订阅的数据子类型
     */
    void subMessage(IMessage* pMsg,
                    ESubDataType subDataType = ESubDataType::E_RealTimeData);

    /**
     * @brief unSubMessage  取消订阅
     */
    void unSubMessage(IMessage* pMsg,
                      ESubDataType subDataType = ESubDataType::E_RealTimeData);

    /**
     * @brief unSubMessageAll  取消该订阅者的所有订阅（析构时调用）
     */
    void unSubMessageAll(IMessage* pMsg);

protected:
    bool event(QEvent* event) override;

private:
    /**
     * @brief dispatchToSubscribers  将事件分发给对应订阅者列表
     */
    void dispatchToSubscribers(IEvent* pEvent,
                               const QVector<IMessage*>& subscribers);

    /**
     * @brief shouldDeliverWhenInactive  非活跃状态下是否仍需投递
     * （超时数据、控制反馈等特殊情况）
     */
    bool shouldDeliverWhenInactive(IMessage* pMsg, IEvent* pEvent) const;

private:
    // 订阅映射：subType → 订阅者列表
    // 使用 QReadWriteLock 保护：多读单写
    QMap<ESubDataType, QVector<IMessage*>> m_subscriptions;
    mutable QReadWriteLock                  m_rwLock;
};

#endif // MESSAGEHANDLE_H
