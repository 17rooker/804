#include "ExpandAnalysis.h"

ExpandAnalysis::ExpandAnalysis(QObject *parent) : IDataAnalysis(parent)
{

}

ExpandAnalysis::~ExpandAnalysis()
{

}

bool ExpandAnalysis::canHandle(EChannelType type, const QString &channelId) const
{
    Q_UNUSED(type)
    Q_UNUSED(channelId)
    return false;

}



bool ExpandAnalysis::parseData(const STPackage &stPackage, STParamInfo &stParamInfo)
{
    Q_UNUSED(stPackage)
    Q_UNUSED(stParamInfo)
    return false;
}
