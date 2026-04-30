
#ifndef STATICDATAPROCESS_H
#define STATICDATAPROCESS_H

#include "IDataProcess.h"

/**
 * @ brief: 静态消息处理接口类
 */
class StaticDataProcess : public IDataProcess {
public:

    StaticDataProcess();
    ~StaticDataProcess();

    void init();
    /*******************************************************************************
    * @Function:   dataProcess
    * @Description: 消息处理接口函数
    * @Parameters:  data   - [IN] 消息数据
    * @Return:      无
    *******************************************************************************/
    virtual void dataProcess(STParamInfo stParam) override;


};

#endif // STATICDATAPROCESS_H
