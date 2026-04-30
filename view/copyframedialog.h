#ifndef CopyFrameDialog_H
#define CopyFrameDialog_H

#include <QWidget>
#include <QMap>
#include "src/Common/StructDefine.h"
class StyledLedLabel;
class StyledLineEdit;

class CopyFrameDialog : public QWidget
{
    Q_OBJECT

public:
    CopyFrameDialog(QWidget *parent = nullptr);
    ~CopyFrameDialog();

public slots:
    void setParam(const STParamInfo& param);

private:
    void setupUi();
    QWidget* createColumn1();    // 火保/解控状态灯
    QWidget* createColumn2();    // 机构1/2/3状态灯
    QWidget* createColumn3();    // 机构4/电爆/火引爆状态灯
    QWidget* createColumn4();    // 帧参数输入框
    QWidget* createCombinedColumn(); // 时序+校验列

    void updateData(const STParamInfo& param);

    QMap<QString, StyledLedLabel*>  m_ledMap;
    QMap<QString, StyledLineEdit*>  m_valueMap;
    STParamInfo m_param;
};

#endif // CopyFrameDialog_H
