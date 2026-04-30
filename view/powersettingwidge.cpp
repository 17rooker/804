#include "powersettingwidge.h"

#include <QVBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QGroupBox>
#include <QIntValidator>
#include <QRegExpValidator> // Qt5 使用；Qt6 替换为 QRegularExpressionValidator
#include <QHBoxLayout>
#include <QFont>

PowerSettingWidget::PowerSettingWidget(QWidget *parent) : QWidget(parent)
{
    setupUI();
    // 加载初始配置（从Info.ini读取）
    loadOriginalConfigs();
}

PowerSettingWidget::~PowerSettingWidget()
{
}

void PowerSettingWidget::setupUI()
{
    // --- 主垂直布局 ---
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(15, 15, 15, 15);
    mainLayout->setSpacing(10);

    // --- 1. 顶部大标题 ---
    QLabel *titleLabel = new QLabel("电源设置");
    titleLabel->setAlignment(Qt::AlignCenter);
    titleLabel->setFont(QFont("Microsoft YaHei", 10, QFont::Bold));
    mainLayout->addWidget(titleLabel);

    // --- 2. 使用网格布局来排列“左侧标签”和“右侧分组框” ---
    QGridLayout *gridLayout = new QGridLayout();
    gridLayout->setHorizontalSpacing(15); // 标签和框之间的间距
    gridLayout->setVerticalSpacing(8);    // 行间距
    gridLayout->setColumnStretch(1, 1);   // 让右侧的框随窗口拉伸

    // 辅助函数：添加一行
    auto addRow = [&](const QString &name, const QString &ip, int tcp, const QString &mcIp, int mcPort, int rowIdx) {
        // A. 创建左侧标签 (电源1, 电源2...)
        QLabel *nameLabel = new QLabel(name);
        nameLabel->setFixedWidth(50); // 固定宽度以确保对齐
        nameLabel->setAlignment(Qt::AlignCenter);
        nameLabel->setFont(QFont("Microsoft YaHei", 9));

        // B. 创建右侧的自定义分组框
        QWidget *groupWidget = createStyledRow(ip, tcp, mcIp, mcPort);

        // C. 放入网格布局：第rowIdx行，第0列放标签，第1列放分组框
        gridLayout->addWidget(nameLabel, rowIdx, 0, Qt::AlignVCenter);
        gridLayout->addWidget(groupWidget, rowIdx, 1);
    };

    // --- 3. 添加数据行 (匹配Info.ini配置) ---
    addRow("电源1", "192.168.0.217", 101, "224.168.0.47", 16047, 0);
    addRow("电源2", "192.168.0.218", 201, "224.168.0.48", 16048, 1);
    addRow("电源3", "192.168.0.219", 301, "224.168.0.49", 16049, 2);
    addRow("电源4", "192.168.0.220", 401, "224.168.0.50", 16050, 3);
    addRow("电源5", "192.168.0.250", 501, "224.168.0.51", 16051, 4); // 修正原截图笔误（电源3→电源5）
    addRow("电源6", "192.168.0.251", 603, "224.168.0.52", 16052, 5);

    mainLayout->addLayout(gridLayout);
    mainLayout->addStretch(); // 底部留白
}

QWidget* PowerSettingWidget::createStyledRow(const QString &ip, int tcp, const QString &mcastIp, int mcastPort)
{
    // --- 1. 创建容器 Widget 和主布局 ---
    QWidget *container = new QWidget();
    // 使用 QGridLayout 方便控制对齐
    QGridLayout *mainLayout = new QGridLayout(container);
    mainLayout->setContentsMargins(10, 10, 10, 10); // 内部边距
    mainLayout->setHorizontalSpacing(15); // 列间距
    mainLayout->setVerticalSpacing(5);    // 行间距

    // --- 2. 定义通用样式 ---
    // 标题样式：加粗，居中
    QString labelStyle = "QLabel { font-weight: bold; color: #333; }";
    // 输入框样式：模拟截图中的内嵌效果
    QString editStyle = R"(
        QLineEdit {
            border: 1px solid #8f8f91;
            border-radius: 3px;
            padding: 1px 3px;
            background-color: #ffffff;
            min-width: 60px; /* 关键：设置最小宽度 */
        }
    )";

    // --- 3. 创建并配置第一行（标题行） ---
    auto createTitleLabel = [&](const QString &text) {
        QLabel *lbl = new QLabel(text);
        lbl->setAlignment(Qt::AlignCenter);
        lbl->setStyleSheet(labelStyle);
        return lbl;
    };

    mainLayout->addWidget(createTitleLabel("IP地址"), 0, 0);
    mainLayout->addWidget(createTitleLabel("TCP端口"), 0, 1);
    mainLayout->addWidget(createTitleLabel("组播地址"), 0, 2);
    mainLayout->addWidget(createTitleLabel("组播端口"), 0, 3);

    // --- 4. 创建并配置第二行（输入框行） ---
    auto createEdit = [&](const QString &text) {
        QLineEdit *edit = new QLineEdit(text);
        edit->setStyleSheet(editStyle);
        edit->setAlignment(Qt::AlignCenter);
        edit->setMaximumHeight(24);

        // 【核心修复】设置尺寸策略为 Expanding
        edit->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

        return edit;
    };

    QLineEdit *ipEdit = createEdit(ip);
    QLineEdit *tcpEdit = createEdit(QString::number(tcp));
    QLineEdit *mcastIpEdit = createEdit(mcastIp);
    QLineEdit *mcastPortEdit = createEdit(QString::number(mcastPort));

    // 端口输入框添加数字校验
    tcpEdit->setValidator(new QIntValidator(1, 65535, this));
    mcastPortEdit->setValidator(new QIntValidator(1, 65535, this));

    // IP地址正则校验（简单版）
    QRegExp ipRegExp("\\b(?:(?:25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\\.){3}(?:25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\\b");
    ipEdit->setValidator(new QRegExpValidator(ipRegExp, this));
    mcastIpEdit->setValidator(new QRegExpValidator(ipRegExp, this));

    // 保存到数据列表
    m_rows.push_back({ipEdit, tcpEdit, mcastIpEdit, mcastPortEdit});

    // 将输入框添加到布局
    mainLayout->addWidget(ipEdit, 1, 0);
    mainLayout->addWidget(tcpEdit, 1, 1);
    mainLayout->addWidget(mcastIpEdit, 1, 2);
    mainLayout->addWidget(mcastPortEdit, 1, 3);

    // 【可选】设置列伸缩因子 (Stretch Factor)
    // 这会让 IP 地址列比端口列更宽
    mainLayout->setColumnStretch(0, 3); // IP列权重 3
    mainLayout->setColumnStretch(1, 1); // 端口列权重 1
    mainLayout->setColumnStretch(2, 3); // 组播IP列权重 3
    mainLayout->setColumnStretch(3, 1); // 组播端口列权重 1

    return container;
}

// 加载初始配置（从Info.ini读取）
void PowerSettingWidget::loadOriginalConfigs()
{
    m_originalConfigs.clear();
    ConfigHelper& config = ConfigHelper::getInstance();

    for (const QString& prefix : m_powerPrefixes) {
        PowerConfig cfg;
        // 读取TCP IP和端口（从Info.ini）
        cfg.ip = config.getValue(QString("Communication/TCP_IP_ServerRemote_%1").arg(prefix), "").toString();
        cfg.tcpPort = config.getValue(QString("Communication/TCP_Port_ServerRemote_%1").arg(prefix), 0).toInt();

        // 组播地址/端口按界面初始值赋值（与setupUI中的默认值一致）
        if (prefix == "Power1") {
            cfg.mcastIp = "224.168.0.47";
            cfg.mcastPort = 16047;
        } else if (prefix == "Power2") {
            cfg.mcastIp = "224.168.0.48";
            cfg.mcastPort = 16048;
        } else if (prefix == "Power3_1") {
            cfg.mcastIp = "224.168.0.49";
            cfg.mcastPort = 16049;
        } else if (prefix == "Power4") {
            cfg.mcastIp = "224.168.0.50";
            cfg.mcastPort = 16050;
        } else if (prefix == "Power5") {
            cfg.mcastIp = "224.168.0.51";
            cfg.mcastPort = 16051;
        } else if (prefix == "Power6") {
            cfg.mcastIp = "224.168.0.52";
            cfg.mcastPort = 16052;
        }

        m_originalConfigs.push_back(cfg);
    }
}

// 获取当前界面配置
std::vector<PowerConfig> PowerSettingWidget::getCurrentConfigs() const
{
    std::vector<PowerConfig> currentConfigs;
    for (const auto& row : m_rows) {
        PowerConfig cfg;
        cfg.ip = row.ipEdit->text().trimmed();
        cfg.tcpPort = row.tcpEdit->text().toInt();
        cfg.mcastIp = row.mcastIpEdit->text().trimmed();
        cfg.mcastPort = row.mcastPortEdit->text().toInt();
        currentConfigs.push_back(cfg);
    }
    return currentConfigs;
}

// 判断配置是否有改动
bool PowerSettingWidget::hasConfigChanged() const
{
    auto current = getCurrentConfigs();
    // 行数不一致直接判定为改动
    if (current.size() != m_originalConfigs.size()) {
        return true;
    }
    // 逐行对比配置
    for (size_t i = 0; i < current.size(); ++i) {
        if (current[i] != m_originalConfigs[i]) {
            return true;
        }
    }
    return false;
}
