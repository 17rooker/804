#include "TCPCommunication.h"

TCPCommunication::TCPCommunication(CommChannelConfig config)
    : m_config(std::move(config)) // 新增：保存初始配置
{
    m_pTCPInterface=std::make_unique<TcpChannel>(m_config);
    this->init();
}

TCPCommunication::~TCPCommunication()
{
}

bool TCPCommunication::start()
{
    m_pTCPInterface->setDataCallback([this](const CommRawData& data){
        this->dealRecMsg(data);
    });
    m_pTCPInterface->setStateCallback(m_stateCb);
    bool bRet=m_pTCPInterface->start();
    return bRet;
}

void TCPCommunication::stop()
{
    m_pTCPInterface->stop();
}

bool TCPCommunication::send(const STPackage &data)
{
    bool bRet=m_pTCPInterface->send(data.baDataSend);
    return bRet;
}

bool TCPCommunication::isConnected() const
{
    return m_pTCPInterface->isConnected();
}

EChannelState TCPCommunication::state() const
{
    return m_pTCPInterface->state();
}

QString TCPCommunication::channelId() const
{
    return m_pTCPInterface->channelId();
}

void TCPCommunication::init()
{
}

void TCPCommunication::dealRecMsg(const CommRawData &recData)
{
    STPackage stPackage{};
    stPackage.baDataRecv=recData.payload;
    stPackage.channelId=recData.channelId;
    stPackage.channelType=recData.channelType;
    m_dataCb(std::move(stPackage));
}

// 新增核心：配置更新实现
bool TCPCommunication::updateConfig(const CommChannelConfig& newConfig)
{
    // 1. 校验配置合法性
    if (newConfig.type != EChannelType::TCP || newConfig.channelId != m_config.channelId) {
        qWarning() << "[TCPCommunication] updateConfig: invalid config (type/channelId mismatch)";
        return false;
    }

    // 2. 原子操作：停止旧连接 → 替换配置 → 创建新连接
    bool wasRunning = (state() != EChannelState::Disconnected); // 记录更新前的运行状态
    this->stop(); // 停止旧通道

    // 3. 更新配置并重建TCP接口
    m_config = newConfig;
    m_pTCPInterface.reset(); // 释放旧接口
    m_pTCPInterface = std::make_unique<TcpChannel>(m_config); // 创建新接口

    // 4. 恢复运行状态（如果更新前是启动的）
    if (wasRunning) {
        this->start();
    }

    qInfo() << "[TCPCommunication] config updated for channel:" << newConfig.channelId;
    return true;
}
