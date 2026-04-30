// datacenter.h
#ifndef DATACENTER_H
#define DATACENTER_H

#include <QObject>
#include <QTimer>
#include <QRandomGenerator>
#include "datastruct.h"

class DataCenter : public QObject
{
    Q_OBJECT
public:
    explicit DataCenter(QObject *parent = nullptr);
    static DataCenter* instance(); // 单例模式

//    SystemData m_data;

signals:
    void dataUpdated(); // 数据更新信号

private slots:
    void onTimerTimeout(); // 定时更新数据

private:
    QTimer *m_timer;
};

#endif // DATACENTER_H
