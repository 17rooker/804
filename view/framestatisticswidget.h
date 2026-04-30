#ifndef FRAMESTATISTICSWIDGET_H
#define FRAMESTATISTICSWIDGET_H

#include <QWidget>
#include <QLineEdit>
#include <QTextEdit>
#include <QLabel>
#include <QPushButton>
#include <QGridLayout>
#include <QMap>

// 帧类型枚举
enum FrameType {
    FrameAA,
    FrameBB,
    FrameCC,
    FrameDE
};

// 统计数据结构体
struct FrameStats {
    int frameCount = 0;    // 帧计数
    int errorCount = 0;    // 错帧计数
    int missCount = 0;     // 漏帧计数
};

class FrameStatisticsWidget : public QWidget
{
    Q_OBJECT
public:
    explicit FrameStatisticsWidget(QWidget *parent = nullptr);

    // 对外提供的统计接口
    void addFrameCount(FrameType type);    // 增加帧数
    void addErrorCount(FrameType type);    // 增加错帧数
    void addMissCount(FrameType type);     // 增加漏帧数
    void appendRawFrameText(const QString &text); // 追加遥测口原帧文本

private:
    // 初始化UI布局
    void initUI();

    // 初始化统计数据
    void initStats();

    // 更新输入框显示的计数
    void updateStatsDisplay();

private:
    QMap<FrameType, FrameStats> m_frameStats; // 帧统计数据
    QMap<FrameType, QLineEdit*> m_frameCountEdits; // 帧计数输入框
    QMap<FrameType, QLineEdit*> m_errorCountEdits; // 错帧计数输入框
    QMap<FrameType, QLineEdit*> m_missCountEdits;   // 漏帧计数输入框

    QTextEdit *m_rawFrameTextEdit; // 遥测口原帧文本框
    QLabel *m_rawFrameLabel;       // 遥测口原帧标签
};

#endif // FRAMESTATISTICSWIDGET_H
