#ifndef CLICKABLELABEL_H
#define CLICKABLELABEL_H

#include <QLabel>
#include <QMouseEvent>

class ClickableLabel : public QLabel
{
Q_OBJECT
public:
explicit ClickableLabel(QWidget *parent = nullptr);

signals:
void clicked(); // 自定义点击信号

protected:
// 重写鼠标按下事件，触发点击信号
void mousePressEvent(QMouseEvent *event) override;
};

#endif // CLICKABLELABEL_H
