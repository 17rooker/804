#include "ledindicator.h"
#include <QPainter>

LedIndicator::LedIndicator(QWidget *parent) : QLabel(parent), m_on(false)
{
    // 设置默认大小
    setFixedSize(20, 20);
    // 设置样式表
    // 关键点：使用 qradialgradient 模拟反光/突面效果
    // 灭：深墨绿色 (#003300)
    // 亮：鲜绿色 (#00FF00)
    setStyleSheet(
        "QLabel {"
        "   border: 2px solid gray;"
        "   border-radius: 4px;"
        "   background-color: qradialgradient(cx:0.3, cy:0.3, fx:0.3, fy:0.3, radius:1.0, stop:0 #555555, stop:1 #003300);" // 默认灭的状态，模拟突面反光
        "}"
    );
}

void LedIndicator::paintEvent(QPaintEvent *event)
{
    // 动态更新背景色
    QString color = m_on ? "#00FF00" : "#003300"; // 亮绿 vs 墨绿
    QString highlight = m_on ? "#88FF88" : "#555555"; // 亮时的反光更强

    // 重新设置样式表来改变颜色
    // 注意：频繁setStyleSheet性能一般，但在工业监控这种刷新率下是可以接受的
    setStyleSheet(
        QString("QLabel { border: 2px solid gray; border-radius: 4px; "
                "background-color: qradialgradient(cx:0.3, cy:0.3, radius:1.0, stop:0 %1, stop:1 %2); }")
        .arg(highlight).arg(color)
    );
    QLabel::paintEvent(event);
}

void LedIndicator::setOn(bool on)
{
    if (m_on != on) {
        m_on = on;
        update(); // 触发重绘
    }
}

bool LedIndicator::isOn() const
{
    return m_on;
}
