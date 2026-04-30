#include "doublespinwidget.h"

#include <QDoubleSpinBox>
#include <QLabel>
#include <QHBoxLayout>
#include <QVBoxLayout>

DoubleSpinWidget::DoubleSpinWidget(const QString &title, const QString &unit, QWidget *parent)
    : QWidget(parent)
{
    // 1. 创建控件
    m_labelTitle = new QLabel(title);
    m_labelUnit = new QLabel(unit);
    m_spinBox = new QDoubleSpinBox();

    // 2. 设置 SpinBox 属性
    m_spinBox->setButtonSymbols(QAbstractSpinBox::UpDownArrows); // 确保显示箭头
    m_spinBox->setAlignment(Qt::AlignCenter);
    m_spinBox->setKeyboardTracking(false); // 只有按回车或失去焦点才触发信号，防止输入时跳动

    // 3. 设置黑底绿字样式 (关键部分)
    m_spinBox->setStyleSheet(
        "QDoubleSpinBox {"
        "background-color: #000000;"
        "color: #00FF00;"
        "border: 1px solid #555;"
        "padding-right: 15px;" // 给右侧按钮留点空间
        "font-weight: bold;"
        "font-family: 'Consolas', 'Courier New', monospace;"
        "}"
        // 上下按钮的样式
        "QDoubleSpinBox::up-button, QDoubleSpinBox::down-button {"
        "subcontrol-origin: border;"
        "subcontrol-position: top right;" // 强制按钮在右侧
        "width: 15px;"
        "border-left: 1px solid #555;"
        "background-color: #222;"
        "}"
        "QDoubleSpinBox::up-button:hover, QDoubleSpinBox::down-button:hover {"
        "background-color: #444;"
        "}"
        "QDoubleSpinBox::up-arrow {"
        "image: url(:/qt-project.org/styles/commonstyle/images/up-16.png);" // 使用 Qt 内置箭头或自定义
        "width: 8px;"
        "height: 8px;"
        "}"
        "QDoubleSpinBox::down-arrow {"
        "image: url(:/qt-project.org/styles/commonstyle/images/down-16.png);"
        "width: 8px;"
        "height: 8px;"
        "}"
    );

    // 4. 布局管理
    // 整体垂直布局：上边是标题，下边是输入框+单位
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(2); // 标题和输入框的间距

    mainLayout->addWidget(m_labelTitle, 0, Qt::AlignLeft);

    // 输入框和单位的水平布局
    QHBoxLayout *inputLayout = new QHBoxLayout();
    inputLayout->setContentsMargins(0, 0, 0, 0);
    inputLayout->addWidget(m_spinBox);
    inputLayout->addWidget(m_labelUnit);
    // 右侧加一个伸缩，让输入框靠左（如果需要整体居中可以调整）
    inputLayout->addStretch();

    mainLayout->addLayout(inputLayout);
}

double DoubleSpinWidget::value() const {
    return m_spinBox->value();
}

void DoubleSpinWidget::setValue(double val) {
    m_spinBox->setValue(val);
}

void DoubleSpinWidget::setRange(double min, double max) {
    m_spinBox->setRange(min, max);
}

void DoubleSpinWidget::setDecimals(int decimals) {
    m_spinBox->setDecimals(decimals);
}
