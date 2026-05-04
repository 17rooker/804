#ifndef WAVECHART_H
#define WAVECHART_H

#include <QWidget>
#include <QMutex>
#include <QByteArray>
#include "src/chart/chartwidget.h"
#include "src/Common/StructDefine.h"

class QTimer;
class QThread;

class FrameWaveWorker : public QObject
{
    Q_OBJECT
public:
    QString m_channelId = "serial_E";
public slots:
    void processData(const QByteArray &data);
signals:
    void dataProcessed(const STParamInfo &param);
};

namespace Ui { class wavechart; }

class wavechart : public QWidget
{
    Q_OBJECT

public:
    explicit wavechart(QWidget *parent = nullptr);
    ~wavechart();

public slots:
    void setParam(const STParamInfo& param);
    void appendData(const QByteArray &data);
    void clearPlaybackCache();

private:
    void setupChart(ChartWidget *chart, const QString &title);
    void setupChart(ChartWidget *chart, const QString &title, double yMin, double yMax);
    void plotAinData(const STParamInfo &param);

    Ui::wavechart *ui;
    ChartWidget *m_chart1 = nullptr, *m_chart2 = nullptr;
    ChartWidget *m_chart3 = nullptr, *m_chart4 = nullptr;
    STParamInfo m_param;

    // 回放
    QByteArray   m_dataCache;
    QMutex       m_cacheMutex;
    QTimer      *m_updateTimer = nullptr;
    QThread     *m_workerThread = nullptr;
    FrameWaveWorker *m_worker = nullptr;

    // 绘图节流
    QTimer      *m_plotTimer = nullptr;
};

#endif // WAVECHART_H
