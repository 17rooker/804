#ifndef GASSUPPLYWIDGET_H
#define GASSUPPLYWIDGET_H

#include <QWidget>

class QLineEdit;

class GasSupplyWidget : public QWidget
{
    Q_OBJECT

public:
    explicit GasSupplyWidget(QWidget *parent = nullptr);

private:
    void setupUI();

    // 成员变量声明
    QLineEdit *m_editReplenish; // 自动补气阈值
    QLineEdit *m_editStop;      // 自动停气阈值
    QLineEdit *m_editCurrentMin;// 供气电流下限
    QLineEdit *m_editCurrentMax;// 供气电流上限
};

#endif // GASSUPPLYWIDGET_H
