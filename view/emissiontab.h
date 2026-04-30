#ifndef EmissionTab_H
#define EmissionTab_H

#include <QDialog>
#include <QString>

// Qt 基础组件包含
#include <QWidget>
#include <QLineEdit>
#include <QPushButton>
#include <QTabWidget>
#include <QLabel>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QDesktopServices>
#include <QUrl>
#include <QMessageBox>
#include <QDir>
#include "src/CustomMessage/IMessage.h"
// 前置声明
class LaunchFrameDialog;
class LaunchProcessDialog;
class Controller422Dialog;
class Serial422Dialog;
class CopyFrameDialog;  // 新增：测试帧界面前置声明
class wavechart;        // 新增：波形图界面前置声明
class ControllerPanel ;
class EmissionTab : public QDialog,public IMessage
{
    Q_OBJECT

public:
    // 修改构造函数：添加Controller422Dialog、Serial422Dialog实例参数 + 控制器名称参数
    explicit EmissionTab(QWidget *parent = nullptr,
                         Controller422Dialog *controller422Dialog = nullptr,
                         Serial422Dialog *serial422Dialog = nullptr,
                         const QString &controllerName = "控制器1");
    ~EmissionTab();
    // IMessage::onMessage() — 主线程调用，可安全操作 UI
    void onMessage(IEvent* pEvent) override;
    void setControllerPanelDialog(ControllerPanel *dialog);
    void setLaunchProcessDialog(LaunchProcessDialog *dialog);

signals:
    // 向LaunchFrameDialog返回计算结果的信号
    void sendCollectionCoeffResult(double result);

private slots:
    // 处理LaunchFrameDialog的计算请求的槽
    void handleCalculateCollectionCoeff(int row, int col, double x);
private slots:
    // 打开文件夹槽函数
    void onOpenFolder();
private:
    // UI 组件指针
    double calculateCollectionCoeff(int row, int col, double x);
    QLineEdit *m_pathLineEdit;
    QTabWidget *m_tabWidget;
    LaunchFrameDialog *m_launchFrameDialog;
    ControllerPanel *m_dataPanel;
    LaunchProcessDialog*m_LaunchProcessDialog;
    CopyFrameDialog *m_copyFrameDialog;    // 新增：测试帧界面指针
    wavechart *m_waveChart;                // 新增：波形图界面指针
    // 保存422指令弹窗实例指针
    Controller422Dialog *m_controller422Dialog;
    // 保存422设置弹窗实例指针
    Serial422Dialog *m_serial422Dialog;
    // 控制器名称（用于区分不同实例）
    QString m_controllerName;
    // 初始化界面函数
    void setupUI();
};

#endif // EmissionTab_H
