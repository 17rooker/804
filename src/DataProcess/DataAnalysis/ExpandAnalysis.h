#ifndef EXPANDANALYSIS_H
#define EXPANDANALYSIS_H

#include "IDataAnalysis.h"

//- 作用: 用于拓展解析功能，可能包含多种数据解析方式。
//- 继承自IDataAnalysis类，实现了具体的数据解析方法。
//- 包含了一个IDataAnalysis类的列表m_expandAnalysisLst，用于存储多个数据解析类的实例，实现了多样的数据解析功能。

class ExpandAnalysis : public IDataAnalysis
{
    Q_OBJECT
public:
    explicit ExpandAnalysis(QObject *parent = nullptr);
    virtual ~ExpandAnalysis();

    /**
     * @brief isContainDevID
     * @param package
     * @return
     */
    virtual bool canHandle(EChannelType type, const QString& channelId) const;

    /**
     * @brief ParseData 数据解析处理
     * @param input 输入参数
     * @param output    输出参数
     * @return
     */
    virtual bool parseData(const STPackage& stPackage, STParamInfo& stParamInfo);

};

#endif // EXPANDANALYSIS_H
