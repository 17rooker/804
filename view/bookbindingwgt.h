#ifndef DIALOG_422_H
#define DIALOG_422_H

#include <QDialog>
#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QComboBox>
#include <QMouseEvent>
#include "styledlineedit.h"
#include <QAbstractItemView>
#include <QByteArray>
#include <QtGlobal>
#include <QMenu>       // 新增：右键菜单头文件
#include <QClipboard>  // 新增：剪贴板头文件

// 自定义ComboBox：点击任意位置弹出下拉选项
class ClickableComboBox : public QComboBox
{
    Q_OBJECT
public:
    explicit ClickableComboBox(QWidget *parent = nullptr) : QComboBox(parent) {}

protected:
    // 重写鼠标点击事件：点击控件任意位置都弹出下拉列表
    void mousePressEvent(QMouseEvent *e) override
    {
        Q_UNUSED(e);
        showPopup(); // 强制弹出下拉列表
    }
};

class bookbindingwgt : public QDialog
{
    Q_OBJECT

public:
    explicit bookbindingwgt(QWidget *parent = nullptr);
    ~bookbindingwgt() override = default;

private:
    void initUI();
    void initStyle();

    // CRC16校验计算函数
    quint16 calculateCRC16(const QByteArray &data);
    // 整数转16位补码（2字节，低字节在前）
    QByteArray intTo16BitComplement(int value);
    // 帧显示函数
    void displayFrame(const QString &frameStr);
    void displayReadbackFrame(const QString &frameStr); // 新增：回读帧显示函数

    // 新增：右键菜单创建函数
    void createContextMenu(QLabel *label);

    // 槽函数声明
    void onAllowBindClicked();    // 允许装订
    void onForbidBindClicked();   // 禁止装订
    void onReadParamClicked();    // 参数回读
    void onBindParamClicked();    // 参数装订
    void onFrameClicked();        // 回传
    void onClearClicked();        // 清零
    void onSendClicked();         // 发送
    // 控件声明
    ClickableComboBox *cbSerialPort;    // 串口通道下拉框
    ClickableComboBox *cbControlWord;   // 控制字下拉框
    StyledLineEdit *leBindingFrame;
    StyledLineEdit *leReadbackFrame;
    StyledLineEdit *leFireTime;
    StyledLineEdit *leRelayCloseTime;
    StyledLineEdit *leValveDelayTime;
    StyledLineEdit *leCheckSum1;
    StyledLineEdit *leCheckSum2;
    StyledLineEdit *leCheckSum3;

    // 帧显示标签
    QLabel *frameDisplayLabel;
    QLabel *readbackFrameDisplayLabel; // 新增：回读帧显示标签

    // 新增：存储组帧后的帧数据
    QByteArray m_currentDat;
};

#endif // DIALOG_422_H
