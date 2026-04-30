#ifndef INITIATIVEMSGEVENT_H
#define INITIATIVEMSGEVENT_H

#include "IEvent.h"
#include "src/Common/StructDefine.h"

/**
 * @brief The InitiativeMsgEvent class 主动上报消息事件
 */

class  InitiativeMsgEvent : public IEvent {
public:

    InitiativeMsgEvent(ESubDataType subType = ESubDataType::E_RealTimeData, EventType msgType = EventType::E_InitiativeMsg,
                       QObject  *parent = nullptr);

    void         setData(STParamInfo stParam);

    STParamInfo getParamData();

    STParamInfo* getParam();

private:

    // 上报数据
    STParamInfo m_stParamInfo;
};

#endif // INITIATIVEMSGEVENT_H
