#ifndef LaunchFrameDialog_H
#define LaunchFrameDialog_H

#include <QWidget>
#include <QMap>
#include <qgridlayout.h>
#include <QMutex>
#include "styledledlabel.h"
#include "styledlineedit.h"
#include "src/Common/StructDefine.h"
class QLabel;
class FrameDataAnalysis;

// 子线程数据处理工作类（分离耗时操作）
class FrameDataWorker : public QObject
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

class LaunchFrameDialog : public QWidget
{
    Q_OBJECT

public:
    explicit LaunchFrameDialog(QWidget *parent = nullptr);
    ~LaunchFrameDialog();
    void resetUI();
    void setParam(const STParamInfo& param);
    // 新增：更新控制器发射帧解析结果UI的函数
    void updateControllerFrameUI(const QMap<QString, bool> &ledStates, const QMap<QString, QString> &editValues);
public slots:
    void appendData(const QByteArray &data);
    void clearPlaybackCache();
private:
    void setupUI();
    void upDataUi();
    void initWorkerThread();// 初始化子线程
    void onDataProcessed(const QMap<QString, bool> &ledStates, const QMap<QString, QString> &editValues);
    void onTimerTimeout();
    void onTimerTimeout_ser();
    QMap<QString, StyledLedLabel*> m_ledMap;      // LED名称 -> LED控件映射
    QMap<QString, StyledLineEdit*> m_valueMap;    // 输入框名称 -> 输入框控件映射
    void addRow(QGridLayout *layout, int row, const QString &labelText, const QString &ledName);

    QList<StyledLedLabel*> m_allLeds;
    QList<StyledLineEdit*> m_allLineEdits;
    QGridLayout *m_collectorGrid; // 采集器汇总网格布局
    STParamInfo m_param;
    QMap<QString, bool> m_ledStates;
    QMap<QString, QString> m_editValues;
//    FrameDataAnalysis* m_analy;

    // 优化新增成员
    QTimer *m_updateTimer;    // 防抖定时器（合并短时间内的多次更新）
    QTimer *m_updateTimer_ser;    // 控制器来的数据
    QByteArray m_dataCache;   // 数据缓存（合并多次传入的data）
    QMutex m_cacheMutex;      // 缓存锁（保证线程安全）
    FrameDataWorker *m_worker;// 子线程工作对象
    QThread *m_workerThread;  // 工作线程
    double m_result;
//    QMap<QString, QString> m_timeSeriesData;

//    QMap<QString, bool> m_ledCollectorSummary;
//    QMap<QString, QString> m_CollectorSummary;
};

#endif // LaunchFrameDialog_H
