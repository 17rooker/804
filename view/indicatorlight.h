// indicatorlight.h
#ifndef INDICATORLIGHT_H
#define INDICATORLIGHT_H

#include <QWidget>
#include <QPainter>

class IndicatorLight : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(QColor color READ color WRITE setColor)

public:
    explicit IndicatorLight(QWidget *parent = nullptr);

    void setColor(QColor c);
    QColor color() const { return m_color; }
    void setOn(bool on);

private:
    void paintEvent(QPaintEvent *) override;
    QColor m_color;
    bool m_isOn;
};

#endif // INDICATORLIGHT_H
