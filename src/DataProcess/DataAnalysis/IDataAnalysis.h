#ifndef IDATAANALYSIS_H
#define IDATAANALYSIS_H

#include <QThread>
#include <QQueue>
#include <QMutex>

#include "src/Common/StructDefine.h"


class IDataAnalysis : public QObject {
    Q_OBJECT

public:

    explicit IDataAnalysis(QObject *parent = nullptr);

    virtual ~IDataAnalysis();

    /**
     * @brief reciveData 接收的数据包
     * @param package
     */
    void    reciveData(const STPackage& package,STParamInfo& stParaminfo);
    /**
     * @brief canHandle  是否能处理此通道类型的数据
     */
    virtual bool canHandle(EChannelType type, const QString& channelId) const = 0;

    /**m_bigEndig
     * @brief ParseData 数据解析处理
     * @param input 输入参数
     * @param output    输出参数
     * @return
     */
    virtual bool parseData(const STPackage& stPackage, STParamInfo& stParamInfo) = 0;

protected:

};

using IDataAnalysisPtr = std::shared_ptr<IDataAnalysis>;

#endif // IDATAANALYSIS_H
