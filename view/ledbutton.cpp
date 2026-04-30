#include "ledbutton.h"

LedButton::LedButton(const QString &text, QWidget *parent)
    : QPushButton(text, parent), m_isActive(false)
{
    // 设置按钮的基本属性
    setCheckable(true); // 允许按钮保持按下状态
    setFixedSize(80, 30); // 设置固定大小，根据你的截图估算

    // 连接点击信号到槽函数
    connect(this, &QPushButton::clicked, this, &LedButton::toggleState);

    // 初始化样式
    updateStyle();
}

void LedButton::toggleState()
{
    // 切换状态
    m_isActive = !m_isActive;
    updateStyle();
}

void LedButton::updateStyle()
{
    if (m_isActive) {
        // 按下后的样式：黄色背景，黑色文字，类似“校验”的颜色
        setStyleSheet(
            "QPushButton {"
            "   background-color: #FFFF00;" // 亮黄色
            "   color: black;"
            "   border: 2px solid #BDB76B;" // 深一点的边框
            "   border-radius: 5px;"
            "   font-weight: bold;"
            "}"
        );
    } else {
        // 默认样式：墨绿色背景，亮绿色文字
        setStyleSheet(
            "QPushButton {"
            "   background-color: #2F4F2F;" // 墨绿色 (Dark Olive Green)
            "   color: #00FF00;"            // 亮绿色文字
            "   border: 2px solid #006400;" // 深绿色边框
            "   border-radius: 5px;"
            "   font-size: 10px;"
            "}"
        );
    }
}
