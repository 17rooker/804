#ifndef CONTROLDATAPROCESS_H
#define CONTROLDATAPROCESS_H

#include <QMap>
#include <QTimer>
#include "IDataProcess.h"


class ControlDataProcess : public IDataProcess
{
    Q_OBJECT



public:

    ControlDataProcess();
    ~ControlDataProcess();
    void init();

    /*******************************************************************************
    * @Function:   dataProcess
    * @Description: 消息处理接口函数
    * @Parameters:  data   - [IN] 消息数据
    * @Return:      无
    *******************************************************************************/
    virtual void dataProcess(STParamInfo stParam) override;


};

#endif // CONTROLDATAPROCESS_H
