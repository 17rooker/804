#include "CommManager.h"

#include <QDebug>
#include <QMetaObject>

CommManager& CommManager::instance() {
    static CommManager sIns;
    return sIns;
}

CommManager::CommManager(QObject* parent)
    : QObject(parent)
    , m_processor(0)   // 0 = CPU核心数自动
{}

CommManager::~CommManager() {
    stop();
}

void CommManager::registerParser(CommDataProcessor::EAnalysisType type,IDataAnalysisPtr parser) {
    m_processor.registerParser(type,std::move(parser));
}

void CommManager::registerProcessor(CommDataProcessor::EProcessCategory type, IDataProcessPtr processor)
{
    m_processor.registerProcessor(type,std::move(processor));
}

// ─────────────────────────────────────────────
//  通道添加
// ─────────────────────────────────────────────

bool CommManager::addTcpChannel(const QString& channelId,
                                TcpConfig   config)
{
    auto channel = CommChannelFactory::createTcp(
        channelId, std::move(config),
        [this](STPackage d) { onRawDataReceived(std::move(d)); },
        [this](const QString& id, EChannelState s) { onChannelState(id, s); }
        );
    if (!channel) return false;

    std::lock_guard<std::mutex> lock(m_channelsMtx);
    if (m_channels.count(channelId)) {
        qWarning() << "[CommManager] channelId already exists:" << channelId;
        return false;
    }
    if (m_running) channel->start();
    m_channels[channelId] = std::move(channel);
    return true;
}

bool CommManager::addUdpMulticastChannel(const QString& channelId,
                                         UdpMulticastConfig config)
{
    auto channel = CommChannelFactory::createUdpMulticast(
        channelId, std::move(config),
        [this](STPackage d) { onRawDataReceived(std::move(d)); },
        [this](const QString& id, EChannelState s) { onChannelState(id, s); }
        );
    if (!channel) return false;

    std::lock_guard<std::mutex> lock(m_channelsMtx);
    if (m_channels.count(channelId)) return false;
    if (m_running) channel->start();
    m_channels[channelId] = std::move(channel);
    return true;
}

bool CommManager::addSerialChannel(const QString& channelId,
                                   SerialConfig config)
{
    auto channel = CommChannelFactory::createSerial(
        channelId, std::move(config),
        [this](STPackage d) { onRawDataReceived(std::move(d)); },
        [this](const QString& id, EChannelState s) { onChannelState(id, s); }
        );
    if (!channel) return false;

    std::lock_guard<std::mutex> lock(m_channelsMtx);
    if (m_channels.count(channelId)) return false;
    if (m_running) channel->start();
    m_channels[channelId] = std::move(channel);
    return true;
}

// ─────────────────────────────────────────────
//  TCP通道管理（新增）
// ─────────────────────────────────────────────
bool CommManager::removeTcpChannel(const QString& channelId)
{
    std::lock_guard<std::mutex> lock(m_channelsMtx);
    auto it = m_channels.find(channelId);
    if (it == m_channels.end()) {
        qWarning() << "[CommManager] removeTcpChannel: channel not found -" << channelId;
        return false;
    }

    // 校验是否为TCP通道
    if (it->second->channelType() != EChannelType::TCP) {
        qWarning() << "[CommManager] removeTcpChannel: not a TCP channel -" << channelId;
        return false;
    }

    it->second->stop();
    m_channels.erase(it);
    qInfo() << "[CommManager] TCP channel removed -" << channelId;
    return true;
}

bool CommManager::updateTcpChannel(const QString& channelId, const TcpConfig& newConfig)
{
    removeTcpChannel(channelId);
//    std::lock_guard<std::mutex> lock(m_channelsMtx);

//    auto it = m_channels.find(channelId);
//    if (it == m_channels.end()) {
//        qWarning() << "[CommManager] updateTcpChannel: channel not found -" << channelId;
//        return false;
//    }

//    // 校验通道类型
//    if (it->second->channelType() != EChannelType::TCP) {
//        qWarning() << "[CommManager] updateTcpChannel: not a TCP channel -" << channelId;
//        return false;
//    }
    // 2. 添加新配置的通道
    bool ret = addTcpChannel(channelId, newConfig);
//    // 构造新的通道配置
//    CommChannelConfig cfg{};
//    cfg.type = EChannelType::TCP;
//    cfg.channelId = channelId;
//    cfg.tcpCfg = newConfig;

//    // 调用TCP通道的配置更新
//    bool ret = it->second->updateConfig(cfg);
//    if (ret) {
//        qInfo() << "[CommManager] TCP channel updated -" << channelId;
//    } else {
//        qWarning() << "[CommManager] Failed to update TCP channel -" << channelId;
//    }
    return ret;
}

// ─────────────────────────────────────────────
//  UDP组播通道管理（新增）
// ─────────────────────────────────────────────
bool CommManager::removeUdpMulticastChannel(const QString& channelId)
{
    std::lock_guard<std::mutex> lock(m_channelsMtx);
    auto it = m_channels.find(channelId);
    if (it == m_channels.end()) {
        qWarning() << "[CommManager] removeUdpMulticastChannel: not found -" << channelId;
        return false;
    }
    if (it->second->channelType() != EChannelType::UDP_Multicast) {
        qWarning() << "[CommManager] removeUdpMulticastChannel: not a UDP channel -" << channelId;
        return false;
    }
    it->second->stop();
    m_channels.erase(it);
    qInfo() << "[CommManager] UDP channel removed -" << channelId;
    return true;
}

bool CommManager::updateUdpMulticastChannel(const QString& channelId, const UdpMulticastConfig& newConfig)
{
    removeUdpMulticastChannel(channelId);
    bool ret = addUdpMulticastChannel(channelId, newConfig);
    if (ret)
        qInfo() << "[CommManager] UDP channel updated -" << channelId;
    else
        qWarning() << "[CommManager] Failed to update UDP channel -" << channelId;
    return ret;
}

// ─────────────────────────────────────────────
//  串口通道管理
// ─────────────────────────────────────────────
bool CommManager::removeSerialChannel(const QString& channelId)
{
    std::lock_guard<std::mutex> lock(m_channelsMtx);
    auto it = m_channels.find(channelId);
    if (it == m_channels.end()) {
        qWarning() << "[CommManager] removeSerialChannel: channel not found -" << channelId;
        return false;
    }

    // 先停止通道，再删除
    it->second->stop();
    m_channels.erase(it);
    qInfo() << "[CommManager] Serial channel removed -" << channelId;
    return true;
}

bool CommManager::updateSerialChannel(const QString& channelId, const SerialConfig& newConfig)
{
    // 1. 先删除旧通道
    removeSerialChannel(channelId);

    // 2. 添加新配置的通道
    bool ret = addSerialChannel(channelId, newConfig);

    if (ret) {
        qInfo() << "[CommManager] Serial channel updated -" << channelId;
    } else {
        qWarning() << "[CommManager] Failed to update serial channel -" << channelId;
    }
    return ret;
}

// ─────────────────────────────────────────────
//  发送
// ─────────────────────────────────────────────

bool CommManager::sendTo(const QString& channelId, const STDataPrcSendMsg& data) {
    std::lock_guard<std::mutex> lock(m_channelsMtx);
    auto it = m_channels.find(channelId);
    if (it == m_channels.end()) {
        qWarning() << "[CommManager] sendTo: channelId not found:" << channelId;
        return false;
    }

    STPackage stSendPackage{};
    stSendPackage.baDataSend=data.btData;
    return it->second->send(stSendPackage);
}

// ─────────────────────────────────────────────
//  生命周期
// ─────────────────────────────────────────────

void CommManager::start() {
    if (m_running) return;
    m_running = true;

    m_processor.start();

    std::lock_guard<std::mutex> lock(m_channelsMtx);
    for (auto& [id, ch] : m_channels) {
        ch->start();
    }

    qInfo() << "[CommManager] started with"
            << m_channels.size() << "channels.";
}

void CommManager::stop() {
    if (!m_running) return;
    m_running = false;

    {
        std::lock_guard<std::mutex> lock(m_channelsMtx);
        for (auto& [id, ch] : m_channels) {
            ch->stop();
        }
    }

    m_processor.stop();
    qInfo() << "[CommManager] stopped.";
}

size_t CommManager::queueSize() const {
    return m_processor.queueSize();
}

size_t CommManager::processedCount() const {
    return m_processor.processedCount();
}

// ─────────────────────────────────────────────
//  内部回调
// ─────────────────────────────────────────────

void CommManager::onRawDataReceived(STPackage data) {
    // 在 IO 线程调用，无锁入队，极低延迟
    m_processor.enqueue(std::move(data));
}

void CommManager::onChannelState(const QString& id, EChannelState state) {
    // 跨线程 emit（Qt::QueuedConnection 自动处理）
    QMetaObject::invokeMethod(this, [this, id, state] {
        emit channelStateChanged(id, state);
    }, Qt::QueuedConnection);
}
