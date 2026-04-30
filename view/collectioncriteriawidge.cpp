#include "collectioncriteriawidge.h"

#include <QFormLayout>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLabel>
#include <QFont>

// 构造函数
// 注意：初始化列表顺序必须与 .h 文件中声明的顺序完全一致！
CollectionCriteriaWidget::CollectionCriteriaWidget(QWidget *parent)
    : QWidget(parent),
      m_editTensionUpper(nullptr),
      m_editTensionLower(nullptr),
      m_editAngleUpper(nullptr),
      m_editAngleLower(nullptr),
      m_editPressureUpper(nullptr),
      m_editPressureLower(nullptr)
{
    setupUI();
}

void CollectionCriteriaWidget::setupUI()
{
    // 1. 主垂直布局
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(20, 20, 20, 20);

    // 2. 创建顶部表头 (用于对齐输入框)
    QHBoxLayout *headerLayout = new QHBoxLayout();
    headerLayout->setSpacing(20);
    headerLayout->addSpacing(80); // 左侧留空，对应标签宽度

    QLabel *lblUpper = new QLabel("上限");
    QLabel *lblLower = new QLabel("下限");

    QFont font = lblUpper->font();
    font.setBold(true);
    lblUpper->setFont(font);
    lblLower->setFont(font);

    lblUpper->setFixedWidth(100);
    lblUpper->setAlignment(Qt::AlignCenter);
    lblLower->setFixedWidth(100);
    lblLower->setAlignment(Qt::AlignCenter);

    headerLayout->addWidget(lblUpper);
    headerLayout->addWidget(lblLower);
    headerLayout->addStretch();

    // 3. 创建表单布局
    QFormLayout *formLayout = new QFormLayout();
    formLayout->setHorizontalSpacing(20);
    formLayout->setVerticalSpacing(15);
    formLayout->setLabelAlignment(Qt::AlignLeft);
    formLayout->setFormAlignment(Qt::AlignTop);

    // --- 辅助 Lambda：创建黑底绿字输入框 ---
    // 使用局部变量创建，赋值给成员指针，避免内存泄漏
    auto createValueBox = [this](const QString &text, QLineEdit *&memberPtr) -> QLineEdit* {
        QLineEdit *edit = new QLineEdit(text);
        edit->setFixedWidth(100);
        edit->setAlignment(Qt::AlignCenter);
        edit->setStyleSheet(
            "QLineEdit {"
            "background-color: #000000;"
            "color: #00FF00;"
            "border: 1px solid #555;"
            "font-weight: bold;"
            "font-family: 'Consolas', 'Courier New', monospace;"
            "}"
        );
        memberPtr = edit; // 将创建的指针赋值给成员变量
        return edit;
    };

    // --- 4. 初始化数据 ---

    // 第一行：拉力
    QHBoxLayout *row1 = new QHBoxLayout();
    row1->addWidget(createValueBox("60.000", m_editTensionUpper));
    row1->addWidget(createValueBox("-3.000", m_editTensionLower));
    row1->addStretch();
    formLayout->addRow(new QLabel("拉力(kN)"), row1);

    // 第二行：角度
    QHBoxLayout *row2 = new QHBoxLayout();
    row2->addWidget(createValueBox("3.000", m_editAngleUpper));
    row2->addWidget(createValueBox("2.000", m_editAngleLower));
    row2->addStretch();
    formLayout->addRow(new QLabel("角度(°)"), row2);

    // 第三行：压力
    QHBoxLayout *row3 = new QHBoxLayout();
    row3->addWidget(createValueBox("1.300", m_editPressureUpper));
    row3->addWidget(createValueBox("-0.600", m_editPressureLower));
    row3->addStretch();
    formLayout->addRow(new QLabel("压力(MPa)"), row3);

    // 5. 组装布局
    mainLayout->addLayout(headerLayout);
    mainLayout->addLayout(formLayout);
    mainLayout->addStretch();
}
