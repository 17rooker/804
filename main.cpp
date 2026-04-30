#include "mainwindow.h"

#include <QApplication>
#include "src/CustomMessage/DataInteractionManager.h"
#include "src/CommManager.h"
#include "src/DataProcess/DataAnalysis/FrameDataAnalysis.h"
#include "src/DataProcess/DataAnalysis/ExpandAnalysis.h"
#include "src/DataProcess/StaticDataProcess.h"
#include "src/DataProcess/ControlDataProcess.h"
#include "src/Common/LoggerManager.h"
#include "src/Common/ConfigHelper.h"
#include "src/DataProcess/ScheduledSendService.h"

// 定义6路电源的配置项前缀（匹配Info.ini中的命名规则）
const QList<QString> powerTcpPrefixes = {
    "Power1", "Power2", "Power3_1", "Power4", "Power5", "Power6"
};

int main(int argc, char *argv[])
{
    QApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
    QApplication a(argc, argv);

    // 加载配置文件
    ConfigHelper::getInstance().loadConfig(QCoreApplication::applicationDirPath()+"/config/Info.ini");
    auto& comm = CommManager::instance();
    auto& dim  = DataInteractionManager::getInstance();

    // ── Step 1: 注册解析器 ──────────────────────
    comm.registerParser(CommDataProcessor::EAnalysisType::E_DataAnalysis,std::make_shared<FrameDataAnalysis>());
    comm.registerParser(CommDataProcessor::EAnalysisType::E_ExpandAnalysis,std::make_shared<ExpandAnalysis>());

    // ── Step 2: 注册处理器 ──────────────────────
    comm.registerProcessor(CommDataProcessor::EProcessCategory::E_StaticData,std::make_shared<StaticDataProcess>());
    comm.registerProcessor(CommDataProcessor::EProcessCategory::E_ControlData,std::make_shared<ControlDataProcess>());

    // ── Step 3: 添加通信通道 ─────────────────────
    // 3.1 原有 TCP 远程通道（ServerRemote）- 保留兼容
    TcpConfig tcpConfigRemote{};
    tcpConfigRemote.host = ConfigHelper::getInstance().getValue("Communication/TCP_IP_ServerRemote", "127.0.0.1").toString();
    tcpConfigRemote.port = ConfigHelper::getInstance().getValue("Communication/TCP_Port_ServerRemote", 7008).toUInt();
    tcpConfigRemote.autoReconnect = true;
    tcpConfigRemote.reconnectIntervalMs = 3000;
    tcpConfigRemote.connectTimeoutMs = 5000;
    QString tcpChannelIdRemote = ConfigHelper::getInstance().getValue("Communication/TCP_Name_ServerRemote", "tcp_device_serverRemote").toString();
    comm.addTcpChannel(tcpChannelIdRemote, tcpConfigRemote);

    // 3.2 原有 TCP 本地通道（ServerLocal）- 保留
    TcpConfig tcpConfigLocal{};
    tcpConfigLocal.host = ConfigHelper::getInstance().getValue("Communication/TCP_IP_ServerLocal", "127.0.0.1").toString();
    tcpConfigLocal.port = ConfigHelper::getInstance().getValue("Communication/TCP_Port_ServerLocal", 16008).toUInt();
    tcpConfigLocal.autoReconnect = true;
    comm.addTcpChannel(ConfigHelper::getInstance().getValue("Communication/TCP_Name_ServerLocal", "tcp_device_serverLocal").toString(), tcpConfigLocal);

    // 3.3 新增：6路电源TCP通道（从配置文件读取）
    ConfigHelper& config = ConfigHelper::getInstance();
    for (const QString& prefix : powerTcpPrefixes) {
        // 构建配置项键名
        QString nameKey = QString("Communication/TCP_Name_ServerRemote_%1").arg(prefix);
        QString ipKey = QString("Communication/TCP_IP_ServerRemote_%1").arg(prefix);
        QString portKey = QString("Communication/TCP_Port_ServerRemote_%1").arg(prefix);

        // 读取配置（带默认值兜底）
        QString channelName = config.getValue(nameKey, QString("tcp_power_%1").arg(prefix)).toString();
        QString host = config.getValue(ipKey, "127.0.0.1").toString();
        quint16 port = config.getValue(portKey, 0).toUInt();

        // 跳过端口为0的无效配置
        if (port == 0) {
            // 可添加日志提示无效配置
            continue;
        }

        // 构建TCP配置（复用原有重连策略）
        TcpConfig powerTcpConfig{};
        powerTcpConfig.host = host;
        powerTcpConfig.port = port;
        powerTcpConfig.autoReconnect = true;    // 自动重连
        powerTcpConfig.reconnectIntervalMs = 3000; // 重连间隔3秒
        powerTcpConfig.connectTimeoutMs = 5000;    // 连接超时5秒

        // 添加电源TCP通道
        comm.addTcpChannel(channelName, powerTcpConfig);
    }

    // 3.4 UDP 组播通道 - 保留
    UdpMulticastConfig udpConfig{};
    udpConfig.multicastGroup = ConfigHelper::getInstance().getValue("Communication/UDPGroup_IP_ServerRemote", "224.168.0.40").toString();
    udpConfig.port = ConfigHelper::getInstance().getValue("Communication/UDPGroup_Port_ServerRemote", 16040).toUInt();
    udpConfig.listenAddr = "0.0.0.0";
    udpConfig.ttl = 1;
    QString udpChannelId = ConfigHelper::getInstance().getValue("Communication/UDPGroup_Name_ServerRemote", "udp_multicast_groupRemote").toString();
    comm.addUdpMulticastChannel(udpChannelId, udpConfig);

    // 3.5 串口通道（E/F/G）- 保留
    // 串口E（控制器1）
    SerialConfig serialConfigE{};
    serialConfigE.portName = ConfigHelper::getInstance().getValue("Communication/Serial_Port_E", "/dev/ttyS0").toString();
    serialConfigE.baudRate = ConfigHelper::getInstance().getValue("Communication/Serial_Baud_E", 460800).toInt();
    comm.addSerialChannel(ConfigHelper::getInstance().getValue("Communication/Serial_Name_E", "serial_E").toString(), serialConfigE);

    // 串口F（控制器2）
    SerialConfig serialConfigF{};
    serialConfigF.portName = ConfigHelper::getInstance().getValue("Communication/Serial_Port_F", "/dev/ttyS1").toString();
    serialConfigF.baudRate = ConfigHelper::getInstance().getValue("Communication/Serial_Baud_F", 460800).toInt();
    comm.addSerialChannel(ConfigHelper::getInstance().getValue("Communication/Serial_Name_F", "serial_F").toString(), serialConfigF);

    // 串口G（控制器3）
    SerialConfig serialConfigG{};
    serialConfigG.portName = ConfigHelper::getInstance().getValue("Communication/Serial_Port_G", "/dev/ttyS2").toString();
    serialConfigG.baudRate = ConfigHelper::getInstance().getValue("Communication/Serial_Baud_G", 460800).toInt();
    comm.addSerialChannel(ConfigHelper::getInstance().getValue("Communication/Serial_Name_G", "serial_G").toString(), serialConfigG);

    // ── Step 4: 注册下行发送回调 ─────────────────
    dim.setSendMsgFunc([&comm](const STDataPrcSendMsg& msg) {
        comm.sendTo(msg.channelId, msg);
    });

    MainWindow w;
    w.show();
    ScheduledSendService sss;
    sss.start();

    // 启动所有通道和处理线程
    comm.start();

    sss.stop();
    return a.exec();
}
