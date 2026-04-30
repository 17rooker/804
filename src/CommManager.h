#ifndef COMMMANAGER_H
#define COMMMANAGER_H

#include "src/LogicCommunication/CommChannelFactory.h"
#include "src/DataProcess/CommDataProcessor.h"

#include <QObject>
#include <map>
#include <QString>
#include <memory>
#include <mutex>

/**
 * @brief CommManager  通信总管理门面（Facade）
 *
 * 整合工厂、通道、处理器，提供统一的通信入口：
 *
 *   CommManager::instance().addTcpChannel(...)
 *   CommManager::instance().addUdpMulticastChannel(...)
 *   CommManager::instance().addSerialChannel(...)
 *   CommManager::instance().start()
 *
 * 数据流向：
 *   addXxxChannel() → Factory::create() → channel.start()
 *       ↓ onReceived callback（IO线程）
 *   CommDataProcessor::enqueue()
 *       ↓ dispatchLoop（单线程）→ BS::thread_pool
 *   IDataParser::parse()
 *       ↓
 *   DataInteractionManager::post*Event()
 *       ↓
 *   MessageHandle::event() → IMessage::onMessage()
 */
class CommManager : public QObject {
    Q_OBJECT

public:
    static CommManager& instance();

    // ── 解析器注册（在 start() 前调用）──────────
    void registerParser(CommDataProcessor::EAnalysisType type,IDataAnalysisPtr parser);

    void registerProcessor(CommDataProcessor::EProcessCategory type,IDataProcessPtr processor);

    // ── 通道添加（在 start() 前或运行时均可）────
    bool addTcpChannel(
        const QString&       channelId,
        TcpConfig           config
    );

    bool addUdpMulticastChannel(
        const QString&       channelId,
        UdpMulticastConfig   config
    );

    bool addSerialChannel(
        const QString&       channelId,
        SerialConfig        config
    );

    // ── 通道管理（新增TCP相关）───────────────────
    // 删除TCP通道（线程安全）
    bool removeTcpChannel(const QString& channelId);
    // 更新TCP通道配置（原子操作）
    bool updateTcpChannel(const QString& channelId, const TcpConfig& newConfig);

    // ── 串口通道管理 ────────────────────────────
    bool removeSerialChannel(const QString& channelId);
    bool updateSerialChannel(const QString& channelId, const SerialConfig& newConfig);

    // ── 发送接口 ────────────────────────────────
    bool sendTo(const QString& channelId, const  STDataPrcSendMsg& data);

    // ── 生命周期 ────────────────────────────────
    void start();
    void stop();

    // ── 统计 ────────────────────────────────────
    size_t queueSize()      const;
    size_t processedCount() const;

signals:
    // 通道状态变化（统一向外广播，供 UI 订阅）
    void channelStateChanged(const QString& channelId,
                             EChannelState  state);

private:
    explicit CommManager(QObject* parent = nullptr);
    ~CommManager() override;

    // 回调：由各通道IO线程触发，直接入队
    void onRawDataReceived(STPackage data);

    // 回调：状态变化，emit 到Qt主线程
    void onChannelState(const QString& id, EChannelState state);

private:
    CommDataProcessor                          m_processor;

    mutable std::mutex                         m_channelsMtx;
    std::map<QString, ICommChannelPtr>         m_channels;   // channelId → channel

    std::atomic<bool>                          m_running { false };
};

#endif // COMMMANAGER_H
