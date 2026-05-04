#include "serial422dialog.h"
#include "powersettingwidge.h"
#include "remotesettingwidget.h"
#include "collectioncoeffwidget.h"
#include "collectioncriteriawidge.h"
#include "gassupplywidget.h"
#include "src/CommManager.h"
#include "src/Common/ConfigHelper.h"
#include <QSerialPortInfo>
#include <QIntValidator>
#include <QTabWidget>
#include <QMessageBox>
#include <qcombobox.h>
#include <qboxlayout.h>
#include <qgroupbox.h>
#include <qlabel.h>
#include <qlineedit.h>
#include <qpushbutton.h>
#include <qtoolbutton.h>
#include <QIcon>
#include <QSettings>
#include <QDir>
#include <QCoreApplication>
#include <QFile>
Serial422Dialog::Serial422Dialog(QWidget *parent) : QDialog(parent)
{
    // 去除窗口左上角的问号按钮
    this->setWindowFlags(this->windowFlags() & ~Qt::WindowContextHelpButtonHint);

    setupUI();
    scanPorts(); // 初始化时扫描一次
    m_originalSerialConfigs = getAllControllerConfigs();
}

Serial422Dialog::~Serial422Dialog()
{

}

void Serial422Dialog::setupUI()
{
    this->setWindowTitle("422设置");
    this->resize(750, 550); // 稍微加宽一点以适应新增的刷新按钮

    // --- 1. 主垂直布局 ---
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(10, 10, 10, 10);

    // --- 2. 创建 TabWidget ---
    m_tabWidget = new QTabWidget(this);

    // --- 3. 构建 "串口设置" 页面 ---
    m_tabSerial = new QWidget();
    QGridLayout *gridLayout = new QGridLayout(m_tabSerial);
    gridLayout->setContentsMargins(15, 15, 15, 15);
    gridLayout->setHorizontalSpacing(20);
    gridLayout->setVerticalSpacing(15);

    // 辅助 Lambda：创建分组
    auto createGroup = [&](const QString &title, SerialGroup &group) -> QGroupBox* {
        QGroupBox *box = new QGroupBox(title);
        QGridLayout *boxLayout = new QGridLayout(box);
        boxLayout->setContentsMargins(10, 20, 10, 10);
        boxLayout->setHorizontalSpacing(10);
        boxLayout->setVerticalSpacing(8);

        // 标签
        QLabel *lblPort = new QLabel("串口号");
        QLabel *lblBaud = new QLabel("波特率");
        QLabel *lblStop = new QLabel("停止位");
        QLabel *lblParity = new QLabel("奇偶校验");

        // 控件
        group.cmbPort = new QComboBox();
        group.cmbPort->setMinimumWidth(100);

        group.editBaud = new QLineEdit();
        group.editBaud->setFixedWidth(80);
        group.editBaud->setValidator(new QIntValidator(1200, 921600, this));
        group.editBaud->setText("460800");

        group.cmbStop = new QComboBox();
        group.cmbStop->setFixedWidth(60);
        group.cmbStop->addItems({"1.0", "1.5", "2.0"});
        group.cmbStop->setCurrentText("1.0");

        group.cmbParity = new QComboBox();
        group.cmbParity->setFixedWidth(60);
        group.cmbParity->addItems({"None", "Odd", "Even", "Mark", "Space"});
        group.cmbParity->setCurrentText("Odd");

        // --- 新增：刷新按钮 ---
        QToolButton *btnRefresh = new QToolButton();
        btnRefresh->setText("刷新");
        btnRefresh->setIcon(QIcon::fromTheme("view-refresh")); // 尝试使用系统图标
        btnRefresh->setToolTip("重新扫描串口");
        // 连接信号槽：点击刷新按钮 -> 执行 scanPorts
        connect(btnRefresh, &QToolButton::clicked, this, &Serial422Dialog::scanPorts);

        // 布局 (注意列索引的变化)
        // 第0列: 标签, 第1列: 控件, 第2列: 标签, 第3列: 控件, 第4列: 刷新按钮
        boxLayout->addWidget(lblPort, 0, 0, Qt::AlignLeft);
        boxLayout->addWidget(group.cmbPort, 0, 1, Qt::AlignLeft);
        boxLayout->addWidget(lblBaud, 0, 2, Qt::AlignLeft);
        boxLayout->addWidget(group.editBaud, 0, 3, Qt::AlignLeft);
        boxLayout->addWidget(btnRefresh, 0, 4, Qt::AlignLeft); // 添加刷新按钮

        boxLayout->addWidget(lblStop, 1, 0, Qt::AlignLeft);
        boxLayout->addWidget(group.cmbStop, 1, 1, Qt::AlignLeft);
        boxLayout->addWidget(lblParity, 1, 2, Qt::AlignLeft);
        boxLayout->addWidget(group.cmbParity, 1, 3, Qt::AlignLeft);
        // 第二行第4列留空或放置提示文字

        return box;
    };

    // 创建三个分组
    QGroupBox *grp1 = createGroup("串口E (控制器1)", m_ctrl1);
    QGroupBox *grp2 = createGroup("串口F (控制器2)", m_ctrl2);
    QGroupBox *grp3 = createGroup("串口G (控制器3)", m_ctrl3);

    gridLayout->addWidget(grp1, 0, 0);
    gridLayout->addWidget(grp2, 1, 0);
    gridLayout->addWidget(grp3, 2, 0);

    m_tabWidget->addTab(m_tabSerial, "串口设置");

    // --- 4. 构建 "电源设置" 页面 (使用新组件) ---
    m_powerWidget = new PowerSettingWidget(this);
    m_tabWidget->addTab(m_powerWidget, "电源设置");

    // --- 5. 构建其他空的 Tab 页 ---
    RemoteSettingWidget *remoteWidget = new RemoteSettingWidget(this);
    m_tabWidget->addTab(remoteWidget, "远控设置");
    m_coeffWidget = new CollectionCoeffWidget(this);
    m_tabWidget->addTab(m_coeffWidget, "采集系数");
    // 新增：保存采集判据控件指针
    CollectionCriteriaWidget *criteriaWidget = new CollectionCriteriaWidget(this);
    m_criteriaWidget = criteriaWidget;  // 赋值给成员变量
    m_tabWidget->addTab(criteriaWidget, "采集判据");
    GasSupplyWidget *gasWidget = new GasSupplyWidget(this);
    m_tabWidget->addTab(gasWidget, "供气参数");
    // --- 6. 底部按钮 ---
    QHBoxLayout *btnLayout = new QHBoxLayout();
    btnLayout->addStretch();

    this->m_btnOk = new QPushButton("确认");
    this->m_btnCancel = new QPushButton("取消");

    this->m_btnOk->setMinimumWidth(80);
    this->m_btnCancel->setMinimumWidth(80);

    // 连接信号槽（仅保留确认/取消）
    connect(this->m_btnOk, &QPushButton::clicked, this, &QDialog::accept);
    connect(this->m_btnCancel, &QPushButton::clicked, this, &QDialog::reject);

    // 添加按钮到布局（仅保留确认、取消）
    btnLayout->addWidget(this->m_btnOk);
    btnLayout->addWidget(this->m_btnCancel);

    // --- 7. 组装主布局 ---
    mainLayout->addWidget(m_tabWidget);
    mainLayout->addLayout(btnLayout);
}


double Serial422Dialog::calculateCollectionCoeffSingleFormula(int row, int col, double x) const
{
    // 1. 校验采集系数控件是否初始化
    if (!m_coeffWidget) {
        qWarning() << "[Serial422Dialog] CollectionCoeffWidget is not initialized!";
        return 0.0;
    }

    // 2. 参数越界前置校验（可选，CollectionCoeffWidget内部也会校验）
    if (row < 0 || row >= COEFF_ROWS || col < 0 || col >= COEFF_COLS) {
        qWarning() << "[Serial422Dialog] Invalid row/col: row=" << row << ", col=" << col;
        return 0.0;
    }

    // 3. 调用CollectionCoeffWidget的核心计算接口
    return m_coeffWidget->calculateSingleFormula(row, col, x);
}

void Serial422Dialog::scanPorts()
{
    const auto ports = QSerialPortInfo::availablePorts();
    QString placeholder = "无可用串口";

    // 辅助 Lambda：填充单个下拉框
    auto fillCombo = [placeholder](QComboBox *cmb, const QList<QSerialPortInfo> &list) {
        cmb->clear(); // 关键：先清空现有内容，防止重复添加
        if (list.isEmpty()) {
            cmb->addItem(placeholder);
            cmb->setEnabled(false);
        } else {
            cmb->setEnabled(true);
            for (const QSerialPortInfo &info : list) {
                QString text = info.portName() + " (" + info.description() + ")";
                cmb->addItem(text, info.portName());
            }
        }
    };

    // 分别填充三个控制器的串口下拉框
    fillCombo(m_ctrl1.cmbPort, ports);
    fillCombo(m_ctrl2.cmbPort, ports);
    fillCombo(m_ctrl3.cmbPort, ports);
}

SerialConfig Serial422Dialog::convertToSerialConfig(const SerialGroup& group) const
{
    SerialConfig cfg;

    // 1. 串口号（提取真实端口名，如从 "/dev/ttyS0 (USB Serial)" 提取 "/dev/ttyS0"）
    QString portText = group.cmbPort->currentData().toString();
    if (portText.isEmpty()) portText = group.cmbPort->currentText().split(" ").first();
    cfg.portName = portText;

    // 2. 波特率
    cfg.baudRate = group.editBaud->text().toInt();

    // 3. 停止位（映射QSerialPort::StopBits到整数）
    QString stopBitText = group.cmbStop->currentText();
    if (stopBitText == "1.0") cfg.stopBits = 1;    // QSerialPort::OneStop
    else if (stopBitText == "1.5") cfg.stopBits = 2;// QSerialPort::OneAndHalfStop
    else if (stopBitText == "2.0") cfg.stopBits = 3;// QSerialPort::TwoStop

    // 4. 奇偶校验（映射QSerialPort::Parity到整数）
    QString parityText = group.cmbParity->currentText();
    if (parityText == "None") cfg.parity = 0;     // QSerialPort::NoParity
    else if (parityText == "Odd") cfg.parity = 1; // QSerialPort::OddParity
    else if (parityText == "Even") cfg.parity = 2;// QSerialPort::EvenParity
    else if (parityText == "Mark") cfg.parity = 3;// QSerialPort::MarkParity
    else if (parityText == "Space") cfg.parity = 4;// QSerialPort::SpaceParity

    // 5. 固定默认值（可根据需求扩展为界面配置）
    cfg.dataBits = 8;                // 数据位默认8
    cfg.flowControl = 0;             // 无流控
    cfg.readTimeoutMs = 50;          // 读取超时50ms

    return cfg;
}

// 获取单个控制器配置
SerialControllerConfig Serial422Dialog::getControllerConfig(const QString& ctrlName) const
{
    SerialControllerConfig ctrlCfg;
    if (ctrlName == "ctrl1") {
        ctrlCfg.channelId = "serial_E";  // 对应界面"串口E（控制器1）"
        ctrlCfg.serialCfg = convertToSerialConfig(m_ctrl1);
    } else if (ctrlName == "ctrl2") {
        ctrlCfg.channelId = "serial_F";  // 对应界面"串口F（控制器2）"
        ctrlCfg.serialCfg = convertToSerialConfig(m_ctrl2);
    } else if (ctrlName == "ctrl3") {
        ctrlCfg.channelId = "serial_G";  // 对应界面"串口G（控制器3）"
        ctrlCfg.serialCfg = convertToSerialConfig(m_ctrl3);
    }
    return ctrlCfg;
}

// 获取所有控制器配置
QList<SerialControllerConfig> Serial422Dialog::getAllControllerConfigs() const
{
    QList<SerialControllerConfig> cfgs;
    cfgs << getControllerConfig("ctrl1")
         << getControllerConfig("ctrl2")
         << getControllerConfig("ctrl3");
    return cfgs;
}

// ── 检测串口配置是否发生变化 ──
bool Serial422Dialog::serialConfigChanged() const
{
    QList<SerialControllerConfig> current = getAllControllerConfigs();
    if (current.size() != m_originalSerialConfigs.size()) return true;
    for (int i = 0; i < current.size(); i++) {
        const auto &a = current[i];
        const auto &b = m_originalSerialConfigs[i];
        if (a.channelId != b.channelId) return true;
        if (a.serialCfg.portName    != b.serialCfg.portName)    return true;
        if (a.serialCfg.baudRate    != b.serialCfg.baudRate)    return true;
        if (a.serialCfg.dataBits    != b.serialCfg.dataBits)    return true;
        if (a.serialCfg.stopBits    != b.serialCfg.stopBits)    return true;
        if (a.serialCfg.parity      != b.serialCfg.parity)      return true;
        if (a.serialCfg.flowControl != b.serialCfg.flowControl) return true;
    }
    return false;
}

// 重写accept函数，整合「确认配置」的逻辑
void Serial422Dialog::accept()
{
    bool allSuccess = true;
    QString failChannels;
    auto& commMgr = CommManager::instance();

    // ========== 1. 串口配置（仅在变化时更新）==========
    if (serialConfigChanged()) {
        QList<SerialControllerConfig> ctrlCfgs = getAllControllerConfigs();
        for (const auto& cfg : ctrlCfgs) {
            if (cfg.serialCfg.portName.isEmpty() || cfg.serialCfg.portName == "无可用串口") {
                failChannels += cfg.channelId + "（无有效串口）、";
                allSuccess = false;
                continue;
            }
            bool ret = commMgr.updateSerialChannel(cfg.channelId, cfg.serialCfg);
            if (!ret) {
                allSuccess = false;
                failChannels += cfg.channelId + "、";
            }
        }
        m_originalSerialConfigs = getAllControllerConfigs();
    }

    // ========== 2. 新增：电源TCP配置更新逻辑 ==========
    if (m_powerWidget && m_powerWidget->hasConfigChanged()) {
        ConfigHelper& config = ConfigHelper::getInstance();
        auto currentPowerConfigs = m_powerWidget->getCurrentConfigs();

        // 遍历每个电源配置，更新TCP通道
        for (int i = 0; i < m_powerPrefixes.size() && i < currentPowerConfigs.size(); ++i) {
            const QString& prefix = m_powerPrefixes[i];
            const PowerConfig& cfg = currentPowerConfigs[i];

            // 跳过非法端口
            if (cfg.tcpPort <= 0 || cfg.tcpPort > 65535) {
                failChannels += QString("电源%1(%2)（TCP端口非法）、").arg(i+1).arg(prefix);
                allSuccess = false;
                continue;
            }

            // 1. 构建通道ID和TCP配置
            QString channelName = config.getValue(QString("Communication/TCP_Name_ServerRemote_%1").arg(prefix),
                                                  QString("tcp_power_%1").arg(prefix)).toString();
            TcpConfig tcpConfig{};
            tcpConfig.host = cfg.ip;
            tcpConfig.port = cfg.tcpPort;
            tcpConfig.autoReconnect = true;
            tcpConfig.reconnectIntervalMs = 3000;
            tcpConfig.connectTimeoutMs = 5000;

            // 2. 更新TCP通道（CommManager需实现updateTcpChannel方法）
            bool ret = commMgr.updateTcpChannel(channelName, tcpConfig);
            if (!ret) {
                failChannels += QString("电源%1(%2)、").arg(i+1).arg(prefix);
                allSuccess = false;
            }

            // 3. 同步更新Info.ini配置（持久化）
            config.setValue(QString("Communication/TCP_IP_ServerRemote_%1").arg(prefix), cfg.ip);
            config.setValue(QString("Communication/TCP_Port_ServerRemote_%1").arg(prefix), cfg.tcpPort);
        }
    }

    // ========== 新增：保存采集系数公式到配置文件 ==========
    if (m_coeffWidget) {
        // 1. 构建config文件夹路径（运行路径下的config）
        QString appPath = QCoreApplication::applicationDirPath();
        QDir configDir(appPath + QDir::separator() + "config");

        // 2. 确保config文件夹存在（Linux下自动创建）
        if (!configDir.exists()) {
            if (!configDir.mkpath(".")) { // mkpath支持多级目录创建
                qWarning() << "[Serial422Dialog] 创建config文件夹失败：" << configDir.absolutePath();
            } else {
                qInfo() << "[Serial422Dialog] 创建config文件夹成功：" << configDir.absolutePath();
            }
        }

        // 3. 构建配置文件绝对路径
        QString coeffConfigPath = configDir.absoluteFilePath("coeff_config.ini");

        // 4. 初始化QSettings（INI格式，支持Linux）
        QSettings coeffSettings(coeffConfigPath, QSettings::IniFormat);

        // 5. 清空旧配置（避免残留无效数据）
        coeffSettings.remove("CollectionCoeff");

        // 6. 遍历7行4列，保存所有公式
        for (int row = 0; row < COEFF_ROWS; ++row) {
            for (int col = 0; col < COEFF_COLS; ++col) {
                QString formula = m_coeffWidget->getFormula(row, col);
                QString key = QString("CollectionCoeff/Row%1_Col%2").arg(row).arg(col);
                coeffSettings.setValue(key, formula);
            }
        }

        // 7. 强制同步到文件（Linux下确保立即写入）
        coeffSettings.sync();
        qInfo() << "[Serial422Dialog] 采集系数公式已保存到：" << coeffConfigPath;
    }

    // ========== 3. 提示框逻辑 ==========
    if (failChannels.endsWith("、")) {
        failChannels.chop(1); // 移除最后一个顿号
    }

    if (allSuccess) {
//        QMessageBox::information(this, "配置成功", "所有串口/电源TCP通道配置已更新！");
        QDialog::accept();
    } else {
//        QMessageBox::warning(this, "配置失败",
//            QString("以下通道配置更新失败：%1\n请检查配置是否合法或通道是否存在").arg(failChannels));
        // 配置失败不关闭对话框，让用户修改
         QDialog::accept();
    }
    // ========== 新增：保存采集判据（拉力/角度/压力）到配置文件 ==========
    if (m_criteriaWidget) {
        // 1. 构建config文件夹路径（运行路径下的config）
        QString appPath = QCoreApplication::applicationDirPath();
        QDir configDir(appPath + QDir::separator() + "config");

        // 2. 确保config文件夹存在
        if (!configDir.exists()) {
            if (!configDir.mkpath(".")) {
                qWarning() << "[Serial422Dialog] 创建config文件夹失败：" << configDir.absolutePath();
            } else {
                qInfo() << "[Serial422Dialog] 创建config文件夹成功：" << configDir.absolutePath();
            }
        }

        // 3. 构建采集判据配置文件路径
        QString criteriaConfigPath = configDir.absoluteFilePath("criteria_config.ini");

        // 4. 初始化QSettings（INI格式，跨平台兼容）
        QSettings criteriaSettings(criteriaConfigPath, QSettings::IniFormat);

        // 5. 清空旧配置（避免残留）
        criteriaSettings.remove("CollectionCriteria");

        // 6. 声明临时指针，读取界面值（需友元或新增接口，这里先通过反射/直接访问，推荐新增接口）
        // ---- 方案1：若允许直接访问成员变量（需修改CollectionCriteriaWidget的访问权限） ----
        // 先修改collectioncriteriawidge.h，将成员变量改为protected/public，或新增getter接口
        // 推荐方案2：给CollectionCriteriaWidget新增getter接口（更优雅），以下是完整方案：

        // 临时读取值（先通过成员变量读取，后续可优化为接口）
        QString tensionUpper = m_criteriaWidget->getTensionUpper();
        QString tensionLower = m_criteriaWidget->getTensionLower();
        QString angleUpper = m_criteriaWidget->getAngleUpper();
        QString angleLower = m_criteriaWidget->getAngleLower();
        QString pressureUpper = m_criteriaWidget->getPressureUpper();
        QString pressureLower = m_criteriaWidget->getPressureLower();

        // 7. 写入配置文件
        criteriaSettings.beginGroup("CollectionCriteria");
        criteriaSettings.setValue("Tension_Upper", tensionUpper);    // 拉力上限
        criteriaSettings.setValue("Tension_Lower", tensionLower);    // 拉力下限
        criteriaSettings.setValue("Angle_Upper", angleUpper);        // 角度上限
        criteriaSettings.setValue("Angle_Lower", angleLower);        // 角度下限
        criteriaSettings.setValue("Pressure_Upper", pressureUpper);  // 压力上限
        criteriaSettings.setValue("Pressure_Lower", pressureLower);  // 压力下限
        criteriaSettings.endGroup();

        // 8. 强制同步到文件（确保立即写入）
        criteriaSettings.sync();

        qInfo() << "[Serial422Dialog] 采集判据配置已保存到：" << criteriaConfigPath;
    } else {
        qWarning() << "[Serial422Dialog] CollectionCriteriaWidget 未初始化，跳过配置保存";
    }
}
