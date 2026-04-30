// UdpMulticastChannel.h（纯 Asio，无 CppServer）
#ifndef UDPMULTICASTCHANNEL_H
#define UDPMULTICASTCHANNEL_H

#include "src/LogicCommunication/ICommChannel.h"

#include <asio.hpp>
#include <atomic>
#include <array>
#include <thread>
#include <functional>

/**
 * @brief UdpMulticastChannel  基于 Asio 的 UDP 组播通道
 *
 * - 加入指定组播组持续异步接收
 * - send() 支持向组播地址发送（或单播，取决于 targetEndpoint）
 * - 每个通道独立 io_context + 线程，与 Qt 主线程完全隔离
 */
class UdpMulticastChannel
{
protected:
    // 接收消息的回调函数
    using UDPDataReceivedCallback = std::function<void(const CommRawData&)>;
public:
    explicit UdpMulticastChannel(CommChannelConfig config);
    ~UdpMulticastChannel();
    bool updateConfig(const CommChannelConfig& newConfig);
    bool start();
    void stop();
    bool send(const QByteArray& data);

    bool isConnected() const;
    EChannelState state() const;
    QString channelId() const;
    EChannelType channelType() const { return EChannelType::UDP_Multicast; }

    // ── 回调注册（必须在 start() 之前调用）────────
    void setDataCallback(UDPDataReceivedCallback cb) {
        m_dataCb = std::move(cb);
    }
    void setStateCallback(ChannelStateCallback cb) {
        m_stateCb = std::move(cb);
    }

private:
    void doOpen();
    void doReceive();

private:
    CommChannelConfig               m_config;
    std::atomic<EChannelState>      m_state { EChannelState::Disconnected };
    std::atomic<bool>               m_stopFlag { false };

    // 每个通道独立的 io_context 和运行线程
    asio::io_context                m_io;
    asio::ip::udp::socket           m_socket { m_io };
    asio::ip::udp::endpoint         m_senderEndpoint;   // 记录每次数据报的来源
    std::thread                     m_ioThread;

    // 接收缓冲区（UDP 单包最大 64KB）
    static constexpr size_t kRecvBufSize = 65536;
    std::array<char, kRecvBufSize> m_recvBuf;

    UDPDataReceivedCallback m_dataCb;
    ChannelStateCallback m_stateCb;
};

#endif // UDPMULTICASTCHANNEL_H
