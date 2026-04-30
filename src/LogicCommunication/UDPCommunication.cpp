// UDPCommunication.cpp
#include "UDPCommunication.h"

UDPCommunication::UDPCommunication(CommChannelConfig config)
    : m_config(std::move(config))  // 初始化配置
{
    // 【修正】变量名统一为m_pUdpInterface
    m_pUdpInterface = std::make_unique<UdpMulticastChannel>(config);
    this->init();
}

UDPCommunication::~UDPCommunication()
{
}

// 【关键】实现 updateConfig 纯虚函数
bool UDPCommunication::updateConfig(const CommChannelConfig& newConfig)
{
    // 1. 校验配置合法性（必须是UDP组播类型）
    if (newConfig.type != EChannelType::UDP_Multicast) {
        qWarning() << "[UDPCommunication] updateConfig: invalid channel type";
        return false;
    }

    // 2. 停止当前UDP通道（原子更新，避免配置冲突）
    m_pUdpInterface->stop();

    // 3. 更新底层UdpMulticastChannel配置（需确保UdpMulticastChannel实现了updateConfig）
    bool ret = m_pUdpInterface->updateConfig(newConfig);

    // 4. 若更新成功，刷新本地配置；若通道处于运行态，重启通道
    if (ret) {
        m_config = newConfig;
        if (this->isConnected()) { // 若原通道已连接，重启生效
            m_pUdpInterface->start();
        }
    }

    return ret;
}

// 以下为原有函数，仅修正变量名引用（m_pUDPInterface → m_pUdpInterface）
bool UDPCommunication::start()
{
    m_pUdpInterface->setDataCallback([this](const CommRawData& data){
        this->dealRecMsg(data);
    });
    m_pUdpInterface->setStateCallback(m_stateCb);
    bool bRet = m_pUdpInterface->start();
    return bRet;
}

void UDPCommunication::stop()
{
    m_pUdpInterface->stop();
}

bool UDPCommunication::send(const STPackage &data)
{
    bool bRet = m_pUdpInterface->send(data.baDataSend);
    return bRet;
}

bool UDPCommunication::isConnected() const
{
    return m_pUdpInterface->isConnected();
}

EChannelState UDPCommunication::state() const
{
    return m_pUdpInterface->state();
}

QString UDPCommunication::channelId() const
{
    return m_pUdpInterface->channelId();
}

void UDPCommunication::init()
{
}

void UDPCommunication::dealRecMsg(const CommRawData &recData)
{
    STPackage stPackage{};
    stPackage.baDataRecv = recData.payload;
    stPackage.channelId = recData.channelId;
    stPackage.channelType = recData.channelType;
    m_dataCb(std::move(stPackage));
}
