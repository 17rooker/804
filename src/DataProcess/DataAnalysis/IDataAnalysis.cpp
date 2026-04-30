#include "IDataAnalysis.h"
#include "FrameDataAnalysis.h"

IDataAnalysis::IDataAnalysis(QObject *parent) : QObject(parent)
{

}

IDataAnalysis::~IDataAnalysis()
{

}

void IDataAnalysis::reciveData(const STPackage &package,STParamInfo& stParamInfo)
{
    // 开始解析

    bool bRet = parseData(package, stParamInfo);

    if (bRet)
    {

        stParamInfo.unExtendCode = package.unExtendCode;
        stParamInfo.channelId    = package.channelId;
        stParamInfo.channelType  = package.channelType;

        //时间戳处理
        QDateTime stSysDateTime =  QDateTime::currentDateTime();
        if(abs(stSysDateTime.toMSecsSinceEpoch()*1000 - package.stArrDateTime.toTime_t()) > 5000000)
        {
            stParamInfo.stArrDateTime = stSysDateTime;
        }
        else
        {
            stParamInfo.stArrDateTime = package.stArrDateTime;
        }
    }


}


