#ifndef ICOMMCHANNEL_H
#define ICOMMCHANNEL_H

#include "src/Common/CommTypes.h"
#include "src/Common/StructDefine.h"
#include <memory>

/**
 * @brief ICommChannel  通信通道抽象接口
 *
 * 所有通信方式（TCP / UDP组播 / 串口）均实现此接口，
 * 上层通过工厂创建，仅依赖此接口，与具体实现完全解耦。
 */
class ICommChannel {
public:
    virtual ~ICommChannel() = default;

    // ── 生命周期 ────────────────────────────────
    /**
     * @brief start 启动通道（异步，非阻塞）
     * @return true 启动指令已发出
     */
    virtual bool start() = 0;

    /**
     * @brief stop 停止并释放底层资源
     */
    virtual void stop() = 0;

    // ── 数据发送 ────────────────────────────────
    /**
     * @brief send 发送数据（线程安全）
     * @param data 原始字节流
     * @return true 数据已提交给底层发送队列
     */
    virtual bool send(const STPackage& data) = 0;

    // ── 状态查询 ────────────────────────────────
    virtual bool         isConnected() const = 0;
    virtual EChannelState state()      const = 0;
    virtual QString      channelId()   const = 0;
    virtual EChannelType channelType() const = 0;

    // ── 配置更新（新增核心接口）───────────────────
    /**
     * @brief updateConfig 实时更新通道配置（原子操作）
     * @param newConfig 新的通道配置（需匹配通道类型）
     * @return true=更新成功；false=配置不合法/通道类型不匹配
     */
    virtual bool updateConfig(const CommChannelConfig& newConfig) = 0;

    // ── 回调注册（必须在 start() 之前调用）────────
    void setDataCallback(DataReceivedCallback cb) {
        m_dataCb = std::move(cb);
    }
    void setStateCallback(ChannelStateCallback cb) {
        m_stateCb = std::move(cb);
    }

protected:
    DataReceivedCallback m_dataCb;
    ChannelStateCallback m_stateCb;
};

using ICommChannelPtr = std::unique_ptr<ICommChannel>;

#endif // ICOMMCHANNEL_H
