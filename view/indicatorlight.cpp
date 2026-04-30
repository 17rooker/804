// indicatorlight.cpp
#include "indicatorlight.h"

IndicatorLight::IndicatorLight(QWidget *parent) : QWidget(parent), m_color(Qt::green), m_isOn(true)
{
    setFixedSize(12, 12); // 灯的大小
}

void IndicatorLight::setColor(QColor c)
{
    m_color = c;
    update();
}

void IndicatorLight::setOn(bool on)
{
    m_isOn = on;
    update();
}

void IndicatorLight::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    if (!m_isOn) {
        p.setBrush(Qt::darkGray);
    } else {
        p.setBrush(m_color);
    }

    p.setPen(Qt::NoPen);
    p.drawEllipse(1, 1, width()-2, height()-2);

    // 画个边框让它更立体
    p.setBrush(Qt::NoBrush);
    p.setPen(QPen(Qt::black, 1));
    p.drawEllipse(1, 1, width()-2, height()-2);
}
