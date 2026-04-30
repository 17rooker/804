#ifndef DOUBLESPINWIDGET_H
#define DOUBLESPINWIDGET_H

#include <QWidget>

class QDoubleSpinBox;
class QLabel;

class DoubleSpinWidget : public QWidget
{
    Q_OBJECT

public:
    explicit DoubleSpinWidget(const QString &title, const QString &unit, QWidget *parent = nullptr);

    // 获取数值
    double value() const;
    // 设置数值
    void setValue(double val);
    // 设置范围
    void setRange(double min, double max);
    // 设置小数位数
    void setDecimals(int decimals);

private:
    QLabel *m_labelTitle;
    QDoubleSpinBox *m_spinBox;
    QLabel *m_labelUnit;
};

#endif // DOUBLESPINWIDGET_H
