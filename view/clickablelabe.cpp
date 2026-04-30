#include "clickablelabel.h"

ClickableLabel::ClickableLabel(QWidget *parent)
: QLabel(parent)
{
// 可选：设置鼠标样式为“手型”，提示可点击
setCursor(Qt::PointingHandCursor);
}

void ClickableLabel::mousePressEvent(QMouseEvent *event)
{
// 触发点击信号
emit clicked();
// 调用父类实现（保持默认行为）
QLabel::mousePressEvent(event);
}
