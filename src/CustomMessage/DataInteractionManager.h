#ifndef DATAINTERACTIONMANAGER_H
#define DATAINTERACTIONMANAGER_H

#include "src/CustomEvent/IEvent.h"
#include "src/CustomMessage/MessageHandle.h"
#include "src/Common/StructDefine.h"
#include <QObject>
#include <functional>
// 新增字节流依赖
#include <QByteArray>


/**
 * @brief DataInteractionManager  数据交互管理（单例）
 *
 * 职责：
 *  1. 持有 MessageHandle，负责事件订阅/分发
 *  2. 提供各类 postXxx() 接口，将数据封装为 IEvent 投递到主线程
 *  3. 提供 sendMsg() 接口，向下层协议栈发送控制指令
 *
 * 线程安全：
 *  - postXxx() 系列接口可在任意线程调用（QCoreApplication::postEvent 线程安全）
 *  - sendMsg() 通过注册的函数指针回调，调用方自行保证线程安全
 */
class  DataInteractionManager : public QObject {
    Q_OBJECT

public:
    static DataInteractionManager& getInstance();

    ~DataInteractionManager() override;

    // ── 消息订阅分发对象 ────────────────────────
    /**
     * @brief getMsgHandle  获取订阅/分发器（供 UI 层订阅消息）
     */
    MessageHandle* getMsgHandle();

    // ── 上行数据投递（通信层 → UI 层）──────────

    /**
     * @brief postStaticMsg   设备实时/静态数据上报
     */
    void postStaticMsg(STParamInfo  stParam,
                       ESubDataType subType = ESubDataType::E_RealTimeData);

    /**
     * @brief postSendFailedNotify  数据发送失败通知
     */
    void postSendFailedNotify(const STSendFailedNotify& stNotify);

    /**
     * @brief postCtrlMsgFeedBack  控制指令反馈结果
     */
    void postCtrlMsgFeedBack(const STControlMsgFeedBackPtr& stCtrlMsg);

    /**
     * @brief postCustomEvent  推送自定义事件（调用方负责 new，Qt 接管生命周期）
     */
    void postCustomEvent(IEvent*      pEvent,
                         ESubDataType subType = ESubDataType::E_RealTimeData);

    // ── 下行数据发送（UI 层 → 通信层）──────────

    /**
     * @brief sendMsg  向设备发送结构化控制指令
     *
     * 内部调用注册的 sendMsgFunc 函数指针；
     * 推荐通过 setSendMsgFunc() 代替直接访问 func 成员。
     */
    void sendMsg(const STDataPrcSendMsg& stParam);

    /**
     * @brief sendMsg  重载：向设备发送字节流形式的原始控制指令
     * @param rawData  原始字节流数据
     *
     * 内部调用注册的 sendRawMsgFunc 函数指针；
     * 推荐通过 setSendRawMsgFunc() 代替直接访问 func 成员。
     */
    void sendMsg(const QByteArray& rawData);

    /**
     * @brief setSendMsgFunc  注册结构化指令发送回调（线程安全替代裸函数指针）
     */
    using SendMsgFunc = std::function<void(const STDataPrcSendMsg&)>;
    void setSendMsgFunc(SendMsgFunc func);

    /**
     * @brief setSendRawMsgFunc  注册字节流指令发送回调（线程安全）
     */
    using SendRawMsgFunc = std::function<void(const QByteArray&)>;
    void setSendRawMsgFunc(SendRawMsgFunc func);

signals:
    /** @brief sigSendMsg  备用信号（槽订阅方式发送结构化指令） */
    void sigSendMsg(const STDataPrcSendMsg& stParam);
    /** @brief sigSendRawMsg  备用信号（槽订阅方式发送字节流指令） */
    void sigSendRawMsg(const QByteArray& rawData);

private:
    DataInteractionManager();
    Q_DISABLE_COPY(DataInteractionManager)

    void postEvent(IEvent* pEvent);

private:
    MessageHandle* m_pMsgHandle = nullptr;
    SendMsgFunc    m_sendFunc;
    // 新增字节流发送函数指针
    SendRawMsgFunc m_sendRawFunc;
};

#endif // DATAINTERACTIONMANAGER_H
