#ifndef CUSTOMWIDGETS_H
#define CUSTOMWIDGETS_H

#include <QWidget>
#include <QPainter>
#include <QMouseEvent>

// 1. 工业风格方形按钮 (LabVIEW 风格)
class IndustrialButton : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(bool on READ isOn WRITE setOn)

public:
    explicit IndustrialButton(QWidget *parent = nullptr, const QString &text = "");

    bool isOn() const { return m_on; }
    void setOn(bool on);

    void setText(const QString &text) { m_text = text; update(); }
    QString text() const { return m_text; }

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;

private:
    bool m_on;
    QString m_text;
};

// 2. 黑色背景绿色数字显示框
class DigitalDisplay : public QWidget
{
    Q_OBJECT
public:
    explicit DigitalDisplay(QWidget *parent = nullptr, const QString &value = "0");

    void setValue(const QString &val) { m_value = val; update(); }

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QString m_value;
};

#endif // CUSTOMWIDGETS_H
