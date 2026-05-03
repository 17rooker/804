#include "wavechart.h"
#include "ui_wavechart.h"

#include "src/DataProcess/DataAnalysis/FrameDataAnalysis.h"
#include "src/Common/CommTypes.h"

#include <QTimer>
#include <QThread>
#include <QMetaObject>

// ═══════════════════════════════════════════════════════════════════════════
//  FrameWaveWorker
// ═══════════════════════════════════════════════════════════════════════════
void FrameWaveWorker::processData(const QByteArray &data)
{
    QByteArray buf = data;
    const int MAX_FRAMES = 50;
    const int EMIT_INTERVAL = 3;
    int n = 0;
    STParamInfo last;

    while (buf.size() >= 8 && n < MAX_FRAMES) {
        if (buf.left(4) != QByteArray::fromHex("FDB18540")) { buf.remove(0,1); continue; }
        QByteArray lenB = buf.mid(4,4);
        qint32 fl = (quint8(lenB[0])<<24)|(quint8(lenB[1])<<16)|(quint8(lenB[2])<<8)|quint8(lenB[3]);
        if (fl<=85||fl>1024*1024) { buf.remove(0,4); continue; }
        if (buf.size()<fl) break;
        QByteArray fd = buf.left(fl); buf.remove(0,fl);

        FrameDataAnalysis an;
        STPackage pk; pk.channelId=m_channelId; pk.channelType=EChannelType::Serial; pk.baDataRecv=fd;
        STParamInfo pm{}; an.parseData(pk,pm);

        last = pm;
        if (n%EMIT_INTERVAL==0) emit dataProcessed(pm);
        n++;
    }
    if (n>0 && n%EMIT_INTERVAL!=0) emit dataProcessed(last);
}

// ═══════════════════════════════════════════════════════════════════════════
wavechart::wavechart(QWidget *parent) :
    QWidget(parent), ui(new Ui::wavechart),
    m_updateTimer(new QTimer(this)),
    m_worker(new FrameWaveWorker()),
    m_workerThread(new QThread(this))
{
    ui->setupUi(this);

    // 创建4个波形图
    m_chart1 = new ChartWidget(this); ui->gridLayout->addWidget(m_chart1); setupChart(m_chart1, "机构1 电磁阀电压");
    m_chart2 = new ChartWidget(this); ui->gridLayout_6->addWidget(m_chart2); setupChart(m_chart2, "机构2 电磁阀电压");
    m_chart3 = new ChartWidget(this); ui->gridLayout_3->addWidget(m_chart3); setupChart(m_chart3, "机构3 电磁阀电压");
    m_chart4 = new ChartWidget(this); ui->gridLayout_7->addWidget(m_chart4); setupChart(m_chart4, "机构4 电磁阀电压");

    // 回放定时器
    m_updateTimer->setSingleShot(true);
    m_updateTimer->setInterval(10);
    connect(m_updateTimer, &QTimer::timeout, this, [this]() {
        QMutexLocker l(&m_cacheMutex);
        if (m_dataCache.isEmpty()) return;
        QByteArray d = m_dataCache; m_dataCache.clear(); l.unlock();
        QMetaObject::invokeMethod(m_worker, "processData", Qt::QueuedConnection, Q_ARG(QByteArray, d));
    });

    m_worker->moveToThread(m_workerThread);
    connect(m_worker, &FrameWaveWorker::dataProcessed, this, &wavechart::setParam, Qt::QueuedConnection);
    m_workerThread->start();
}

wavechart::~wavechart() {
    m_workerThread->quit(); m_workerThread->wait();
    delete ui;
}

void wavechart::setupChart(ChartWidget *chart, const QString &title) {
    chart->setTitle(title);
    chart->setAxisLabels("采样点", "电流 (A)");
    chart->setXRange(0.5, 12.5);
    chart->setLegendHide(false);

    SeriesData sd;
    sd.name = "电磁阀电流";
    sd.color = QColor("#00CCFF");
    sd.width = 2;
    QVector<double> x(12), y(12);
    for (int i = 0; i < 12; i++) { x[i] = i+1; y[i] = 0; }
    sd.x = x; sd.y = y;
    chart->addSeries(sd);
    chart->setYRange(0, 12);
}

void wavechart::appendData(const QByteArray &data) {
    QMutexLocker l(&m_cacheMutex);
    m_dataCache.append(data);
    m_updateTimer->start();
}

void wavechart::clearPlaybackCache() {
    QMutexLocker l(&m_cacheMutex);
    m_dataCache.clear();
    m_updateTimer->stop();
}

// ═══════════════════════════════════════════════════════════════════════════
//  数据更新绘图
// ═══════════════════════════════════════════════════════════════════════════
void wavechart::setParam(const STParamInfo &param) {
    m_param = param;
    plotAinData(param);
}

void wavechart::plotAinData(const STParamInfo &param)
{
    // 从 mapParams 中提取 AIN4_t1~AIN7_t12 绘制4条曲线
    struct MechData { QString prefix; ChartWidget *chart; };
    QList<MechData> mechs = {
        {"AIN4_t", m_chart1}, {"AIN5_t", m_chart2},
        {"AIN6_t", m_chart3}, {"AIN7_t", m_chart4}
    };

    for (auto &m : mechs) {
        if (!m.chart) continue;
        QVector<double> x(12), y(12);
        bool hasData = false;
        for (int i = 1; i <= 12; i++) {
            x[i-1] = i;
            QString key = m.prefix + QString::number(i);
            auto it = param.mapParams.find(key);
            if (it != param.mapParams.end()) {
                double v = it.value().varParaValue.toUInt() * 0.01952 / 0.51;
                y[i-1] = v;
                if (v > 0) hasData = true;
            } else {
                y[i-1] = 0;
            }
        }
        m.chart->updateSeries(0, x, y);
    }
}
