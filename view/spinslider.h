#ifndef SPINSLIDER_H
#define SPINSLIDER_H

#include <QDoubleSpinBox>
#include <QPainter>

class SpinSlider : public QDoubleSpinBox
{
    Q_OBJECT

public:
    explicit SpinSlider(QWidget *parent = nullptr);

protected:
    // 重写绘制事件，绘制左侧的白色三角
    void paintEvent(QPaintEvent *event) override;

    // 调整文本显示区域，防止文字被左侧图标遮挡
    void stepBy(int steps) override;

    // 声明鼠标点击事件处理函数
    void mousePressEvent(QMouseEvent *event) override;

private:
    // 绘制单个三角形的辅助函数
    void drawTriangle(QPainter *painter, const QRect &rect, bool isUp);
};

#endif // SPINSLIDER_H
