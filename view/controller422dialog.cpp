#include "controller422dialog.h"
#include <QComboBox>
#include <QLabel>
#include <QPushButton>
#include <QTextEdit>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QMessageBox>
#include <QFrame>
#include <QStandardItemModel>
#include <QDebug>
#include <QTimer>
#include "src/CustomMessage/DataInteractionManager.h"

// ================= CRC16 校验表定义 =================
const quint16 Controller422Dialog::crc16_table[256] = {
    0x0000,0x1021,0x2042,0x3063,0x4084,0x50a5,0x60c6,0x70e7,
    0x8108,0x9129,0xa14a,0xb16b,0xc18c,0xd1ad,0xe1ce,0xf1ef,
    0x1231,0x0210,0x3273,0x2252,0x52b5,0x4294,0x72f7,0x62d6,
    0x9339,0x8318,0xb37b,0xa35a,0xd3bd,0xc39c,0xf3ff,0xe3de,
    0x2462,0x3443,0x0420,0x1401,0x64e6,0x74c7,0x44a4,0x5485,
    0xa56a,0xb54b,0x8528,0x9509,0xe5ee,0xf5cf,0xc5ac,0xd58d,
    0x3653,0x2672,0x1611,0x0630,0x76d7,0x66f6,0x5695,0x46b4,
    0xb75b,0xa77a,0x9719,0x8738,0xf7df,0xe7fe,0xd79d,0xc7bc,
    0x48c4,0x58e5,0x6886,0x78a7,0x0840,0x1861,0x2802,0x3823,
    0xc9cc,0xd9ed,0xe98e,0xf9af,0x8948,0x9969,0xa90a,0xb92b,
    0x5af5,0x4ad4,0x7ab7,0x6a96,0x1a71,0x0a50,0x3a33,0x2a12,
    0xdbfd,0xcbdc,0xfbbf,0xeb9e,0x9b79,0x8b58,0xbb3b,0xab1a,
    0x6ca6,0x7c87,0x4ce4,0x5cc5,0x2c22,0x3c03,0x0c60,0x1c41,
    0xedae,0xfd8f,0xcdec,0xddcd,0xad2a,0xbd0b,0x8d68,0x9d49,
    0x7e97,0x6eb6,0x5ed5,0x4ef4,0x3e13,0x2e32,0x1e51,0x0e70,
    0xff9f,0xefbe,0xdfdd,0xcffc,0xbf1b,0xaf3a,0x9f59,0x8f78,
    0x9188,0x81a9,0xb1ca,0xa1eb,0xd10c,0xc12d,0xf14e,0xe16f,
    0x1080,0x00a1,0x30c2,0x20e3,0x5004,0x4025,0x7046,0x6067,
    0x83b9,0x9398,0xa3fb,0xb3da,0xc33d,0xd31c,0xe37f,0xf35e,
    0x02b1,0x1290,0x22f3,0x32d2,0x4235,0x5214,0x6277,0x7256,
    0xb5ea,0xa5cb,0x95a8,0x8589,0xf56e,0xe54f,0xd52c,0xc50d,
    0x34e2,0x24c3,0x14a0,0x0481,0x7466,0x6447,0x5424,0x4405,
    0xa7db,0xb7fa,0x8799,0x97b8,0xe75f,0xf77e,0xc71d,0xd73c,
    0x26d3,0x36f2,0x0691,0x16b0,0x6657,0x7676,0x4615,0x5634,
    0xd94c,0xc96d,0xf90e,0xe92f,0x99c8,0x89e9,0xb98a,0xa9ab,
    0x5844,0x4865,0x7806,0x6827,0x18c0,0x08e1,0x3882,0x28a3,
    0xcb7d,0xdb5c,0xeb3f,0xfb1e,0x8bf9,0x9bd8,0xabbb,0xbb9a,
    0x4a75,0x5a54,0x6a37,0x7a16,0x0af1,0x1ad0,0x2ab3,0x3a92,
    0xfd2e,0xed0f,0xdd6c,0xcd4d,0xbdaa,0xad8b,0x9de8,0x8dc9,
    0x7c26,0x6c07,0x5c64,0x4c45,0x3ca2,0x2c83,0x1ce0,0x0cc1,
    0xef1f,0xff3e,0xcf5d,0xdf7c,0xaf9b,0xbfba,0x8fd9,0x9ff8,
    0x6e17,0x7e36,0x4e55,0x5e74,0x2e93,0x3eb2,0x0ed1,0x1ef0
};

quint16 Controller422Dialog::calculateCRC16(const QByteArray &data)
{
    quint16 crc = 0x0000; // 初值
    for (char byte : data) {
        quint8 index = (crc >> 8) ^ (quint8)byte;
        crc = (crc << 8) ^ crc16_table[index];
    }
    return crc;
}

// ================= 构造函数 =================
Controller422Dialog::Controller422Dialog(QWidget *parent) :
    QDialog(parent),
    m_currentDat(QByteArray()) // 初始化成员变量
{
    setWindowTitle("422界面_控制器指令.vi");
    setMinimumSize(800, 500);
    setStyleSheet("background-color: #B0C4DE; font-family: 'SimHei';");

    // ================= 重构顶部布局（核心修改部分）=================
    // 顶部总容器：分为左右两部分
    QHBoxLayout *topLayout = new QHBoxLayout;
    topLayout->setSpacing(20);
    topLayout->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);

    // ----- 左侧区域：串口通道+控制字+操作要求 + 组帧/发送/退出按钮 -----
    QVBoxLayout *leftTopLayout = new QVBoxLayout;
    leftTopLayout->setSpacing(10);
    leftTopLayout->setAlignment(Qt::AlignTop);

    // 第一行：串口通道、控制字、操作要求
    QHBoxLayout *comboRowLayout = new QHBoxLayout;
    comboRowLayout->setSpacing(15);
    comboRowLayout->setAlignment(Qt::AlignLeft);

    // --- 串口通道 ---
    QVBoxLayout *col1 = new QVBoxLayout;
    col1->setAlignment(Qt::AlignCenter);
    col1->addWidget(new QLabel("串口通道"));
    QComboBox *comboSerial = new QComboBox;
    QStringList serialPorts = {"串口A", "串口B", "串口C", "串口D", "串口E", "串口F", "串口G", "串口H", "串口I", "串口J"};
    QStandardItemModel *model = new QStandardItemModel(this);
    for (int i = 0; i < serialPorts.size(); ++i) {
        QStandardItem *item = new QStandardItem(serialPorts[i]);
        if (i < 4 || i > 6) {
            item->setEnabled(false);
            item->setForeground(QBrush(QColor(Qt::gray)));
        }
        model->appendRow(item);
    }
    comboSerial->setModel(model);
    comboSerial->setCurrentText("串口E");
    comboSerial->setStyleSheet("QComboBox { background: black; color: #00FF00; min-width: 80px; }");
    col1->addWidget(comboSerial);

    // --- 控制字 ---
    QVBoxLayout *col2 = new QVBoxLayout;
    col2->setAlignment(Qt::AlignCenter);
    col2->addWidget(new QLabel("控制字"));
    QComboBox *comboControl = new QComboBox;
    comboControl->setStyleSheet("QComboBox { background: black; color: #00FF00; min-width: 150px; }");
    col2->addWidget(comboControl);

    // --- 操作要求 ---
    QVBoxLayout *col3 = new QVBoxLayout;
    col3->setAlignment(Qt::AlignCenter);
    col3->addWidget(new QLabel("操作要求"));
    QComboBox *comboAction = new QComboBox;
    comboAction->addItems({"断开 (55)", "接通 (AA)"});
    comboAction->setCurrentText("断开 (55)");
    comboAction->setStyleSheet("QComboBox { background: black; color: #00FF00; min-width: 100px; }");
    col3->addWidget(comboAction);

    comboRowLayout->addLayout(col1);
    comboRowLayout->addLayout(col2);
    comboRowLayout->addLayout(col3);
    comboRowLayout->addStretch();

    // 第二行：组帧、发送、退出按钮
    QHBoxLayout *btnRowLayout = new QHBoxLayout;
    btnRowLayout->setSpacing(10);
    btnRowLayout->setAlignment(Qt::AlignLeft);
    QPushButton *btnFrame = new QPushButton("组帧");
    QPushButton *btnSend = new QPushButton("发送");
    QPushButton *btnExit = new QPushButton("退出");
    QString btnStyle =
           "QPushButton { min-width: 80px; min-height: 25px; background: #E0E0E0; border: 1px solid gray; }"
           "QPushButton:hover { background: #D0D0D0; border: 1px solid #666; }"
           "QPushButton:pressed { background: #B0B0B0; border: 1px solid #333; }";
    btnFrame->setStyleSheet(btnStyle);
    btnSend->setStyleSheet(btnStyle);
    btnExit->setStyleSheet(btnStyle);
    btnRowLayout->addWidget(btnFrame);
    btnRowLayout->addWidget(btnSend);
    btnRowLayout->addWidget(btnExit);
    btnRowLayout->addStretch();

    // 添加到左侧布局
    leftTopLayout->addLayout(comboRowLayout);
    leftTopLayout->addLayout(btnRowLayout);

    // ----- 右侧区域：发送帧 + 数字显示 + 文本框 -----
    QVBoxLayout *rightTopLayout = new QVBoxLayout;
    rightTopLayout->setAlignment(Qt::AlignTop | Qt::AlignRight);
    rightTopLayout->setSpacing(5);

    // 发送帧标签 + 数字显示（水平布局，高度适配）
    QHBoxLayout *sendFrameHeaderLayout = new QHBoxLayout;
    sendFrameHeaderLayout->setSpacing(5);
    sendFrameHeaderLayout->setAlignment(Qt::AlignRight);

    QLabel *lblSendFrame = new QLabel("发送帧");
    lblSendFrame->setAlignment(Qt::AlignCenter);

    QLabel *lblStatusVal = new QLabel("0");
    // 关键修改：设置固定高度，取消多余内边距
    lblStatusVal->setStyleSheet("background-color: black; color: #00FF00; padding: 2px 8px; border: 1px solid #333; min-width: 30px;");
    lblStatusVal->setAlignment(Qt::AlignCenter);
    lblStatusVal->setFixedHeight(20); // 固定高度，匹配标签高度

    sendFrameHeaderLayout->addWidget(lblSendFrame);
    sendFrameHeaderLayout->addWidget(lblStatusVal);

    // 发送帧文本框
    QTextEdit *textSendFrame = new QTextEdit;
    textSendFrame->setFixedHeight(80);
    textSendFrame->setMinimumWidth(300); // 保证宽度
    textSendFrame->setStyleSheet("background-color: black; color: white; border: 1px solid #333; font-family: 'Courier New';");

    // 添加到右侧布局
    rightTopLayout->addLayout(sendFrameHeaderLayout);
    rightTopLayout->addWidget(textSendFrame);

    // 组合左右布局
    topLayout->addLayout(leftTopLayout);
    topLayout->addLayout(rightTopLayout);
    topLayout->setStretch(0, 1); // 左侧占1份
    topLayout->setStretch(1, 1); // 右侧占1份

    // ================= 中部按钮矩阵（保留原有逻辑） =================
    // 1. 定义完整的映射表
    QMap<QString, quint16> commandMap;
    commandMap.insert("1-解锁主", 0xAA11); commandMap.insert("1-解锁备", 0xAA22); commandMap.insert("1-解锁主备", 0xAA33);
    commandMap.insert("1-锁定Y2", 0xAA44); commandMap.insert("1-供气Y3", 0xAA77); commandMap.insert("1-起爆", 0xAA99);
    commandMap.insert("2-解锁主", 0xBB11); commandMap.insert("2-解锁备", 0xBB22); commandMap.insert("2-解锁主备", 0xBB33);
    commandMap.insert("2-锁定Y2", 0xBB44); commandMap.insert("2-供气Y3", 0xBB77); commandMap.insert("2-起爆", 0xBB99);
    commandMap.insert("3-解锁主", 0xCC11); commandMap.insert("3-解锁备", 0xCC22); commandMap.insert("3-解锁主备", 0xCC33);
    commandMap.insert("3-锁定Y2", 0xCC44); commandMap.insert("3-供气Y3", 0xCC77); commandMap.insert("3-起爆", 0xCC99);
    commandMap.insert("4-解锁主", 0xDD11); commandMap.insert("4-解锁备", 0xDD22); commandMap.insert("4-解锁主备", 0xDD33);
    commandMap.insert("4-锁定Y2", 0xDD44); commandMap.insert("4-供气Y3", 0xDD77); commandMap.insert("4-起爆", 0xDD99);
    commandMap.insert("1~4-解锁主", 0xEE11); commandMap.insert("1~4-解锁备", 0xEE22); commandMap.insert("1~4-解锁主备", 0xEE33);
    commandMap.insert("1~4-锁定Y2", 0xEE44); commandMap.insert("1~4-供气Y3", 0xEE77); commandMap.insert("1~4-起爆", 0xEE99);
    commandMap.insert("波形采集1", 0xAADD); commandMap.insert("波形采集2", 0xAAEE); commandMap.insert("波形采集3", 0xBBAA);
    commandMap.insert("波形采集4", 0xBBBB); commandMap.insert("火工品解控", 0xCCAA); commandMap.insert("火工品保护", 0xCCBB);
    commandMap.insert("火工品解保", 0xCCCC); commandMap.insert("手动模式", 0xEECC); commandMap.insert("测试模式", 0xEECC);
    commandMap.insert("自动模式", 0xEEDD); commandMap.insert("电爆电路采集", 0xCCDD); commandMap.insert("牵制释放准备好", 0xEEBB);
    commandMap.insert("模拟允许释放", 0xEECC); commandMap.insert("允许模式切换", 0xEEDD); commandMap.insert("开锁", 0xEEAA);

    // 2. 填充下拉框
    for (auto it = commandMap.constBegin(); it != commandMap.constEnd(); ++it) {
        comboControl->addItem(it.key());
    }

    // 3. 定义8个区域的按钮名称（严格对应图片布局）
    QList<QList<QString>> buttonRegions = {
        // 区域1：1-xxx 组（2行3列）
        {"1-解锁主", "1-解锁备", "1-解锁主备",
         "1-锁定Y2", "1-供气Y3", "1-起爆"},
        // 区域2：2-xxx 组（2行3列）
        {"2-解锁主", "2-解锁备", "2-解锁主备",
         "2-锁定Y2", "2-供气Y3", "2-起爆"},
        // 区域3：3-xxx 组（2行3列）
        {"3-解锁主", "3-解锁备", "3-解锁主备",
         "3-锁定Y2", "3-供气Y3", "3-起爆"},
        // 区域4：4-xxx 组（2行3列）
        {"4-解锁主", "4-解锁备", "4-解锁主备",
         "4-锁定Y2", "4-供气Y3", "4-起爆"},
        // 区域5：1~4-xxx 组（2行3列）
        {"1~4-解锁主", "1~4-解锁备", "1~4-解锁主备",
         "1~4-锁定Y2", "1~4-供气Y3", "1~4-起爆"},
        // 区域6：波形采集组（2行，第一行3个，第二行1个）
        {"波形采集1", "波形采集2", "波形采集3",
         "波形采集4"},
        // 区域7：火工品+模式组（2行3列）
        {"火工品解控", "火工品保护", "火工品解保",
         "手动模式", "测试模式", "自动模式"},
        // 区域8：电爆电路+释放+开锁组（2行，第一行3个，第二行2个）
        {"电爆电路采集", "牵制释放准备好", "模拟允许释放",
         "允许模式切换", "开锁"}
    };

    // 4. 创建总矩阵布局（4行2列，放置8个区域Frame）
    QGridLayout *matrixMainLayout = new QGridLayout;
    matrixMainLayout->setHorizontalSpacing(10);
    matrixMainLayout->setVerticalSpacing(10);

    // 5. 遍历创建每个区域的Frame和按钮
    for (int regionIdx = 0; regionIdx < buttonRegions.size(); ++regionIdx) {
        QList<QString> btnNames = buttonRegions[regionIdx];
        QFrame *regionFrame = new QFrame;
        regionFrame->setFrameShape(QFrame::Box);  // 加边框，和原图一致
        regionFrame->setStyleSheet("background-color: #B0C4DE;"); // 背景与窗体一致
        QGridLayout *regionLayout = new QGridLayout(regionFrame);
        regionLayout->setHorizontalSpacing(5);
        regionLayout->setVerticalSpacing(5);

        // 填充当前区域的按钮
        int row = 0, col = 0;
        for (QString btnName : btnNames) {
            QPushButton *btn = new QPushButton(btnName);
            btn->setMinimumHeight(30);
            btn->setStyleSheet("background-color: #E0E0E0; border: 1px solid gray;");

            // 设置命令码属性
            quint16 cmdCode = commandMap.value(btnName, 0x0000);
            btn->setProperty("cmdCode", cmdCode);

            // 初始化按钮状态
            m_matrixButtons[btn] = {btnName, false};

            // 连接点击信号
            connect(btn, &QPushButton::clicked, this, &Controller422Dialog::onMatrixButtonClicked);

            // 添加到区域布局
            regionLayout->addWidget(btn, row, col);

            // 列递增，每3列换行（适配大部分区域的2行3列布局）
            col++;
            if (col >= 3) {
                col = 0;
                row++;
            }
        }

        // 计算当前区域在总布局的位置（4行2列）
        int mainRow = regionIdx / 2;
        int mainCol = regionIdx % 2;
        matrixMainLayout->addWidget(regionFrame, mainRow, mainCol);
    }

    // 6. 外层Frame包裹总矩阵布局
    QFrame *frameMatrix = new QFrame;
    frameMatrix->setFrameShape(QFrame::Box);
    frameMatrix->setLayout(matrixMainLayout);

    // ================= 主布局 =================
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->addLayout(topLayout); // 使用重构后的顶部布局
    mainLayout->addWidget(frameMatrix);
    mainLayout->setSpacing(15);

    // 连接信号槽
    connect(btnExit, &QPushButton::clicked, this, &QDialog::reject);
    connect(btnFrame, &QPushButton::clicked, this, &Controller422Dialog::onFrameClicked);
    connect(btnSend, &QPushButton::clicked, this, &Controller422Dialog::onSendClicked);
}

Controller422Dialog::~Controller422Dialog() {}

// ================= 按钮点击逻辑 (自动组帧) =================
void Controller422Dialog::onMatrixButtonClicked()
{
    QPushButton *clickedBtn = qobject_cast<QPushButton*>(sender());
    if (!clickedBtn) return;

    auto combos = findChildren<QComboBox*>();
    if (combos.size() < 3) return;

    QComboBox *comboControl = combos[1];
    QComboBox *comboAction = combos[2];

    BtnInfo &info = m_matrixButtons[clickedBtn];

    if (!info.active) {
        // 选中逻辑
        QMessageBox::StandardButton reply = QMessageBox::question(this, "确认发送",
            QString("是否发出指令：\n%1 ?").arg(info.name), QMessageBox::Yes | QMessageBox::No);
        if (reply == QMessageBox::Yes) {
            info.active = true;
            updateButtonStyle(clickedBtn);
            comboControl->setCurrentText(info.name);
            int idx = comboAction->findText("接通 (AA)");
            if (idx != -1) comboAction->setCurrentIndex(idx);

            // --- 自动组帧 ---
            onFrameClicked();
            onSendClicked();
        }
    } else {
        // 取消选中逻辑
        info.active = false;
        updateButtonStyle(clickedBtn);
        comboControl->setCurrentText(info.name);
        int idx = comboAction->findText("断开 (55)");
        if (idx != -1) comboAction->setCurrentIndex(idx);

        // --- 取消时自动组帧 ---
        onFrameClicked();
        onSendClicked();
    }
}

void Controller422Dialog::updateButtonStyle(QPushButton *btn)
{
    BtnInfo info = m_matrixButtons[btn];
    if (info.active) {
        btn->setStyleSheet("background-color: #006400; color: white; border: 1px solid gray;");
    } else {
        btn->setStyleSheet("background-color: #E0E0E0; border: 1px solid gray;");
    }
}

// ================= 组帧逻辑 =================
void Controller422Dialog::onFrameClicked()
{
    auto combos = findChildren<QComboBox*>();
    if (combos.size() < 3) return;

    QComboBox *comboSerial = combos[0];
    QComboBox *comboControl = combos[1];
    QComboBox *comboAction = combos[2];
    QTextEdit *textSendFrame = findChild<QTextEdit*>();

    QString serialName = comboSerial->currentText();
    QString controlName = comboControl->currentText();
    QString actionText = comboAction->currentText();

    // 1. 解析命令码和操作码
    quint16 cmdCode = 0x0000;
    quint8 opCode = 0x55; // 默认断开
    if (actionText.contains("接通")) opCode = 0xAA;

    // 命令码映射
    struct CmdMap { const char* name; quint16 code; };
    QList<CmdMap> map = {
        {"1-解锁主", 0xAA11}, {"1-解锁备", 0xAA22}, {"1-解锁主备", 0xAA33},
        {"1-锁定Y2", 0xAA44}, {"1-供气Y3", 0xAA77}, {"1-起爆", 0xAA99},
        {"2-解锁主", 0xBB11}, {"2-解锁备", 0xBB22}, {"2-解锁主备", 0xBB33},
        {"2-锁定Y2", 0xBB44}, {"2-供气Y3", 0xBB77}, {"2-起爆", 0xBB99},
        {"3-解锁主", 0xCC11}, {"3-解锁备", 0xCC22}, {"3-解锁主备", 0xCC33},
        {"3-锁定Y2", 0xCC44}, {"3-供气Y3", 0xCC77}, {"3-起爆", 0xCC99},
        {"4-解锁主", 0xDD11}, {"4-解锁备", 0xDD22}, {"4-解锁主备", 0xDD33},
        {"4-锁定Y2", 0xDD44}, {"4-供气Y3", 0xDD77}, {"4-起爆", 0xDD99},
        {"1~4-解锁主", 0xEE11}, {"1~4-解锁备", 0xEE22}, {"1~4-解锁主备", 0xEE33},
        {"1~4-锁定Y2", 0xEE44}, {"1~4-供气Y3", 0xEE77}, {"1~4-起爆", 0xEE99},
        {"波形采集1", 0xAADD}, {"波形采集2", 0xAAEE}, {"波形采集3", 0xBBAA}, {"波形采集4", 0xBBBB},
        {"火工品解控", 0xCCAA}, {"火工品保护", 0xCCBB}, {"火工品解保", 0xCCCC},
        {"电爆电路采集", 0xCCDD}, {"牵制释放准备好", 0xEEBB},
        {"测试模式", 0xEECC}, {"手动模式", 0xEECC},
        {"自动模式", 0xEEDD}, {"允许模式切换", 0xDDEE},
        {"开锁", 0xEEAA}
    };

    bool found = false;
    for (const auto &item : map) {
        if (controlName == item.name) {
            cmdCode = item.code;
            found = true;
            break;
        }
    }
    if (!found) {
        QMessageBox::warning(this, "错误", "未知的控制字指令");
        return;
    }

    // 2. 构造帧
    QByteArray frame;
    // 帧头 (4字节)
    quint32 header = 0xFDB18540; // 串口E/F/G默认值
    frame.append((char)(header >> 24));
    frame.append((char)(header >> 16));
    frame.append((char)(header >> 8));
    frame.append((char)(header));

    // 帧长 (4字节)
    quint32 length = 17;
    frame.append((char)(length >> 24));
    frame.append((char)(length >> 16));
    frame.append((char)(length >> 8));
    frame.append((char)(length));

    // 数据区 (3字节：命令码2字节+操作码1字节)
    frame.append((char)(cmdCode >> 8)); // 高字节
    frame.append((char)(cmdCode));      // 低字节
    frame.append((char)(opCode));       // 操作要求

    // 校验和 (2字节)
    QByteArray crcData = frame.mid(4, 7);
    quint16 crc = calculateCRC16(crcData);
    frame.append((char)(crc >> 8));
    frame.append((char)(crc));

    // 帧尾 (4字节)
    quint32 footer = 0xEB90146F;
    frame.append((char)(footer >> 24));
    frame.append((char)(footer >> 16));
    frame.append((char)(footer >> 8));
    frame.append((char)(footer));

    m_currentDat = frame;

    // 3. 显示十六进制帧数据
    QString hexStr = frame.toHex(' ').toUpper();
    textSendFrame->setText(hexStr);
}

void Controller422Dialog::onSendClicked()
{
    auto combos = findChildren<QComboBox*>();
    if (combos.size() < 3) return;

    STDataPrcSendMsg msg;
    QComboBox *comboSerial = combos[0];
    if(!comboSerial) return;

    // 串口通道映射
    if(comboSerial->currentText()=="串口E")
    {
        msg.channelId = "serial_E";
    }
    else if(comboSerial->currentText()=="串口F")
    {
        msg.channelId = "serial_F"; // 默认值
    }
    else if (comboSerial->currentText()=="串口G")
    {
        msg.channelId = "serial_G"; // 默认值
    }
    else
    {
        msg.channelId = "serial_E";
    }

    msg.channelType = EChannelType::Serial;
    msg.eDataType   = EDataType::E_Unknown;
    msg.btData      = m_currentDat;
    DataInteractionManager::getInstance().sendMsg(msg);
}

void Controller422Dialog::startAutoSequence(const QString &channelId)
{
    // 命令序列：{命令码, 操作码}
    struct SeqStep { quint16 cmd; quint8 op; };
    SeqStep steps[] = {
        {0xEEAA, 0xAA},  // 开锁 + 接通
        {0xCCAA, 0xAA},  // 火工品解控 + 接通
        {0xCCCC, 0xAA},  // 火工品解保 + 接通
        {0xCCAA, 0x55},  // 火工品解控 + 断开 (=非解控)
        {0xEEAA, 0x55},  // 开锁 + 断开 (=关锁)
    };

    auto *timer = new QTimer(this);
    auto *stepIndex = new int(0);

    connect(timer, &QTimer::timeout, this, [this, timer, stepIndex, steps, channelId]() {
        if (*stepIndex >= 5) {
            timer->stop();
            timer->deleteLater();
            delete stepIndex;
            return;
        }
        const auto &s = steps[*stepIndex];

        // 构造帧（复用onFrameClicked的组帧逻辑）
        QByteArray frame;
        quint32 header = 0xFDB18540;
        frame.append((char)(header >> 24)); frame.append((char)(header >> 16));
        frame.append((char)(header >> 8));  frame.append((char)(header));
        quint32 length = 17;
        frame.append((char)(length >> 24)); frame.append((char)(length >> 16));
        frame.append((char)(length >> 8));  frame.append((char)(length));
        frame.append((char)(s.cmd >> 8));   frame.append((char)(s.cmd));
        frame.append((char)(s.op));
        QByteArray crcData = frame.mid(4, 7);
        quint16 crc = calculateCRC16(crcData);
        frame.append((char)(crc >> 8));     frame.append((char)(crc));
        quint32 footer = 0xEB90146F;
        frame.append((char)(footer >> 24)); frame.append((char)(footer >> 16));
        frame.append((char)(footer >> 8));  frame.append((char)(footer));

        STDataPrcSendMsg msg;
        msg.channelId = channelId;
        msg.channelType = EChannelType::Serial;
        msg.eDataType = EDataType::E_Unknown;
        msg.btData = frame;
        DataInteractionManager::getInstance().sendMsg(msg);

        (*stepIndex)++;
    });

    timer->start(1000);
}
