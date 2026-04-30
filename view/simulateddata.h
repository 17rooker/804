#ifndef simulateddata_H
#define simulateddata_H

#include <QDialog>
#include <QPushButton>
#include <QLabel>
#include <QGroupBox>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QFormLayout>
// 新增：引入自定义控件头文件
#include "styledlineedit.h"
#include "styledledlabel.h"

class simulateddata : public QDialog
{
    Q_OBJECT

public:
    explicit simulateddata(QWidget *parent = nullptr);

private:
    // 修改：返回类型从 QLabel* 改为 StyledLedLabel*
    StyledLedLabel* createIndicatorLabel();
    // 辅助函数：创建「指示灯+标签」行组件
    QWidget* createIndicatorLabelRow(const QString &text);
    // 修改：返回类型从 QLineEdit* 改为 StyledLineEdit*
    StyledLineEdit* createLineEdit(const QString &defaultText = "0.000");

    // 创建左侧「控制器发射帧解析结果」分组
    QGroupBox *createControllerGroup();
    // 创建右侧「采集器汇总解析结果」分组
    QGroupBox *createCollectorGroup();

    // 退出按钮
    QPushButton *m_btnExit;
};

#endif // simulateddata_H
