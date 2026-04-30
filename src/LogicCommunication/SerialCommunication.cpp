#include "SerialCommunication.h"


SerialCommunication::SerialCommunication(CommChannelConfig config)
{

    m_pSerialInterface=std::make_unique<SerialChannel>(config);
    this->init();
}

SerialCommunication::~SerialCommunication()
{

}

bool SerialCommunication::start()
{
    m_pSerialInterface->setDataCallback([this](const CommRawData& data){
        this->dealRecMsg(data);
    });
    m_pSerialInterface->setStateCallback(m_stateCb);
    bool bRet=m_pSerialInterface->start();
    return bRet;
}

void SerialCommunication::stop()
{
    m_pSerialInterface->stop();
}

bool SerialCommunication::send(const STPackage &data)
{
    bool bRet=m_pSerialInterface->send(data.baDataSend);
    return bRet;
}

bool SerialCommunication::updateConfig(const CommChannelConfig& newConfig) {
    // 校验通道ID一致性（避免更新错误通道）
    if (newConfig.channelId != m_pSerialInterface->channelId()) {
//        LOG_WARN("system", "updateConfig failed: channelId mismatch");
        return false;
    }
    // 调用底层 SerialChannel 的配置更新
    return m_pSerialInterface->updateConfig(newConfig);
}

bool SerialCommunication::isConnected() const
{
    return m_pSerialInterface->isConnected();
}

EChannelState SerialCommunication::state() const
{
    return m_pSerialInterface->state();
}

QString SerialCommunication::channelId() const
{
    return m_pSerialInterface->channelId();
}

void SerialCommunication::init()
{

}

void SerialCommunication::dealRecMsg(const CommRawData &recData)
{
    STPackage stPackage{};
    stPackage.baDataRecv=recData.payload;
    stPackage.channelId=recData.channelId;
    stPackage.channelType=recData.channelType;
    m_dataCb(std::move(stPackage));
}
