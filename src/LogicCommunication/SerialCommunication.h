#ifndef SERIALCOMMUNICATION_H
#define SERIALCOMMUNICATION_H


#include "src/LogicCommunication/ICommChannel.h"
#include "src/LogicCommunication/SerialInterface/SerialChannel.h"

class SerialCommunication:public ICommChannel
{
public:
    explicit SerialCommunication(CommChannelConfig config);
    ~SerialCommunication();
    bool  start()      override;
    void  stop()       override;
    bool  send(const STPackage& data) override;
    bool updateConfig(const CommChannelConfig& newConfig) override;
    bool          isConnected() const override;
    EChannelState state()       const override;
    QString       channelId()   const override;
    EChannelType  channelType() const override { return EChannelType::Serial; }
protected:
    void init();
    void dealRecMsg(const CommRawData& recData);
private:
    std::unique_ptr<SerialChannel> m_pSerialInterface;

};

#endif // SERIALCOMMUNICATION_H
