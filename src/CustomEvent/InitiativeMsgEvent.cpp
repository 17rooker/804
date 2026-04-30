#include "InitiativeMsgEvent.h"


InitiativeMsgEvent::InitiativeMsgEvent(ESubDataType subType, EventType msgType,
                                       QObject  *parent) : IEvent(subType, msgType,  parent)
{}

void InitiativeMsgEvent::setData(STParamInfo stParam)
{
    m_stParamInfo = stParam;
}

STParamInfo InitiativeMsgEvent::getParamData()
{
    return m_stParamInfo;
}

STParamInfo * InitiativeMsgEvent::getParam()
{
    return &m_stParamInfo;
}
