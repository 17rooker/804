#include "wavechart.h"
#include "ui_wavechart.h"

wavechart::wavechart(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::wavechart)
{
    ui->setupUi(this);
    // ---- 创建图表 ----
    ChartWidget *chart1 = new ChartWidget(this);
    ui->gridLayout->addWidget(chart1);
    chart1->setTitle(QString::fromUtf8("信号分析曲线"));
    chart1->setAxisLabels(QString::fromUtf8("时间 (ms)"),
                         QString::fromUtf8("幅值 (V)"));

    ChartWidget *chart2 = new ChartWidget(this);
    ui->gridLayout_6->addWidget(chart2);
    chart2->setTitle(QString::fromUtf8("信号分析曲线"));
    chart2->setAxisLabels(QString::fromUtf8("时间 (ms)"),
                         QString::fromUtf8("幅值 (V)"));

    ChartWidget *chart3 = new ChartWidget(this);
    ui->gridLayout_3->addWidget(chart3);
    chart3->setTitle(QString::fromUtf8("信号分析曲线"));
    chart3->setAxisLabels(QString::fromUtf8("时间 (ms)"),
                         QString::fromUtf8("幅值 (V)"));

    ChartWidget *chart4 = new ChartWidget(this);
    ui->gridLayout_7->addWidget(chart4);
    chart4->setTitle(QString::fromUtf8("信号分析曲线"));
    chart4->setAxisLabels(QString::fromUtf8("时间 (ms)"),
                         QString::fromUtf8("幅值 (V)"));

}

wavechart::~wavechart()
{
    delete ui;
}

void wavechart::setParam(const STParamInfo &param)
{
    m_param=param;
}
