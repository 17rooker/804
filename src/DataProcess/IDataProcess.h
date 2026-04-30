#ifndef IDATAPROCESS_H
#define IDATAPROCESS_H

#include <QObject>
#include <QVariantHash>

#include "src/Common//StructDefine.h"

/**
 * @ brief: 消息处理接口类
 */
class IDataProcess : public QObject {
    Q_OBJECT

public:

    IDataProcess();
    virtual ~IDataProcess();

    /*******************************************************************************
    * @Function:   dataProcess
    * @Description: 消息处理接口函数，子类必须实现此函数
    * @Parameters:  data   - [IN] 消息数据
    * @Return:      无
    *******************************************************************************/
    virtual void dataProcess(STParamInfo stParam)=0;


};
using IDataProcessPtr = std::shared_ptr<IDataProcess>;
#endif // IDATAPROCESS_H
