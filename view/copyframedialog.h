#ifndef CopyFrameDialog_H
#define CopyFrameDialog_H

#include <QWidget>
#include "src/Common/StructDefine.h"
class StyledLedLabel;
class StyledLineEdit;

class CopyFrameDialog : public QWidget
{
    Q_OBJECT

public:
    CopyFrameDialog(QWidget *parent = nullptr);
    ~CopyFrameDialog();
    void setParam(const STParamInfo& param);
private:
    void setupUi();
    QWidget* createColumn1();    // 第一列：火保/解控等状态灯
    QWidget* createColumn2();    // 第二列：机构1/2相关状态灯
    QWidget* createColumn3();    // 第三列：机构3/4/电爆/火引爆状态灯
    QWidget* createColumn4();    // 第四列：帧长/帧计数等参数
    QWidget* createColumn5();    // 第五列：机构1电压1~12
    QWidget* createColumn6();    // 第六列：校验结果/帧类型等
    QWidget* createCombinedColumn();
    STParamInfo m_param;
};

#endif // CopyFrameDialog_H
