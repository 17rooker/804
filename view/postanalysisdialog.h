#ifndef POSTANALYSISDIALOG_H
#define POSTANALYSISDIALOG_H

#include <QDialog>
#include <QCheckBox>
#include <QPushButton>
#include <QLabel>
#include <QLineEdit>

// 前置声明自定义图表控件
class ChartWidget;

class PostAnalysisDialog : public QDialog
{
    Q_OBJECT

public:
    explicit PostAnalysisDialog(QWidget *parent = nullptr);
    ~PostAnalysisDialog() override;

private:
    // 右侧功能按钮
    QCheckBox *cbShowCoord;       // 显示坐标
    QCheckBox *cbLinkCurve;       // 链接曲线
    QPushButton *btnCursorMode;   // 游标模式
    QPushButton *btnPanMode;      // 平移模式
    QPushButton *btnAutoXAxis;    // 自动X轴
    QPushButton *btnAutoYAxis;    // 自动Y轴
    QPushButton *btnXAxisZoomIn;  // X轴放大
    QPushButton *btnXAxisZoomOut; // X轴缩小
    QPushButton *btnYAxisZoomIn;  // Y轴放大
    QPushButton *btnYAxisZoomOut; // Y轴缩小
    QPushButton *btnAreaZoom;     // 区域放大
    QPushButton *btnFullView;     // 全景察看
    QPushButton *btnCenterZoomIn; // 中心放大
    QPushButton *btnCenterZoomOut;// 中心缩小
    QPushButton *btnCurveColor;   // 曲线颜色
    QPushButton *btnClearData;    // 清空数据
    QPushButton *btnStartScroll;  // 开始滚动
    QPushButton *btnStopScroll;   // 停止滚动

    // 下方参数显示框
    QLineEdit *leParam1;          // 显示参数1
    QLineEdit *leParam2;          // 显示参数2
    QLineEdit *leParam3;          // 显示参数3
    QLineEdit *leParam4;          // 显示参数4
    QLineEdit *leParam5;          // 显示参数5
    QLineEdit *leCalcParam;       // 计算参数
    QLineEdit *leStartTimestamp;  // 起始时标
    QLineEdit *leStopTimestamp;   // 中止时标
    QLineEdit *leMaxTimestamp;    // 最大值时标
    QLineEdit *leMinTimestamp;    // 最小值时标
    QLineEdit *leAreaMax;         // 区域最大值
    QLineEdit *leAreaMin;         // 区域最小值
    QLineEdit *leAreaAvg;         // 区域平均值
    QLineEdit *leAreaRms;         // 区域均方值
    QLineEdit *leScrollSpeed;     // 滚动速度
    QLineEdit *leDataTimestamp;   // 数据对应时标

    // 核心图表控件
    ChartWidget *chartWidget;

    // 辅助函数：统一创建按钮（简化重复代码）
    QPushButton *createButton(const QString &text);
    // 辅助函数：构建右侧按钮面板
    QWidget *createRightPanel();
    // 辅助函数：构建下方参数显示面板
    QWidget *createParamPanel();
};

#endif // POSTANALYSISDIALOG_H
