#include "InternalMsgEvent.h"

InternalMsgEvent::InternalMsgEvent(EMsgDataType msgDataType,
                                   EventType    msgType,
                                   ESubDataType subType,
                                   QObject*     parent)
    : IEvent(subType, msgType, parent)
    , m_curMsgDataType(msgDataType)
{}

InternalMsgEvent::EMsgDataType InternalMsgEvent::getMsgDataType() const {
    return m_curMsgDataType;
}
