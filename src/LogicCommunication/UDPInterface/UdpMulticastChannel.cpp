#include "UdpMulticastChannel.h"

#include <QDebug>
#include <QByteArray>

// ═══════════════════════════════════════════════
//  构造 / 析构
// ═══════════════════════════════════════════════

UdpMulticastChannel::UdpMulticastChannel(CommChannelConfig config)
    : m_config(std::move(config))
{}

UdpMulticastChannel::~UdpMulticastChannel() {
    stop();
}

bool UdpMulticastChannel::updateConfig(const CommChannelConfig &newConfig)
{
    // 仅当通道未启动时允许更新配置
    if (m_state != EChannelState::Disconnected) {
        qWarning() << "[UdpMulticast]" << m_config.channelId
                   << "update config failed: channel is not disconnected";
        return false;
    }

    m_config = newConfig;
    return true;
}

// ═══════════════════════════════════════════════
//  生命周期
// ═══════════════════════════════════════════════

bool UdpMulticastChannel::start() {
    if (m_state != EChannelState::Disconnected) {
        qWarning() << "[UdpMulticast]" << m_config.channelId
                   << "already started.";
        return false;
    }

    m_stopFlag = false;
    m_state    = EChannelState::Connecting;

    // 触发状态回调
    if (m_stateCb) {
        m_stateCb(m_config.channelId, EChannelState::Connecting);
    }

    // 在独立线程运行 io_context，不阻塞 Qt 主线程
    m_ioThread = std::thread([this] {
        // work_guard 防止 io_context 无任务时自动退出
        auto work = asio::make_work_guard(m_io);

        // 初始化 socket 并加入组播组
        doOpen();

        // 阻塞运行，直到 stop() 调用 m_io.stop()
        m_io.run();
    });

    return true;
}

void UdpMulticastChannel::stop() {
    if (m_stopFlag.exchange(true)) {
        return;   // 已经在 stop 流程中，防止重入
    }

    // 更新状态为断开中
    if (m_state != EChannelState::Disconnected) {
        m_state = EChannelState::Disconnected;
        if (m_stateCb) {
            m_stateCb(m_config.channelId, EChannelState::Disconnected);
        }
    }

    // 在 io 线程内安全关闭 socket，避免跨线程直接操作
    asio::post(m_io, [this] {
        asio::error_code ec;

        // 离开组播组
        if (m_socket.is_open()) {
            const auto& cfg = m_config.udpCfg;
            try {
                m_socket.set_option(
                    asio::ip::multicast::leave_group(
                        asio::ip::make_address(cfg.multicastGroup.toStdString())
                    ), ec);
            } catch (const std::exception& e) {
                qWarning() << "[UdpMulticast]" << m_config.channelId
                           << "leave multicast group failed:" << e.what();
                // 离开组播组失败不阻塞关闭流程
            }

            m_socket.cancel(ec);
            m_socket.close(ec);
        }

        m_io.stop();
    });

    if (m_ioThread.joinable()) {
        m_ioThread.join();
    }

    m_state = EChannelState::Disconnected;
    if (m_stateCb) {
        m_stateCb(m_config.channelId, EChannelState::Disconnected);
    }

    qInfo() << "[UdpMulticast]" << m_config.channelId << "stopped.";
}

// ═══════════════════════════════════════════════
//  发送
// ═══════════════════════════════════════════════
bool UdpMulticastChannel::send(const QByteArray& data) {
    if (!isConnected() || data.isEmpty()) return false;

    const auto& cfg = m_config.udpCfg;

    // 构造目标端点（向组播地址发送）
    asio::ip::udp::endpoint target(
        asio::ip::make_address(cfg.multicastGroup.toStdString()),
        cfg.port
        );

    // 拷贝数据到堆，供异步操作持有生命周期
    auto buf = std::make_shared<std::vector<char>>(
        data.begin(), data.end()
        );

    // post 到 io 线程执行，线程安全
    asio::post(m_io, [this, buf, target] {
        if (!m_socket.is_open() || m_stopFlag) return;

        m_socket.async_send_to(
            asio::buffer(*buf),
            target,
            [this, buf](const asio::error_code& ec, size_t bytesSent)
            {
                if (ec) {
                    qWarning() << "[UdpMulticast]" << m_config.channelId
                               << "send error:" << ec.message().c_str();
                } else {
                    Q_UNUSED(bytesSent)
                }
            }
            );
    });

    return true;
}
// ═══════════════════════════════════════════════
//  状态查询
// ═══════════════════════════════════════════════

bool UdpMulticastChannel::isConnected() const {
    return m_state == EChannelState::Connected && !m_stopFlag;
}

EChannelState UdpMulticastChannel::state() const {
    return m_state.load();
}

QString UdpMulticastChannel::channelId() const {
    return m_config.channelId;
}

// ═══════════════════════════════════════════════
//  私有：初始化 socket 并加入组播
// ═══════════════════════════════════════════════

void UdpMulticastChannel::doOpen() {
    const auto& cfg = m_config.udpCfg;

    try {
        // 1. 打开 IPv4 UDP socket
        m_socket.open(asio::ip::udp::v4());

        // 2. reuse_address：组播必须开启，允许多个实例绑定同一端口
        m_socket.set_option(asio::socket_base::reuse_address(true));

        // 3. 绑定到本地监听地址 + 端口（0.0.0.0 监听所有网卡）
        asio::ip::udp::endpoint listenEndpoint(
            asio::ip::make_address(cfg.listenAddr.toStdString()),
            cfg.port
        );
        m_socket.bind(listenEndpoint);

        // 4. 加入组播组
        m_socket.set_option(
            asio::ip::multicast::join_group(
                asio::ip::make_address(cfg.multicastGroup.toStdString())
            )
        );

        // 5. 设置组播 TTL（跨路由器需要 > 1，局域网默认 1 即可）
        m_socket.set_option(
            asio::ip::multicast::hops(cfg.ttl > 0 ? cfg.ttl : 1)
        );

//        // 6. 可选：禁用组播环回（本机发的包本机不再收到）
//        if (!cfg.loopbackEnable) {
//            m_socket.set_option(asio::ip::multicast::enable_loopback(false));
//        }

        // 更新状态并触发回调
        m_state = EChannelState::Connected;
        if (m_stateCb) {
            m_stateCb(m_config.channelId, EChannelState::Connected);
        }

        qInfo() << "[UdpMulticast]" << m_config.channelId
                << "joined group" << cfg.multicastGroup
                << "port" << cfg.port
                << "listen addr" << cfg.listenAddr;

        // 7. 开始持续异步接收
        doReceive();

    } catch (const std::exception& e) {
        m_state = EChannelState::Error;
        if (m_stateCb) {
            m_stateCb(m_config.channelId, EChannelState::Error);
        }

        qCritical() << "[UdpMulticast]" << m_config.channelId
                    << "doOpen() exception:" << e.what();

        // 关闭 socket 并停止 io 线程
        asio::error_code ec;
        m_socket.close(ec);
        m_io.stop();
    }
}

// ═══════════════════════════════════════════════
//  私有：异步接收循环
// ═══════════════════════════════════════════════

void UdpMulticastChannel::doReceive() {
    if (m_stopFlag || !m_socket.is_open()) return;

    // async_receive_from：收到一个完整 UDP 数据报即触发回调
    // m_senderEndpoint 记录数据来源地址（按需使用）
    m_socket.async_receive_from(
        asio::buffer(m_recvBuf),
        m_senderEndpoint,
        [this](const asio::error_code& ec, size_t bytes)
        {
            if (m_stopFlag) return;

            if (!ec) {
                // 正常收到数据
                if (bytes > 0 && m_dataCb) {
                    CommRawData raw(
                        EChannelType::UDP_Multicast,
                        m_config.channelId,
                        QByteArray(m_recvBuf.data(), static_cast<int>(bytes))
                    );
                    // 热路径：直接回调，由 CommDataProcessor 无锁入队
                    m_dataCb(std::move(raw));
                }
                // 挂起下一次接收，形成持续接收循环
                doReceive();

            } else if (ec == asio::error::operation_aborted) {
                // socket 被主动取消（stop() 触发），正常退出
                qInfo() << "[UdpMulticast]" << m_config.channelId
                        << "receive cancelled.";

            } else {
                // 真正的网络错误
                qWarning() << "[UdpMulticast]" << m_config.channelId
                           << "receive error:" << ec.message().c_str();

                // 仅在未停止且状态未为 Error 时更新状态
                if (m_state != EChannelState::Error) {
                    m_state = EChannelState::Error;
                    if (m_stateCb) {
                        m_stateCb(m_config.channelId, EChannelState::Error);
                    }
                }

                // 非致命错误（如临时网络抖动），继续尝试接收
                // 致命错误（socket 已关闭）由 m_stopFlag 拦截
                if (!m_stopFlag && m_socket.is_open()) {
                    doReceive();
                }
            }
        }
    );
}
