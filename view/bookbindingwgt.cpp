#include "bookbindingwgt.h"
#include <QGroupBox>
#include <QScrollArea>
#include <QComboBox>
#include <QStyleOptionComboBox>
#include <QFileDialog>
#include <QFile>
#include <QTextStream>
#include <QMessageBox>
#include <qlistview.h>
#include <QApplication>  // 新增：剪贴板依赖
#include <QMenu>         // 新增：右键菜单依赖
#include "src/CustomMessage/DataInteractionManager.h"

bookbindingwgt::bookbindingwgt(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle("422界面_控制器装订回读显示");
    setFixedSize(530, 580);
    initStyle();
    initUI();
}

void bookbindingwgt::initStyle()
{
    // 全局样式：优化ComboBox样式（加宽、文字居中、匹配Controller422Dialog）
    QString globalStyle = R"(
        QDialog {
            background-color: #9999CC;
        }
        QPushButton {
            background-color: #E0E0E0;
            border: 1px solid #444444;
            padding: 4px 12px;
            font-size: 12px;
            min-width: 30px;
            min-height: 24px;
        }
        QPushButton:hover {
            background-color: #F0F0F0;
        }
        QPushButton:pressed { /* 新增：按下状态样式，增强点击反馈 */
            background-color: #C0C0C0;
            border: 1px solid #222222;
            padding: 5px 11px 3px 13px; /* 模拟按下位移，强化视觉反馈 */
        }
        QLabel {
            color: #000000;
            font-size: 12px;
            font-weight: bold;
        }
        QGroupBox {
            border: 1px solid #444444;
            font-weight: bold;
            color: #000000;
            /* 给组框加内边距，避免内容贴边 */
            padding: 8px;
        }
        /* 优化ComboBox样式：参考Controller422Dialog，加宽+文字居中 */
        ClickableComboBox {
            background: black;
            color: #00FF00;
            border: 1px solid #444444;
            padding: 4px;
            font-size: 12px;
            min-width: 60px; /* 加宽下拉框 */
            min-height: 24px;
            text-align: center; /* 控件内文字居中 */
        }
        ClickableComboBox::drop-down {
            border-left: 1px solid #444444;
            width: 20px; /* 下拉箭头区域宽度 */
        }
        ClickableComboBox::down-arrow {
            image: none;
            border: none;
        }
        /* 下拉列表样式：加宽+文字居中+匹配Controller422Dialog */
        ClickableComboBox QAbstractItemView {
            background: black;
            color: #00FF00;
            selection-background-color: #333333;
            selection-color: #00FF00;
            border: 1px solid #444444;
            min-width: 60px; /* 下拉选项宽度和控件一致 */
            text-align: center; /* 选项文字居中 */
            padding: 4px 0; /* 选项内边距，避免文字挤在一起 */
        }
        /* 给参数描述用的StyledLineEdit加样式（只读、文字加粗） */
        StyledLineEdit[paramDesc="true"] {
            background: #9999CC;
            color: #000000;
            font-size: 12px;
            font-weight: bold;
            border: none;
            min-width: 150px; /* 加宽：保证文字完整显示 */
            text-align: left;
            padding-left: 4px;
        }
        /* 数值和校验和的StyledLineEdit样式（加宽+黑色背景+绿色文字） */
        StyledLineEdit[paramValue="true"] {
            background: black;
            color: #00FF00;
            font-size: 12px;
            border: 1px solid #444444;
            min-width: 60px; /* 加宽数值输入框 */
            text-align: center;
        }
    )";
    setStyleSheet(globalStyle);
}

// 新增：创建右键菜单（支持复制Label文本）
void bookbindingwgt::createContextMenu(QLabel *label)
{
    label->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(label, &QLabel::customContextMenuRequested, this, [=](const QPoint &pos) {
        QMenu menu(this);
        QAction *copyAction = new QAction("复制", &menu);
        // 绑定复制动作：将Label文本复制到剪贴板
        connect(copyAction, &QAction::triggered, this, [=]() {
            QClipboard *clipboard = QApplication::clipboard();
            clipboard->setText(label->text());
        });
        menu.addAction(copyAction);
        menu.exec(label->mapToGlobal(pos));
    });
}

void bookbindingwgt::initUI()
{
    // ========== 第一行：串口通道 + 控制字 + 组帧/发送 ==========
    QHBoxLayout *hlayout1 = new QHBoxLayout;

    // 串口通道：替换为自定义ClickableComboBox
    QLabel *lblSerial = new QLabel("串口通道");
    cbSerialPort = new ClickableComboBox(this); // 使用自定义下拉框
    cbSerialPort->addItems({"串口E"});
    cbSerialPort->setCurrentText("串口G");
    // 强制下拉列表文字居中（兼容不同Qt版本）
    QListView* listView1 = qobject_cast<QListView*>(cbSerialPort->view());
    if (listView1) listView1->setItemAlignment(Qt::AlignCenter);
    hlayout1->addWidget(lblSerial);
    hlayout1->addWidget(cbSerialPort);
    hlayout1->addSpacing(20);

    // 控制字：替换为自定义ClickableComboBox
    QLabel *lblControl = new QLabel("控制字");
    cbControlWord = new ClickableComboBox(this); // 使用自定义下拉框
    cbControlWord->addItems({"允许装订", "禁止装订", "参数回读", "参数装订", "参数回传"});
    cbControlWord->setCurrentText("允许装订");
    // 强制下拉列表文字居中
    QListView* listView2 = qobject_cast<QListView*>(cbControlWord->view());
    if (listView2) listView2->setItemAlignment(Qt::AlignCenter);
    hlayout1->addWidget(lblControl);
    hlayout1->addWidget(cbControlWord);
    hlayout1->addSpacing(40);

    // 组帧、发送按钮
    QPushButton *btnFrame = new QPushButton("组帧");
    QPushButton *btnSend = new QPushButton("发送");
    hlayout1->addWidget(btnFrame);
    hlayout1->addWidget(btnSend);
    hlayout1->addStretch();

    // ========== 第二行：导出/导入/清零/退出 ==========
    QHBoxLayout *hlayout2 = new QHBoxLayout;
    hlayout2->setContentsMargins(0, 0, 0, 0);
    hlayout2->setSpacing(8);

    QPushButton *btnExport = new QPushButton("导出");
    QPushButton *btnImport = new QPushButton("导入");
    QPushButton *btnClear = new QPushButton("清零");
    QPushButton *btnQuit = new QPushButton("退出");

    int btnMinWidth = 80;
    btnExport->setMinimumWidth(btnMinWidth);
    btnImport->setMinimumWidth(btnMinWidth);
    btnClear->setMinimumWidth(btnMinWidth);
    btnQuit->setMinimumWidth(btnMinWidth);

    hlayout2->addWidget(btnExport);
    hlayout2->addWidget(btnImport);
    hlayout2->addWidget(btnClear);
    hlayout2->addWidget(btnQuit);

    hlayout2->setStretchFactor(btnExport, 1);
    hlayout2->setStretchFactor(btnImport, 1);
    hlayout2->setStretchFactor(btnClear, 1);
    hlayout2->setStretchFactor(btnQuit, 1);

    // ========== 第三行：快捷功能按钮（允许装订/禁止装订等） ==========
    QHBoxLayout *hlayout3 = new QHBoxLayout;
    // 统一布局边距和间距（和存储行保持一致）
    hlayout3->setContentsMargins(0, 0, 0, 0);
    hlayout3->setSpacing(8);

    QPushButton *btnAllowBind = new QPushButton("允许装订");
    QPushButton *btnForbidBind = new QPushButton("禁止装订");
    QPushButton *btnReadParam = new QPushButton("参数回读");
    QPushButton *btnBindParam = new QPushButton("参数装订");

    // 统一按钮最小宽度（和存储行按钮完全一致）
    btnAllowBind->setMinimumWidth(btnMinWidth);
    btnForbidBind->setMinimumWidth(btnMinWidth);
    btnReadParam->setMinimumWidth(btnMinWidth);
    btnBindParam->setMinimumWidth(btnMinWidth);

    hlayout3->addWidget(btnAllowBind);
    hlayout3->addWidget(btnForbidBind);
    hlayout3->addWidget(btnReadParam);
    hlayout3->addWidget(btnBindParam);

    // 设置拉伸因子：按钮均匀分布占满整行（和存储行逻辑一致）
    hlayout3->setStretchFactor(btnAllowBind, 1);
    hlayout3->setStretchFactor(btnForbidBind, 1);
    hlayout3->setStretchFactor(btnReadParam, 1);
    hlayout3->setStretchFactor(btnBindParam, 1);

    // ========== 装订帧显示区域（修改：居中显示） ==========
    QHBoxLayout *hlayout4 = new QHBoxLayout;
    QVBoxLayout *vlayoutBinding = new QVBoxLayout;
    QLabel *lblBindingFrame = new QLabel("装订帧");
    leBindingFrame = new StyledLineEdit(this);
    leBindingFrame->setText("0");

    QHBoxLayout *hlBindingHeader = new QHBoxLayout;
    hlBindingHeader->addWidget(lblBindingFrame);
    hlBindingHeader->addWidget(leBindingFrame);
    hlBindingHeader->addStretch();

    QWidget *widgetBinding = new QWidget;
    widgetBinding->setFixedSize(480, 80);
    widgetBinding->setStyleSheet("background-color: black; border: 1px solid #444;");
    // 添加帧显示Label的布局
    QVBoxLayout *vlayoutBindingWidget = new QVBoxLayout(widgetBinding);
    frameDisplayLabel = new QLabel(widgetBinding);
    frameDisplayLabel->setStyleSheet("color: #00FF00; font-size: 12px; padding: 4px;");
    frameDisplayLabel->setWordWrap(true); // 自动换行适配宽度
    frameDisplayLabel->setAlignment(Qt::AlignCenter); // 核心修改：文字居中
    vlayoutBindingWidget->addWidget(frameDisplayLabel);
    // 新增：给装订帧Label添加右键复制功能
    createContextMenu(frameDisplayLabel);

    vlayoutBinding->addLayout(hlBindingHeader);
    vlayoutBinding->addWidget(widgetBinding, 0, Qt::AlignCenter); // 核心修改：widget居中
    hlayout4->addStretch(); // 左侧拉伸
    hlayout4->addLayout(vlayoutBinding); // 中间居中
    hlayout4->addStretch(); // 右侧拉伸

    // ========== 回读帧显示区域（修改：和装订帧保持一致，居中显示） ==========
    QHBoxLayout *hlayout5 = new QHBoxLayout;
    QVBoxLayout *vlayoutReadback = new QVBoxLayout;
    QLabel *lblReadbackFrame = new QLabel("回读帧");
    leReadbackFrame = new StyledLineEdit(this);
    leReadbackFrame->setText("0");

    QHBoxLayout *hlReadbackHeader = new QHBoxLayout;
    hlReadbackHeader->addWidget(lblReadbackFrame);
    hlReadbackHeader->addWidget(leReadbackFrame);
    hlReadbackHeader->addStretch();

    QWidget *widgetReadback = new QWidget;
    widgetReadback->setFixedSize(480, 80);
    widgetReadback->setStyleSheet("background-color: black; border: 1px solid #444;");
    // 新增：回读帧添加显示Label的布局（和装订帧一致）
    QVBoxLayout *vlayoutReadbackWidget = new QVBoxLayout(widgetReadback);
    readbackFrameDisplayLabel = new QLabel(widgetReadback);
    readbackFrameDisplayLabel->setStyleSheet("color: #00FF00; font-size: 12px; padding: 4px;");
    readbackFrameDisplayLabel->setWordWrap(true); // 自动换行适配宽度
    readbackFrameDisplayLabel->setAlignment(Qt::AlignCenter); // 核心修改：文字居中
    vlayoutReadbackWidget->addWidget(readbackFrameDisplayLabel);
    // 新增：给回读帧Label添加右键复制功能
    createContextMenu(readbackFrameDisplayLabel);

    vlayoutReadback->addLayout(hlReadbackHeader);
    vlayoutReadback->addWidget(widgetReadback, 0, Qt::AlignCenter); // 核心修改：widget居中
    hlayout5->addStretch(); // 左侧拉伸
    hlayout5->addLayout(vlayoutReadback); // 中间居中
    hlayout5->addStretch(); // 右侧拉伸

    // ========== 装订参数区域 ==========
    QHBoxLayout *hlayout6 = new QHBoxLayout;
    QGroupBox *gbParam = new QGroupBox("装订参数");
    // 加宽组框，保证内部控件有足够空间
    gbParam->setMinimumWidth(480);
    QVBoxLayout *vlayoutParam = new QVBoxLayout(gbParam);
    // 调整行间距，避免拥挤
    vlayoutParam->setSpacing(6);

    QHBoxLayout *hlParam1 = new QHBoxLayout;
    // 调整控件间距
    hlParam1->setSpacing(8);
    // 参数描述：火工品引爆时间
    StyledLineEdit *lblFire = new StyledLineEdit(this);
    lblFire->setText("火工品引爆时间 (ms)");
    lblFire->setProperty("paramDesc", true); // 标记属性，用于样式区分
    leFireTime = new StyledLineEdit(this);
    leFireTime->setText("190");
    leFireTime->setProperty("paramValue", true); // 标记为数值控件
    leCheckSum1 = new StyledLineEdit(this);
    leCheckSum1->setText("0");
    leCheckSum1->setProperty("paramValue", true); // 标记为数值控件
    hlParam1->addWidget(lblFire);
    hlParam1->addWidget(leFireTime);
    hlParam1->addWidget(leCheckSum1);
    vlayoutParam->addLayout(hlParam1);

    QHBoxLayout *hlParam2 = new QHBoxLayout;
    hlParam2->setSpacing(8);
    // 参数描述：继电器关闭时间
    StyledLineEdit *lblRelay = new StyledLineEdit(this);
    lblRelay->setText("继电器关闭时间 (ms)");
    lblRelay->setProperty("paramDesc", true);
    leRelayCloseTime = new StyledLineEdit(this);
    leRelayCloseTime->setText("100");
    leRelayCloseTime->setProperty("paramValue", true);
    leCheckSum2 = new StyledLineEdit(this);
    leCheckSum2->setText("0");
    leCheckSum2->setProperty("paramValue", true);
    hlParam2->addWidget(lblRelay);
    hlParam2->addWidget(leRelayCloseTime);
    hlParam2->addWidget(leCheckSum2);
    vlayoutParam->addLayout(hlParam2);

    QHBoxLayout *hlParam3 = new QHBoxLayout;
    hlParam3->setSpacing(8);
    // 参数描述：电磁阀Y3-Y2延时
    StyledLineEdit *lblValve = new StyledLineEdit(this);
    lblValve->setText("电磁阀Y3-Y2延时 (ms)");
    lblValve->setProperty("paramDesc", true);
    leValveDelayTime = new StyledLineEdit(this);
    leValveDelayTime->setText("200");
    leValveDelayTime->setProperty("paramValue", true);
    leCheckSum3 = new StyledLineEdit(this);
    leCheckSum3->setText("0");
    leCheckSum3->setProperty("paramValue", true);
    hlParam3->addWidget(lblValve);
    hlParam3->addWidget(leValveDelayTime);
    hlParam3->addWidget(leCheckSum3);
    vlayoutParam->addLayout(hlParam3);

    QLabel *lblCheckSum = new QLabel("校验和");
    hlayout6->addWidget(gbParam);
    hlayout6->addStretch();

    // ========== 主布局整合 ==========
    QVBoxLayout *vlayoutMain = new QVBoxLayout(this);
    vlayoutMain->addLayout(hlayout1);
    vlayoutMain->addSpacing(10);
    vlayoutMain->addLayout(hlayout2);
    vlayoutMain->addSpacing(10);
    vlayoutMain->addLayout(hlayout3);
    vlayoutMain->addSpacing(10);
    vlayoutMain->addLayout(hlayout4);
    vlayoutMain->addSpacing(10);
    vlayoutMain->addLayout(hlayout5);
    vlayoutMain->addSpacing(10);
    vlayoutMain->addLayout(hlayout6);
    vlayoutMain->addStretch();

    // ========== 按钮信号连接 ==========
    connect(btnQuit, &QPushButton::clicked, this, &QDialog::close);
    connect(btnAllowBind, &QPushButton::clicked, this, &bookbindingwgt::onAllowBindClicked);
    connect(btnForbidBind, &QPushButton::clicked, this, &bookbindingwgt::onForbidBindClicked);
    connect(btnReadParam, &QPushButton::clicked, this, &bookbindingwgt::onReadParamClicked);
    connect(btnBindParam, &QPushButton::clicked, this, &bookbindingwgt::onBindParamClicked);
    connect(btnFrame, &QPushButton::clicked, this, &bookbindingwgt::onFrameClicked);
    connect(btnClear, &QPushButton::clicked, this, &bookbindingwgt::onClearClicked);
    connect(btnSend, &QPushButton::clicked, this, &bookbindingwgt::onSendClicked);
    connect(btnExport, &QPushButton::clicked, this, &bookbindingwgt::onExportClicked);
    connect(btnImport, &QPushButton::clicked, this, &bookbindingwgt::onImportClicked);

}

// CRC16/XMODEM校验计算（按指定校验表）
quint16 bookbindingwgt::calculateCRC16(const QByteArray &data)
{
    static const quint16 crcTable[] = {
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
    };

    quint16 crc = 0x0000;
    for (char byte : data) {
        quint8 high = (crc >> 8) & 0xFF;
        crc = (crc << 8) ^ crcTable[(high ^ static_cast<quint8>(byte))];
    }
    return crc;
}

// 显示帧数据到装订帧黑色显示区域
void bookbindingwgt::displayFrame(const QString &frameStr)
{
    frameDisplayLabel->setText(frameStr);
}

// 新增：显示帧数据到回读帧黑色显示区域
void bookbindingwgt::displayReadbackFrame(const QString &frameStr)
{
    readbackFrameDisplayLabel->setText(frameStr);
}

// 整数转16位补码（2字节，高字节在前，低字节在后，符合协议要求）
QByteArray bookbindingwgt::intTo16BitComplement(int value)
{
    qint16 val = static_cast<qint16>(value);
    QByteArray bytes;
    bytes.append(static_cast<char>((val >> 8) & 0xFF)); // 高字节在前
    bytes.append(static_cast<char>(val & 0xFF));        // 低字节在后
    return bytes;
}

// 允许装订按钮槽函数
void bookbindingwgt::onAllowBindClicked()
{
    QByteArray frameData;
    // 帧头：0xFDB18540（字节序：FD B1 85 40）
    frameData.append(0xFD);
    frameData.append(0xB1);
    frameData.append(0x85);
    frameData.append(0x40);
    // 帧类型：0xAA
    frameData.append(0xAA);
    // 数据区：0xFF
    frameData.append(0xFF);
    // 计算CRC16（帧类型+数据区）
    QByteArray crcData = frameData.mid(4, 2);
    quint16 crc = calculateCRC16(crcData);
    // CRC字节顺序：高字节在前，低字节在后
    frameData.append(static_cast<char>((crc >> 8) & 0xFF)); // 高字节
    frameData.append(static_cast<char>(crc & 0xFF));        // 低字节
    // 帧尾：0xEB90146F（字节序：EB 90 14 6F）
    frameData.append(0xEB);
    frameData.append(0x90);
    frameData.append(0x14);
    frameData.append(0x6F);

    // 存储组帧后的数据到m_currentDat
    m_currentDat = frameData;
    // 转换为十六进制字符串显示
    displayFrame(frameData.toHex(' ').toUpper());
}

// 禁止装订按钮槽函数
void bookbindingwgt::onForbidBindClicked()
{
    QByteArray frameData;
    // 帧头：0xFDB18540
    frameData.append(0xFD);
    frameData.append(0xB1);
    frameData.append(0x85);
    frameData.append(0x40);
    // 帧类型：0xBB
    frameData.append(0xBB);
    // 数据区：0xFF
    frameData.append(0xFF);
    // 计算CRC16
    QByteArray crcData = frameData.mid(4, 2);
    quint16 crc = calculateCRC16(crcData);
    // CRC字节顺序调整
    frameData.append(static_cast<char>((crc >> 8) & 0xFF)); // 高字节
    frameData.append(static_cast<char>(crc & 0xFF));        // 低字节
    // 帧尾
    frameData.append(0xEB);
    frameData.append(0x90);
    frameData.append(0x14);
    frameData.append(0x6F);

    // 存储组帧后的数据到m_currentDat
    m_currentDat = frameData;
    displayFrame(frameData.toHex(' ').toUpper());
}

// 参数回读按钮槽函数
void bookbindingwgt::onReadParamClicked()
{
    QByteArray frameData;
    // 帧头：0xFDB18540
    frameData.append(0xFD);
    frameData.append(0xB1);
    frameData.append(0x85);
    frameData.append(0x40);
    // 帧类型：0xDD
    frameData.append(0xDD);
    // 数据区：0xFF
    frameData.append(0xFF);
    // 计算CRC16
    QByteArray crcData = frameData.mid(4, 2);
    quint16 crc = calculateCRC16(crcData);
    // CRC字节顺序调整
    frameData.append(static_cast<char>((crc >> 8) & 0xFF)); // 高字节
    frameData.append(static_cast<char>(crc & 0xFF));        // 低字节
    // 帧尾
    frameData.append(0xEB);
    frameData.append(0x90);
    frameData.append(0x14);
    frameData.append(0x6F);

    // 存储组帧后的数据到m_currentDat
    m_currentDat = frameData;
    displayFrame(frameData.toHex(' ').toUpper());
}

// 参数装订按钮槽函数（匹配表B4协议）
void bookbindingwgt::onBindParamClicked()
{
    // 读取界面数值（增加合法性校验，避免非数字输入）
    bool fireOk, relayOk, valveOk;
    int fireTime = leFireTime->text().toInt(&fireOk);
    int relayTime = leRelayCloseTime->text().toInt(&relayOk);
    int valveTime = leValveDelayTime->text().toInt(&valveOk);

    // 非法值默认处理
    if (!fireOk) fireTime = 170;
    if (!relayOk) relayTime = 100;
    if (!valveOk) valveTime = 200;

    QByteArray frameData;
    // 1~4字节：帧头 0xFDB18540（字节序：FD B1 85 40）
    frameData.append(0xFD);
    frameData.append(0xB1);
    frameData.append(0x85);
    frameData.append(0x40);

    // 5字节：帧类型 0xCC
    frameData.append(0xCC);

    // 6~7字节：火工品引爆时间（2字节，16位补码，高字节在前）
    frameData.append(intTo16BitComplement(fireTime));
    // 8~9字节：继电器关闭时间（2字节，16位补码，高字节在前）
    frameData.append(intTo16BitComplement(relayTime));
    // 10~11字节：电磁阀Y3-Y2延时（2字节，16位补码，高字节在前）
    frameData.append(intTo16BitComplement(valveTime));

    // 12~13字节：校验值（对5~11字节做CRC16，共7字节）
    QByteArray crcData = frameData.mid(4, 7); // 索引4对应第5字节，长度7
    quint16 crc = calculateCRC16(crcData);
    // CRC字节顺序调整
    frameData.append(static_cast<char>((crc >> 8) & 0xFF)); // 高字节
    frameData.append(static_cast<char>(crc & 0xFF));        // 低字节
    // 14~17字节：帧尾 0xEB90146F
    frameData.append(0xEB);
    frameData.append(0x90);
    frameData.append(0x14);
    frameData.append(0x6F);

    // 存储组帧后的数据到m_currentDat
    m_currentDat = frameData;
    // 转换为十六进制字符串显示
    displayFrame(frameData.toHex(' ').toUpper());
}

// 组帧按钮槽函数：支持参数回传等所有控制字组帧
void bookbindingwgt::onFrameClicked()
{
    QString controlWord = cbControlWord->currentText();
    if (controlWord == "允许装订") {
        onAllowBindClicked(); // 复用允许装订组帧逻辑
    } else if (controlWord == "禁止装订") {
        onForbidBindClicked(); // 复用禁止装订组帧逻辑
    } else if (controlWord == "参数回读") {
        onReadParamClicked(); // 复用参数回读组帧逻辑
    } else if (controlWord == "参数装订") {
        onBindParamClicked(); // 复用参数装订组帧逻辑
    } else if (controlWord == "参数回传") {
        // ========== 表B6 参数回传通信协议组帧 ==========
        // 1. 读取界面参数（火工品/继电器/电磁阀时间），增加合法性校验
        bool fireOk, relayOk, valveOk;
        int fireTime = leFireTime->text().toInt(&fireOk);
        int relayTime = leRelayCloseTime->text().toInt(&relayOk);
        int valveTime = leValveDelayTime->text().toInt(&valveOk);
        // 非法值默认处理
        if (!fireOk) fireTime = 190;
        if (!relayOk) relayTime = 100;
        if (!valveOk) valveTime = 200;

        QByteArray frameData;
        // 1~4字节：帧头 0xFDB18540（字节序：FD B1 85 40）
        frameData.append(0xFD);
        frameData.append(0xB1);
        frameData.append(0x85);
        frameData.append(0x40);

        // 5~8字节：帧长 0x19（4字节，高位补0 → 00 00 00 19）
        frameData.append(static_cast<char>(0x00));
        frameData.append(static_cast<char>(0x00));
        frameData.append(static_cast<char>(0x00));
        frameData.append(0x19);

        // 9~12字节：帧计数 常值0（4字节 → 00 00 00 00）
        frameData.append(static_cast<char>(0x00));
        frameData.append(static_cast<char>(0x00));
        frameData.append(static_cast<char>(0x00));
        frameData.append(static_cast<char>(0x00));

        // 13字节：帧类型 0xEE
        frameData.append(0xEE);

        // 14~15字节：火工品引爆时间（2字节，16位补码，高字节在前）
        frameData.append(intTo16BitComplement(fireTime));

        // 16~17字节：继电器关闭时间（2字节，16位补码，高字节在前）
        frameData.append(intTo16BitComplement(relayTime));

        // 18~19字节：电磁阀Y3-Y2延时（2字节，16位补码，高字节在前）
        frameData.append(intTo16BitComplement(valveTime));

        // 20~21字节：CRC16校验（对5~11字节，表B6备注要求）
        QByteArray crcData = frameData.mid(4, 7); // 索引4对应第5字节，长度7
        quint16 crc = calculateCRC16(crcData);
        // CRC字节顺序：高字节在前，低字节在后
        frameData.append(static_cast<char>((crc >> 8) & 0xFF));
        frameData.append(static_cast<char>(crc & 0xFF));

        // 22~25字节：帧尾 0xEB90146F（字节序：EB 90 14 6F）
        frameData.append(0xEB);
        frameData.append(0x90);
        frameData.append(0x14);
        frameData.append(0x6F);

        // 存储组帧后的数据到m_currentDat
        m_currentDat = frameData;
        // 显示组帧结果（十六进制空格分隔，大写）
        displayFrame(frameData.toHex(' ').toUpper());
    } else {
        // 清空无效数据
        m_currentDat.clear();
        displayFrame("未知控制字，请选择有效选项");
    }
}

// 清零按钮槽函数：清空装订帧/回读帧显示框，重置计数为0
void bookbindingwgt::onClearClicked()
{
    // 清空装订帧显示标签（黑色背景的文本区域）
    frameDisplayLabel->clear();
    // 清空回读帧显示标签
    readbackFrameDisplayLabel->clear();

    // 重置装订帧计数输入框为初始值0
    leBindingFrame->setText("0");
    // 重置回读帧计数输入框为初始值0
    leReadbackFrame->setText("0");

    // 清空存储的帧数据
    m_currentDat.clear();
}

// 导出装订参数到CSV文件
void bookbindingwgt::onExportClicked()
{
    QString filePath = QFileDialog::getSaveFileName(this, "导出装订参数", "binding_params.csv", "CSV文件 (*.csv)");
    if (filePath.isEmpty()) return;

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) return;

    // 写入UTF-8 BOM防止Excel打开乱码
    file.write("\xEF\xBB\xBF");
    file.write(QString("参数名称,数值\n").toUtf8());
    file.write(QString("火工品引爆时间(ms),%1\n").arg(leFireTime->text()).toUtf8());
    file.write(QString("继电器关闭时间(ms),%1\n").arg(leRelayCloseTime->text()).toUtf8());
    file.write(QString("电磁阀Y3-Y2延时(ms),%1\n").arg(leValveDelayTime->text()).toUtf8());
    file.close();
    QMessageBox::information(this, "导出成功", "装订参数已导出到\n" + filePath);
}

// 导入装订参数并从CSV文件设置界面
void bookbindingwgt::onImportClicked()
{
    QString filePath = QFileDialog::getOpenFileName(this, "导入装订参数", "", "CSV文件 (*.csv);;所有文件 (*.*)");
    if (filePath.isEmpty()) return;

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        QMessageBox::warning(this, "导入失败", "无法打开文件：" + filePath);
        return;
    }

    QTextStream in(&file);
    QStringList lines = in.readAll().split('\n', Qt::SkipEmptyParts);
    file.close();

    // 解析CSV：第一行为表头，后续每行为 "名称,数值"
    for (int i = 1; i < lines.size(); i++) {
        QStringList cols = lines[i].split(',');
        if (cols.size() < 2) continue;
        QString name = cols[0].trimmed();
        QString val  = cols[1].trimmed();
        if (name.contains("火工品引爆时间")) leFireTime->setText(val);
        else if (name.contains("继电器关闭时间")) leRelayCloseTime->setText(val);
        else if (name.contains("电磁阀Y3-Y2延时")) leValveDelayTime->setText(val);
    }
    QMessageBox::information(this, "导入成功", "装订参数已从文件加载");
}

void bookbindingwgt::onSendClicked()
{
    // 校验是否有有效数据
    if (m_currentDat.isEmpty()) {
        displayFrame("无有效帧数据，请先组帧！");
        return;
    }

    STDataPrcSendMsg msg;

    // 从下拉框获取串口通道（适配界面选择的串口）
    QString serialText = cbSerialPort->currentText(); // 如"串口G"
    QString channelId = "serial_" + serialText.right(1); // 转换为"serial_G"

//    msg.channelId = channelId;
    msg.channelId =  "serial_E";
    msg.channelType = EChannelType::Serial;
    msg.eDataType   = EDataType::E_Unknown;
    msg.btData      = m_currentDat; // 使用组帧后的有效数据
    DataInteractionManager::getInstance().sendMsg(msg);
}
