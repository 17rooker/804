/**
 * @file   chartwidget.h
 * @brief  基于 QCustomPlot 封装的通用图表组件
 *
 * 交互模式（三种互斥，工具栏按钮切换）：
 *   【拖拽模式】左键拖拽平移，滚轮缩放          ← 默认
 *   【框选模式】左键框选区域放大，滚轮缩放
 *   【测量模式】左键依次点击两点，显示差值
 *
 * 通用操作（任何模式下都有效）：
 *   - 滚轮缩放
 *   - 双击还原视图
 *   - 十字准星跟随鼠标
 *
 * 兼容环境：
 *   - Windows 10/11 + Qt 5.12.9 MinGW64
 *   - 银河麒麟 V10 + Qt 5.12.9
 */

#ifndef CHARTWIDGET_H
#define CHARTWIDGET_H

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QVector>
#include <QPointF>
#include "qcustomplot.h"
#include "IMessage.h"

/**
 * @brief 图表类型枚举，方便后续扩展
 */
enum class ChartType {
    Curve,      // 平滑曲线图（当前实现）
    Line,       // 折线图（预留）
    Bar,        // 柱状图（预留）
    Scatter     // 散点图（预留）
};

/**
 * @brief 交互模式枚举
 *
 * 三种模式互斥，同一时刻只能处于一种模式。
 * 切换模式时自动配置 QCustomPlot 的 interaction 属性和 selectionRectMode，
 * 从根本上避免拖拽/框选/测量之间的左键冲突。
 */
enum class InteractMode {
    Drag,       ///< 拖拽平移模式（默认）
    ZoomSelect, ///< 框选放大模式
    Measure     ///< 两点测量模式
};

/**
 * @brief 曲线数据描述结构体
 */
struct SeriesData {
    QString   name;     ///< 曲线名称（图例显示）
    QVector<double> x;  ///< X 轴数据
    QVector<double> y;  ///< Y 轴数据
    QColor    color;    ///< 曲线颜色
    int       width;    ///< 线宽（像素）

    SeriesData() : color(Qt::blue), width(2) {}
};

/**
 * @class  ChartWidget
 * @brief  可复用的图表控件，封装 QCustomPlot
 */
class ChartWidget : public QWidget
{
    Q_OBJECT

public:
    explicit ChartWidget(QWidget *parent = nullptr);
    ~ChartWidget();

    // ======================== 基础设置 ========================

    void setTitle(const QString &title);
    void setAxisLabels(const QString &xLabel, const QString &yLabel);
    void setXRange(double lower, double upper);
    void setYRange(double lower, double upper);

    // ======================== 数据操作 ========================

    int  addSeries(const SeriesData &series, ChartType type = ChartType::Curve);
    void updateSeries(int index, const QVector<double> &x, const QVector<double> &y);
    void removeSeries(int index);
    void clearAll();

    // ======================== 交互控制 ========================

    void switchMode(InteractMode mode);
    void autoFitView();
    QCustomPlot* plot() const { return m_plot; }

    //This codes was added by CyrusChen in 2025
    void setToolWigetHide(bool flag);
    void setLegendHide(bool flag);

signals:
    /** @brief 两点测量完成时发射 */
    void measureResult(QPointF p1, QPointF p2, double dx, double dy);

    void titleDoubleClickedSignal();

private slots:
    void onMouseMove(QMouseEvent *event);
    void onMousePress(QMouseEvent *event);
    void onMouseDoubleClick(QMouseEvent *event);

    void onBtnDragMode();
    void onBtnZoomSelectMode();
    void onBtnMeasureMode();
    void onBtnAutoFit();
    void onBtnClearMeasure();

private:
    void initUI();
    void initPlot();
    void initConnections();
    void updateModeUI();
    QPointF snapToNearestPoint(const QPoint &pixelPos);
    /** @brief 仅移除图上的测量标记 item（不动状态变量） */
    void removeMarkerItems();

    /** @brief 绘制测量标记（圆点、连线、差值文本） */
    void drawMeasureMarkers();

    /** @brief 清除标记 + 重置测量状态（用于"清除标记"按钮和 clearAll） */
    void clearMeasureMarkers();

private:
    // ---- 核心 ----
    QCustomPlot     *m_plot;
    QVBoxLayout     *m_mainLayout;
    QHBoxLayout     *m_toolbarLayout;

    // ---- 工具栏 ----
    QPushButton     *m_btnDrag;
    QPushButton     *m_btnZoomSelect;
    QPushButton     *m_btnMeasure;
    QPushButton     *m_btnAutoFit;
    QPushButton     *m_btnClearMeasure;
    QLabel          *m_lblStatus;

    QWidget * m_toolWidget;
    // ---- 模式 ----
    InteractMode     m_currentMode;

    // ---- 十字准星 ----
    QCPItemLine     *m_crossHairH;
    QCPItemLine     *m_crossHairV;
    QCPItemText     *m_coordLabel;

    // ---- 测量 ----
    int              m_measureStep;     ///< 0=等待第一点, 1=等待第二点
    QPointF          m_measureP1;
    QPointF          m_measureP2;
    QCPItemEllipse  *m_markerDot1;
    QCPItemEllipse  *m_markerDot2;
    QCPItemLine     *m_markerLine;
    QCPItemText     *m_markerText;

    // ---- 数据 ----
    QVector<QCPGraph*> m_graphs;


};

#endif // CHARTWIDGET_H
