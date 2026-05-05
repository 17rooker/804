#ifndef PROGRAMPOWERSUPPLY_H
#define PROGRAMPOWERSUPPLY_H

#include <QDialog>
#include <QMap>
#include <qgroupbox.h>
#include <qlabel.h>
#include <qlineedit.h>


// 定义电源通道结构体（扩展原有结构）
struct PowerChannel {
    QLineEdit *editSetVolt;    // 设置电压输入框
    QLineEdit *editSetCurr;    // 设置电流输入框
    QLabel *lblActVolt;        // 实际电压显示标签
    QLabel *lblActCurr;        // 实际电流显示标签
    QLabel *lblPowerStatus;    // 电源状态标签（未输出/输出中）
    QLabel *lblFaultStatus;    // 故障状态标签
    QLabel *statusLed;         // 远控状态LED
    QLabel *lblReconnCount;    // 重连次数标签
    QPushButton *btnSetVolt;   // 设置电压按钮
    QPushButton *btnSetCurr;   // 设置电流按钮
    QPushButton *btnOutput;    // 电源输出按钮
    QPushButton *btnReadSet;   // 读取设置按钮
    QPushButton *btnReconn;    // 重连按钮
};
class ProgramPowerSupply : public QDialog
{
    Q_OBJECT

public:
    explicit ProgramPowerSupply(QWidget *parent = nullptr);

private:
    void setupUI();
    QGroupBox* createPowerGroup(int channelNum); // 创建单个电源组

    QMap<QString, PowerChannel> m_channels; // 存储所有电源通道
};

#endif // PROGRAMPOWERSUPPLY_H
