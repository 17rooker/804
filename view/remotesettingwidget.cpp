#include "remotesettingwidget.h"

#include <QLabel>
#include <QPushButton>
#include <QLineEdit>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QGroupBox>
#include <QFileDialog>
#include <QDir>
#include <QStyle>
#include <QIntValidator>
#include <QMessageBox>
#include <QDateTime>

#include "src/Common/ConfigHelper.h"
#include "src/Common/CommTypes.h"
#include "src/CommManager.h"
#include "src/CustomMessage/DataInteractionManager.h"
#include "InitiativeMsgEvent.h"

RemoteSettingWidget::RemoteSettingWidget(QWidget *parent) : QWidget(parent)
{
    setupUI();
    loadConfig();

    // 订阅实时数据
    DataInteractionManager::getInstance()
        .getMsgHandle()->subMessage(this, ESubDataType::E_RealTimeData);
}

RemoteSettingWidget::~RemoteSettingWidget()
{
    DataInteractionManager::getInstance()
        .getMsgHandle()->unSubMessageAll(this);
}

void RemoteSettingWidget::onMessage(IEvent *pEvent)
{
    if (!pEvent) return;
    if (pEvent->getType() != EventType::E_InitiativeMsg) return;

    auto *pInit = static_cast<InitiativeMsgEvent*>(pEvent);
    const STParamInfo &param = pInit->getParamData();

    if (param.channelId != "tcp_device_serverRemote") return;

    // 更新TCP运控数据显示
    auto setVal = [&](QLabel *lbl, const QString &key) {
        if (!lbl) return;
        auto it = param.mapParams.find(key);
        if (it != param.mapParams.end())
            lbl->setText(it.value().varParaValue.toString());
    };

    setVal(m_lblTcpFrameType,  "TcpFrameType");
    setVal(m_lblTcpFrameCount, "TcpFrameCount");
    setVal(m_lblTcpWordCount,  "TcpWordCount");
    setVal(m_lblTcpWordType,   "TcpWordType");

    if (m_lblTcpSrc) {
        auto si = param.mapParams.find("TcpSrcSys");
        auto ni = param.mapParams.find("TcpSrcNode");
        if (si != param.mapParams.end() && ni != param.mapParams.end())
            m_lblTcpSrc->setText(QString("%1.%2").arg(si.value().varParaValue.toUInt()).arg(ni.value().varParaValue.toUInt()));
    }
    if (m_lblTcpDst) {
        auto si = param.mapParams.find("TcpDstSys");
        auto ni = param.mapParams.find("TcpDstNode");
        if (si != param.mapParams.end() && ni != param.mapParams.end())
            m_lblTcpDst->setText(QString("%1.%2").arg(si.value().varParaValue.toUInt()).arg(ni.value().varParaValue.toUInt()));
    }

    // 日期时间
    if (m_lblTcpDate) {
        auto yr = param.mapParams.find("TcpDateYear");
        auto mo = param.mapParams.find("TcpDateMonth");
        auto dy = param.mapParams.find("TcpDateDay");
        if (yr != param.mapParams.end() && mo != param.mapParams.end() && dy != param.mapParams.end())
            m_lblTcpDate->setText(QString("%1-%2-%3").arg(yr.value().varParaValue.toUInt()).arg(mo.value().varParaValue.toUInt(),2,10,QChar('0')).arg(dy.value().varParaValue.toUInt(),2,10,QChar('0')));
    }
    if (m_lblTcpTime) {
        auto ti = param.mapParams.find("TcpTime");
        if (ti != param.mapParams.end()) {
            quint32 t = ti.value().varParaValue.toUInt();
            m_lblTcpTime->setText(QString("%1:%2:%3").arg(t/3600,2,10,QChar('0')).arg((t%3600)/60,2,10,QChar('0')).arg(t%60,2,10,QChar('0')));
        }
    }
}

void RemoteSettingWidget::loadConfig()
{
    ConfigHelper& cfg = ConfigHelper::getInstance();

    m_editServerIp->setText(
        cfg.getValue("Communication/TCP_IP_ServerRemote", "127.0.0.1").toString());
    m_editServerPort->setText(
        cfg.getValue("Communication/TCP_Port_ServerRemote", "7008").toString());
    m_editMcastIp->setText(
        cfg.getValue("Communication/UDPGroup_IP_ServerRemote", "224.168.0.40").toString());
    m_editMcastPort->setText(
        cfg.getValue("Communication/UDPGroup_Port_ServerRemote", "16040").toString());
    m_editDataPath->setText(
        cfg.getValue("Storage/DataPath", "D:/数据").toString());
}

void RemoteSettingWidget::setupUI()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(15, 15, 15, 15);
    mainLayout->setSpacing(15);

    // ── TCP参数设置 ──
    QGroupBox *groupTcp = new QGroupBox("TCP 远程服务器");
    QGridLayout *tcpGrid = new QGridLayout(groupTcp);
    tcpGrid->setHorizontalSpacing(10);
    tcpGrid->setVerticalSpacing(8);
    tcpGrid->setContentsMargins(10, 20, 10, 10);

    QLabel *lblTcpIp = new QLabel("服务器IP地址");
    QLabel *lblTcpPort = new QLabel("服务器端口");
    m_editServerIp = new QLineEdit;
    m_editServerPort = new QLineEdit;
    m_editServerPort->setValidator(new QIntValidator(1, 65535, this));
    m_editServerIp->setFixedWidth(130);
    m_editServerPort->setFixedWidth(100);
    m_btnApplyTcp = new QPushButton("应用");
    m_btnApplyTcp->setFixedWidth(60);

    tcpGrid->addWidget(lblTcpIp, 0, 0);
    tcpGrid->addWidget(m_editServerIp, 0, 1);
    tcpGrid->addWidget(lblTcpPort, 0, 2);
    tcpGrid->addWidget(m_editServerPort, 0, 3);
    tcpGrid->addWidget(m_btnApplyTcp, 0, 4);

    // ── UDP组播参数设置 ──
    QGroupBox *groupUdp = new QGroupBox("UDP 组播");
    QGridLayout *udpGrid = new QGridLayout(groupUdp);
    udpGrid->setHorizontalSpacing(10);
    udpGrid->setVerticalSpacing(8);
    udpGrid->setContentsMargins(10, 20, 10, 10);

    QLabel *lblMcastIp = new QLabel("组播地址");
    QLabel *lblMcastPort = new QLabel("组播端口");
    m_editMcastIp = new QLineEdit;
    m_editMcastPort = new QLineEdit;
    m_editMcastPort->setValidator(new QIntValidator(1, 65535, this));
    m_editMcastIp->setFixedWidth(130);
    m_editMcastPort->setFixedWidth(100);
    m_btnApplyUdp = new QPushButton("应用");
    m_btnApplyUdp->setFixedWidth(60);

    udpGrid->addWidget(lblMcastIp, 0, 0);
    udpGrid->addWidget(m_editMcastIp, 0, 1);
    udpGrid->addWidget(lblMcastPort, 0, 2);
    udpGrid->addWidget(m_editMcastPort, 0, 3);
    udpGrid->addWidget(m_btnApplyUdp, 0, 4);

    // ── 数据存储路径 ──
    QGroupBox *groupPath = new QGroupBox("数据存储路径");
    QHBoxLayout *pathLayout = new QHBoxLayout(groupPath);
    pathLayout->setContentsMargins(10, 20, 10, 10);

    m_editDataPath = new QLineEdit;
    m_btnBrowse = new QPushButton;
    m_btnBrowse->setIcon(style()->standardIcon(QStyle::SP_DirIcon));
    m_btnBrowse->setToolTip("选择文件夹");
    m_btnBrowse->setFixedSize(30, 25);

    pathLayout->addWidget(m_editDataPath);
    pathLayout->addWidget(m_btnBrowse);

    // ── TCP运控数据显示 ──
    QGroupBox *groupDisplay = new QGroupBox("TCP 运控数据");
    setupDisplayUI(groupDisplay);

    // ── 加入主布局 ──
    mainLayout->addWidget(groupTcp);
    mainLayout->addWidget(groupUdp);
    mainLayout->addWidget(groupDisplay);
    mainLayout->addWidget(groupPath);
    mainLayout->addStretch();

    // ── 信号连接 ──
    connect(m_btnBrowse, &QPushButton::clicked, this, &RemoteSettingWidget::onBrowseFolder);
    connect(m_btnApplyTcp, &QPushButton::clicked, this, &RemoteSettingWidget::onApplyTcp);
    connect(m_btnApplyUdp, &QPushButton::clicked, this, &RemoteSettingWidget::onApplyUdp);
}

void RemoteSettingWidget::setupDisplayUI(QWidget *parent)
{
    auto *grid = new QGridLayout(parent);
    grid->setSpacing(4);
    grid->setContentsMargins(10, 20, 10, 10);

    auto addRow = [&](int row, const QString &label, QLabel *&value) {
        auto *lbl = new QLabel(label);
        lbl->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        value = new QLabel("--");
        value->setStyleSheet("background: black; color: #00FF00; padding: 2px 6px; font-family: Consolas;");
        value->setFixedHeight(24);
        grid->addWidget(lbl,  row, 0);
        grid->addWidget(value, row, 1);
    };

    addRow(0, "帧类型",     m_lblTcpFrameType);
    addRow(1, "帧计数",     m_lblTcpFrameCount);
    addRow(2, "信息字数",   m_lblTcpWordCount);
    addRow(3, "信息字类别", m_lblTcpWordType);
    addRow(4, "信源",       m_lblTcpSrc);
    addRow(5, "信宿",       m_lblTcpDst);
    addRow(6, "日期",       m_lblTcpDate);
    addRow(7, "时间",       m_lblTcpTime);
}

void RemoteSettingWidget::onBrowseFolder()
{
    QString currentPath = m_editDataPath->text();
    if (currentPath.isEmpty() || !QDir(currentPath).exists())
        currentPath = QDir::homePath();

    QString dir = QFileDialog::getExistingDirectory(this, tr("选择数据存储文件夹"),
                                                    currentPath,
                                                    QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
    if (!dir.isEmpty()) {
        m_editDataPath->setText(dir);
        ConfigHelper::getInstance().setValue("Storage/DataPath", dir);
    }
}

void RemoteSettingWidget::onApplyTcp()
{
    QString ip   = m_editServerIp->text().trimmed();
    QString port = m_editServerPort->text().trimmed();

    if (ip.isEmpty() || port.isEmpty()) {
        QMessageBox::warning(this, "参数错误", "请填写完整的TCP服务器IP和端口");
        return;
    }
    bool ok = false;
    quint16 portNum = port.toUShort(&ok);
    if (!ok || portNum == 0) {
        QMessageBox::warning(this, "参数错误", "端口号格式无效（1~65535）");
        return;
    }

    ConfigHelper& cfg = ConfigHelper::getInstance();
    cfg.setValue("Communication/TCP_IP_ServerRemote", ip);
    cfg.setValue("Communication/TCP_Port_ServerRemote", portNum);

    QString channelId = cfg.getValue("Communication/TCP_Name_ServerRemote", "tcp_device_serverRemote").toString();

    TcpConfig tcpCfg;
    tcpCfg.host = ip;
    tcpCfg.port = portNum;
    tcpCfg.autoReconnect = true;
    tcpCfg.reconnectIntervalMs = 3000;
    tcpCfg.connectTimeoutMs = 5000;

    if (CommManager::instance().updateTcpChannel(channelId, tcpCfg))
        QMessageBox::information(this, "提示", "TCP配置已应用");
    else
        QMessageBox::warning(this, "错误", "TCP配置更新失败，请检查通道名称");
}

void RemoteSettingWidget::onApplyUdp()
{
    QString ip   = m_editMcastIp->text().trimmed();
    QString port = m_editMcastPort->text().trimmed();

    if (ip.isEmpty() || port.isEmpty()) {
        QMessageBox::warning(this, "参数错误", "请填写完整的组播地址和端口");
        return;
    }
    bool ok = false;
    quint16 portNum = port.toUShort(&ok);
    if (!ok || portNum == 0) {
        QMessageBox::warning(this, "参数错误", "端口号格式无效（1~65535）");
        return;
    }

    ConfigHelper& cfg = ConfigHelper::getInstance();
    cfg.setValue("Communication/UDPGroup_IP_ServerRemote", ip);
    cfg.setValue("Communication/UDPGroup_Port_ServerRemote", portNum);

    QString channelId = cfg.getValue("Communication/UDPGroup_Name_ServerRemote", "udp_multicast_groupRemote").toString();

    UdpMulticastConfig udpCfg;
    udpCfg.multicastGroup = ip;
    udpCfg.port = portNum;
    udpCfg.listenAddr = "0.0.0.0";
    udpCfg.ttl = 1;

    if (CommManager::instance().updateUdpMulticastChannel(channelId, udpCfg))
        QMessageBox::information(this, "提示", "UDP组播配置已应用");
    else
        QMessageBox::warning(this, "错误", "UDP配置更新失败，请检查通道名称");
}
