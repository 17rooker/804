// TcpChannel.cpp
#include "TcpChannel.h"
#include <QDebug>

TcpChannel::TcpChannel(CommChannelConfig config)
    : m_config(std::move(config))
    , m_socket(m_io)
    , m_resolver(m_io)
    , m_reconnectTimer(m_io)
{}

TcpChannel::~TcpChannel() {
    stop();
}

bool TcpChannel::start() {
    m_stopFlag = false;
    m_state    = EChannelState::Connecting;

    // 在独立线程运行 io_context（不阻塞 Qt 主线程）
    m_ioThread = std::thread([this] {
        // 防止 io_context 无任务时立即退出
        auto work = asio::make_work_guard(m_io);
        doConnect();
        m_io.run();   // 阻塞直到 stop() 调用 m_io.stop()
    });

    return true;
}

void TcpChannel::stop() {
    m_stopFlag = true;

    // 在 io 线程内安全关闭 socket
    asio::post(m_io, [this] {
        asio::error_code ec;
        m_socket.cancel(ec);
        m_socket.close(ec);
        m_reconnectTimer.cancel();
        m_io.stop();
    });

    if (m_ioThread.joinable()) {
        m_ioThread.join();
    }
    m_state = EChannelState::Disconnected;
}

void TcpChannel::doConnect() {
    const auto& cfg = m_config.tcpCfg;

    m_resolver.async_resolve(
        cfg.host.toStdString(),
        std::to_string(cfg.port),
        [this](const asio::error_code& ec,
               asio::ip::tcp::resolver::results_type results)
        {
            if (ec || m_stopFlag) return;

            asio::async_connect(m_socket, results,
                                [this](const asio::error_code& ec2,
                                       const asio::ip::tcp::endpoint&)
                                {
                                    if (m_stopFlag) return;

                                    if (!ec2) {
                                        // 开启 TCP_NODELAY，降低延迟
                                        m_socket.set_option(
                                            asio::ip::tcp::no_delay(true));

                                        m_state = EChannelState::Connected;
                                        if (m_stateCb)
                                            m_stateCb(m_config.channelId,
                                                      EChannelState::Connected);
                                        doRead();   // 开始持续异步读
                                    } else {
                                        m_state = EChannelState::Error;
                                        if (m_stateCb)
                                            m_stateCb(m_config.channelId,
                                                      EChannelState::Error);
                                        scheduleReconnect();
                                    }
                                });
        });
}

void TcpChannel::doRead() {
    // async_read_some：数据就绪即回调，不等填满缓冲区
    // 可能需要拼包处理
    m_socket.async_read_some(
        asio::buffer(m_recvBuf),
        [this](const asio::error_code& ec, size_t bytes)
        {
            if (m_stopFlag) return;

            if (!ec && bytes > 0) {
                if (m_dataCb) {
                    CommRawData raw(
                        EChannelType::TCP,
                        m_config.channelId,
                        QByteArray(m_recvBuf.data(),
                                   static_cast<int>(bytes))
                        );
                    m_dataCb(std::move(raw));
                }
                doRead();   // 继续挂起下一次读
            } else {
                // 连接断开
                m_state = EChannelState::Disconnected;
                if (m_stateCb)
                    m_stateCb(m_config.channelId,
                              EChannelState::Disconnected);
                if (m_config.tcpCfg.autoReconnect)
                    scheduleReconnect();
            }
        });
}

bool TcpChannel::send(const QByteArray& data) {
    if (!isConnected() || data.isEmpty()) return false;

    // 将数据拷贝进发送队列（线程安全）
    std::vector<char> buf(data.begin(), data.end());

    std::unique_lock<std::mutex> lock(m_sendMtx);
    m_sendQueue.push_back(std::move(buf));
    bool needTrigger = !m_writing;
    lock.unlock();

    // 若当前没有正在进行的写操作，触发 doWrite
    if (needTrigger) {
        asio::post(m_io, [this] { doWrite(); });
    }
    return true;
}

void TcpChannel::doWrite() {
    std::unique_lock<std::mutex> lock(m_sendMtx);
    if (m_sendQueue.empty()) {
        m_writing = false;
        return;
    }
    m_writing = true;
    auto buf = std::move(m_sendQueue.front());
    m_sendQueue.pop_front();
    lock.unlock();

    // async_write 保证整个 buf 全部写完（内部循环）
    asio::async_write(
        m_socket,
        asio::buffer(buf),
        [this, buf = std::move(buf)](const asio::error_code& ec, size_t)
        {
            if (!ec && !m_stopFlag) {
                doWrite();   // 继续发送队列中下一条
            } else {
                std::lock_guard<std::mutex> lk(m_sendMtx);
                m_writing = false;
            }
        });
}

void TcpChannel::scheduleReconnect() {
    const int interval = m_config.tcpCfg.reconnectIntervalMs;
    m_reconnectTimer.expires_after(
        std::chrono::milliseconds(interval));
    m_reconnectTimer.async_wait([this](const asio::error_code& ec) {
        if (!ec && !m_stopFlag) {
            asio::error_code ignored;
            m_socket.close(ignored);
            m_state = EChannelState::Connecting;
            doConnect();
        }
    });
}

bool          TcpChannel::isConnected() const { return m_state == EChannelState::Connected; }
EChannelState TcpChannel::state()       const { return m_state.load(); }
QString       TcpChannel::channelId()   const { return m_config.channelId; }
