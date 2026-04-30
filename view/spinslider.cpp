#include "spinslider.h"
#include <QEvent>
#include <QPainter>
#include <qevent.h>

SpinSlider::SpinSlider(QWidget *parent)
    : QDoubleSpinBox(parent)
{
    // 1. 基础外观设置
    setButtonSymbols(QAbstractSpinBox::NoButtons);
    setFocusPolicy(Qt::StrongFocus);

    // 2. 样式表：黑底绿字，字体加粗，整体尺寸变大
    setStyleSheet(
        "QDoubleSpinBox {"
        "   background-color: #000000;"
        "   color: #00FF00;"
        "   border: 1px solid #333;"
        "   padding-left: 30px;"      // 进一步增加左侧空间
        "   padding-right: 15px;"
        "   font-size: 22px;"         // 进一步增大字体
        "   font-weight: bold;"
        "   font-family: 'Consolas', 'Courier New', monospace;"
        "   height: 50px;"            // 高度保持不变
        "   width: 250px;"           // 宽度再增加一倍，从500px变为1000px
        "}"
    );
}

// 其他函数保持不变
void SpinSlider::paintEvent(QPaintEvent *event)
{
    QDoubleSpinBox::paintEvent(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    QRect iconRect(2, height() / 2 - 11, 22, 22);

    painter.setBrush(QColor("#111"));
    painter.setPen(Qt::NoPen);
    painter.drawRect(iconRect);

    painter.setBrush(Qt::white);
    painter.setPen(Qt::NoPen);

    QPolygon upArrow;
    upArrow << QPoint(iconRect.center().x() - 6, iconRect.center().y() - 2)
            << QPoint(iconRect.center().x() + 6, iconRect.center().y() - 2)
            << QPoint(iconRect.center().x(), iconRect.center().y() - 7);
    painter.drawPolygon(upArrow);

    QPolygon downArrow;
    downArrow << QPoint(iconRect.center().x() - 6, iconRect.center().y() + 2)
              << QPoint(iconRect.center().x() + 6, iconRect.center().y() + 2)
              << QPoint(iconRect.center().x(), iconRect.center().y() + 7);
    painter.drawPolygon(downArrow);
}

void SpinSlider::stepBy(int steps)
{
    QDoubleSpinBox::stepBy(steps);
}

void SpinSlider::mousePressEvent(QMouseEvent *event)
{
    QRect iconRect(2, height() / 2 - 11, 22, 22);

    if (iconRect.contains(event->pos())) {
        if (event->pos().y() < height() / 2) {
            stepBy(1);
        } else {
            stepBy(-1);
        }
    } else {
        setFocus();
        QAbstractSpinBox::mousePressEvent(event);
    }
}
