#include "simulateddata.h"
#include <QFont>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include "styledlineedit.h"
#include "styledledlabel.h"

simulateddata::simulateddata(QWidget *parent)
    : QDialog(parent)
{
    // 窗口基础设置
    setWindowTitle("UDPtest - new.vi");
    resize(1200, 700);

    // 主布局
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(5);
    mainLayout->setContentsMargins(5, 5, 5, 5);

    QHBoxLayout *hLayout = new QHBoxLayout;
    hLayout->setSpacing(5);
    hLayout->addWidget(createControllerGroup(), 3);
    hLayout->addWidget(createCollectorGroup(), 2);
    mainLayout->addLayout(hLayout);

    // 全局样式表
    setStyleSheet(R"(
        QDialog { background-color: #c0c0c0; }
        QGroupBox {
            font-size: 11pt; font-weight: bold;
            border: 1px solid #888; border-radius: 4px;
            margin-top: 6px; padding-top: 4px;
            background-color: #c0c0c0;
        }
        QGroupBox::title {
            subcontrol-origin: margin; left:0; right:0;
            text-align: center; padding:0 4px;
        }
        QPushButton {
            font-size: 12pt; font-weight: bold;
            border: 2px solid; border-color: #f0f0f0 #808080 #808080 #f0f0f0;
            border-radius: 4px; padding:4px 10px; background-color:#e5e5e5;
        }
        QPushButton:hover { background-color:#cccccc; }
    )");
}

StyledLedLabel* simulateddata::createIndicatorLabel()
{
    StyledLedLabel *lightLabel = new StyledLedLabel;
    lightLabel->setFixedSize(16, 16);
    lightLabel->setAlignment(Qt::AlignCenter);
    lightLabel->setOn(false);
    lightLabel->setSwitchable(true);
    return lightLabel;
}

QWidget* simulateddata::createIndicatorLabelRow(const QString &text)
{
    QWidget *widget = new QWidget;
    QHBoxLayout *layout = new QHBoxLayout(widget);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(1);

    StyledLedLabel *indicator = createIndicatorLabel();
    QLabel *label = new QLabel(text);
    label->setFixedHeight(16);
    label->setFont(QFont("Microsoft YaHei", 9));
    label->setAlignment(Qt::AlignVCenter | Qt::AlignLeft);

    layout->addWidget(indicator);
    layout->addWidget(label);
    layout->addStretch();
    return widget;
}

StyledLineEdit* simulateddata::createLineEdit(const QString &defaultText)
{
    StyledLineEdit *edit = new StyledLineEdit;
    edit->setText(defaultText);
    return edit;
}

QGroupBox* simulateddata::createControllerGroup()
{
    QGroupBox *group = new QGroupBox("控制器发射帧解析结果");
    QVBoxLayout *mainLayout = new QVBoxLayout(group);
    mainLayout->setSpacing(3);
    mainLayout->setContentsMargins(3, 3, 3, 3);

    // 1. 上方指示灯区域
    QGridLayout *indicatorGrid = new QGridLayout;
    indicatorGrid->setVerticalSpacing(0);
    indicatorGrid->setHorizontalSpacing(2);
    indicatorGrid->setContentsMargins(0, 10, 0, 0);

    QStringList col1 = {
        "火保K1", "火保K2", "火保K3", "火保K4",
        "火保1", "火保2", "火保3", "火保4",
        "解控JKt", "解控JKbac", "解控K1", "解控K2",
        "解控K3", "解控K4"
    };
    for (int i = 0; i < col1.size(); ++i) {
        indicatorGrid->addWidget(createIndicatorLabelRow(col1[i]), i, 0);
    }

    QStringList col2 = {
        "XF01D连接", "XF01E连接", "机构1连接", "机构1释放主",
        "机构1释放备", "机构1锁定", "机构1供气", "SFH1_7.1_1",
        "SFH1_7.2_1", "SFH1_7.3_1", "机构1释放好", "火引爆1",
        "5VK打开"
    };
    for (int i = 0; i < col2.size(); ++i) {
        indicatorGrid->addWidget(createIndicatorLabelRow(col2[i]), i, 1);
    }

    QStringList col3 = {
        "地面允择", "箭上允择", "机构2连接", "机构2释放主",
        "机构2释放备", "机构2锁定", "机构2供气", "SFH2_7.1",
        "SFH2_7.2", "SFH2_7.3", "机构2释放好", "火引爆2"
    };
    for (int i = 0; i < col3.size(); ++i) {
        indicatorGrid->addWidget(createIndicatorLabelRow(col3[i]), i, 2);
    }

    QStringList col4 = {
        "转发允择1", "转发允择2", "机构3连接", "机构3释放主",
        "机构3释放备", "机构3锁定", "机构3供气", "SFH3_7.1",
        "SFH3_7.2", "SFH3_7.3", "机构3释放好", "火引爆3"
    };
    for (int i = 0; i < col4.size(); ++i) {
        indicatorGrid->addWidget(createIndicatorLabelRow(col4[i]), i, 3);
    }

    QStringList col5 = {
        "开锁", "牵制释放好", "机构4连接", "机构4释放主",
        "机构4释放备", "机构4锁定", "机构4供气", "SFH4_7.1",
        "SFH4_7.2", "SFH4_7.3", "机构4释放好", "火引爆4"
    };
    for (int i = 0; i < col5.size(); ++i) {
        indicatorGrid->addWidget(createIndicatorLabelRow(col5[i]), i, 4);
    }

    mainLayout->addLayout(indicatorGrid);

    // 2. 时间表格 + 参数列表
    QHBoxLayout *bottomHLayout = new QHBoxLayout;
    bottomHLayout->setSpacing(10);
    bottomHLayout->setContentsMargins(0, 5, 0, 0);

    // 2.1 时间参数表格
    QGridLayout *timeLayout = new QGridLayout;
    timeLayout->setSpacing(2);
    timeLayout->setContentsMargins(0, 0, 0, 0);

    QList<QPair<QStringList, QString>> timeRows = {
        {{"1", "1.1", "1.2", "1.3"}, "解锁时间"},
        {{"2", "2.1", "2.2", "2.3"}, "解锁到位时间"},
        {{"3", "3.1", "3.2", "3.3"}, "释放好时间"},
        {{"4", "4.1", "4.2", "4.3"}, "释放到位时间"},
        {{"5", "5.1", "5.2", "5.3"}, "火引爆时间"}
    };

    for (int row = 0; row < timeRows.size(); ++row) {
        auto &rowData = timeRows[row];
        for (int col = 0; col < 4; ++col) {
            timeLayout->addWidget(createLineEdit(rowData.first[col]), row, col);
        }
        QLabel *label = new QLabel(rowData.second);
        label->setFixedHeight(22);
        label->setFont(QFont("Microsoft YaHei", 9));
        label->setAlignment(Qt::AlignVCenter | Qt::AlignLeft);
        timeLayout->addWidget(label, row, 4);
    }
    bottomHLayout->addLayout(timeLayout, 2);

    // 参数列表 标签右置
    QVBoxLayout *paramLayout = new QVBoxLayout;
    paramLayout->setSpacing(2);
    paramLayout->setContentsMargins(0, 0, 0, 0);

    QStringList paramNames = {
        "帧长", "帧计数", "帧类型", "时间标志", "数字供电",
        "驱动供电1", "驱动供电1", "5V1", "5V2", "5V3",
        "数字板温度", "火引爆装订时间", "继电器关闭时间"
    };

    for (const QString &name : paramNames) {
        QWidget *rowWidget = new QWidget;
        QHBoxLayout *rowLayout = new QHBoxLayout(rowWidget);
        rowLayout->setContentsMargins(0,0,0,0);
        rowLayout->setSpacing(4);

        StyledLineEdit *edit = createLineEdit("0");
        QLabel *label = new QLabel(name);
        label->setFont(QFont("Microsoft YaHei", 9));
        label->setAlignment(Qt::AlignVCenter | Qt::AlignLeft);

        rowLayout->addWidget(edit);
        rowLayout->addWidget(label);
        rowLayout->addStretch();

        paramLayout->addWidget(rowWidget);
    }
    bottomHLayout->addLayout(paramLayout, 1);
    mainLayout->addLayout(bottomHLayout);

    // 退出按钮
    QHBoxLayout *btnLayout = new QHBoxLayout;
    btnLayout->addStretch();
    m_btnExit = new QPushButton("退出");
    m_btnExit->setFixedSize(90, 40);
    connect(m_btnExit, &QPushButton::clicked, this, &QDialog::accept);
    btnLayout->addWidget(m_btnExit);
    mainLayout->addLayout(btnLayout);

    return group;
}

// 右侧采集器分组 精准修复对齐问题
QGroupBox* simulateddata::createCollectorGroup()
{
    QGroupBox *group = new QGroupBox("采集器汇总解析结果");
    QVBoxLayout *mainLayout = new QVBoxLayout(group);
    mainLayout->setSpacing(3);
    mainLayout->setContentsMargins(3, 8, 3, 3);

    QGridLayout *gridLayout = new QGridLayout;
    gridLayout->setSpacing(2);       // 控件间距
    gridLayout->setContentsMargins(0, 0, 0, 0);
    gridLayout->setVerticalSpacing(0); // 行间距紧凑

    QList<QPair<QPair<QString, QStringList>, QString>> mainRows = {
        {{"SQ1.1-1", {"0", "0", "0", "0"}}, "帧计数"},
        {{"SQ2.1-1", {"1", "1.1", "1.2", "1.3"}}, "拉力1"},
        {{"SQ3.1-1", {"2", "2.1", "2.2", "2.3"}}, "拉力2"},
        {{"SQ5.1-1", {"3", "3.1", "3.2", "3.3"}}, "角度"},
        {{"SQ6.1-1", {"4", "4.1", "4.2", "4.3"}}, "压力1"},
        {{"SQ1.2-1", {"5", "5.1", "5.2", "5.3"}}, "压力2"},
        {{"SQ2.2-1", {"6", "6.1", "6.2", "6.3"}}, "温度1"},
        {{"SQ3.2-1", {"7", "7.1", "7.2", "7.3"}}, "温度2"},
        {{"SQ5.2-1", {"0", "0", "0", "0"}}, "28V"},
        {{"SQ6.2-1", {"0", "0", "0", "0"}}, "24V"},
        {{"校验", {"0", "0", "0", "0"}}, "15V"}
    };

    // ✅ 修复1：表头 1/2/3/4 直接放在 LED 列的正上方 (列 0-3)，下间距=0
    for (int col = 0; col < 4; ++col) {
        QLabel *label = new QLabel(QString::number(col + 1));
        label->setAlignment(Qt::AlignCenter);
        label->setFont(QFont("Microsoft YaHei", 9, QFont::Bold));
        // 核心：强制表头标签 下外边距、下内边距 全部为0
        label->setStyleSheet("margin-bottom:0px; padding-bottom:0px; border:none;");
        gridLayout->addWidget(label, 0, col); // 行0，列0-3
    }
    // 采集器标识行
    QLabel *labelCollectorId = new QLabel("采集器标识");
    labelCollectorId->setFont(QFont("Microsoft YaHei", 9));
    gridLayout->addWidget(labelCollectorId, 1, 9); // 参数名列
    for (int col = 0; col < 4; ++col) {
        gridLayout->addWidget(createLineEdit(QString::number(col+1)), 1, 5+col); // 输入框列 5-8
    }

    int currentRow = 2;
    for (auto &rowData : mainRows) {
        // ✅ 修复2：LED 指示灯填充满 0-3 列，与表头对齐
        for (int col = 0; col < 4; ++col) {
            gridLayout->addWidget(createIndicatorLabel(), currentRow, col);
        }
        // SQ 标签在第 4 列
        QLabel *sqLabel = new QLabel(rowData.first.first);
        sqLabel->setFont(QFont("Microsoft YaHei", 9));
        gridLayout->addWidget(sqLabel, currentRow, 4);
        // 数值输入框在 5-8 列
        for (int col = 0; col < 4; ++col) {
            gridLayout->addWidget(createLineEdit(rowData.first.second[col]), currentRow, 5+col);
        }
        // 参数名称在第 9 列
        QLabel *paramLabel = new QLabel(rowData.second);
        paramLabel->setFont(QFont("Microsoft YaHei", 9));
        gridLayout->addWidget(paramLabel, currentRow, 9);
        currentRow++;
    }

    // 电压参数行
    QStringList voltageNames = {
         "-15V", "5V", "3.3V", "1.8V", "1.0V", "热敏电阻", "FPGA温度"
    };
    for (auto &name : voltageNames) {
        for (int col = 0; col < 4; ++col) {
            gridLayout->addWidget(createLineEdit("0"), currentRow, 5 + col);
        }
        QLabel *label = new QLabel(name);
        label->setFont(QFont("Microsoft YaHei", 9));
        gridLayout->addWidget(label, currentRow, 9);
        currentRow++;
    }

    mainLayout->addLayout(gridLayout);
    mainLayout->addStretch();
    return group;
}
