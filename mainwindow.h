#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTimer>
#include <QDateTime>
#include <QTextEdit>
#include "view/controllerpanel.h"
#include "view/datastruct.h"
#include "view/ledindicator.h"
#include "view/serial422dialog.h"
#include "view/controller422dialog.h"
#include "view/launchprocessdialog.h"

#include "view/simulateddata.h"
#include "view/emissiontab.h"

#include "src/DataProcess/DataAnalysis/FrameDataAnalysis.h"
#include "src/Common/CommTypes.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE
// 子线程数据处理工作类（分离耗时操作）
class FrameDataAnalysis;


class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();
    // IMessage::condition() — 返回当前窗体是否可见

private slots:
    void updateTime();        // 更新左上角时间
    void updateTestData();    // 更新右上角测试时间（模拟）
    void updateSystemData();  // 更新中间数据（模拟）
private:
    Ui::MainWindow *ui;
    ControllerPanel *m_dataPanel; // 核心数据面板
    LaunchProcessDialog* m_LaunchProcess;
    void initUi();
    // 顶部状态组件
    QLabel *m_lblSysTime;     // 系统时间
    QLabel *m_lblTestTimeVal; // 测试时间数值
    QLabel *m_lblStatusLight; // 机构准备好指示灯
    QLabel *m_lblStatusText;  // "测发控"文字

    QTimer *m_timerSys;       // 系统时钟定时器
    QTimer *m_timerTest;      // 测试计时器
    int m_testSeconds;        // 测试秒数计数
    QLabel *m_lblCecLight;
    QTextEdit *m_logText = nullptr;

    // 新增：全局唯一的422指令弹窗实例
    Controller422Dialog *m_controller422Dialog;
    // 新增：全局唯一的422设置弹窗实例
    Serial422Dialog *m_serial422Dialog;

    // 新增：三个控制器的唯一实例
    EmissionTab *m_emissionTab1; // 控制器1实例
    EmissionTab *m_emissionTab2; // 控制器2实例
    EmissionTab *m_emissionTab3; // 控制器3实例
   // 输入框名称 -> 输入框控件映射
};

#endif // MAINWINDOW_H
