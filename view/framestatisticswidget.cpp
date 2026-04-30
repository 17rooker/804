#include "framestatisticswidget.h"
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QPushButton>

FrameStatisticsWidget::FrameStatisticsWidget(QWidget *parent)
    : QWidget(parent)
{
    initUI();
    initStats();
}

void FrameStatisticsWidget::initUI()
{
    // ========== 1. 顶部统计表格布局 ==========
    QGridLayout *gridLayout = new QGridLayout();

    // 第一行：帧ID标签 + AA/BB/CC/DE
    QLabel *frameIdLabel = new QLabel("帧ID");
    gridLayout->addWidget(frameIdLabel, 0, 0);

    QMap<FrameType, QString> frameTypeNames = {
        {FrameAA, "AA"},
        {FrameBB, "BB"},
        {FrameCC, "CC"},
        {FrameDE, "DE"}
    };

    int col = 1;
    for (auto type : frameTypeNames.keys()) {
        QLabel *typeLabel = new QLabel(frameTypeNames[type]);
        gridLayout->addWidget(typeLabel, 0, col++);
    }

    // 第二行：帧计数
    QLabel *frameCountLabel = new QLabel("帧计数");
    gridLayout->addWidget(frameCountLabel, 1, 0);
    col = 1;
    for (auto type : frameTypeNames.keys()) {
        QLineEdit *edit = new QLineEdit("0");
        edit->setReadOnly(true); // 只读，禁止手动修改
        edit->setAlignment(Qt::AlignCenter); // 居中显示
        m_frameCountEdits[type] = edit;
        gridLayout->addWidget(edit, 1, col++);
    }

    // 第三行：错帧计数
    QLabel *errorCountLabel = new QLabel("错帧计数");
    gridLayout->addWidget(errorCountLabel, 2, 0);
    col = 1;
    for (auto type : frameTypeNames.keys()) {
        QLineEdit *edit = new QLineEdit("0");
        edit->setReadOnly(true);
        edit->setAlignment(Qt::AlignCenter);
        m_errorCountEdits[type] = edit;
        gridLayout->addWidget(edit, 2, col++);
    }

    // 第四行：漏帧计数
    QLabel *missCountLabel = new QLabel("漏帧计数");
    gridLayout->addWidget(missCountLabel, 3, 0);
    col = 1;
    for (auto type : frameTypeNames.keys()) {
        QLineEdit *edit = new QLineEdit("0");
        edit->setReadOnly(true);
        edit->setAlignment(Qt::AlignCenter);
        m_missCountEdits[type] = edit;
        gridLayout->addWidget(edit, 3, col++);
    }

    // ========== 2. 遥测口原帧区域 ==========
    QWidget *rawFrameWidget = new QWidget();
    QHBoxLayout *rawFrameHLayout = new QHBoxLayout(rawFrameWidget);

    m_rawFrameLabel = new QLabel("遥测口原帧 0");
    QPushButton *clearBtn = new QPushButton();
    // 设置清空按钮图标（Linux下Qt内置图标，也可替换为自定义图标）
    clearBtn->setIcon(QIcon::fromTheme("edit-clear"));
    clearBtn->setFixedSize(24, 24); // 固定按钮大小

    rawFrameHLayout->addWidget(m_rawFrameLabel);
    rawFrameHLayout->addStretch(); // 拉伸空白区域
    rawFrameHLayout->addWidget(clearBtn);

    // 遥测口原帧文本框
    m_rawFrameTextEdit = new QTextEdit();
    m_rawFrameTextEdit->setReadOnly(true); // 只读

    // ========== 3. 整体布局 ==========
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->addLayout(gridLayout);
    mainLayout->addSpacing(10); // 间距
    mainLayout->addWidget(rawFrameWidget);
    mainLayout->addWidget(m_rawFrameTextEdit);

    // 绑定清空按钮事件
    connect(clearBtn, &QPushButton::clicked, this, [=]() {
        m_rawFrameTextEdit->clear();
        m_rawFrameLabel->setText("遥测口原帧 0");
    });

    // 设置整体样式（适配Linux）
    this->setLayout(mainLayout);
    this->setMinimumSize(500, 400); // 设置最小尺寸
}

void FrameStatisticsWidget::initStats()
{
    // 初始化4种帧的统计数据为0
    m_frameStats[FrameAA] = FrameStats();
    m_frameStats[FrameBB] = FrameStats();
    m_frameStats[FrameCC] = FrameStats();
    m_frameStats[FrameDE] = FrameStats();
}

void FrameStatisticsWidget::updateStatsDisplay()
{
    // 更新所有计数输入框的显示
    for (auto type : m_frameStats.keys()) {
        m_frameCountEdits[type]->setText(QString::number(m_frameStats[type].frameCount));
        m_errorCountEdits[type]->setText(QString::number(m_frameStats[type].errorCount));
        m_missCountEdits[type]->setText(QString::number(m_frameStats[type].missCount));
    }
}

void FrameStatisticsWidget::addFrameCount(FrameType type)
{
    m_frameStats[type].frameCount++;
    updateStatsDisplay();
}

void FrameStatisticsWidget::addErrorCount(FrameType type)
{
    m_frameStats[type].errorCount++;
    updateStatsDisplay();
}

void FrameStatisticsWidget::addMissCount(FrameType type)
{
    m_frameStats[type].missCount++;
    updateStatsDisplay();
}

void FrameStatisticsWidget::appendRawFrameText(const QString &text)
{
    m_rawFrameTextEdit->append(text);
    // 更新遥测口原帧计数（统计文本框行数）
    int lineCount = m_rawFrameTextEdit->document()->lineCount();
    m_rawFrameLabel->setText(QString("遥测口原帧 %1").arg(lineCount - 1)); // 减1是因为默认空行
}
