#ifndef COMMCHANNELFACTORY_H
#define COMMCHANNELFACTORY_H

#include "ICommChannel.h"
#include "src/Common/CommTypes.h"

/**
 * @brief CommChannelFactory  通信通道工厂
 *
 * 使用方式：
 * @code
 *   CommChannelConfig cfg;
 *   cfg.type      = EChannelType::TCP;
 *   cfg.channelId = "device_001";
 *   cfg.tcpCfg    = { "192.168.1.10", 8080 };
 *
 *   auto channel = CommChannelFactory::create(cfg,
 *       [](CommRawData data) { // 收到数据 },
 *       [](const QString& id, EChannelState s) { // 状态变化 }
 *   );
 *   channel->start();
 * @endcode
 */
class CommChannelFactory {
public:
    CommChannelFactory()  = delete;
    ~CommChannelFactory() = delete;

    /**
     * @brief create  根据配置创建对应的通道实例
     *
     * @param config       通道配置（含类型、连接参数）
     * @param dataCb       数据接收回调（在 cppserver IO 线程 / 串口工作线程触发）
     * @param stateCb      连接状态变化回调（可为空）
     * @return             通道智能指针；配置非法时返回 nullptr
     */
    static ICommChannelPtr create(
        const CommChannelConfig& config,
        DataReceivedCallback     dataCb,
        ChannelStateCallback     stateCb = nullptr
    );

    /**
     * @brief createTcp     快捷创建 TCP 通道
     */
    static ICommChannelPtr createTcp(
        const QString&       channelId,
        TcpConfig            config,
        DataReceivedCallback dataCb,
        ChannelStateCallback stateCb = nullptr
    );

    /**
     * @brief createUdpMulticast  快捷创建 UDP 组播接收通道
     */
    static ICommChannelPtr createUdpMulticast(
        const QString&       channelId,
        UdpMulticastConfig   config,
        DataReceivedCallback dataCb,
        ChannelStateCallback stateCb = nullptr
    );

    /**
     * @brief createSerial  快捷创建串口通道
     */
    static ICommChannelPtr createSerial(
        const QString&       channelId,
        SerialConfig       config,
        DataReceivedCallback dataCb,
        ChannelStateCallback stateCb = nullptr
    );
};

#endif // COMMCHANNELFACTORY_H
