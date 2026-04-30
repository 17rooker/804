// UDPCommunication.h
#ifndef UDPCOMMUNICATION_H
#define UDPCOMMUNICATION_H

#include "src/LogicCommunication/ICommChannel.h"
#include "src/LogicCommunication/UDPInterface/UdpMulticastChannel.h"

class UDPCommunication : public ICommChannel
{
public:
    explicit UDPCommunication(CommChannelConfig config);
    ~UDPCommunication() override;

    // 必须实现ICommChannel的所有纯虚函数
    bool          start() override;
    void          stop() override;
    bool          send(const STPackage& data) override;
    bool          isConnected() const override;
    EChannelState state() const override;
    QString       channelId() const override;
    EChannelType  channelType() const override { return EChannelType::UDP_Multicast; }

    // 【关键】补充缺失的 updateConfig 纯虚函数声明
    bool          updateConfig(const CommChannelConfig& newConfig) override;

protected:
    void init();
    void dealRecMsg(const CommRawData& recData);

private:
    // 【修正】统一变量名（原m_pUDPInterface改为m_pUdpInterface，与cpp中一致）
    std::unique_ptr<UdpMulticastChannel> m_pUdpInterface;
    CommChannelConfig                    m_config;
};

#endif // UDPCOMMUNICATION_H
