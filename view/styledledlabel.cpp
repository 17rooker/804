#include "styledledlabel.h"

StyledLedLabel::StyledLedLabel(QWidget *parent)
    : QLabel(parent),
      m_on(false),
      m_isSwitchable(false),
      m_isDisabled(false)
{
    setFixedSize(24, 24);
    setAlignment(Qt::AlignCenter);
    updateStyle();
}

// 亮灭状态设置
void StyledLedLabel::setOn(bool on)
{
    if (m_on != on) {
        m_on = on;
        updateStyle();
    }
}

bool StyledLedLabel::isOn() const
{
    return m_on;
}

// 可切换设置
void StyledLedLabel::setSwitchable(bool enable)
{
    m_isSwitchable = enable;
    if (!m_isDisabled) {
        setCursor(enable ? Qt::PointingHandCursor : Qt::ArrowCursor);
    }
}

bool StyledLedLabel::isSwitchable() const
{
    return m_isSwitchable;
}

// 禁用状态设置
void StyledLedLabel::setDisabledLed(bool disabled)
{
    if (m_isDisabled != disabled) {
        m_isDisabled = disabled;
        updateStyle();
        if (m_isDisabled) {
            setCursor(Qt::ArrowCursor);
        } else {
            setCursor(m_isSwitchable ? Qt::PointingHandCursor : Qt::ArrowCursor);
        }
    }
}

bool StyledLedLabel::isDisabledLed() const
{
    return m_isDisabled;
}

// 重写点击：禁用时不响应点击
void StyledLedLabel::mousePressEvent(QMouseEvent *event)
{
    if (!m_isDisabled && event->button() == Qt::LeftButton && m_isSwitchable) {
        setOn(!m_on);
    }
    QLabel::mousePressEvent(event);
}

// 更新样式：优化禁用灭灯的灰绿朦胧感
void StyledLedLabel::updateStyle()
{
    QString style;

    // 最高优先级——禁用状态
    if (m_isDisabled) {
        if (m_on) {
            // 禁用+亮灯：保留浅绿半透，和原逻辑一致
            style = R"(
                QLabel {
                    border-radius: 4px;
                    border: 1px solid rgba(46, 139, 87, 0.5);
                    background-color: qradialgradient(
                        cx:0.5, cy:0.5, radius: 0.8,
                        fx:0.3, fy:0.3,
                        stop:0 rgba(204, 255, 204, 0.4),
                        stop:0.5 rgba(0, 255, 0, 0.3),
                        stop:1 rgba(0, 100, 0, 0.5)
                    );
                    padding: 1px;
                }
            )";
        } else {
            // --------------------------
            // 重点修改：禁用+灭灯状态
            // 调整为「灰蒙蒙带点绿」的效果，和图中第三个灯匹配
            // --------------------------
            style = R"(
                QLabel {
                    border-radius: 4px;
                    /* 浅灰绿边框，弱化深色感 */
                    border: 1px solid rgba(120, 140, 110, 0.6);
                    background-color: qradialgradient(
                        cx:0.3, cy:0.3, radius: 0.75,
                        fx:0.25, fy:0.25, /* 高光点更偏左上，模拟原图的小高光 */
                        stop:0 rgba(180, 200, 170, 0.6),  /* 高光：浅灰绿，带朦胧感 */
                        stop:0.5 rgba(100, 120, 90, 0.5), /* 主体：中等灰绿，偏浅不发黑 */
                        stop:1 rgba(60, 80, 55, 0.7)      /* 阴影：深灰绿，保留绿调，不是纯黑 */
                    );
                    padding: 1px;
                }
            )";
        }
    }
    // 正常亮灯状态
    else if (m_on) {
        style = R"(
            QLabel {
                border-radius: 4px;
                border: 1px solid #2E8B57;
                background-color: qradialgradient(
                    cx:0.5, cy:0.5, radius: 0.8,
                    fx:0.3, fy:0.3,
                    stop:0 #CCFFCC,
                    stop:0.5 #00FF00,
                    stop:1 #006400
                );
                padding: 1px;
            }
        )";
    }
    // 正常灭灯状态
    else {
        style = R"(
            QLabel {
                border-radius: 4px;
                border: 2px solid #4B5320;
                background-color: qradialgradient(
                    cx:0.3, cy:0.3, radius: 0.7,
                    fx:0.2, fy:0.2,
                    stop:0 #3B5030,
                    stop:0.5 #1A2515,
                    stop:1 #051005
                );
            }
        )";
    }

    setStyleSheet(style);
}
