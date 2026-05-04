/**
 * @file   chartwidget.cpp
 * @brief  ChartWidget 实现
 *
 * 核心设计：三种交互模式互斥，通过 switchMode() 统一切换
 *   - Drag 模式：  iRangeDrag 开, srmNone   → 左键拖拽平移
 *   - ZoomSelect： iRangeDrag 关, srmZoom   → 左键框选放大
 *   - Measure：    iRangeDrag 关, srmNone   → 左键点选测量点
 *   - 滚轮缩放（iRangeZoom）在所有模式下始终可用
 */

#include "chartwidget.h"
#include <QDebug>
#include <QtMath>
#include <limits>
#include <QSlider>
// ============================================================================
//  构造 & 析构
// ============================================================================

ChartWidget::ChartWidget(QWidget *parent)
    : QWidget(parent)
    , m_plot(nullptr)
    , m_currentMode(InteractMode::Drag)
    , m_crossHairH(nullptr)
    , m_crossHairV(nullptr)
    , m_coordLabel(nullptr)
    , m_measureStep(0)
    , m_markerDot1(nullptr)
    , m_markerDot2(nullptr)
    , m_markerLine(nullptr)
    , m_markerText(nullptr)
{

    initUI();
    initPlot();
    initConnections();
    clearMeasureMarkers();
    // 默认进入拖拽模式
    switchMode(InteractMode::Drag);
}

ChartWidget::~ChartWidget()
{
    // QCustomPlot 会自动释放所有子 item，无需手动 delete
}

// ============================================================================
//  界面初始化
// ============================================================================

void ChartWidget::initUI()
{
    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setContentsMargins(4, 4, 4, 4);
    m_mainLayout->setSpacing(4);

    // ---- 工具栏 ----
    m_toolbarLayout = new QHBoxLayout();
    m_toolbarLayout->setSpacing(4);

    // 模式切换按钮组（视觉上互斥高亮）
    m_btnDrag = new QPushButton(QString::fromUtf8("✋ 拖拽"), this);
    m_btnDrag->setFixedHeight(28);
    m_btnDrag->setToolTip(QString::fromUtf8("左键拖拽平移视图，滚轮缩放"));

    m_btnZoomSelect = new QPushButton(QString::fromUtf8("🔍 框选"), this);
    m_btnZoomSelect->setFixedHeight(28);
    m_btnZoomSelect->setToolTip(QString::fromUtf8("左键框选一个区域，松开后放大到该区域"));

    m_btnMeasure = new QPushButton(QString::fromUtf8("📏 测量"), this);
    m_btnMeasure->setFixedHeight(28);
    m_btnMeasure->setToolTip(QString::fromUtf8("点击图上两点，计算 ΔX、ΔY 差值"));

    // 功能按钮
    m_btnAutoFit = new QPushButton(QString::fromUtf8("⊞ 自适应"), this);
    m_btnAutoFit->setFixedHeight(28);
    m_btnAutoFit->setToolTip(QString::fromUtf8("双击图表也可还原视图"));

    m_btnClearMeasure = new QPushButton(QString::fromUtf8("✕ 清除标记"), this);
    m_btnClearMeasure->setFixedHeight(28);

    m_lblStatus = new QLabel(this);
    m_lblStatus->setStyleSheet("color: #555; font-size: 12px;");

    // 创建水平滑动条
    QSlider *slider = new QSlider(Qt::Horizontal);
    slider->setRange(0, 100);        // 设置范围
    slider->setValue(50);           // 设置初始值
    slider->setTickPosition(QSlider::TicksBelow);  // 刻度位置
    slider->setTickInterval(10);    // 刻度间隔

    // 创建标签显示当前值
    QLabel *label = new QLabel("当前值: 50");

    // 连接信号槽
    QObject::connect(slider, &QSlider::valueChanged, [=](int value) {
        label->setText(QString("当前值: %1").arg(value));
    });

    // 分隔线效果：在模式按钮和功能按钮之间加个竖线
    QFrame *sep = new QFrame(this);
    sep->setFrameShape(QFrame::VLine);
    sep->setFixedWidth(2);
    sep->setStyleSheet("color: #ccc;");

    m_toolbarLayout->addWidget(m_btnDrag);
    m_toolbarLayout->addWidget(m_btnZoomSelect);
    m_toolbarLayout->addWidget(m_btnMeasure);
    m_toolbarLayout->addWidget(sep);
    m_toolbarLayout->addWidget(m_btnAutoFit);
    m_toolbarLayout->addWidget(m_btnClearMeasure);
    m_toolbarLayout->addStretch();
    m_toolbarLayout->addWidget(slider);
    m_toolbarLayout->addWidget(label);

    // ---- 绘图控件 ----
    m_plot = new QCustomPlot(this);
    // m_plot->setMinimumSize(400, 300);//This codes was modified by CyrusChen in 2025
#if 0
    m_mainLayout->addLayout(m_toolbarLayout);
#else

    m_toolWidget=new QWidget(this);
    m_toolWidget->setLayout(m_toolbarLayout);
    m_toolWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
    m_mainLayout->addWidget(m_toolWidget);

#endif
    m_mainLayout->addWidget(m_plot, 1);
}

void ChartWidget::initPlot()
{
    // ======================== 样式 ========================

    m_plot->setBackground(QBrush(QColor(255, 255, 255)));
    m_plot->axisRect()->setBackground(QBrush(QColor(250, 250, 250)));

    QFont axisFont = font();
    axisFont.setPointSize(6);//This codes was modified by CyrusChen in 2025

    // X 轴
    m_plot->xAxis->setLabelFont(axisFont);
    m_plot->xAxis->setTickLabelFont(axisFont);
    m_plot->xAxis->setBasePen(QPen(QColor(80, 80, 80), 1));
    m_plot->xAxis->setTickPen(QPen(QColor(80, 80, 80), 1));
    m_plot->xAxis->setSubTickPen(QPen(QColor(160, 160, 160), 1));
    m_plot->xAxis->setTickLabelColor(QColor(60, 60, 60));
    m_plot->xAxis->setLabelColor(QColor(40, 40, 40));

    // Y 轴
    m_plot->yAxis->setLabelFont(axisFont);
    m_plot->yAxis->setTickLabelFont(axisFont);
    m_plot->yAxis->setBasePen(QPen(QColor(80, 80, 80), 1));
    m_plot->yAxis->setTickPen(QPen(QColor(80, 80, 80), 1));
    m_plot->yAxis->setSubTickPen(QPen(QColor(160, 160, 160), 1));
    m_plot->yAxis->setTickLabelColor(QColor(60, 60, 60));
    m_plot->yAxis->setLabelColor(QColor(40, 40, 40));

    // 网格线
    m_plot->xAxis->grid()->setPen(QPen(QColor(220, 220, 220), 1, Qt::DotLine));
    m_plot->yAxis->grid()->setPen(QPen(QColor(220, 220, 220), 1, Qt::DotLine));

    // 图例
    m_plot->legend->setVisible(true);
    m_plot->legend->setFont(axisFont);
    m_plot->legend->setBrush(QBrush(QColor(255, 255, 255, 200)));
#if 0
    // m_plot->legend->setBorderPen(QPen(QColor(180, 180, 180)));
#else
    m_plot->legend->setBorderPen(Qt::NoPen);         // 隐藏图例边框（可选，图二无边框）
#endif
    m_plot->axisRect()->insetLayout()->setInsetAlignment(0, Qt::AlignTop | Qt::AlignRight);

    // ======================== 交互基础 ========================

    // 滚轮缩放始终开启（不受模式影响）
    m_plot->setInteraction(QCP::iRangeZoom, true);
    m_plot->axisRect()->setRangeZoom(Qt::Horizontal | Qt::Vertical);
    m_plot->axisRect()->setRangeDrag(Qt::Horizontal | Qt::Vertical);

    // 鼠标跟踪（不按键也触发 mouseMove）
    m_plot->setMouseTracking(true);

    // ======================== 十字准星 ========================

    m_crossHairH = new QCPItemLine(m_plot);
    m_crossHairH->setPen(QPen(QColor(120, 120, 120), 1, Qt::DashLine));
    m_crossHairH->setVisible(false);

    m_crossHairV = new QCPItemLine(m_plot);
    m_crossHairV->setPen(QPen(QColor(120, 120, 120), 1, Qt::DashLine));
    m_crossHairV->setVisible(false);

    m_coordLabel = new QCPItemText(m_plot);
    m_coordLabel->setPositionAlignment(Qt::AlignLeft | Qt::AlignBottom);
    m_coordLabel->position->setType(QCPItemPosition::ptPlotCoords);
    m_coordLabel->setFont(QFont(font().family(), 9));
    m_coordLabel->setColor(QColor(80, 80, 80));
    m_coordLabel->setPadding(QMargins(4, 2, 4, 2));
    m_coordLabel->setBrush(QBrush(QColor(255, 255, 230, 200)));
    m_coordLabel->setPen(QPen(QColor(180, 180, 150)));
    m_coordLabel->setVisible(false);
}

void ChartWidget::initConnections()
{
    // QCustomPlot 鼠标信号
    connect(m_plot, &QCustomPlot::mouseMove,        this, &ChartWidget::onMouseMove);
    connect(m_plot, &QCustomPlot::mousePress,       this, &ChartWidget::onMousePress);
    connect(m_plot, &QCustomPlot::mouseDoubleClick, this, &ChartWidget::onMouseDoubleClick);

    // 工具栏按钮
    connect(m_btnDrag,         &QPushButton::clicked, this, &ChartWidget::onBtnDragMode);
    connect(m_btnZoomSelect,   &QPushButton::clicked, this, &ChartWidget::onBtnZoomSelectMode);
    connect(m_btnMeasure,      &QPushButton::clicked, this, &ChartWidget::onBtnMeasureMode);
    connect(m_btnAutoFit,      &QPushButton::clicked, this, &ChartWidget::onBtnAutoFit);
    connect(m_btnClearMeasure, &QPushButton::clicked, this, &ChartWidget::onBtnClearMeasure);
}

// ============================================================================
//  模式切换  —— 核心逻辑，解决拖拽/框选/测量的左键冲突
// ============================================================================

void ChartWidget::switchMode(InteractMode mode)
{
    m_currentMode = mode;

    switch (mode) {
    case InteractMode::Drag:
        // 开启拖拽，关闭框选
        m_plot->setInteraction(QCP::iRangeDrag, true);
        m_plot->setSelectionRectMode(QCP::srmNone);
        m_plot->setCursor(Qt::OpenHandCursor);
        m_lblStatus->setText(QString::fromUtf8("拖拽模式：左键拖拽平移，滚轮缩放"));
        break;

    case InteractMode::ZoomSelect:
        // 关闭拖拽，开启框选
        m_plot->setInteraction(QCP::iRangeDrag, false);
        m_plot->setSelectionRectMode(QCP::srmZoom);
        m_plot->setCursor(Qt::CrossCursor);
        m_lblStatus->setText(QString::fromUtf8("框选模式：左键框选区域放大"));
        break;

    case InteractMode::Measure:
        // 关闭拖拽，关闭框选
        m_plot->setInteraction(QCP::iRangeDrag, false);
        m_plot->setSelectionRectMode(QCP::srmNone);
        m_measureStep = 0;
        m_plot->setCursor(Qt::CrossCursor);
        m_lblStatus->setText(QString::fromUtf8("测量模式：请点击第一个测量点"));
        break;
    }

    updateModeUI();
}

void ChartWidget::updateModeUI()
{
    // 高亮当前模式的按钮，其余按钮恢复默认样式
    const QString styleActive =
        "QPushButton { background-color: #3498db; color: white; "
        "border: 1px solid #2980b9; border-radius: 3px; padding: 2px 8px; font-weight: bold; }";
    const QString styleNormal =
        "QPushButton { background-color: #f0f0f0; color: #333; "
        "border: 1px solid #ccc; border-radius: 3px; padding: 2px 8px; }";

    m_btnDrag->setStyleSheet(
        m_currentMode == InteractMode::Drag ? styleActive : styleNormal);
    m_btnZoomSelect->setStyleSheet(
        m_currentMode == InteractMode::ZoomSelect ? styleActive : styleNormal);
    m_btnMeasure->setStyleSheet(
        m_currentMode == InteractMode::Measure ? styleActive : styleNormal);
}

// ============================================================================
//  基础设置
// ============================================================================

void ChartWidget::setTitle(const QString &title)
{
    // 检查是否已有标题行，避免重复插入
    if (m_plot->plotLayout()->rowCount() == 1) {
        m_plot->plotLayout()->insertRow(0);
    }
    QCPTextElement *titleElem = new QCPTextElement(m_plot, title);
    connect(titleElem, &QCPTextElement::doubleClicked, this, [this]() {
        // 发射自定义信号
        emit titleDoubleClickedSignal();
    });

    QFont f = font();
    f.setPointSize(10);
    f.setBold(true);
    titleElem->setFont(f);
    titleElem->setTextColor(QColor("#1C3A52"));
    m_plot->plotLayout()->addElement(0, 0, titleElem);
    m_plot->replot();
}

void ChartWidget::setAxisLabels(const QString &xLabel, const QString &yLabel)
{
    m_plot->xAxis->setLabel(xLabel);
    m_plot->yAxis->setLabel(yLabel);
    m_plot->replot();
}

void ChartWidget::setXRange(double lower, double upper)
{
    m_plot->xAxis->setRange(lower, upper);
    m_plot->replot();
}

void ChartWidget::setYRange(double lower, double upper)
{
    m_plot->yAxis->setRange(lower, upper);
    m_plot->replot();
}

// ============================================================================
//  数据操作
// ============================================================================

int ChartWidget::addSeries(const SeriesData &series, ChartType type)
{
    QCPGraph *graph = m_plot->addGraph();
    graph->setName(series.name);
    graph->setData(series.x, series.y);

    QPen pen(series.color, series.width);

    switch (type) {
    case ChartType::Curve:
        // 平滑曲线：实线，不显示数据点标记
        pen.setCapStyle(Qt::RoundCap);
        pen.setJoinStyle(Qt::RoundJoin);
        graph->setPen(pen);
        graph->setScatterStyle(QCPScatterStyle::ssNone);
        break;

    case ChartType::Line:
        // 折线：带数据点圆标记
        graph->setPen(pen);
        graph->setScatterStyle(
            QCPScatterStyle(QCPScatterStyle::ssCircle, series.color, 6));
        break;

    case ChartType::Scatter:
        // 散点：不连线，只显示点
        graph->setLineStyle(QCPGraph::lsNone);
        graph->setScatterStyle(
            QCPScatterStyle(QCPScatterStyle::ssDisc, series.color, 8));
        break;

    case ChartType::Bar:
        // 柱状图预留（QCPGraph 用脉冲线模拟）
        graph->setLineStyle(QCPGraph::lsImpulse);
        pen.setWidth(qMax(series.width, 6));
        graph->setPen(pen);
        break;
    }

    // 自适应范围，留 5% 边距
    m_plot->rescaleAxes(true);
    double xPad = m_plot->xAxis->range().size() * 0.05;
    double yPad = m_plot->yAxis->range().size() * 0.05;
    m_plot->xAxis->setRange(
        m_plot->xAxis->range().lower - xPad,
        m_plot->xAxis->range().upper + xPad);
    m_plot->yAxis->setRange(
        m_plot->yAxis->range().lower - yPad,
        m_plot->yAxis->range().upper + yPad);

    m_plot->replot();

    int index = m_graphs.size();
    m_graphs.append(graph);
    return index;
}

void ChartWidget::updateSeries(int index,
                               const QVector<double> &x,
                               const QVector<double> &y)
{
    if (index < 0 || index >= m_graphs.size() || !m_graphs[index]) {
        qWarning() << QString::fromUtf8("[ChartWidget] updateSeries 无效索引:") << index;
        return;
    }
    m_graphs[index]->setData(x, y);
    m_plot->replot();
}

void ChartWidget::removeSeries(int index)
{
    if (index < 0 || index >= m_graphs.size() || !m_graphs[index]) return;
    m_plot->removeGraph(m_graphs[index]);
    m_graphs[index] = nullptr;
    m_plot->replot();
}

void ChartWidget::clearAll()
{
    m_plot->clearGraphs();
    m_graphs.clear();
    clearMeasureMarkers();
    m_plot->replot();
}

void ChartWidget::autoFitView()
{
    if (m_plot->graphCount() == 0) return;

    m_plot->rescaleAxes(true);
    double xPad = m_plot->xAxis->range().size() * 0.05;
    double yPad = m_plot->yAxis->range().size() * 0.05;
    m_plot->xAxis->setRange(
        m_plot->xAxis->range().lower - xPad,
        m_plot->xAxis->range().upper + xPad);
    m_plot->yAxis->setRange(
        m_plot->yAxis->range().lower - yPad,
        m_plot->yAxis->range().upper + yPad);
    m_plot->replot();
}

void ChartWidget::setToolWigetHide(bool flag)
{
    if(m_toolWidget)
    {
        m_toolWidget->setVisible(flag);
    }
}

void ChartWidget::setLegendHide(bool flag)
{
    if(m_plot)
    {
        m_plot->legend->setVisible(flag);
    }

}

// ============================================================================
//  鼠标事件
// ============================================================================

void ChartWidget::onMouseMove(QMouseEvent *event)
{
    double x = m_plot->xAxis->pixelToCoord(event->pos().x());
    double y = m_plot->yAxis->pixelToCoord(event->pos().y());
    bool inPlot = m_plot->axisRect()->rect().contains(event->pos());

    // ---- 十字准星 ----
    m_crossHairH->setVisible(inPlot);
    m_crossHairV->setVisible(inPlot);
    m_coordLabel->setVisible(inPlot);

    if (inPlot) {
        // 水平线贯穿整个 X 轴
        m_crossHairH->start->setCoords(m_plot->xAxis->range().lower, y);
        m_crossHairH->end->setCoords(m_plot->xAxis->range().upper, y);
        // 垂直线贯穿整个 Y 轴
        m_crossHairV->start->setCoords(x, m_plot->yAxis->range().lower);
        m_crossHairV->end->setCoords(x, m_plot->yAxis->range().upper);

        // 坐标标签带偏移，避免遮挡十字心
        double ox = m_plot->xAxis->range().size() * 0.02;
        double oy = m_plot->yAxis->range().size() * 0.02;
        m_coordLabel->position->setCoords(x + ox, y + oy);
        m_coordLabel->setText(QString("X: %1\nY: %2")
                                  .arg(x, 0, 'f', 4)
                                  .arg(y, 0, 'f', 4));
    }

    // 排队刷新，避免高频 replot 卡顿
    m_plot->replot(QCustomPlot::rpQueuedReplot);
}

void ChartWidget::onMousePress(QMouseEvent *event)
{
    // ---- 拖拽模式：按下时切换手型光标 ----
    if (m_currentMode == InteractMode::Drag && event->button() == Qt::LeftButton) {
        m_plot->setCursor(Qt::ClosedHandCursor);
        // 松开时恢复（通过 mouseRelease，这里用 lambda 一次性连接）
        auto conn = std::make_shared<QMetaObject::Connection>();
        *conn = connect(m_plot, &QCustomPlot::mouseRelease, this,
                        [this, conn](QMouseEvent *) {
                            m_plot->setCursor(Qt::OpenHandCursor);
                            disconnect(*conn);
                        });
        return;
    }

    // ---- 测量模式 ----
    if (m_currentMode != InteractMode::Measure)
        return;
    if (event->button() != Qt::LeftButton)
        return;

    // 吸附到最近的曲线数据点
    QPointF snapped = snapToNearestPoint(event->pos());

    if (m_measureStep == 0) {
        // 第一点
        m_measureP1 = snapped;
        m_measureStep = 1;
        m_lblStatus->setText(
            QString::fromUtf8("已选第一点 (%1, %2)  →  请点击第二个点")
                .arg(snapped.x(), 0, 'f', 4)
                .arg(snapped.y(), 0, 'f', 4));
        drawMeasureMarkers();  // 只画第一个点
    }
    else {
        // 第二点，计算差值
        m_measureP2 = snapped;
        m_measureStep = 0;

        double dx = m_measureP2.x() - m_measureP1.x();
        double dy = m_measureP2.y() - m_measureP1.y();
        double dist = qSqrt(dx * dx + dy * dy);

        drawMeasureMarkers();  // 画第二个点 + 连线 + 差值

        // 发射信号
        emit measureResult(m_measureP1, m_measureP2, dx, dy);

        // 自动切回拖拽模式，但保留标记和状态文本
        switchMode(InteractMode::Drag);
        m_lblStatus->setText(
            QString::fromUtf8("测量完成  ΔX: %1   ΔY: %2   距离: %3")
                .arg(dx, 0, 'f', 4)
                .arg(dy, 0, 'f', 4)
                .arg(dist, 0, 'f', 4));
    }
}

void ChartWidget::onMouseDoubleClick(QMouseEvent *event)
{
    Q_UNUSED(event);
    autoFitView();
}

// ============================================================================
//  工具栏按钮槽
// ============================================================================

void ChartWidget::onBtnDragMode()       { switchMode(InteractMode::Drag); }
void ChartWidget::onBtnZoomSelectMode() { switchMode(InteractMode::ZoomSelect); }
void ChartWidget::onBtnMeasureMode()    { switchMode(InteractMode::Measure); }
void ChartWidget::onBtnAutoFit()        { autoFitView(); }

void ChartWidget::onBtnClearMeasure()
{
    clearMeasureMarkers();
    m_lblStatus->setText("");
    m_plot->replot();
}

// ============================================================================
//  数据点吸附
// ============================================================================

QPointF ChartWidget::snapToNearestPoint(const QPoint &pixelPos)
{
    double x = m_plot->xAxis->pixelToCoord(pixelPos.x());
    double y = m_plot->yAxis->pixelToCoord(pixelPos.y());
    QPointF result(x, y);

    double minDist = std::numeric_limits<double>::max();
    const double snapThreshold = 20.0;  // 吸附阈值（像素）

    for (QCPGraph *graph : m_graphs) {
        if (!graph || !graph->visible())
            continue;

        QSharedPointer<QCPGraphDataContainer> data = graph->data();
        for (auto it = data->constBegin(); it != data->constEnd(); ++it) {
            double px = m_plot->xAxis->coordToPixel(it->key);
            double py = m_plot->yAxis->coordToPixel(it->value);
            double dist = qSqrt(qPow(px - pixelPos.x(), 2)
                                + qPow(py - pixelPos.y(), 2));
            if (dist < minDist) {
                minDist = dist;
                result = QPointF(it->key, it->value);
            }
        }
    }

    // 超出阈值则不吸附，用鼠标原始坐标
    if (minDist > snapThreshold) {
        result = QPointF(x, y);
    }

    return result;
}

// ============================================================================
//  测量标记绘制与清除
// ============================================================================

void ChartWidget::drawMeasureMarkers()
{
    // 只移除旧的视觉标记，不重置测量状态（m_measureStep / m_measureP1 / P2）
    removeMarkerItems();

    // ---- 第一个点：红色圆点 ----
    m_markerDot1 = new QCPItemEllipse(m_plot);
    // 用像素大小换算成坐标偏移，保证圆点大小不随缩放变化太大
    double rx = m_plot->xAxis->range().size() * 0.006;
    double ry = m_plot->yAxis->range().size() * 0.006;
    m_markerDot1->topLeft->setCoords(m_measureP1.x() - rx, m_measureP1.y() + ry);
    m_markerDot1->bottomRight->setCoords(m_measureP1.x() + rx, m_measureP1.y() - ry);
    m_markerDot1->setPen(QPen(QColor(220, 50, 50), 2));
    m_markerDot1->setBrush(QBrush(QColor(220, 50, 50, 140)));

    // ---- 如果只选了第一个点，到此结束 ----
    if (m_measureStep == 1) {
        m_plot->replot();
        return;
    }

    // ---- 第二个点：蓝色圆点 ----
    m_markerDot2 = new QCPItemEllipse(m_plot);
    m_markerDot2->topLeft->setCoords(m_measureP2.x() - rx, m_measureP2.y() + ry);
    m_markerDot2->bottomRight->setCoords(m_measureP2.x() + rx, m_measureP2.y() - ry);
    m_markerDot2->setPen(QPen(QColor(50, 50, 220), 2));
    m_markerDot2->setBrush(QBrush(QColor(50, 50, 220, 140)));

    // ---- 连线：橙色虚线 ----
    m_markerLine = new QCPItemLine(m_plot);
    m_markerLine->setPen(QPen(QColor(255, 100, 50), 2, Qt::DashDotLine));
    m_markerLine->start->setCoords(m_measureP1.x(), m_measureP1.y());
    m_markerLine->end->setCoords(m_measureP2.x(), m_measureP2.y());

    // ---- 差值文本：连线中点上方 ----
    double dx = m_measureP2.x() - m_measureP1.x();
    double dy = m_measureP2.y() - m_measureP1.y();
    double midX = (m_measureP1.x() + m_measureP2.x()) / 2.0;
    double midY = (m_measureP1.y() + m_measureP2.y()) / 2.0;

    m_markerText = new QCPItemText(m_plot);
    m_markerText->position->setCoords(midX, midY);
    m_markerText->setPositionAlignment(Qt::AlignBottom | Qt::AlignHCenter);
    m_markerText->setText(QString::fromUtf8("ΔX:%1  ΔY:%2")
                              .arg(dx, 0, 'f', 3)
                              .arg(dy, 0, 'f', 3));
    m_markerText->setFont(QFont(font().family(), 10, QFont::Bold));
    m_markerText->setColor(QColor(200, 60, 20));
    m_markerText->setPadding(QMargins(6, 3, 6, 3));
    m_markerText->setBrush(QBrush(QColor(255, 255, 240, 220)));
    m_markerText->setPen(QPen(QColor(200, 180, 150)));

    m_plot->replot();
}

void ChartWidget::removeMarkerItems()
{
    // 仅移除图上的 QCPItem 标记对象，不修改任何测量状态变量
    if (m_markerDot1) {
        m_plot->removeItem(m_markerDot1);
        m_markerDot1 = nullptr;
    }
    if (m_markerDot2) {
        m_plot->removeItem(m_markerDot2);
        m_markerDot2 = nullptr;
    }
    if (m_markerLine) {
        m_plot->removeItem(m_markerLine);
        m_markerLine = nullptr;
    }
    if (m_markerText) {
        m_plot->removeItem(m_markerText);
        m_markerText = nullptr;
    }
}

void ChartWidget::clearMeasureMarkers()
{
    // 移除视觉标记
    removeMarkerItems();

    // 重置测量状态
    m_measureStep = 0;
    m_measureP1 = QPointF();
    m_measureP2 = QPointF();
}
