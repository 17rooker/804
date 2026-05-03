#include "copyframedialog.h"
#include "styledledlabel.h"
#include "styledlineedit.h"

#include "src/DataProcess/DataAnalysis/FrameDataAnalysis.h"
#include "src/Common/CommTypes.h"

#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLabel>
#include <QGroupBox>
#include <QTimer>
#include <QThread>
#include <QMetaObject>

// ═══════════════════════════════════════════════════════════════════════════
//  FrameCopyWorker
// ═══════════════════════════════════════════════════════════════════════════
void FrameCopyWorker::processData(const QByteArray &data)
{
    QByteArray buf = data;
    const int MAX_FRAMES = 100;
    const int EMIT_INTERVAL = 5;
    int framesProcessed = 0;
    STParamInfo lastParam;

    while (buf.size() >= 8 && framesProcessed < MAX_FRAMES) {
        if (buf.left(4) != QByteArray::fromHex("FDB18540")) {
            buf.remove(0, 1); continue;
        }
        QByteArray lenBytes = buf.mid(4, 4);
        qint32 frameLen = (static_cast<quint8>(lenBytes[0]) << 24) |
                          (static_cast<quint8>(lenBytes[1]) << 16) |
                          (static_cast<quint8>(lenBytes[2]) << 8) |
                           static_cast<quint8>(lenBytes[3]);
        if (frameLen <= 85 || frameLen > 1024 * 1024) {
            buf.remove(0, 4); continue;
        }
        if (buf.size() < frameLen) break;

        QByteArray frameData = buf.left(frameLen);
        buf.remove(0, frameLen);

        FrameDataAnalysis analy;
        STPackage pack;
        pack.channelId = "serial_E";
        pack.channelType = EChannelType::Serial;
        pack.baDataRecv = frameData;

        STParamInfo param{};
        analy.parseData(pack, param);

        // CRC16
        int totalLen = frameData.size();
        if (totalLen >= 10) {
            quint16 calc = FrameDataAnalysis::crc16Xmodem(frameData, 4, totalLen - 10);
            quint16 stored = (static_cast<quint8>(frameData[totalLen - 6]) << 8) |
                              static_cast<quint8>(frameData[totalLen - 5]);
            STParamItem crcItem;
            crcItem.varParaValue = (calc == stored) ? "校验正确" : "校验错误";
            param.mapParams["CRC校验"] = crcItem;
        }

        lastParam = param;
        if (framesProcessed % EMIT_INTERVAL == 0)
            emit dataProcessed(param);
        ++framesProcessed;
    }
    if (framesProcessed > 0 && framesProcessed % EMIT_INTERVAL != 0)
        emit dataProcessed(lastParam);
}

// ═══════════════════════════════════════════════════════════════════════════
CopyFrameDialog::CopyFrameDialog(QWidget *parent)
    : QWidget(parent)
    , m_updateTimer(new QTimer(this))
    , m_worker(new FrameCopyWorker())
    , m_workerThread(new QThread(this))
{
    setupUi();
    initWorkerThread();

    m_updateTimer->setSingleShot(true);
    m_updateTimer->setInterval(10);
    connect(m_updateTimer, &QTimer::timeout, this, [this]() {
        QMutexLocker lock(&m_cacheMutex);
        if (m_dataCache.isEmpty()) return;
        QByteArray data = m_dataCache;
        m_dataCache.clear();
        lock.unlock();
        QMetaObject::invokeMethod(m_worker, "processData",
            Qt::QueuedConnection, Q_ARG(QByteArray, data));
    });
}

CopyFrameDialog::~CopyFrameDialog() {
    m_workerThread->quit();
    m_workerThread->wait();
}

void CopyFrameDialog::initWorkerThread()
{
    m_worker->moveToThread(m_workerThread);
    connect(m_worker, &FrameCopyWorker::dataProcessed,
            this, &CopyFrameDialog::setParam, Qt::QueuedConnection);
    m_workerThread->start();
}

void CopyFrameDialog::appendData(const QByteArray &data)
{
    QMutexLocker lock(&m_cacheMutex);
    m_dataCache.append(data);
    m_updateTimer->start();
}

void CopyFrameDialog::clearPlaybackCache()
{
    QMutexLocker lock(&m_cacheMutex);
    m_dataCache.clear();
    m_updateTimer->stop();
}

void CopyFrameDialog::setParam(const STParamInfo &param)
{
    m_param = param;
    updateData(param);
}

// ═══════════════════════════════════════════════════════════════════════════
//  UI（保持原布局不变）
// ═══════════════════════════════════════════════════════════════════════════
void CopyFrameDialog::setupUi()
{
    this->setWindowTitle("控制器测试帧解析结果");
    this->setStyleSheet(R"(
        QWidget { background-color: #E0E0E0; font-family: "Microsoft YaHei", Arial, sans-serif; font-size: 12px; }
        QLabel { color: #333333; font-weight: 500; }
        QGroupBox { border: none; margin: 0; padding: 0; }
        StyledLineEdit { border: 1px solid #B0B0B0; border-radius: 2px; }
    )");
    this->setMinimumSize(1000, 500);

    QWidget *centralWidget = new QWidget(this);
    QHBoxLayout *mainLayout = new QHBoxLayout(centralWidget);
    mainLayout->setSpacing(12);
    mainLayout->setContentsMargins(12, 12, 12, 12);

    QWidget *col1 = createColumn1(); col1->setMinimumWidth(100);
    QWidget *col2 = createColumn2(); col2->setMinimumWidth(110);
    QWidget *col3 = createColumn3(); col3->setMinimumWidth(110);
    QWidget *col4 = createColumn4(); col4->setMinimumWidth(140);
    QWidget *col5 = createCombinedColumn(); col5->setMinimumWidth(350);

    mainLayout->addWidget(col1);
    mainLayout->addWidget(col2);
    mainLayout->addWidget(col3);
    mainLayout->addWidget(col4);
    mainLayout->addWidget(col5);

    QVBoxLayout *rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(0,0,0,0);
    rootLayout->addWidget(centralWidget);
    setLayout(rootLayout);
}

// ── 第1列：火保/解控 ─────────────────────────────────────────────────────
QWidget* CopyFrameDialog::createColumn1()
{
    QWidget *widget = new QWidget;
    QVBoxLayout *layout = new QVBoxLayout(widget);
    layout->setSpacing(4);
    layout->setContentsMargins(4, 4, 4, 4);

    auto createLedItem = [&](const QString &text) {
        QWidget *rowWidget = new QWidget;
        QHBoxLayout *hLay = new QHBoxLayout(rowWidget);
        hLay->setContentsMargins(0, 1, 0, 1);
        hLay->setSpacing(6);

        StyledLedLabel *led = new StyledLedLabel(this);
        led->setOn(false); led->setSwitchable(false);
        led->setDisabledLed(false); led->setFixedSize(20, 20);
        hLay->addWidget(led, 0, Qt::AlignCenter | Qt::AlignVCenter);

        QLabel *lbl = new QLabel(text);
        lbl->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        lbl->setFixedWidth(70);
        hLay->addWidget(lbl, 1);
        hLay->addStretch();

        m_ledMap[text] = led;
        return rowWidget;
    };

    for (const QString &text : {"火保K1","火保K2","火保K3","火保K4",
        "火保1","火保2","火保3","火保4",
        "电爆继电器","非解控JKt","非解控JKbac",
        "解控K1","解控K2","解控K3","解控K4","转发允释1"})
        layout->addWidget(createLedItem(text));

    layout->addStretch();
    return widget;
}

// ── 第2列：机构1/2/3 ─────────────────────────────────────────────────────
QWidget* CopyFrameDialog::createColumn2()
{
    QWidget *widget = new QWidget;
    QVBoxLayout *layout = new QVBoxLayout(widget);
    layout->setSpacing(4);
    layout->setContentsMargins(4, 4, 4, 4);

    auto createLedItem = [&](const QString &text) {
        QWidget *rowWidget = new QWidget;
        QHBoxLayout *hLay = new QHBoxLayout(rowWidget);
        hLay->setContentsMargins(0, 1, 0, 1);
        hLay->setSpacing(6);

        StyledLedLabel *led = new StyledLedLabel(this);
        led->setOn(false); led->setSwitchable(false);
        led->setDisabledLed(false); led->setFixedSize(20, 20);
        hLay->addWidget(led, 0, Qt::AlignCenter | Qt::AlignVCenter);

        QLabel *lbl = new QLabel(text);
        lbl->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        lbl->setFixedWidth(80);
        hLay->addWidget(lbl, 1);
        hLay->addStretch();

        m_ledMap[text] = led;
        return rowWidget;
    };

    for (const QString &text : {"机构1连接","机构1供气","机构1锁定","机构1释放主","机构1释放备",
        "转发允释2","机构2连接","机构2供气","机构2锁定","机构2释放主","机构2释放备",
        "机构3连接","机构3供气","机构3锁定","机构3释放主"})
        layout->addWidget(createLedItem(text));

    layout->addStretch();
    return widget;
}

// ── 第3列：机构4/电爆/火引爆 ─────────────────────────────────────────────
QWidget* CopyFrameDialog::createColumn3()
{
    QWidget *widget = new QWidget;
    QVBoxLayout *layout = new QVBoxLayout(widget);
    layout->setSpacing(4);
    layout->setContentsMargins(4, 4, 4, 4);

    auto createLedItem = [&](const QString &text) {
        QWidget *rowWidget = new QWidget;
        QHBoxLayout *hLay = new QHBoxLayout(rowWidget);
        hLay->setContentsMargins(0, 1, 0, 1);
        hLay->setSpacing(6);

        StyledLedLabel *led = new StyledLedLabel(this);
        led->setOn(false); led->setSwitchable(false);
        led->setDisabledLed(false); led->setFixedSize(20, 20);
        hLay->addWidget(led, 0, Qt::AlignCenter | Qt::AlignVCenter);

        QLabel *lbl = new QLabel(text);
        lbl->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        lbl->setFixedWidth(80);
        hLay->addWidget(lbl, 1);
        hLay->addStretch();

        m_ledMap[text] = led;
        return rowWidget;
    };

    for (const QString &text : {"机构3释放备","机构4连接","机构4供气","机构4锁定",
        "机构4释放主","机构4释放备","电爆1","电爆2","电爆3","电爆4",
        "火引爆1","火引爆2","火引爆3","火引爆4"})
        layout->addWidget(createLedItem(text));

    layout->addStretch();
    return widget;
}

// ── 第4列：帧参数 ────────────────────────────────────────────────────────
QWidget* CopyFrameDialog::createColumn4()
{
    QWidget *widget = new QWidget;
    QVBoxLayout *layout = new QVBoxLayout(widget);
    layout->setSpacing(4);
    layout->setContentsMargins(4, 4, 4, 4);

    QStringList labels = {"帧长","帧计数","帧类型","时间标志",
        "火引爆时间","继电器关闭时","数字供电",
        "驱动供电1","驱动供电2","5V1","5V2","5V3"};

    for (const QString &labelText : labels) {
        QWidget *rowWidget = new QWidget;
        QHBoxLayout *rowLayout = new QHBoxLayout(rowWidget);
        rowLayout->setContentsMargins(0, 1, 0, 1);
        rowLayout->setSpacing(6);

        QLabel *label = new QLabel(labelText);
        label->setFixedWidth(75);
        label->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        rowLayout->addWidget(label);

        StyledLineEdit *editBox = new StyledLineEdit(this);
        editBox->setGrayInputMode(true);
        editBox->setFixedSize(65, 22);
        editBox->setAlignment(Qt::AlignCenter);
        rowLayout->addWidget(editBox);
        rowLayout->addStretch();

        m_valueMap[labelText] = editBox;
        layout->addWidget(rowWidget);
    }

    layout->addStretch();
    return widget;
}

// ── 第5列：合并列 ────────────────────────────────────────────────────────
QWidget* CopyFrameDialog::createCombinedColumn()
{
    QWidget *widget = new QWidget;
    QHBoxLayout *combinedLayout = new QHBoxLayout(widget);
    combinedLayout->setSpacing(10);
    combinedLayout->setContentsMargins(4, 4, 4, 4);

    // 1. 4列时间输入框
    QWidget *inputBoxColumn = new QWidget;
    QVBoxLayout *inputBoxLayout = new QVBoxLayout(inputBoxColumn);
    inputBoxLayout->setSpacing(4);
    inputBoxLayout->setContentsMargins(0,0,0,0);
    for (int i = 0; i < 12; ++i) {
        QWidget *rowWidget = new QWidget;
        QHBoxLayout *rowLayout = new QHBoxLayout(rowWidget);
        rowLayout->setContentsMargins(0, 1, 0, 1);
        rowLayout->setSpacing(4);

        for (int j = 0; j < 4; ++j) {
            StyledLineEdit *editBox = new StyledLineEdit(this);
            editBox->setGrayInputMode(false);
            editBox->setFixedSize(42, 22);
            editBox->setAlignment(Qt::AlignCenter);
            rowLayout->addWidget(editBox);
            m_valueMap[QString("t%1_c%2").arg(i+1).arg(j+1)] = editBox;
        }
        inputBoxLayout->addWidget(rowWidget);
    }
    combinedLayout->addWidget(inputBoxColumn);

    // 2. 机构1电压标签
    QWidget *labelColumn = new QWidget;
    QVBoxLayout *labelLayout = new QVBoxLayout(labelColumn);
    labelLayout->setSpacing(4);
    labelLayout->setContentsMargins(0,0,0,0);
    for (int i = 1; i <= 12; ++i) {
        QLabel *voltageLabel = new QLabel(QString("机构1电压%1").arg(i));
        voltageLabel->setFixedSize(85, 22);
        voltageLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        labelLayout->addWidget(voltageLabel);
    }
    combinedLayout->addWidget(labelColumn);

    // 3. 右侧状态控件
    QWidget *statusColumn = new QWidget;
    QGridLayout *statusLayout = new QGridLayout(statusColumn);
    statusLayout->setSpacing(4);
    statusLayout->setContentsMargins(0,0,0,0);
    statusLayout->setHorizontalSpacing(8);
    statusLayout->setVerticalSpacing(2);
    statusLayout->setAlignment(Qt::AlignTop);

    const int EDIT_SMALL_WIDTH = 65;
    const int EDIT_LARGE_WIDTH = 85;
    const int ROW_HEIGHT = 22;
    const int LABEL_WIDTH = 70;

    // 帧长/校验结果
    QLabel *lblFrameLen = new QLabel("帧长");
    lblFrameLen->setAlignment(Qt::AlignCenter);
    lblFrameLen->setFixedWidth(EDIT_SMALL_WIDTH);
    statusLayout->addWidget(lblFrameLen, 0, 0);

    QLabel *lblCheck = new QLabel("校验结果");
    lblCheck->setAlignment(Qt::AlignCenter);
    lblCheck->setFixedWidth(EDIT_LARGE_WIDTH);
    statusLayout->addWidget(lblCheck, 0, 1);

    StyledLineEdit *e1 = new StyledLineEdit(this);
    e1->setGrayInputMode(false); e1->setText("0");
    e1->setFixedSize(EDIT_SMALL_WIDTH, ROW_HEIGHT);
    e1->setAlignment(Qt::AlignCenter);
    statusLayout->addWidget(e1, 1, 0);
    m_valueMap["测试帧_帧长"] = e1;

    StyledLineEdit *e2 = new StyledLineEdit(this);
    e2->setGrayInputMode(false);
    e2->setFixedSize(EDIT_LARGE_WIDTH, ROW_HEIGHT);
    e2->setAlignment(Qt::AlignCenter);
    statusLayout->addWidget(e2, 1, 1);
    m_valueMap["测试帧_校验"] = e2;

    // 帧类型/工作模式/继电器状态/命令码
    QList<QPair<QString, QString>> items = {
        {"帧类型", "7B"}, {"工作模式", "0"}, {"继电器状态", "0"}, {"命令码", "0"}
    };
    for (int i = 0; i < items.size(); ++i) {
        int row = 2 + i;
        auto &item = items[i];

        StyledLineEdit *edit = new StyledLineEdit(this);
        edit->setText(item.second);
        edit->setFixedSize(EDIT_SMALL_WIDTH, ROW_HEIGHT);
        edit->setAlignment(Qt::AlignCenter);
        edit->setGrayInputMode(item.first != "帧类型");
        statusLayout->addWidget(edit, row, 0);
        m_valueMap[QString("测试帧_%1").arg(item.first)] = edit;

        StyledLineEdit *checkEdit = new StyledLineEdit(this);
        checkEdit->setFixedSize(EDIT_LARGE_WIDTH, ROW_HEIGHT);
        checkEdit->setAlignment(Qt::AlignCenter);
        checkEdit->setGrayInputMode(true);
        statusLayout->addWidget(checkEdit, row, 1);
        m_valueMap[QString("测试帧_%1_校验").arg(item.first)] = checkEdit;

        QLabel *lab = new QLabel(item.first);
        lab->setFixedSize(LABEL_WIDTH, ROW_HEIGHT);
        lab->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        statusLayout->addWidget(lab, row, 2);
    }

    int placeholderStartRow = 2 + items.size();
    for (int i = placeholderStartRow; i < 12; ++i) {
        QWidget *p = new QWidget;
        p->setFixedHeight(ROW_HEIGHT);
        statusLayout->addWidget(p, i, 0, 1, 3);
    }

    combinedLayout->addWidget(statusColumn);
    return widget;
}

// ═══════════════════════════════════════════════════════════════════════════
//  数据映射（A6 + A5 兼容）
// ═══════════════════════════════════════════════════════════════════════════
void CopyFrameDialog::updateData(const STParamInfo& param)
{
    auto setLed = [&](const QString& k, bool on) {
        auto it = m_ledMap.find(k);
        if (it != m_ledMap.end()) it.value()->setOn(on);
    };
    auto setVal = [&](const QString& k, const QString& v) {
        auto it = m_valueMap.find(k);
        if (it != m_valueMap.end()) {
            it.value()->setText(v);
            it.value()->setGrayInputMode(false);
        }
    };

    for (auto it = param.mapParams.cbegin(); it != param.mapParams.cend(); ++it)
    {
        const auto& key = it.key();
        const auto& item = it.value();

        // ── 公共帧头 ──
        if (key == "FrameLength") {
            quint32 v = item.varParaValue.toUInt();
            setVal("帧长", QString::number(v));
            setVal("测试帧_帧长", QString::number(v));
        }
        else if (key == "FrameCount") setVal("帧计数", item.varParaValue.toString());
        else if (key == "FrameType") {
            quint32 r = item.varParaValue.toUInt();
            setVal("帧类型", QString::number(r, 16).toUpper());
            setVal("测试帧_帧类型", QString::number(r, 16).toUpper());
        }
        else if (key == "TimeFlag") setVal("时间标志", item.varParaValue.toString());
        else if (key == "CRC校验") setVal("测试帧_校验", item.varParaValue.toString());

        // ── 火保 ──
        else if (key == "InitiatorProtectionStatus1" || key == "OE_KJ7") {
            quint8 r = item.varParaValue.toUInt();
            setLed("火保K1", r&0x01); setLed("火保K2", r&0x02);
            setLed("火保K3", r&0x04); setLed("火保K4", r&0x08);
        }
        else if (key == "InitiatorProtectionStatus2" || key == "OE_KJ8") {
            quint8 r = item.varParaValue.toUInt();
            setLed("火保1", r&0x01); setLed("火保2", r&0x02);
            setLed("火保3", r&0x04); setLed("火保4", r&0x08);
        }

        // ── 解控 ──
        else if (key == "UncontrolStatus" || key == "OE_KJ9") {
            quint8 r = item.varParaValue.toUInt();
            setLed("电爆继电器", r&0x40);
            setLed("非解控JKt",   !(r&0x20));
            setLed("非解控JKbac", !(r&0x10));
            setLed("解控K1", r&0x01); setLed("解控K2", r&0x02);
            setLed("解控K3", r&0x04); setLed("解控K4", r&0x08);
        }

        // ── 机构 ──
        else if (key == "SolenoidValveStatus1" || key == "OE_KJ1") {
            quint8 r = item.varParaValue.toUInt();
            setLed("机构1连接", r&0x40); setLed("机构1释放主", r&0x01);
            setLed("机构1释放备", r&0x02); setLed("机构1锁定", r&0x04); setLed("机构1供气", r&0x10);
        }
        else if (key == "SolenoidValveStatus2" || key == "OE_KJ2") {
            quint8 r = item.varParaValue.toUInt();
            setLed("转发允释2", r&0x80); setLed("机构2释放主", r&0x01);
            setLed("机构2释放备", r&0x02); setLed("机构2锁定", r&0x04); setLed("机构2供气", r&0x10);
        }
        else if (key == "SolenoidValveStatus3" || key == "OE_KJ4") {
            quint8 r = item.varParaValue.toUInt();
            setLed("机构3连接", r&0x40); setLed("机构3释放主", r&0x01);
            setLed("机构3释放备", r&0x02); setLed("机构3锁定", r&0x04); setLed("机构3供气", r&0x10);
        }
        else if (key == "SolenoidValveStatus4" || key == "OE_KJ5") {
            quint8 r = item.varParaValue.toUInt();
            setLed("机构4连接", r&0x40); setLed("机构4释放主", r&0x01);
            setLed("机构4释放备", r&0x02); setLed("机构4锁定", r&0x04); setLed("机构4供气", r&0x10);
        }

        // ── 电爆 + 火引爆 ──
        else if (key == "InitiatorDetonateRelayStatus" || key == "OE_KJ10") {
            quint8 r = item.varParaValue.toUInt();
            setLed("火引爆1", r&0x01); setLed("火引爆2", r&0x02);
            setLed("火引爆3", r&0x04); setLed("火引爆4", r&0x08);
            setLed("电爆1", r&0x10); setLed("电爆2", r&0x20);
            setLed("电爆3", r&0x40); setLed("电爆4", r&0x80);
        }
        else if (key == "ReleasePermitAndPowerStatus")
            setLed("转发允释1", item.varParaValue.toUInt() & 0x80);

        // ── 电磁阀波形电压 AIN4_t1~AIN7_t12 → 4×12 时间框 ──
        else if (key.startsWith("AIN4_t")) {
            double v = item.varParaValue.toUInt() * 0.01952 / 0.51;
            setVal(QString("t%1_c1").arg(key.mid(6).toInt()), QString::number(v, 'f', 2));
        }
        else if (key.startsWith("AIN5_t")) {
            double v = item.varParaValue.toUInt() * 0.01952 / 0.51;
            setVal(QString("t%1_c2").arg(key.mid(6).toInt()), QString::number(v, 'f', 2));
        }
        else if (key.startsWith("AIN6_t")) {
            double v = item.varParaValue.toUInt() * 0.01952 / 0.51;
            setVal(QString("t%1_c3").arg(key.mid(6).toInt()), QString::number(v, 'f', 2));
        }
        else if (key.startsWith("AIN7_t")) {
            double v = item.varParaValue.toUInt() * 0.01952 / 0.51;
            setVal(QString("t%1_c4").arg(key.mid(6).toInt()), QString::number(v, 'f', 2));
        }

        // ── 电压 ──
        else if (key == "DigitalPowerVoltage") setVal("数字供电", item.varParaValue.toString());
        else if (key == "DrivePowerVoltage1")  setVal("驱动供电1", item.varParaValue.toString());
        else if (key == "DrivePowerVoltage2")  setVal("驱动供电2", item.varParaValue.toString());
        else if (key == "Digital5V1")          setVal("5V1", item.varParaValue.toString());
        else if (key == "Digital5V2")          setVal("5V2", item.varParaValue.toString());
        else if (key == "Digital5V3")          setVal("5V3", item.varParaValue.toString());
        else if (key == "AIN8")  { double v=item.varParaValue.toUInt()*0.75656; setVal("数字供电",QString::number(v,'f',2)); }
        else if (key == "AIN9") { double v=item.varParaValue.toUInt()*0.75656; setVal("驱动供电1",QString::number(v,'f',2)); }
        else if (key == "AIN9_2"){ double v=item.varParaValue.toUInt()*0.75656; setVal("驱动供电2",QString::number(v,'f',2)); }
        else if (key=="AIN10"||key=="AIN12"||key=="AIN13") {
            double v = item.varParaValue.toUInt() * 0.0293;
            setVal(key=="AIN10"?"5V1":key=="AIN12"?"5V2":"5V3", QString::number(v,'f',2));
        }

        // ── 时间 ──
        else if (key == "InitiatorDetonateTime") setVal("火引爆时间", item.varParaValue.toString());
        else if (key == "InitiatorDetonateRelayCloseTime" || key == "InitiatorRelayCloseTime")
            setVal("继电器关闭时", item.varParaValue.toString());

        // ── 工作模式 ──
        else if (key == "WorkMode") {
            quint8 r = item.varParaValue.toUInt();
            QString m;
            switch (r) {
            case 0xAA: m="测试模式"; break; case 0xBB: m="手动模式"; break;
            case 0xCC: m="自动模式"; break; default: m=QString::number(r,16); break;
            }
            setVal("测试帧_工作模式", m);
        }

        // ── 命令码 ──
        else if (key == "CommandCodeHigh")
            setVal("测试帧_命令码", item.varParaValue.toString());
        else if (key == "CommandCode")
            setVal("测试帧_命令码", QString::number(item.varParaValue.toUInt(), 16).toUpper());

        // ── FPGAID → 继电器 ──
        else if (key == "FPGAID") {
            quint8 r = item.varParaValue.toUInt(); QString id;
            switch(r){case 0xAA:id="FPGA1";break;case 0xBB:id="FPGA2";break;case 0xCC:id="FPGA3";break;default:id=QString::number(r,16);}
            setVal("测试帧_继电器状态", id);
        }
        else if (key == "SolenoidValveRelayPath")
            setVal("测试帧_继电器状态", item.varParaValue.toString());
    }
}
