#include "MessageHandle.h"
#include "IMessage.h"
#include "IEvent.h"
#include "src/CustomEvent/InternalMsgEvent.h".h"

#include <QWriteLocker>
#include <QReadLocker>
#include <QDebug>

MessageHandle::MessageHandle(QObject* parent)
    : QObject(parent)
{}

// ─────────────────────────────────────────────
//  订阅管理
// ─────────────────────────────────────────────

void MessageHandle::subMessage(IMessage* pMsg, ESubDataType subDataType) {
    if (!pMsg) return;

    QWriteLocker lock(&m_rwLock);
    auto& subscribers = m_subscriptions[subDataType];
    if (!subscribers.contains(pMsg)) {
        subscribers.append(pMsg);
    }
}

void MessageHandle::unSubMessage(IMessage* pMsg, ESubDataType subDataType) {
    if (!pMsg) return;

    QWriteLocker lock(&m_rwLock);
    m_subscriptions[subDataType].removeOne(pMsg);
}

void MessageHandle::unSubMessageAll(IMessage* pMsg) {
    if (!pMsg) return;

    QWriteLocker lock(&m_rwLock);
    for (auto& subscribers : m_subscriptions) {
        subscribers.removeOne(pMsg);
    }
}

// ─────────────────────────────────────────────
//  事件分发入口
// ─────────────────────────────────────────────

bool MessageHandle::event(QEvent* event) {
    IEvent* pEvent = dynamic_cast<IEvent*>(event);
    if (!pEvent) {
        return QObject::event(event);
    }

    // 读锁：复制订阅列表（避免分发过程中持锁导致死锁）
    QVector<IMessage*> subscribers;
    {
        QReadLocker lock(&m_rwLock);
        subscribers = m_subscriptions.value(pEvent->getMSgSubType());
    }

    dispatchToSubscribers(pEvent, subscribers);
    return true;
}

// ─────────────────────────────────────────────
//  分发实现
// ─────────────────────────────────────────────

void MessageHandle::dispatchToSubscribers(IEvent* pEvent,
                                           const QVector<IMessage*>& subscribers)
{
    for (IMessage* pMsg : subscribers) {
        if (!pMsg) continue;

        if (pMsg->condition()) {
            // 订阅者处于活跃状态，直接投递
            pMsg->onMessage(pEvent);
        } else {
            // 非活跃状态：仅投递特定优先级消息
            if (shouldDeliverWhenInactive(pMsg, pEvent)) {
                pMsg->onMessage(pEvent);
            }
        }
    }
}

bool MessageHandle::shouldDeliverWhenInactive(IMessage* /*pMsg*/,
                                               IEvent* pEvent) const
{
    const EventType evType = pEvent->getType();

    // ── 主动消息（InitiativeMsgEvent）──────────
    // if (evType == EventType::E_InitiativeMsg) {
    //     const auto* pInit = static_cast<const InitiativeMsgEvent*>(pEvent);
    //     const STParamInfo& param = pInit->getParamData();

    //     // 超时消息：强制投递（设备离线告警）
    //     if (param.eMsgType == EStaticMsgType::E_TimeoutMsg) {
    //         return true;
    //     }
    //     // 控制指令回复：强制投递
    //     if (param.eDataType == EDataType::E_ControlRes ||
    //         param.eDataType == EDataType::E_VoltageCtrRes) {
    //         return true;
    //     }
    //     return false;
    // }



    // 内部消息默认不强制投递
    return false;
}
