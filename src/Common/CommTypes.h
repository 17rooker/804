#ifndef COMMTYPES_H
#define COMMTYPES_H

#include <QByteArray>
#include <QString>
#include <QDateTime>
#include <functional>
#include <cstdint>
#include <QIODevice>

// ─────────────────────────────────────────────
//  通道类型
// ─────────────────────────────────────────────
enum class EChannelType : uint8_t {
    NoDefine = 0,
    TCP           ,
    UDP_Multicast ,
    Serial
};

// ─────────────────────────────────────────────
//  通道连接状态
// ─────────────────────────────────────────────
enum class EChannelState : uint8_t {
    Disconnected = 0,
    Connecting   = 1,
    Connected    = 2,
    Error        = 3
};
Q_DECLARE_METATYPE(EChannelState)
// ─────────────────────────────────────────────
//  原始帧数据（入队单元）
// ─────────────────────────────────────────────
struct CommRawData {
    EChannelType channelType  = EChannelType::TCP;
    QString      channelId;          // 唯一标识: "192.168.1.10:8080" or "/dev/ttyS0"
    QByteArray   payload;
    QDateTime    recvTime;

    CommRawData() = default;
    CommRawData(EChannelType type, QString id, QByteArray data)
        : channelType(type)
        , channelId(std::move(id))
        , payload(std::move(data))
        , recvTime(QDateTime::currentDateTime())
    {}
};

// ─────────────────────────────────────────────
//  TCP 配置
// ─────────────────────────────────────────────
struct TcpConfig {
    QString  host;
    uint16_t port               = 0;
    int      reconnectIntervalMs = 3000;   // 断线重连间隔
    int      connectTimeoutMs    = 5000;
    bool     autoReconnect       = true;
};

// ─────────────────────────────────────────────
//  UDP 组播配置
// ─────────────────────────────────────────────
struct UdpMulticastConfig {
    QString  multicastGroup = "239.255.0.1";   // 组播地址
    uint16_t port           = 0;
    QString  listenAddr     = "0.0.0.0";       // 绑定的本地接口
    int      ttl            = 1;               // 组播 TTL（局域网=1，跨路由器需>1）
};

// ─────────────────────────────────────────────
//  串口配置
// ─────────────────────────────────────────────
struct SerialConfig {
    QString portName    = "/dev/ttyS0";
    int     baudRate    = 115200;
    int     dataBits    = 8;               // QSerialPort::Data8
    int     stopBits    = 1;               // QSerialPort::OneStop
    int     parity      = 0;               // QSerialPort::NoParity
    int     flowControl = 0;               // QSerialPort::NoFlowControl
    int     readTimeoutMs = 50;
};

// ─────────────────────────────────────────────
//  统一通道配置
// ─────────────────────────────────────────────
struct CommChannelConfig {
    EChannelType       type      = EChannelType::TCP;
    QString            channelId;          // 业务层唯一 ID（自定义）
    TcpConfig          tcpCfg;
    UdpMulticastConfig udpCfg;
    SerialConfig       serialCfg;
};



#endif // COMMTYPES_H
