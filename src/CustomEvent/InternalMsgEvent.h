#ifndef INTERNALMSGEVENT_H
#define INTERNALMSGEVENT_H

#include "IEvent.h"

/**
 * @brief InternalMsgEvent  软件内部状态上报事件
 *
 * 用于传递与设备无直接关联的内部状态变化，包括：
 *  - 数据发送失败通知（E_SendFailed）
 *  - 控制指令反馈数据（E_FeedBackData）
 */
class  InternalMsgEvent : public IEvent {
public:
    enum class EMsgDataType {
        E_DevComLinkStatus = 0,   // 设备通讯链路状态
        E_SendFailed,             // 消息发送失败
        E_FeedBackData,           // 控制指令反馈结果
    };

    explicit InternalMsgEvent(
        EMsgDataType msgDataType,
        EventType    msgType = EventType::E_InternalMsg,
        ESubDataType subType = ESubDataType::E_RealTimeData,
        QObject*     parent  = nullptr
    );

    /** @brief getMsgDataType  获取内部消息具体类型 */
    EMsgDataType getMsgDataType() const;

private:
    EMsgDataType m_curMsgDataType;
};

#endif // INTERNALMSGEVENT_H
