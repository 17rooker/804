#include "DataInteractionManager.h"
#include "MessageHandle.h"
#include "InternalMsgEvent.h"
#include "InitiativeMsgEvent.h"

#include <QCoreApplication>
#include <QDebug>

// ═══════════════════════════════════════════════
//  单例
// ═══════════════════════════════════════════════

DataInteractionManager& DataInteractionManager::getInstance() {
    static DataInteractionManager sIns;
    return sIns;
}

DataInteractionManager::DataInteractionManager()
    : QObject(nullptr)
    , m_pMsgHandle(new MessageHandle(this))   // 与本对象同线程
{}

DataInteractionManager::~DataInteractionManager() {
    // m_pMsgHandle 由 Qt 父子关系自动销毁
}

// ═══════════════════════════════════════════════
//  公共接口
// ═══════════════════════════════════════════════

MessageHandle* DataInteractionManager::getMsgHandle() {
    return m_pMsgHandle;
}

void DataInteractionManager::setSendMsgFunc(SendMsgFunc func) {
    m_sendFunc = std::move(func);
}

// 新增：注册字节流发送回调
void DataInteractionManager::setSendRawMsgFunc(SendRawMsgFunc func) {
    m_sendRawFunc = std::move(func);
}

// ─────────────────────────────────────────────
//  上行投递
// ─────────────────────────────────────────────

void DataInteractionManager::postStaticMsg(STParamInfo  stParam,
                                            ESubDataType subType)
{
    auto* pEvent = new InitiativeMsgEvent(subType);
    pEvent->setData(stParam);
    postEvent(pEvent);
}

void DataInteractionManager::postSendFailedNotify(
    const STSendFailedNotify& stNotify)
{
    auto* pEvent = new InternalMsgEvent(InternalMsgEvent::EMsgDataType::E_SendFailed);
    pEvent->setData(QVariant::fromValue<STSendFailedNotify>(stNotify));
    postEvent(pEvent);
}

void DataInteractionManager::postCtrlMsgFeedBack(
    const STControlMsgFeedBackPtr& stCtrlMsg)
{
    auto* pEvent = new InternalMsgEvent(InternalMsgEvent::EMsgDataType::E_FeedBackData);
    pEvent->setData(QVariant::fromValue<STControlMsgFeedBackPtr>(stCtrlMsg));
    postEvent(pEvent);
}

void DataInteractionManager::postCustomEvent(IEvent*      pEvent,
                                              ESubDataType subType)
{
    if (!pEvent) return;
    pEvent->setMSgSubType(subType);
    postEvent(pEvent);
}

// ─────────────────────────────────────────────
//  下行发送
// ─────────────────────────────────────────────

void DataInteractionManager::sendMsg(const STDataPrcSendMsg& stParam) {
    if (m_sendFunc) {
        m_sendFunc(stParam);
    } else {
        qWarning() << "[DataInteractionManager] sendMsg: no sendFunc registered.";
    }
}

// 新增：字节流版本发送实现
void DataInteractionManager::sendMsg(const QByteArray& rawData) {
    if (m_sendRawFunc) {
        m_sendRawFunc(rawData);
    } else {
        qWarning() << "[DataInteractionManager] sendMsg(rawData): no sendRawFunc registered.";
    }
}

// ─────────────────────────────────────────────
//  私有：事件投递
// ─────────────────────────────────────────────

void DataInteractionManager::postEvent(IEvent* pEvent) {
    // postEvent 是线程安全的，可从任意线程调用
    // Qt 事件系统保证在 m_pMsgHandle 所在线程（主线程）分发
    QCoreApplication::postEvent(m_pMsgHandle, pEvent);
}
