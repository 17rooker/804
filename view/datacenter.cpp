// datacenter.cpp
#include "datacenter.h"

DataCenter* DataCenter::instance()
{
    static DataCenter inst;
    return &inst;
}

DataCenter::DataCenter(QObject *parent) : QObject(parent)
{
    m_timer = new QTimer(this);
    connect(m_timer, &QTimer::timeout, this, &DataCenter::onTimerTimeout);
    m_timer->start(200); // 200ms 刷新一次
}

void DataCenter::onTimerTimeout()
{
//    // --- 模拟数据逻辑 ---
//    for(int i=1; i<=4; i++) {
//        auto &m = m_data.mech[i];

//        // 模拟拉力 3kN ~ 60kN (正常范围)
//        m.tractionForce1 = 3.0 + (qrand() % 570) / 10.0;
//        m.tractionForce2 = 3.0 + (qrand() % 570) / 10.0;

//        // 模拟压力 0.6 ~ 1.3 MPa
//        m.tankPressure1 = 0.6 + (qrand() % 70) / 100.0;
//        m.tankPressure2 = 0.6 + (qrand() % 70) / 100.0;

//        // 模拟温度
//        m.internalTemp = 20.0 + (qrand() % 100) / 10.0;

//        // 模拟角度
//        m.angleState = (qrand() % 50) / 10.0;

//        // 模拟状态灯 (随机闪烁模拟动态，实际应读取硬件)
//        m.statusUnlock = true;
//        m.statusBolt = true;
//        m.statusRelease = true;
//        m.statusReset = true;
//        m.statusLock = true;
//        m.alarmCable = true;

//        // 模拟异常逻辑测试 (这里写死为正常，你可以手动改代码测试红灯)
//        m.alarmTraction = false;
//        m.alarmPressure = false;
//    }

//    emit dataUpdated(); // 发射信号，通知界面刷新
}
