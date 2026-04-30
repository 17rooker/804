#ifndef SIMULATEDIALOG_H
#define SIMULATEDIALOG_H

#include <QDialog>
#include <QGridLayout>
#include <QLabel>
#include <QVector>

class SimulateDialog : public QDialog
{
Q_OBJECT
public:
explicit SimulateDialog(QWidget *parent = nullptr);

private slots:
void onLightClicked(); // 处理指示灯点击事件

private:
void setupUI(); // 初始化界面
QVector<QLabel*> m_lightLabels; // 存储所有指示灯控件
};

#endif // SIMULATEDIALOG_H
