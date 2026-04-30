#ifndef LEDINDICATOR_H
#define LEDINDICATOR_H

#include <QLabel>
#include <QWidget>

class LedIndicator : public QLabel
{
    Q_OBJECT
    Q_PROPERTY(bool on READ isOn WRITE setOn)

public:
    explicit LedIndicator(QWidget *parent = nullptr);

    // 设置灯的状态
    void setOn(bool on);
    bool isOn() const;

protected:
    // 绘制事件（可选，如果需要更复杂的渐变可以用painter，这里优先使用QSS实现）
    void paintEvent(QPaintEvent *event) override;

private:
    bool m_on;
};

#endif // LEDINDICATOR_H
