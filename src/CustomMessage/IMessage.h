#ifndef IMESSAGE_H
#define IMESSAGE_H


#include "IEvent.h"

/**
 * @brief IMessage  消息订阅者接口
 *
 * UI 窗体或业务对象继承此接口并向 MessageHandle 订阅，
 * 即可接收对应类型的 IEvent。
 *
 * 使用示例：
 * @code
 * class MyWidget : public QWidget, public IMessage {
 * public:
 *     MyWidget() {
 *         // 订阅实时数据
 *         DataInteractionManager::getInstance()
 *             .getMsgHandle()
 *             ->subMessage(this, ESubDataType::E_RealTimeData);
 *     }
 *     ~MyWidget() {
 *         // 析构前务必取消订阅，防止野指针
 *         DataInteractionManager::getInstance()
 *             .getMsgHandle()
 *             ->unSubMessageAll(this);
 *     }
 *
 *     // condition() 返回 false 表示当前不可见/非活跃，
 *     // 超时消息等特殊事件仍会强制投递（见 MessageHandle）
 *     bool condition() override { return isVisible(); }
 *
 *     void onMessage(IEvent* pEvent) override {
 *         // 在主线程执行，可安全操作 UI
 *     }
 * };
 * @endcode
 */
class  IMessage {
public:
    virtual ~IMessage() = default;

    /**
     * @brief condition  订阅者当前是否处于活跃状态
     *
     * 返回 false 时，普通消息不会投递；
     * 超时告警、控制指令反馈等优先级消息会跳过此判断强制投递。
     * 默认返回 true（始终接收）。
     */
    virtual bool condition() { return true; }

    /**
     * @brief onMessage  接收并处理事件（在主线程调用）
     * @param pEvent  事件指针，生命周期由 Qt 事件系统管理，禁止 delete
     */
    virtual void onMessage(IEvent* pEvent) = 0;
};

#endif // IMESSAGE_H
