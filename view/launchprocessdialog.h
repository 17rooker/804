#ifndef LAUNCHPROCESSDIALOG_H
#define LAUNCHPROCESSDIALOG_H

#include <QDialog>
#include <QTimer>
#include <QDateTime>
#include <qlabel.h>
// 新增引入自定义控件头文件
#include "styledledlabel.h"
#include "styledlineedit.h"
#include "src/Common/StructDefine.h"
#include "src/DataProcess/DataAnalysis/FrameDataAnalysis.h"
#include "src/CustomMessage/IMessage.h"
QT_BEGIN_NAMESPACE
namespace Ui { class LaunchProcessDialog; }
QT_END_NAMESPACE

// 子线程数据处理工作类（分离耗时操作）
class FrameDatWorker : public QObject
{
    Q_OBJECT
public slots:
    // 处理数据（耗时操作）
    void processData(const QByteArray &data);
    void processData_sel( STParamInfo &param);
signals:
    // 数据处理完成，通知主线程更新UI
    void dataProcessed(const QMap<QString, bool> &ledStates, const QMap<QString, QString> &editValues);
private:
    double calculateCoeff(double x, int row, int col);

    void paramProcess(STParamInfo &m_param, QMap<QString, bool> &m_ledStates,  QMap<QString, QString> &m_editValues);
};

class LaunchProcessDialog : public QDialog,public IMessage
{
    Q_OBJECT

public:
    LaunchProcessDialog(QWidget *parent = nullptr);
    ~LaunchProcessDialog();
    bool condition() override {
        return isVisible();
    }
    // IMessage::onMessage() — 主线程调用，可安全操作 UI
    void onMessage(IEvent* pEvent) override;
private slots:
    void updateTime();
    void onExitClicked();
    void onToggleButtonClicked(bool checked); // 按钮状态切换槽函数
    void appendData(const QByteArray &data);
    void clearPlaybackCache();

private:
    void setupUI();

    void onTimerTimeout();
    void onTimerTimeout_ser();
    void initWorkerThread();// 初始化子线程
    void onDataProcessed(const QMap<QString, bool> &ledStates, const QMap<QString, QString> &editValues);
    void updateControllerFrameUI(const QMap<QString, bool> &ledStates, const QMap<QString, QString> &editValues);
    void setParam(const STParamInfo& param);

    Ui::LaunchProcessDialog *ui;
    QTimer *m_timer;

    // 替换 LightLabel 为 StyledLedLabel
    StyledLedLabel *m_lightPower;
    StyledLedLabel *m_lightCtrl;
    StyledLedLabel *m_lightMeasure;
    StyledLedLabel *m_lightMechGood;
    StyledLedLabel *m_lightCondition;

    StyledLedLabel *m_lightCable;
    StyledLedLabel *m_light5VK;
    StyledLedLabel *m_lightLockPos;
    StyledLedLabel *m_lightResetPos;
    StyledLedLabel *m_lightPullForce;
    StyledLedLabel *m_lightTankPress;
    StyledLedLabel *m_lightGasState;
    StyledLedLabel *m_lightFireSafe;
    StyledLedLabel *m_lightReady;

    QLabel *m_lblTime;

    // 操作按钮及对应状态条
    QPushButton *m_btnAutoGas;
    QPushButton *m_btnManualGas;
    QPushButton *m_btnSwitchAutoMode;
    QPushButton *m_btnFireSafeRelease;

    QLabel *m_statusBarAutoGas;
    QLabel *m_statusBarManualGas;
    QLabel *m_statusBarSwitchAutoMode;
    QLabel *m_statusBarFireSafeRelease;

    QTimer *m_updateTimer;    // 防抖定时器（合并短时间内的多次更新）
    QTimer *m_updateTimer_ser;    // 控制器来的数据
    QByteArray m_dataCache;   // 数据缓存（合并多次传入的data）
    QMutex m_cacheMutex;      // 缓存锁（保证线程安全）
    FrameDatWorker *m_worker;// 子线程工作对象
    QThread *m_workerThread;  // 工作线程
    STParamInfo m_param;

    QMap<QString, StyledLedLabel*> m_ledMap;      // LED名称 -> LED控件映射
    QMap<QString, StyledLineEdit*> m_valueMap;
};

#endif // LAUNCHPROCESSDIALOG_H
