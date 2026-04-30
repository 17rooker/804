#include "styledlineedit.h"
#include <QFocusEvent>

StyledLineEdit::StyledLineEdit(QWidget *parent)
    : QLineEdit(parent)
{
    setFixedWidth(80);
    setFixedHeight(22);
    setAlignment(Qt::AlignCenter);

    // 默认原始黑色样式
    m_normalBg = QColor(0x00, 0x00, 0x00);
    m_focusBg  = QColor(0x00, 0x00, 0x00);

    // 灰色模式初始化
    m_isGrayInputMode = false;
    m_grayBg  = QColor(0xAA, 0xAA, 0xAA);  // 默认灰色
    m_blackBg = QColor(0x00, 0x00, 0x00);  // 激活黑色

    updateStyleSheet();
}

// 新增接口：启用灰色激活模式
void StyledLineEdit::setGrayInputMode(bool enable)
{
    m_isGrayInputMode = enable;
    // 启用后立即设置为灰色背景
    if (enable) {
        m_normalBg = m_grayBg;
        m_focusBg = m_blackBg;
    }
    updateStyleSheet();
}

// 点击输入框（获得焦点）→ 黑色
void StyledLineEdit::focusInEvent(QFocusEvent *event)
{
    if (m_isGrayInputMode) {
        m_normalBg = m_blackBg;
        m_focusBg = m_blackBg;
        updateStyleSheet();
    }
    QLineEdit::focusInEvent(event);
}

// 离开输入框（失去焦点）→ 灰色
void StyledLineEdit::focusOutEvent(QFocusEvent *event)
{
    if (m_isGrayInputMode) {
        m_normalBg = m_grayBg;
        m_focusBg = m_grayBg;
        updateStyleSheet();
    }
    QLineEdit::focusOutEvent(event);
}

// 原有样式刷新（无修改）
void StyledLineEdit::updateStyleSheet()
{
    QString style = QString(
        "QLineEdit {"
        "   background-color: %1;"
        "   color: #00FF00;"
        "   border: 1px solid #444;"
        "   font-family: 'Consolas';"
        "   font-weight: bold;"
        "}"
        "QLineEdit:focus {"
        "   background-color: %2;"
        "}"
    ).arg(m_normalBg.name()).arg(m_focusBg.name());
    setStyleSheet(style);
}

// 原有接口
void StyledLineEdit::setNormalBackground(const QColor &color)
{
    m_normalBg = color;
    updateStyleSheet();
}

void StyledLineEdit::setFocusBackground(const QColor &color)
{
    m_focusBg = color;
    updateStyleSheet();
}

// 新增接口：设置自定义固定宽度
void StyledLineEdit::setCustomFixedWidth(int width)
{
    // 宽度需大于0，避免无效值
    if (width > 0) {
        setFixedWidth(width);
    }
}

// 新增接口：设置自定义固定高度
void StyledLineEdit::setCustomFixedHeight(int height)
{
    // 高度需大于0，避免无效值
    if (height > 0) {
        setFixedHeight(height);
    }
}

// 新增接口：同时设置自定义固定宽高
void StyledLineEdit::setCustomFixedSize(int width, int height)
{
    if (width > 0 && height > 0) {
        setFixedSize(width, height);
    }
}
