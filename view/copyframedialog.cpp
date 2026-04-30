#include "copyframedialog.h"
#include "styledledlabel.h"
#include "styledlineedit.h"

#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLabel>
#include <QFrame>

// ── helper: 创建LED行 ────────────────────────────────────────────────────
static QWidget* makeLedRow(CopyFrameDialog* dlg, const QString& text,
                           QMap<QString, StyledLedLabel*>& ledMap)
{
    auto *row = new QWidget;
    auto *h = new QHBoxLayout(row);
    h->setContentsMargins(0, 0, 0, 0);
    h->setSpacing(4);
    auto *led = new StyledLedLabel(dlg);
    led->setOn(false);
    led->setSwitchable(false);
    led->setDisabledLed(false);
    led->setFixedSize(16, 16);
    h->addWidget(led, 0, Qt::AlignCenter);
    auto *lbl = new QLabel(text);
    lbl->setFixedWidth(65);
    h->addWidget(lbl);
    h->addStretch();
    ledMap[text] = led;
    return row;
}

// ── helper: 创建数值行 ───────────────────────────────────────────────────
static QWidget* makeValueRow(CopyFrameDialog* dlg, const QString& label,
                             QMap<QString, StyledLineEdit*>& valMap,
                             bool gray = true, const QString& def = "--")
{
    auto *row = new QWidget;
    auto *h = new QHBoxLayout(row);
    h->setContentsMargins(0, 0, 0, 0);
    h->setSpacing(4);
    auto *lbl = new QLabel(label);
    lbl->setFixedWidth(55);
    lbl->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    h->addWidget(lbl);
    auto *edit = new StyledLineEdit(dlg);
    edit->setFixedSize(60, 22);
    edit->setAlignment(Qt::AlignCenter);
    edit->setText(def);
    edit->setGrayInputMode(gray);
    h->addWidget(edit);
    h->addStretch();
    valMap[label] = edit;
    return row;
}

// ═══════════════════════════════════════════════════════════════════════════
CopyFrameDialog::CopyFrameDialog(QWidget *parent) : QWidget(parent) { setupUi(); }
CopyFrameDialog::~CopyFrameDialog() {}

void CopyFrameDialog::setParam(const STParamInfo &param)
{
    m_param = param;
    updateData(param);
}

// ═══════════════════════════════════════════════════════════════════════════
//  UI
// ═══════════════════════════════════════════════════════════════════════════

void CopyFrameDialog::setupUi()
{
    this->setWindowTitle("控制器测试帧解析结果");
    this->setMinimumSize(1100, 520);

    auto *root = new QHBoxLayout(this);
    root->setSpacing(8);
    root->setContentsMargins(8, 8, 8, 8);

    auto *c1 = createColumn1(); c1->setMinimumWidth(90);
    auto *c2 = createColumn2(); c2->setMinimumWidth(100);
    auto *c3 = createColumn3(); c3->setMinimumWidth(100);
    auto *c4 = createColumn4(); c4->setMinimumWidth(140);
    auto *c5 = createCombinedColumn(); c5->setMinimumWidth(380);

    root->addWidget(c1);
    root->addWidget(c2);
    root->addWidget(c3);
    root->addWidget(c4);
    root->addWidget(c5);
}

// ── 第1列：火保/解控 ─────────────────────────────────────────────────────
QWidget* CopyFrameDialog::createColumn1()
{
    auto *w = new QWidget;
    auto *v = new QVBoxLayout(w);
    v->setSpacing(3); v->setContentsMargins(2, 2, 2, 2);

    QStringList items = {
        "火保K1","火保K2","火保K3","火保K4",
        "火保1","火保2","火保3","火保4",
        "电爆继电器","非解控JKt","非解控JKbac",
        "解控K1","解控K2","解控K3","解控K4",
        "转发允释1"
    };
    for (auto& s : items) v->addWidget(makeLedRow(this, s, m_ledMap));
    v->addStretch();
    return w;
}

// ── 第2列：机构1/2/3 ─────────────────────────────────────────────────────
QWidget* CopyFrameDialog::createColumn2()
{
    auto *w = new QWidget;
    auto *v = new QVBoxLayout(w);
    v->setSpacing(3); v->setContentsMargins(2, 2, 2, 2);

    QStringList items = {
        "机构1连接","机构1供气","机构1锁定","机构1释放主","机构1释放备",
        "转发允释2",
        "机构2连接","机构2供气","机构2锁定","机构2释放主","机构2释放备",
        "机构3连接","机构3供气","机构3锁定","机构3释放主"
    };
    for (auto& s : items) v->addWidget(makeLedRow(this, s, m_ledMap));
    v->addStretch();
    return w;
}

// ── 第3列：机构4/电爆/火引爆 ─────────────────────────────────────────────
QWidget* CopyFrameDialog::createColumn3()
{
    auto *w = new QWidget;
    auto *v = new QVBoxLayout(w);
    v->setSpacing(3); v->setContentsMargins(2, 2, 2, 2);

    QStringList items = {
        "机构3释放备",
        "机构4连接","机构4供气","机构4锁定","机构4释放主","机构4释放备",
        "电爆1","电爆2","电爆3","电爆4",
        "火引爆1","火引爆2","火引爆3","火引爆4"
    };
    for (auto& s : items) v->addWidget(makeLedRow(this, s, m_ledMap));
    v->addStretch();
    return w;
}

// ── 第4列：帧参数 ────────────────────────────────────────────────────────
QWidget* CopyFrameDialog::createColumn4()
{
    auto *w = new QWidget;
    auto *v = new QVBoxLayout(w);
    v->setSpacing(3); v->setContentsMargins(2, 2, 2, 2);

    QStringList labels = {
        "帧长","帧计数","帧类型","时间标志",
        "火引爆时间","继电器关闭时","数字供电",
        "驱动供电1","驱动供电2","5V1","5V2","5V3"
    };
    for (auto& s : labels) v->addWidget(makeValueRow(this, s, m_valueMap));
    v->addStretch();
    return w;
}

// ── 第5列：时序 + 校验 ───────────────────────────────────────────────────
QWidget* CopyFrameDialog::createCombinedColumn()
{
    auto *w = new QWidget;
    auto *hbox = new QHBoxLayout(w);
    hbox->setSpacing(6); hbox->setContentsMargins(2, 2, 2, 2);

    // 左侧：4×12 时间输入框
    auto *timeCol = new QWidget;
    auto *tv = new QVBoxLayout(timeCol);
    tv->setSpacing(3); tv->setContentsMargins(0,0,0,0);
    for (int i = 0; i < 12; ++i) {
        auto *row = new QWidget;
        auto *h = new QHBoxLayout(row);
        h->setContentsMargins(0,0,0,0); h->setSpacing(3);
        for (int j = 0; j < 4; ++j) {
            auto *e = new StyledLineEdit(this);
            e->setFixedSize(38, 22); e->setAlignment(Qt::AlignCenter);
            QString key = QString("t%1_c%2").arg(i+1).arg(j+1);
            m_valueMap[key] = e;
            h->addWidget(e);
        }
        tv->addWidget(row);
    }
    hbox->addWidget(timeCol);

    // 中间：机构1电压标签
    auto *voltCol = new QWidget;
    auto *vv = new QVBoxLayout(voltCol);
    vv->setSpacing(3); vv->setContentsMargins(0,0,0,0);
    for (int i = 1; i <= 12; ++i) {
        auto *lbl = new QLabel(QString("机构1电压%1").arg(i));
        lbl->setFixedSize(80, 22);
        lbl->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        vv->addWidget(lbl);
    }
    hbox->addWidget(voltCol);

    // 右侧：帧长/校验/帧类型/工作模式/命令码
    auto *rightCol = new QWidget;
    auto *grid = new QGridLayout(rightCol);
    grid->setSpacing(3); grid->setContentsMargins(4,0,4,0);
    grid->setAlignment(Qt::AlignTop);

    // 帧长 + 校验结果（第0行标签，第1行输入框）
    auto addRow = [&](int row, const QString& label, const QString& key,
                      const QString& def = "0", bool gray = false) {
        auto *k = new QLabel(label);
        k->setAlignment(Qt::AlignCenter);
        auto *v = new StyledLineEdit(this);
        v->setFixedSize(50, 22); v->setAlignment(Qt::AlignCenter);
        v->setText(def); v->setGrayInputMode(gray);
        m_valueMap[key] = v;
        grid->addWidget(k, row, 0);
        grid->addWidget(v, row, 1);
    };

    addRow(0, "帧长",   "测试帧_帧长");
    addRow(1, "校验",   "测试帧_校验", "", true);

    // 分隔横线
    auto *line = new QFrame;
    line->setFrameShape(QFrame::HLine);
    grid->addWidget(line, 2, 0, 1, 2);

    addRow(3, "帧类型", "测试帧_帧类型", "7B");
    addRow(4, "工作模式", "测试帧_工作模式", "", true);
    addRow(5, "继电器状态","测试帧_继电器", "", true);
    addRow(6, "命令码", "测试帧_命令码", "", true);

    hbox->addWidget(rightCol);
    return w;
}

// ═══════════════════════════════════════════════════════════════════════════
//  数据映射
// ═══════════════════════════════════════════════════════════════════════════
void CopyFrameDialog::updateData(const STParamInfo& param)
{
    auto setLed = [&](const QString& key, bool on) {
        auto it = m_ledMap.find(key);
        if (it != m_ledMap.end()) it.value()->setOn(on);
    };
    auto setVal = [&](const QString& key, const QString& val) {
        auto it = m_valueMap.find(key);
        if (it != m_valueMap.end()) {
            it.value()->setText(val);
            it.value()->setGrayInputMode(false);
        }
    };

    for (auto it = param.mapParams.cbegin(); it != param.mapParams.cend(); ++it)
    {
        const auto& key = it.key();
        const auto& item = it.value();

        // ── 公共帧头 ──
        if (key == "FrameLength") {
            auto v = item.varParaValue.toUInt();
            setVal("帧长", QString::number(v));
            setVal("测试帧_帧长", QString::number(v));
        }
        else if (key == "FrameCount") {
            setVal("帧计数", item.varParaValue.toString());
        }
        else if (key == "FrameType") {
            auto raw = item.varParaValue.toUInt();
            setVal("帧类型", QString::number(raw, 16).toUpper());
            setVal("测试帧_帧类型", QString::number(raw, 16).toUpper());
        }
        else if (key == "TimeFlag") {
            setVal("时间标志", item.varParaValue.toString());
        }

        // ── 火保 ──
        else if (key == "InitiatorProtectionStatus1") {
            quint8 raw = item.varParaValue.toUInt();
            setLed("火保K1", raw & 0x01);
            setLed("火保K2", raw & 0x02);
            setLed("火保K3", raw & 0x04);
            setLed("火保K4", raw & 0x08);
        }
        else if (key == "InitiatorProtectionStatus2") {
            quint8 raw = item.varParaValue.toUInt();
            setLed("火保1", raw & 0x01);
            setLed("火保2", raw & 0x02);
            setLed("火保3", raw & 0x04);
            setLed("火保4", raw & 0x08);
        }

        // ── 解控 ──
        else if (key == "UncontrolStatus") {
            quint8 raw = item.varParaValue.toUInt();
            setLed("电爆继电器", raw & 0x40);
            setLed("非解控JKt",   !(raw & 0x20));
            setLed("非解控JKbac", !(raw & 0x10));
            setLed("解控K1", raw & 0x01);
            setLed("解控K2", raw & 0x02);
            setLed("解控K3", raw & 0x04);
            setLed("解控K4", raw & 0x08);
        }

        // ── 机构状态（A5） ──
        else if (key == "SolenoidValveStatus1") {
            quint8 raw = item.varParaValue.toUInt();
            setLed("机构1连接",   raw & 0x40);
            setLed("机构1释放主", raw & 0x01);
            setLed("机构1释放备", raw & 0x02);
            setLed("机构1锁定",   raw & 0x04);
            setLed("机构1供气",   raw & 0x10);
        }
        else if (key == "SolenoidValveStatus2") {
            quint8 raw = item.varParaValue.toUInt();
            setLed("转发允释2",   raw & 0x80);
            setLed("机构2释放主", raw & 0x01);
            setLed("机构2释放备", raw & 0x02);
            setLed("机构2锁定",   raw & 0x04);
            setLed("机构2供气",   raw & 0x10);
        }
        else if (key == "SolenoidValveStatus3") {
            quint8 raw = item.varParaValue.toUInt();
            setLed("机构3连接",   raw & 0x40);
            setLed("机构3释放主", raw & 0x01);
            setLed("机构3释放备", raw & 0x02);
            setLed("机构3锁定",   raw & 0x04);
            setLed("机构3供气",   raw & 0x10);
        }
        else if (key == "SolenoidValveStatus4") {
            quint8 raw = item.varParaValue.toUInt();
            setLed("机构4连接",   raw & 0x40);
            setLed("机构4释放主", raw & 0x01);
            setLed("机构4释放备", raw & 0x02);
            setLed("机构4锁定",   raw & 0x04);
            setLed("机构4供气",   raw & 0x10);
        }

        // ── 电爆 + 火引爆 ──
        else if (key == "InitiatorDetonateRelayStatus") {
            quint8 raw = item.varParaValue.toUInt();
            setLed("火引爆1", raw & 0x01);
            setLed("火引爆2", raw & 0x02);
            setLed("火引爆3", raw & 0x04);
            setLed("火引爆4", raw & 0x08);
        }
        else if (key == "ReleasePermitAndPowerStatus") {
            quint8 raw = item.varParaValue.toUInt();
            setLed("转发允释1", raw & 0x80);
        }

        // ── 电压/温度 ──
        else if (key == "DigitalPowerVoltage") {
            setVal("数字供电", item.varParaValue.toString());
        }
        else if (key == "DrivePowerVoltage1") {
            setVal("驱动供电1", item.varParaValue.toString());
        }
        else if (key == "DrivePowerVoltage2") {
            setVal("驱动供电2", item.varParaValue.toString());
        }
        else if (key == "Digital5V1") {
            setVal("5V1", item.varParaValue.toString());
        }
        else if (key == "Digital5V2") {
            setVal("5V2", item.varParaValue.toString());
        }
        else if (key == "Digital5V3") {
            setVal("5V3", item.varParaValue.toString());
        }

        // ── 时间 ──
        else if (key == "InitiatorDetonateTime") {
            setVal("火引爆时间", item.varParaValue.toString());
        }
        else if (key == "InitiatorDetonateRelayCloseTime") {
            setVal("继电器关闭时", item.varParaValue.toString());
        }

        // ── 工作模式 ──
        else if (key == "WorkMode") {
            quint8 raw = item.varParaValue.toUInt();
            QString mode;
            switch (raw) {
            case 0xAA: mode = "测试模式"; break;
            case 0xBB: mode = "手动模式"; break;
            case 0xCC: mode = "自动模式"; break;
            default:   mode = QString::number(raw, 16); break;
            }
            setVal("测试帧_工作模式", mode);
        }

        // ── 命令码（A5：两个独立字节 / A6：UInt16） ──
        else if (key == "CommandCodeHigh") {
            setVal("测试帧_命令码", item.varParaValue.toString());
        }
        else if (key == "CommandCode") {
            quint16 raw = item.varParaValue.toUInt();
            setVal("测试帧_命令码", QString::number(raw, 16).toUpper());
        }

        // ── FPGAID ──
        else if (key == "FPGAID") {
            quint8 raw = item.varParaValue.toUInt();
            QString id;
            switch (raw) {
            case 0xAA: id = "FPGA1"; break;
            case 0xBB: id = "FPGA2"; break;
            case 0xCC: id = "FPGA3"; break;
            default:   id = QString::number(raw, 16); break;
            }
            setVal("测试帧_继电器", id);
        }

        // ── SolenoidValveRelayPath (A6) ──
        else if (key == "SolenoidValveRelayPath") {
            setVal("测试帧_继电器", item.varParaValue.toString());
        }

        // ── 机构1/2释放状态 ──
        else if (key == "Mechanism1_2ReleaseStatus") {
            quint8 raw = item.varParaValue.toUInt();
            // bit解析根据A5协议定义
        }
        else if (key == "Mechanism3_4ReleaseStatus") {
            quint8 raw = item.varParaValue.toUInt();
        }

        // ── 解锁/释放/火引爆时序（5机构 × 5时间）──
        else if (key.startsWith("Mechanism") && key.endsWith("UnlockTime")) {
            // 已经在 ControllerPanel 中详细处理，此处省略避免膨胀
        }
    }
}
