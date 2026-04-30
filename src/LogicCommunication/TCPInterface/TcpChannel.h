// TcpChannel.h（纯 Asio，无 CppServer）
#ifndef TCPCHANNEL_H
#define TCPCHANNEL_H

#include "src/LogicCommunication/ICommChannel.h"
#include <asio.hpp>
#include <atomic>
#include <memory>
#include <deque>
#include <mutex>

class TcpChannel {
protected:
    // 接收消息的回调函数
    using TCPDataReceivedCallback = std::function<void(const CommRawData&)>;
public:
    explicit TcpChannel(CommChannelConfig config);
    ~TcpChannel() ;

    bool          start()      ;
    void          stop()       ;
    bool          send(const QByteArray& data) ;
    bool          isConnected() const ;
    EChannelState state()       const ;
    QString       channelId()   const ;
    EChannelType  channelType() const  { return EChannelType::TCP; }

    // ── 回调注册（必须在 start() 之前调用）────────
    void setDataCallback(TCPDataReceivedCallback cb) {
        m_dataCb = std::move(cb);
    }
    void setStateCallback(ChannelStateCallback cb) {
        m_stateCb = std::move(cb);
    }
private:
    void doConnect();
    void doRead();
    void doWrite();
    void scheduleReconnect();

private:
    CommChannelConfig               m_config;
    std::atomic<EChannelState>      m_state { EChannelState::Disconnected };
    std::atomic<bool>               m_stopFlag { false };

    // Asio 核心（每个通道独立 io_context + 线程）
    asio::io_context                m_io;
    asio::ip::tcp::socket           m_socket;
    asio::ip::tcp::resolver         m_resolver;
    asio::steady_timer              m_reconnectTimer;

    // io_context 运行线程
    std::thread                     m_ioThread;

    // 发送队列（异步写串行化）
    std::deque<std::vector<char>>   m_sendQueue;
    std::mutex                      m_sendMtx;
    bool                            m_writing = false;

    // 接收缓冲区（固定大小，避免频繁 heap 分配）
    static constexpr size_t kRecvBufSize = 65536;
    std::array<char, kRecvBufSize>  m_recvBuf;

    TCPDataReceivedCallback m_dataCb;
    ChannelStateCallback m_stateCb;
};

#endif // TCPCHANNEL_H
