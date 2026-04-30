#include "indicatorlamp.h"
#include <QPainter>
#include <QPaintEvent>
#include <QMouseEvent>

// --- IndustrialButton 实现 ---

IndustrialButton::IndustrialButton(QWidget *parent, const QString &text)
    : QWidget(parent), m_on(false), m_text(text)
{
    // 设置固定大小，模仿 LabVIEW 的紧凑布局
    setFixedSize(70, 25);
    setCursor(Qt::PointingHandCursor);
}

void IndustrialButton::setOn(bool on)
{
    if (m_on != on) {
        m_on = on;
        update(); // 触发重绘
    }
}

void IndustrialButton::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        m_on = !m_on; // 点击切换状态
        update();
    }
    QWidget::mousePressEvent(event);
}

void IndustrialButton::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    QRect rect = this->rect().adjusted(2, 2, -2, -2); // 内部矩形，留出边框

    // 1. 绘制 3D 边框 (类似 LabVIEW 的凹陷/凸起效果)
    QPalette pal = this->palette();
    QColor lightColor = pal.color(QPalette::Light).rgb();
    QColor darkColor = pal.color(QPalette::Dark).rgb();

    painter.setPen(lightColor);
    painter.drawLine(rect.topLeft(), rect.topRight());
    painter.drawLine(rect.topLeft(), rect.bottomLeft());

    painter.setPen(darkColor);
    painter.drawLine(rect.bottomLeft(), rect.bottomRight());
    painter.drawLine(rect.topRight(), rect.bottomRight());

    // 2. 绘制内部填充 (墨绿色 -> 亮绿色)
    QRadialGradient gradient(rect.center().x(), rect.center().y(), rect.width());

    if (m_on) {
        // 点亮状态：荧光绿
        gradient.setColorAt(0, QColor(150, 255, 150)); // 中心高光
        gradient.setColorAt(1, QColor(0, 200, 0));     // 边缘深色
    } else {
        // 熄灭状态：墨绿色
        gradient.setColorAt(0, QColor(80, 100, 80));
        gradient.setColorAt(1, QColor(30, 50, 30));
    }

    painter.setBrush(gradient);
    painter.setPen(Qt::NoPen);
    // 绘制圆角矩形以模仿 LabVIEW 的圆润感
    painter.drawRoundedRect(rect, 4, 4);

    // 3. 绘制文字
    if (!m_text.isEmpty()) {
        painter.setPen(m_on ? Qt::black : Qt::white); // 点亮时文字变黑以便阅读，或保持白色
        painter.setFont(QFont("Arial", 8, QFont::Bold));
        painter.drawText(this->rect(), Qt::AlignCenter, m_text);
    }
}

// --- DigitalDisplay 实现 ---

DigitalDisplay::DigitalDisplay(QWidget *parent, const QString &value)
    : QWidget(parent), m_value(value)
{
    setFixedSize(50, 25);
}

void DigitalDisplay::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    QRect rect = this->rect().adjusted(1, 1, -1, -1);

    // 1. 背景黑色
    painter.setBrush(Qt::black);
    painter.setPen(Qt::NoPen);
    painter.drawRect(rect);

    // 2. 边框灰色
    painter.setPen(QColor(100, 100, 100));
    painter.drawRect(rect);

    // 3. 文字亮绿色
    painter.setPen(QColor(0, 255, 0));
    painter.setFont(QFont("Courier", 10, QFont::Bold)); // 等宽字体更像数码管
    painter.drawText(rect, Qt::AlignCenter, m_value);
}
