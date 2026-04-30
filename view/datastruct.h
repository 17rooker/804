#ifndef DATASTRUCT_H
#define DATASTRUCT_H

#include <QString>
const qint64 CHANNEL_BYTE_LENGTH = 84;
// 单个机构的数据结构
struct MechanismData {
    // 拉力 (kN)
    double tractionForce1;
    double tractionForce2;

    // 压力 (MPa)
    double tankPressure1;
    double tankPressure2;

    // 温度 (C)
    double internalTemp;

    // 状态标志 (true=正常/绿灯, false=异常/红灯)
    bool statusUnlock;      // 快速分离器解锁到位
    bool statusBolt;        // 爆炸螺栓起爆解锁
    bool statusRelease;     // 释放到位
    bool statusReset;       // 牵制臂复位到位
    bool statusLock;        // 锁定到位

    // 异常标志
    bool alarmTraction;     // 牵制拉力异常
    bool alarmPressure;     // 储气罐压力异常
    bool alarmCable;        // 控制电缆连接状态 (true=正常, false=异常)

    // 时间 (ms)
    int timeUnlock;
    int timeReleasePneumatic;
    int timeReleaseBolt;

    // 角度
    double angleState;

    MechanismData() {
        // 初始化默认值
        tractionForce1 = 0.000; tractionForce2 = 0.000;
        tankPressure1 = 0.000; tankPressure2 = 0.000;
        internalTemp = 0.000;
        angleState = 0.000;
        timeUnlock = 0; timeReleasePneumatic = 0; timeReleaseBolt = 0;

        statusUnlock = true; statusBolt = true; statusRelease = true;
        statusReset = true; statusLock = true;
        alarmTraction = false; alarmPressure = false; alarmCable = true;
    }
};

//// 全局数据结构
//struct SystemData {
//    MechanismData mech[5]; // 索引1-4对应机构1-4
//    SystemData() {}
//};

#endif // DATASTRUCT_H
