#include "CommChannelFactory.h"
#include "src/LogicCommunication/TCPCommunication.h"
#include "src/LogicCommunication/UDPCommunication.h"
#include "src/LogicCommunication/SerialCommunication.h"

#include <QDebug>
#include <stdexcept>

ICommChannelPtr CommChannelFactory::create(
    const CommChannelConfig& config,
    DataReceivedCallback     dataCb,
    ChannelStateCallback     stateCb)
{
    if (!dataCb) {
        qCritical() << "[CommChannelFactory] dataCb must not be null.";
        return nullptr;
    }

    ICommChannelPtr channel;

    switch (config.type) {
    case EChannelType::TCP:
        channel = std::make_unique<TCPCommunication>(config);
        break;

    case EChannelType::UDP_Multicast:
        channel = std::make_unique<UDPCommunication>(config);
        break;

    case EChannelType::Serial:
        channel = std::make_unique<SerialCommunication>(config);
        break;

    default:
        qCritical() << "[CommChannelFactory] Unknown channel type.";
        return nullptr;
    }

    channel->setDataCallback(std::move(dataCb));
    if (stateCb) channel->setStateCallback(std::move(stateCb));

    return channel;
}

ICommChannelPtr CommChannelFactory::createTcp(
    const QString&       channelId,
    TcpConfig            config,
    DataReceivedCallback dataCb,
    ChannelStateCallback stateCb)
{
    CommChannelConfig cfg{};
    cfg.type              = EChannelType::TCP;
    cfg.channelId         = channelId;
    cfg.tcpCfg=std::move(config);

    return create(cfg, std::move(dataCb), std::move(stateCb));
}

ICommChannelPtr CommChannelFactory::createUdpMulticast(
    const QString&       channelId,
    UdpMulticastConfig   config,
    DataReceivedCallback dataCb,
    ChannelStateCallback stateCb)
{
    CommChannelConfig cfg{};
    cfg.type                    = EChannelType::UDP_Multicast;
    cfg.channelId               = channelId;
    cfg.udpCfg=std::move(config);

    return create(cfg, std::move(dataCb), std::move(stateCb));
}

ICommChannelPtr CommChannelFactory::createSerial(
    const QString&       channelId,
    SerialConfig       config,
    DataReceivedCallback dataCb,
    ChannelStateCallback stateCb)
{
    CommChannelConfig cfg{};
    cfg.type               = EChannelType::Serial;
    cfg.channelId          = channelId;
    cfg.serialCfg=std::move(config);

    return create(cfg, std::move(dataCb), std::move(stateCb));
}
