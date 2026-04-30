#ifndef TCPCOMMUNICATION_H
#define TCPCOMMUNICATION_H

#include "src/LogicCommunication/ICommChannel.h"
#include "src/LogicCommunication/TCPInterface/TcpChannel.h"
class TCPCommunication:public ICommChannel
{
public:
    explicit TCPCommunication(CommChannelConfig config);
    ~TCPCommunication() override;
    bool          start()      override;
    void          stop()       override;
    bool          send(const STPackage& data) override;
    bool          isConnected() const override;
    EChannelState state()       const override;
    QString       channelId()   const override;
    EChannelType  channelType() const override { return EChannelType::TCP; }

    // 新增：实现配置更新接口
    bool updateConfig(const CommChannelConfig& newConfig) override;

protected:
    void init();
    void dealRecMsg(const CommRawData& recData);
private:
    std::unique_ptr<TcpChannel> m_pTCPInterface;
    CommChannelConfig m_config; // 新增：保存当前配置
};

#endif // TCPCOMMUNICATION_H
