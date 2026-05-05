#include "postanalysisdialog.h"
#include "src/chart/chartwidget.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QCheckBox>
#include <QComboBox>
#include <QPushButton>
#include <QLabel>
#include <QLineEdit>
#include <QColorDialog>
#include <QMouseEvent>

PostAnalysisDialog::PostAnalysisDialog(QWidget *parent)
    : QDialog(parent)
{
    // 基础窗口设置
    setWindowTitle("事后分析");
    resize(1200, 700); // 适配截图的宽高比例
    setStyleSheet("QDialog { background-color: #C8C8F0; }"  // 匹配截图的淡紫色背景
                  "QPushButton { font-size: 12px; }"
                  "QLabel { font-size: 12px; }"
                  "QLineEdit { font-size: 12px; }");

    // 主布局：垂直布局（整体）
    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(10);
    mainLayout->setContentsMargins(10, 10, 10, 10);

    // --------------------- 顶部工具栏：文件选择 + 关闭 ---------------------
    auto *toolBar = new QHBoxLayout();
    auto *btnOpenFile = new QPushButton("打开文件");
    btnOpenFile->setFixedSize(80, 28);
    auto *m_editFilePath = new QLineEdit();
    m_editFilePath->setReadOnly(true);
    m_editFilePath->setPlaceholderText("请选择数据文件...");
    connect(btnOpenFile, &QPushButton::clicked, this, [this, m_editFilePath]() {
        QString fp = QFileDialog::getOpenFileName(this, "选择数据文件", "", "数据文件 (*.csv *.dat *.txt *.xls *.xlsx);;所有文件 (*.*)");
        if (!fp.isEmpty()) { m_editFilePath->setText(fp); loadCsvHeaders(fp); }
    });
    auto *btnClose = new QPushButton("关闭");
    btnClose->setFixedSize(60, 28);
    connect(btnClose, &QPushButton::clicked, this, &QDialog::close);
    toolBar->addWidget(btnOpenFile);
    toolBar->addWidget(m_editFilePath, 1);
    toolBar->addStretch();
    toolBar->addWidget(btnClose);
    mainLayout->addLayout(toolBar);

    // --------------------- 上半部分：图表 + 右侧按钮 ---------------------
    auto *topLayout = new QHBoxLayout();
    topLayout->setSpacing(10);

    chartWidget = new ChartWidget(this);
    chartWidget->setTitle("事后分析图");
    chartWidget->setAxisLabels("时间", "数值");
    chartWidget->setMinimumSize(900, 400);
    topLayout->addWidget(chartWidget);

    auto *rightPanel = createRightPanel();
    topLayout->addWidget(rightPanel);
    mainLayout->addLayout(topLayout);

    // --------------------- 下半部分：参数显示面板 ---------------------
    auto *paramPanel = createParamPanel();
    mainLayout->addWidget(paramPanel);

    // ======================== 按钮功能绑定 ========================
    // 游标模式 / 平移模式
    connect(btnCursorMode, &QPushButton::clicked, this, [this]() {
        chartWidget->switchMode(InteractMode::Measure);
    });
    connect(btnPanMode, &QPushButton::clicked, this, [this]() {
        chartWidget->switchMode(InteractMode::Drag);
    });

    // 自动X轴 / 自动Y轴
    connect(btnAutoXAxis, &QPushButton::clicked, this, [this]() {
        chartWidget->plot()->rescaleAxes(true);
        chartWidget->plot()->replot();
    });
    connect(btnAutoYAxis, &QPushButton::clicked, this, [this]() {
        chartWidget->plot()->yAxis->rescale(true);
        chartWidget->plot()->replot();
    });

    // X轴放大 / 缩小（scaleRange>1=范围变大=缩小视图，<1=范围变小=放大视图）
    connect(btnXAxisZoomIn, &QPushButton::clicked, this, [this]() {
        chartWidget->plot()->xAxis->scaleRange(1.0/1.5, chartWidget->plot()->xAxis->range().center());
        chartWidget->plot()->replot();
    });
    connect(btnXAxisZoomOut, &QPushButton::clicked, this, [this]() {
        chartWidget->plot()->xAxis->scaleRange(1.5, chartWidget->plot()->xAxis->range().center());
        chartWidget->plot()->replot();
    });

    // Y轴放大 / 缩小
    connect(btnYAxisZoomIn, &QPushButton::clicked, this, [this]() {
        chartWidget->plot()->yAxis->scaleRange(1.0/1.5, chartWidget->plot()->yAxis->range().center());
        chartWidget->plot()->replot();
    });
    connect(btnYAxisZoomOut, &QPushButton::clicked, this, [this]() {
        chartWidget->plot()->yAxis->scaleRange(1.5, chartWidget->plot()->yAxis->range().center());
        chartWidget->plot()->replot();
    });

    // 区域放大 / 全景察看
    connect(btnAreaZoom, &QPushButton::clicked, this, [this]() {
        chartWidget->switchMode(InteractMode::ZoomSelect);
    });
    connect(btnFullView, &QPushButton::clicked, this, [this]() {
        chartWidget->autoFitView();
    });

    // 中心放大 / 缩小（双轴同比例）
    connect(btnCenterZoomIn, &QPushButton::clicked, this, [this]() {
        double cx = chartWidget->plot()->xAxis->range().center();
        double cy = chartWidget->plot()->yAxis->range().center();
        chartWidget->plot()->xAxis->scaleRange(1.5, cx);
        chartWidget->plot()->yAxis->scaleRange(1.5, cy);
        chartWidget->plot()->replot();
    });
    connect(btnCenterZoomOut, &QPushButton::clicked, this, [this]() {
        double cx = chartWidget->plot()->xAxis->range().center();
        double cy = chartWidget->plot()->yAxis->range().center();
        chartWidget->plot()->xAxis->scaleRange(1.0/1.5, cx);
        chartWidget->plot()->yAxis->scaleRange(1.0/1.5, cy);
        chartWidget->plot()->replot();
    });

    // 曲线颜色
    connect(btnCurveColor, &QPushButton::clicked, this, [this]() {
        QColor c = QColorDialog::getColor(Qt::blue, this, "选择曲线颜色");
        if (c.isValid() && chartWidget->plot()->graphCount() > 0) {
            QPen pen = chartWidget->plot()->graph(0)->pen();
            pen.setColor(c);
            chartWidget->plot()->graph(0)->setPen(pen);
            chartWidget->plot()->replot();
        }
    });

    // 清空数据
    connect(btnClearData, &QPushButton::clicked, this, [this]() {
        chartWidget->clearAll();
    });

    // 显示坐标：鼠标移动时更新坐标显示
    connect(cbShowCoord, &QCheckBox::toggled, this, [this](bool checked) {
        auto *p = chartWidget->plot();
        p->setMouseTracking(checked);
        if (checked && !m_coordConn) {
            m_coordConn = connect(p, &QCustomPlot::mouseMove, this, [this](QMouseEvent *e) {
                auto *p = chartWidget->plot();
                double x = p->xAxis->pixelToCoord(e->pos().x());
                double y = p->yAxis->pixelToCoord(e->pos().y());
                if (leCoordX) leCoordX->setText(QString::number(x, 'f', 3));
                if (leCoordY) leCoordY->setText(QString::number(y, 'f', 3));
            });
        } else if (!checked && m_coordConn) {
            disconnect(m_coordConn);
            m_coordConn = QMetaObject::Connection();
        }
    });

    // 链接曲线：X/Y轴联动缩放
    connect(cbLinkCurve, &QCheckBox::toggled, this, [this](bool checked) {
        if (checked) {
            // 同一axisRect内所有轴联动
            chartWidget->plot()->axisRect()->setRangeZoom(Qt::Horizontal | Qt::Vertical);
        } else {
            chartWidget->plot()->axisRect()->setRangeZoom(Qt::Horizontal | Qt::Vertical);
        }
    });

    // 开始/停止滚动（占位，暂用双击还原替代）
    connect(btnStartScroll, &QPushButton::clicked, this, [this]() {
        // 占位：滚动模式
    });
    connect(btnStopScroll, &QPushButton::clicked, this, [this]() {
        // 占位：停止滚动
    });
}

PostAnalysisDialog::~PostAnalysisDialog() = default;

// 读取CSV表头并填充下拉框
void PostAnalysisDialog::loadCsvHeaders(const QString &filePath)
{
    m_csvHeaders.clear();
    QFile f(filePath);
    if (!f.open(QIODevice::ReadOnly)) return;
    QString firstLine = QString::fromUtf8(f.readLine()).trimmed();
    f.close();

    // 按逗号拆分表头
    m_csvHeaders = firstLine.split(',');
    // 填充5个下拉框
    for (auto *cb : {cbParam1, cbParam2, cbParam3, cbParam4, cbParam5}) {
        if (!cb) continue;
        cb->clear();
        cb->addItem("不显示");
        for (const auto &h : m_csvHeaders)
            cb->addItem(h.trimmed());
    }
}

// 辅助函数：统一创建按钮（固定尺寸、统一样式）
QPushButton *PostAnalysisDialog::createButton(const QString &text)
{
    auto *btn = new QPushButton(text);
    btn->setFixedSize(100, 30); // 统一按钮尺寸
    return btn;
}

// 构建右侧按钮面板
QWidget *PostAnalysisDialog::createRightPanel()
{
    auto *panel = new QWidget(this);
    auto *vLayout = new QVBoxLayout(panel);
    vLayout->setSpacing(8);
    vLayout->setContentsMargins(5, 5, 5, 5);

    // 复选框：显示坐标、链接曲线
    cbShowCoord = new QCheckBox("显示坐标", panel);
    cbLinkCurve = new QCheckBox("链接曲线", panel);
    vLayout->addWidget(cbShowCoord);
    vLayout->addWidget(cbLinkCurve);
    vLayout->addSpacing(5); // 视觉分隔

    // 按钮行1：游标模式 + 平移模式
    auto *row1 = new QHBoxLayout();
    btnCursorMode = createButton("游标模式");
    btnPanMode = createButton("平移模式");
    row1->addWidget(btnCursorMode);
    row1->addWidget(btnPanMode);
    vLayout->addLayout(row1);

    // 按钮行2：自动X轴 + 自动Y轴
    auto *row2 = new QHBoxLayout();
    btnAutoXAxis = createButton("自动X轴");
    btnAutoYAxis = createButton("自动Y轴");
    row2->addWidget(btnAutoXAxis);
    row2->addWidget(btnAutoYAxis);
    vLayout->addLayout(row2);

    // 按钮行3：X轴放大 + X轴缩小
    auto *row3 = new QHBoxLayout();
    btnXAxisZoomIn = createButton("X轴放大");
    btnXAxisZoomOut = createButton("X轴缩小");
    row3->addWidget(btnXAxisZoomIn);
    row3->addWidget(btnXAxisZoomOut);
    vLayout->addLayout(row3);

    // 按钮行4：Y轴放大 + Y轴缩小
    auto *row4 = new QHBoxLayout();
    btnYAxisZoomIn = createButton("Y轴放大");
    btnYAxisZoomOut = createButton("Y轴缩小");
    row4->addWidget(btnYAxisZoomIn);
    row4->addWidget(btnYAxisZoomOut);
    vLayout->addLayout(row4);

    // 按钮行5：区域放大 + 全景察看
    auto *row5 = new QHBoxLayout();
    btnAreaZoom = createButton("区域放大");
    btnFullView = createButton("全景察看");
    row5->addWidget(btnAreaZoom);
    row5->addWidget(btnFullView);
    vLayout->addLayout(row5);

    // 按钮行6：中心放大 + 中心缩小
    auto *row6 = new QHBoxLayout();
    btnCenterZoomIn = createButton("中心放大");
    btnCenterZoomOut = createButton("中心缩小");
    row6->addWidget(btnCenterZoomIn);
    row6->addWidget(btnCenterZoomOut);
    vLayout->addLayout(row6);

    // 按钮行7：曲线颜色 + 清空数据
    auto *row7 = new QHBoxLayout();
    btnCurveColor = createButton("曲线颜色");
    btnClearData = createButton("清空数据");
    row7->addWidget(btnCurveColor);
    row7->addWidget(btnClearData);
    vLayout->addLayout(row7);

    // 按钮行8：开始滚动 + 停止滚动
    auto *row8 = new QHBoxLayout();
    btnStartScroll = createButton("开始滚动");
    btnStopScroll = createButton("停止滚动");
    row8->addWidget(btnStartScroll);
    row8->addWidget(btnStopScroll);
    vLayout->addLayout(row8);

    // 填充空白，让按钮置顶
    vLayout->addStretch();

    return panel;
}

// 构建下方参数显示面板
QWidget *PostAnalysisDialog::createParamPanel()
{
    auto *panel = new QWidget(this);
    auto *gridLayout = new QGridLayout(panel);
    gridLayout->setSpacing(8);
    gridLayout->setContentsMargins(5, 5, 5, 5);

    // 辅助lambda：创建（标签+下拉框）或（标签+只读输入框）
    auto createParamItem = [&](const QString &labelText, int row, int col, QWidget *&w, bool isCombo) {
        auto *label = new QLabel(labelText, panel);
        label->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        gridLayout->addWidget(label, row, col);
        if (isCombo) {
            auto *cb = new QComboBox(panel);
            cb->setFixedSize(100, 25);
            cb->addItem(“不显示”);
            w = cb;
        } else {
            auto *le = new QLineEdit(panel);
            le->setFixedSize(100, 25);
            le->setReadOnly(true);
            le->setAlignment(Qt::AlignCenter);
            w = le;
        }
        gridLayout->addWidget(w, row, col + 1);
    };
    QWidget *pw;
    createParamItem(“显示参数1”, 0, 0, pw = cbParam1, true); cbParam1 = qobject_cast<QComboBox*>(pw);
    createParamItem(“显示参数3”, 0, 2, pw = cbParam3, true); cbParam3 = qobject_cast<QComboBox*>(pw);
    createParamItem(“显示参数5”, 0, 4, pw = cbParam5, true); cbParam5 = qobject_cast<QComboBox*>(pw);
    createParamItem(“起始时标”, 0, 6, pw = leStartTimestamp, false); leStartTimestamp = qobject_cast<QLineEdit*>(pw);
    createParamItem(“最大值时标”, 0, 8, pw = leMaxTimestamp, false); leMaxTimestamp = qobject_cast<QLineEdit*>(pw);
    createParamItem(“最小值时标”, 0, 10, pw = leMinTimestamp, false); leMinTimestamp = qobject_cast<QLineEdit*>(pw);
    createParamItem(“区域平均值”, 0, 12, pw = leAreaAvg, false); leAreaAvg = qobject_cast<QLineEdit*>(pw);
    createParamItem(“滚动速度”, 0, 14, pw = leScrollSpeed, false); leScrollSpeed = qobject_cast<QLineEdit*>(pw);

    createParamItem(“显示参数2”, 1, 0, pw = cbParam2, true); cbParam2 = qobject_cast<QComboBox*>(pw);
    createParamItem(“显示参数4”, 1, 2, pw = cbParam4, true); cbParam4 = qobject_cast<QComboBox*>(pw);
    createParamItem(“计算参数”, 1, 4, pw = leCalcParam, false); leCalcParam = qobject_cast<QLineEdit*>(pw);
    createParamItem(“中止时标”, 1, 6, pw = leStopTimestamp, false); leStopTimestamp = qobject_cast<QLineEdit*>(pw);
    createParamItem(“区域最大值”, 1, 8, pw = leAreaMax, false); leAreaMax = qobject_cast<QLineEdit*>(pw);
    createParamItem(“区域最小值”, 1, 10, pw = leAreaMin, false); leAreaMin = qobject_cast<QLineEdit*>(pw);
    createParamItem(“区域均方值”, 1, 12, pw = leAreaRms, false); leAreaRms = qobject_cast<QLineEdit*>(pw);
    createParamItem(“数据对应时标”, 1, 14, pw = leDataTimestamp, false); leDataTimestamp = qobject_cast<QLineEdit*>(pw);

    // 初始化值
    leStartTimestamp->setText(“0”);
    leStopTimestamp->setText("0");
    leMaxTimestamp->setText("-1");
    leMinTimestamp->setText("-1");
    leAreaMax->setText("0.000");
    leAreaMin->setText("0.000");
    leAreaAvg->setText("NaN");
    leAreaRms->setText("NaN");
    leScrollSpeed->setText("0S/s");
    leDataTimestamp->setText("");

    return panel;
}
