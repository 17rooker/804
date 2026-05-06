#ifndef CONTROLLERPANEL_H
#define CONTROLLERPANEL_H

#include <QWidget>
#include <QLineEdit>
#include <QLabel>
#include <QVector>
#include <QElapsedTimer>
#include "src/ControllerCSV/CsvController.h"
// 引入自定义样式控件头文件
#include "styledlineedit.h"
#include "styledledlabel.h"
#include "src/Common/StructDefine.h"
#include "src/DataProcess/DataAnalysis/FrameDataAnalysis.h"
#include "src/CustomMessage/IMessage.h"
#include "src/Common/StructDefine.h"
// 定义一个结构体来保存每一行的控件指针
enum class RowType {
    Value,  // 包含数值框
    Status  // 包含指示灯
};

enum class ParamType {
    Tension,   // 拉力
    Angle,     // 角度
    Pressure   // 压力
};
struct DataWidgetRow {
    RowType type;
    StyledLineEdit *valueBox = nullptr; // 替换为自定义数值框
    StyledLedLabel *lightLabel = nullptr;  // 替换为自定义指示灯
    QLabel *textLabel = nullptr;   // 所有行都有文字标签
    bool isAlarmType = false;      // 标记是否为异常类，用于updateData逻辑
};

class FrameWorker : public QObject
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
    bool readParamRange(ParamType type, double &lower, double &upper);
    double calculateCoeff(double x, int row, int col);
    bool isParamOutOfRange(double paramValue, ParamType type);
    void paramProcess(STParamInfo &m_param, QMap<QString, bool> &m_ledStates,  QMap<QString, QString> &m_editValues);
    void paramProcess_A6(STParamInfo &m_param, QMap<QString, bool> &m_ledStates,  QMap<QString, QString> &m_editValues);

    // SQ接近开关历史状态（用于边缘检测）
    QMap<int, bool> m_prevSQ3_1;
    QMap<int, bool> m_prevSQ3_2;
    QMap<int, bool> m_prevSQ7Combined;   // SQ7上升沿检测
    QMap<int, bool> m_prevY1Active;      // Y1.1/Y1.2上升沿检测（气动解锁信号）

    // 气动解锁计时相关
    QMap<int, bool> m_pneuTimingStarted;
    QMap<int, qint64> m_pneuStartUs;     // 起始时刻(μs)
    QMap<int, bool> m_unlockTimeDone;
    QMap<int, bool> m_releaseOkTimeDone;
    QMap<int, bool> m_releaseInPlacePneuDone;

    QElapsedTimer m_workerTimer;         // 工作线程计时器
};

class ControllerPanel : public QWidget,public IMessage
{
    Q_OBJECT

public:
    explicit ControllerPanel(QWidget *parent = nullptr);
    ~ControllerPanel();
    bool condition() override {
        return isVisible();
    }
    // IMessage::onMessage() — 主线程调用，可安全操作 UI
    void onMessage(IEvent* pEvent) override;

signals:
    void mechanismReadyChanged(bool ready);

public slots:
    void appendData(const QByteArray &data);
    void clearPlaybackCache();

private:

    void onTimerTimeout();
    void onTimerTimeout_ser();
    void initWorkerThread();// 初始化子线程
    void onDataProcessed(const QMap<QString, bool> &ledStates, const QMap<QString, QString> &editValues);
    void updateControllerFrameUI(const QMap<QString, bool> &ledStates, const QMap<QString, QString> &editValues);
    void setParam(const STParamInfo& param);

    // 创建单行控件 (黑框 + 标签)
    DataWidgetRow createRow(const QString &text);
    DataWidgetRow createValueRow(const QString &text);
    DataWidgetRow createStatusRow(const QString &text, bool isAlarm);
    // --- 四大列的布局 ---
    QWidget *colWidget1;
    QWidget *colWidget2;
    QWidget *colWidget3;
    QWidget *colWidget4;

    // 存储所有行控件的容器，方便批量更新
    QVector<DataWidgetRow> m_rowsCol1;
    QVector<DataWidgetRow> m_rowsCol2;
    QVector<DataWidgetRow> m_rowsCol3;
    QVector<DataWidgetRow> m_rowsCol4;

    // 右侧状态灯区域
    QWidget *statusWidget;

    // 初始化界面布局
    void setupUI();

    QTimer *m_updateTimer;    // 防抖定时器（合并短时间内的多次更新）
    QTimer *m_updateTimer_ser;    // 控制器来的数据
    QByteArray m_dataCache;   // 数据缓存（合并多次传入的data）
    QMutex m_cacheMutex;      // 缓存锁（保证线程安全）
    FrameWorker *m_worker;// 子线程工作对象
    QThread *m_workerThread;  // 工作线程
    STParamInfo m_param;

    QMap<QString, StyledLedLabel*> m_ledMap;      // LED名称 -> LED控件映射
    QMap<QString, StyledLineEdit*> m_valueMap;

    // CSV 存储
    CsvController *m_csvController = nullptr;
    QTimer        *m_csvTimer = nullptr;
    QString        m_csvFilePath;
    QStringList    m_csvHeader;
    CsvData        m_csvPendingRows;
    void initCsvStorage();
    void flushCsv();
};

#endif // CONTROLLERPANEL_H
