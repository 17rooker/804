#include "copyframedialog.h"
#include "styledledlabel.h"
#include "styledlineedit.h"

#include <QApplication>
#include <QWidget>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLabel>
#include <QGroupBox>
#include <QFont>
#include <QSizePolicy>

CopyFrameDialog::CopyFrameDialog(QWidget *parent)
    : QWidget(parent)
{
    setupUi();
}

CopyFrameDialog::~CopyFrameDialog()
{
}

void CopyFrameDialog::setParam(const STParamInfo &param)
{
   m_param=param;
}

void CopyFrameDialog::setupUi()
{
    this->setWindowTitle("控制器测试帧解析结果");
    // 优化样式：更细腻的背景色、统一字体、标签和控件的视觉层次
    this->setStyleSheet(R"(
        QWidget {
            background-color: #E0E0E0;  /* 更柔和的灰色背景 */
            font-family: "Microsoft YaHei", Arial, sans-serif;
            font-size: 12px;
        }
        QLabel {
            color: #333333;  /* 深灰色文字更护眼 */
            font-weight: 500;
        }
        QGroupBox {
            border: none;
            margin: 0;
            padding: 0;
        }
        /* 给输入框添加轻微边框，提升视觉边界 */
        StyledLineEdit {
            border: 1px solid #B0B0B0;
            border-radius: 2px;
        }
    )");
    // 设置窗口最小尺寸，避免拉伸变形
    this->setMinimumSize(1000, 500);

    QWidget *centralWidget = new QWidget(this);
    QHBoxLayout *mainLayout = new QHBoxLayout(centralWidget);
    mainLayout->setSpacing(12);  // 增大列间距，更透气
    mainLayout->setContentsMargins(12, 12, 12, 12);  // 优化外边距

    // 给每列添加统一的最小宽度，保证布局稳定
    QWidget *col1 = createColumn1();
    col1->setMinimumWidth(100);
    QWidget *col2 = createColumn2();
    col2->setMinimumWidth(110);
    QWidget *col3 = createColumn3();
    col3->setMinimumWidth(110);
    QWidget *col4 = createColumn4();
    col4->setMinimumWidth(140);
    QWidget *col5 = createCombinedColumn();
    col5->setMinimumWidth(350);

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

// 第一列：火保/解控等状态灯（优化间距和对齐）
QWidget* CopyFrameDialog::createColumn1()
{
    QWidget *widget = new QWidget;
    QVBoxLayout *layout = new QVBoxLayout(widget);
    layout->setSpacing(4);  // 优化行间距，更舒适
    layout->setContentsMargins(4, 4, 4, 4);  // 列内边距

    // 封装LED+文字创建函数，统一样式
    auto createLedItem = [&](const QString &text) {
        QWidget *rowWidget = new QWidget;
        QHBoxLayout *hLay = new QHBoxLayout(rowWidget);
        hLay->setContentsMargins(0, 1, 0, 1);  // 行内边距，减少拥挤
        hLay->setSpacing(6);  // LED和文字间距优化

        StyledLedLabel *led = new StyledLedLabel(this);
        led->setOn(false);
        led->setSwitchable(false);
        led->setDisabledLed(false);
        led->setFixedSize(20, 20);  // 缩小LED尺寸，更协调
        // LED居中对齐
        hLay->addWidget(led, 0, Qt::AlignCenter | Qt::AlignVCenter);

        QLabel *lbl = new QLabel(text);
        lbl->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        // 文字标签设置固定宽度，保证对齐
        lbl->setFixedWidth(70);
        hLay->addWidget(lbl, 1);

        // 右侧拉伸，防止文字挤压
        hLay->addStretch();
        return rowWidget;
    };

    QStringList items = {
        "火保K1", "火保K2", "火保K3", "火保K4",
        "火保1", "火保2", "火保3", "火保4",
        "电爆继电器", "非解控JKt", "非解控JKbac",
        "解控K1", "解控K2", "解控K3", "解控K4",
        "转发允释1"
    };

    for (const QString &text : items) {
        layout->addWidget(createLedItem(text));
    }

    // 底部拉伸，让内容置顶，布局更整洁
    layout->addStretch();
    return widget;
}

// 第二列：机构1/2/3状态灯（和第一列样式统一）
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
        led->setOn(false);
        led->setSwitchable(false);
        led->setDisabledLed(false);
        led->setFixedSize(20, 20);
        hLay->addWidget(led, 0, Qt::AlignCenter | Qt::AlignVCenter);

        QLabel *lbl = new QLabel(text);
        lbl->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        lbl->setFixedWidth(80);
        hLay->addWidget(lbl, 1);

        hLay->addStretch();
        return rowWidget;
    };

    QStringList items = {
        "机构1连接", "机构1供气", "机构1锁定", "机构1释放主", "机构1释放备",
        "转发允释2",
        "机构2连接", "机构2供气", "机构2锁定", "机构2释放主", "机构2释放备",
        "机构3连接", "机构3供气", "机构3锁定", "机构3释放主"
    };

    for (const QString &text : items) {
        layout->addWidget(createLedItem(text));
    }

    layout->addStretch();
    return widget;
}

// 第三列：机构4/电爆/火引爆状态灯（样式统一）
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
        led->setOn(false);
        led->setSwitchable(false);
        led->setDisabledLed(false);
        led->setFixedSize(20, 20);
        hLay->addWidget(led, 0, Qt::AlignCenter | Qt::AlignVCenter);

        QLabel *lbl = new QLabel(text);
        lbl->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        lbl->setFixedWidth(80);
        hLay->addWidget(lbl, 1);

        hLay->addStretch();
        return rowWidget;
    };

    QStringList items = {
        "机构3释放备",
        "机构4连接", "机构4供气", "机构4锁定", "机构4释放主", "机构4释放备",
        "电爆1", "电爆2", "电爆3", "电爆4",
        "火引爆1", "火引爆2", "火引爆3", "火引爆4"
    };

    for (const QString &text : items) {
        layout->addWidget(createLedItem(text));
    }

    layout->addStretch();
    return widget;
}

// 第四列：帧参数灰色输入框（优化对齐和尺寸）
QWidget* CopyFrameDialog::createColumn4()
{
    QWidget *widget = new QWidget;
    QVBoxLayout *layout = new QVBoxLayout(widget);
    layout->setSpacing(4);
    layout->setContentsMargins(4, 4, 4, 4);

    QStringList labels = {
        "帧长", "帧计数", "帧类型", "时间标志",
        "火引爆时间", "继电器关闭时", "数字供电",
        "驱动供电1", "驱动供电2", "5V1", "5V2", "5V3"
    };

    for (const QString &labelText : labels) {
        QWidget *rowWidget = new QWidget;
        QHBoxLayout *rowLayout = new QHBoxLayout(rowWidget);
        rowLayout->setContentsMargins(0, 1, 0, 1);
        rowLayout->setSpacing(6);

        QLabel *label = new QLabel(labelText);
        // 统一标签宽度，保证右对齐整齐
        label->setFixedWidth(75);
        label->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        rowLayout->addWidget(label);

        StyledLineEdit *editBox = new StyledLineEdit(this);
        editBox->setGrayInputMode(true);
        editBox->setFixedSize(65, 22);  // 增大输入框尺寸，更易操作
        editBox->setAlignment(Qt::AlignCenter);  // 内容居中，更美观
        rowLayout->addWidget(editBox);

        rowLayout->addStretch();
        layout->addWidget(rowWidget);
    }

    layout->addStretch();
    return widget;
}

// 合并列：优化布局层次和视觉效果
QWidget* CopyFrameDialog::createCombinedColumn()
{
    QWidget *widget = new QWidget;
    QHBoxLayout *combinedLayout = new QHBoxLayout(widget);
    combinedLayout->setSpacing(10);  // 子列间距优化
    combinedLayout->setContentsMargins(4, 4, 4, 4);

    // 1. 中间4列黑色输入框（优化行高和间距）
    QWidget *inputBoxColumn = new QWidget;
    QVBoxLayout *inputBoxLayout = new QVBoxLayout(inputBoxColumn);
    inputBoxLayout->setSpacing(4);
    inputBoxLayout->setContentsMargins(0,0,0,0);
    for (int i = 0; i < 12; ++i) {
        QWidget *rowWidget = new QWidget;
        QHBoxLayout *rowLayout = new QHBoxLayout(rowWidget);
        rowLayout->setContentsMargins(0, 1, 0, 1);
        rowLayout->setSpacing(4);  // 输入框之间间距优化

        for (int j = 0; j < 4; ++j) {
            StyledLineEdit *editBox = new StyledLineEdit(this);
            editBox->setGrayInputMode(false);
            editBox->setFixedSize(42, 22);  // 尺寸微调，更协调
            editBox->setAlignment(Qt::AlignCenter);
            rowLayout->addWidget(editBox);
        }
        inputBoxLayout->addWidget(rowWidget);
    }
    combinedLayout->addWidget(inputBoxColumn);

    // 2. 机构1电压标签（优化对齐和尺寸）
    QWidget *labelColumn = new QWidget;
    QVBoxLayout *labelLayout = new QVBoxLayout(labelColumn);
    labelLayout->setSpacing(4);
    labelLayout->setContentsMargins(0,0,0,0);
    for (int i = 1; i <= 12; ++i) {
        QLabel *voltageLabel = new QLabel(QString("机构1电压%1").arg(i));
        voltageLabel->setFixedSize(85, 22);  // 尺寸匹配输入框
        voltageLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        labelLayout->addWidget(voltageLabel);
    }
    combinedLayout->addWidget(labelColumn);

    // 3. 右侧状态控件（优化布局和视觉）
    QWidget *statusColumn = new QWidget;
    QGridLayout *statusLayout = new QGridLayout(statusColumn);
    statusLayout->setSpacing(4);  // 网格间距优化
    statusLayout->setContentsMargins(0,0,0,0);
    statusLayout->setHorizontalSpacing(8);
    statusLayout->setVerticalSpacing(2);
    statusLayout->setAlignment(Qt::AlignTop);

    // 统一参数：更协调的尺寸
    const int EDIT_SMALL_WIDTH = 65;
    const int EDIT_LARGE_WIDTH = 85;
    const int ROW_HEIGHT = 22;
    const int LABEL_WIDTH = 70;

    // --- 第0行：帧长/校验结果 标签 ---
    QLabel *lblFrameLen = new QLabel("帧长");
    lblFrameLen->setAlignment(Qt::AlignCenter);
    lblFrameLen->setFixedWidth(EDIT_SMALL_WIDTH);
    statusLayout->addWidget(lblFrameLen, 0, 0);

    QLabel *lblCheck = new QLabel("校验结果");
    lblCheck->setAlignment(Qt::AlignCenter);
    lblCheck->setFixedWidth(EDIT_LARGE_WIDTH);
    statusLayout->addWidget(lblCheck, 0, 1);

    // --- 第1行：帧长/校验结果 输入框 ---
    StyledLineEdit *e1 = new StyledLineEdit(this);
    e1->setGrayInputMode(false);
    e1->setText("0");
    e1->setFixedSize(EDIT_SMALL_WIDTH, ROW_HEIGHT);
    e1->setAlignment(Qt::AlignCenter);
    statusLayout->addWidget(e1, 1, 0);

    StyledLineEdit *e2 = new StyledLineEdit(this);
    e2->setGrayInputMode(false);
    e2->setFixedSize(EDIT_LARGE_WIDTH, ROW_HEIGHT);
    e2->setAlignment(Qt::AlignCenter);
    statusLayout->addWidget(e2, 1, 1);

    // --- 第2-5行：帧类型/工作模式/继电器状态/命令码 ---
    QList<QPair<QString, QString>> items = {
        {"帧类型", "7B"},
        {"工作模式", "0"},
        {"继电器状态", "0"},
        {"命令码", "0"}
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

        StyledLineEdit *checkEdit = new StyledLineEdit(this);
        checkEdit->setFixedSize(EDIT_LARGE_WIDTH, ROW_HEIGHT);
        checkEdit->setAlignment(Qt::AlignCenter);
        checkEdit->setGrayInputMode(true);
        statusLayout->addWidget(checkEdit, row, 1);

        QLabel *lab = new QLabel(item.first);
        lab->setFixedSize(LABEL_WIDTH, ROW_HEIGHT);
        lab->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        statusLayout->addWidget(lab, row, 2);
    }

    // 占位填充，优化高度匹配
    int placeholderStartRow = 2 + items.size();
    for (int i = placeholderStartRow; i < 12; ++i) {
        QWidget *p = new QWidget;
        p->setFixedHeight(ROW_HEIGHT);
        statusLayout->addWidget(p, i, 0, 1, 3);
    }

    combinedLayout->addWidget(statusColumn);
    return widget;
}
