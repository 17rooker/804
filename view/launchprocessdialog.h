#ifndef LAUNCHPROCESSDIALOG_H
#define LAUNCHPROCESSDIALOG_H

#include <QDialog>
#include <QTimer>
#include <QDateTime>
#include <QTextEdit>
#include <qlabel.h>
#include <QHBoxLayout>
#include "styledledlabel.h"
#include "styledlineedit.h"
#include "src/Common/StructDefine.h"
#include "src/DataProcess/DataAnalysis/FrameDataAnalysis.h"
#include "src/CustomMessage/IMessage.h"

QT_BEGIN_NAMESPACE
namespace Ui { class LaunchProcessDialog; }
QT_END_NAMESPACE

class FrameDatWorker : public QObject
{
    Q_OBJECT
public slots:
    void processData(const QByteArray &data);
    void processData_sel( STParamInfo &param);
signals:
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
    bool condition() override { return isVisible(); }
    void onMessage(IEvent* pEvent) override;
private slots:
    void updateTime();
    void onExitClicked();
    void onToggleButtonClicked(bool checked);
    void appendData(const QByteArray &data);
    void clearPlaybackCache();

private:
    void setupUI();
    void onTimerTimeout();
    void onTimerTimeout_ser();
    void initWorkerThread();
    void onDataProcessed(const QMap<QString, bool> &ledStates, const QMap<QString, QString> &editValues);
    void updateControllerFrameUI(const QMap<QString, bool> &ledStates, const QMap<QString, QString> &editValues);
    void setParam(const STParamInfo& param);

    Ui::LaunchProcessDialog *ui;
    QTimer *m_timer;

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

    QPushButton *m_btnAutoGas;
    QPushButton *m_btnManualGas;
    QPushButton *m_btnSwitchAutoMode;
    QPushButton *m_btnFireSafeRelease;
    QLabel *m_statusBarAutoGas;
    QLabel *m_statusBarManualGas;
    QLabel *m_statusBarSwitchAutoMode;
    QLabel *m_statusBarFireSafeRelease;

    QTimer *m_updateTimer;
    QTimer *m_updateTimer_ser;
    QByteArray m_dataCache;
    QMutex m_cacheMutex;
    FrameDatWorker *m_worker;
    QThread *m_workerThread;
    STParamInfo m_param;

    QMap<QString, StyledLedLabel*> m_ledMap;
    QMap<QString, StyledLineEdit*> m_valueMap;
    QTextEdit *m_logText = nullptr;

};

#endif // LAUNCHPROCESSDIALOG_H
