#include "programpowersupply.h"
#include "powersettingwidge.h"

#include <QGridLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QMessageBox>
#include <QLineEdit>
#include <QFrame>
#include <QSettings>


ProgramPowerSupply::ProgramPowerSupply(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle("程控电源-20251020"); // 匹配窗口标题
    setMinimumSize(1000, 600);          // 调整窗口最小尺寸
    setupUI();
}

void ProgramPowerSupply::setupUI()
{
    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(10);
    mainLayout->setContentsMargins(10, 10, 10, 10);

    // 6路电源网格布局（2列3行）
    auto *powerGrid = new QGridLayout;
    powerGrid->setSpacing(15);

    // 批量创建6路电源组
    for (int i = 1; i <= 6; ++i) {
        // 计算网格位置（行：(i-1)/2，列：(i-1)%2）
        int row = (i - 1) / 2;
        int col = (i - 1) % 2;
        // 创建单个电源组
        QGroupBox *group = createPowerGroup(i);
        powerGrid->addWidget(group, row, col);
    }

    // 底部按钮布局
    auto *btnLayout = new QHBoxLayout;
    btnLayout->setSpacing(20);
    QPushButton *btnSetting = new QPushButton("设置");
    QPushButton *btnExit = new QPushButton("退出");
    QPushButton *btnSystemPower = new QPushButton("系统加电"); // 右侧系统加电按钮

    // 设置按钮样式
    btnSetting->setFixedSize(80, 30);
    btnExit->setFixedSize(80, 30);
    btnSystemPower->setFixedSize(100, 60);

    btnLayout->addWidget(btnSetting, 0, Qt::AlignLeft);
    btnLayout->addWidget(btnExit, 0, Qt::AlignRight);

    // 主布局组装
    auto *topRightLayout = new QVBoxLayout;
    topRightLayout->addWidget(btnSystemPower, 0, Qt::AlignTop | Qt::AlignRight);
    topRightLayout->addStretch();

    auto *topLayout = new QHBoxLayout;
    topLayout->addLayout(powerGrid);
    topLayout->addLayout(topRightLayout);

    mainLayout->addLayout(topLayout);
    mainLayout->addLayout(btnLayout);

    // 退出按钮点击事件
    connect(btnExit, &QPushButton::clicked, this, &QDialog::close);
}

// 创建单个电源组的UI
QGroupBox* ProgramPowerSupply::createPowerGroup(int channelNum)
{
    QGroupBox *group = new QGroupBox;
    group->setStyleSheet("QGroupBox { border: 1px solid #ccc; padding: 10px; }");

    auto *layout = new QGridLayout(group);
    layout->setSpacing(5);
    layout->setContentsMargins(5, 5, 5, 5);

    // 1. 设置电压/电流输入框（带默认值）
    PowerChannel ch;
    ch.editSetVolt = new QLineEdit;
    ch.editSetCurr = new QLineEdit;
    // 设置默认值（匹配图片中的数值）
    double defaultVolt = 0.0, defaultCurr = 0.0;
    if (channelNum == 1) { defaultVolt = 28.0; defaultCurr = 10.0; }
    else if (channelNum == 2) { defaultVolt = 27.5; defaultCurr = 10.0; }
    else if (channelNum == 3) { defaultVolt = 56.0; defaultCurr = 5.0; }
    else if (channelNum == 4) { defaultVolt = 55.5; defaultCurr = 5.0; }
    else if (channelNum == 5) { defaultVolt = 36.0; defaultCurr = 5.0; }
    else if (channelNum == 6) { defaultVolt = 35.5; defaultCurr = 5.0; }

    ch.editSetVolt->setText(QString::number(defaultVolt, 'f', 1) + "00V");
    ch.editSetCurr->setText(QString::number(defaultCurr, 'f', 1) + "00A");
    // 设置输入框样式（黑色背景、白色文字）
    QString inputStyle = "QLineEdit { background: black; color: white; font-weight: bold; padding: 2px; }";
    ch.editSetVolt->setStyleSheet(inputStyle);
    ch.editSetCurr->setStyleSheet(inputStyle);
    ch.editSetVolt->setFixedWidth(80);
    ch.editSetCurr->setFixedWidth(80);

    // 2. 远控LED和重连计数/按钮
    ch.statusLed = new QLabel("●"); // 绿色LED
    ch.statusLed->setStyleSheet("color: #00ff00; font-size: 18px;");
    ch.statusLed->setFixedSize(16, 16);
    ch.statusLed->setAlignment(Qt::AlignCenter);

    QLabel *lblRemote = new QLabel(QString("远控%1").arg(channelNum));
    ch.lblReconnCount = new QLabel("0"); // 重连次数
    ch.btnReconn = new QPushButton(QString("重连%1").arg(channelNum));
    ch.btnReconn->setFixedSize(60, 25);

    // 3. 控制按钮（设置电压/电流、电源输出、读取设置）
    ch.btnSetVolt = new QPushButton(QString("设置电压%1").arg(channelNum));
    ch.btnSetCurr = new QPushButton(QString("设置电流%1").arg(channelNum));
    ch.btnOutput = new QPushButton(QString("电源输出%1").arg(channelNum));
    ch.btnReadSet = new QPushButton(QString("读取设置%1").arg(channelNum));
    // 按钮固定尺寸
    ch.btnSetVolt->setFixedSize(80, 25);
    ch.btnSetCurr->setFixedSize(80, 25);
    ch.btnOutput->setFixedSize(80, 25);
    ch.btnReadSet->setFixedSize(80, 25);

    // 4. 实际值和状态标签
    ch.lblActVolt = new QLabel("0.000V");
    ch.lblActCurr = new QLabel("0.000A");
    ch.lblPowerStatus = new QLabel("未输出");
    ch.lblFaultStatus = new QLabel("无故障");
    // 状态标签样式（灰色背景）
    QString statusStyle = "QLabel { background: #cccccc; padding: 2px; min-width: 60px; }";
    ch.lblActVolt->setStyleSheet(statusStyle);
    ch.lblActCurr->setStyleSheet(statusStyle);
    ch.lblPowerStatus->setStyleSheet(statusStyle);
    ch.lblFaultStatus->setStyleSheet(statusStyle);
    ch.lblActVolt->setAlignment(Qt::AlignCenter);
    ch.lblActCurr->setAlignment(Qt::AlignCenter);
    ch.lblPowerStatus->setAlignment(Qt::AlignCenter);
    ch.lblFaultStatus->setAlignment(Qt::AlignCenter);

    // 布局排版（按图片位置）
    // 第一行：设置电压、设置电流、远控LED+标签、重连次数+按钮
    layout->addWidget(ch.editSetVolt, 0, 0);
    layout->addWidget(ch.editSetCurr, 0, 1);
    layout->addWidget(ch.statusLed, 0, 2);
    layout->addWidget(lblRemote, 0, 3);
    layout->addWidget(ch.lblReconnCount, 0, 4);
    layout->addWidget(ch.btnReconn, 0, 5);

    // 第二行：控制按钮
    layout->addWidget(ch.btnSetVolt, 1, 0);
    layout->addWidget(ch.btnSetCurr, 1, 1);
    layout->addWidget(ch.btnOutput, 1, 2);
    layout->addWidget(ch.btnReadSet, 1, 3);

    // 第三行：实际电压、实际电流、电源状态、故障状态
    layout->addWidget(new QLabel("实际电压" + QString::number(channelNum)), 2, 0);
    layout->addWidget(new QLabel("实际电流" + QString::number(channelNum)), 2, 1);
    layout->addWidget(new QLabel("电源状态" + QString::number(channelNum)), 2, 2);
    layout->addWidget(new QLabel("故障状态" + QString::number(channelNum)), 2, 3);

    // 第四行：实际值显示
    layout->addWidget(ch.lblActVolt, 3, 0);
    layout->addWidget(ch.lblActCurr, 3, 1);
    layout->addWidget(ch.lblPowerStatus, 3, 2);
    layout->addWidget(ch.lblFaultStatus, 3, 3);

    // 保存通道数据（如果需要后续逻辑处理）
    m_channels[QString("电源%1").arg(channelNum)] = ch;

    return group;
}

// 补充头文件声明（programpowersupply.h中需要添加）
/*
#ifndef PROGRAMPOWERSUPPLY_H
#define PROGRAMPOWERSUPPLY_H

#include <QDialog>
#include <QMap>

struct PowerChannel; // 前置声明

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
*/
