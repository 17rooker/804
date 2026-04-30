#include "gassupplywidget.h"
#include "spinslider.h" // 引入自定义控件

#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLabel>

GasSupplyWidget::GasSupplyWidget(QWidget *parent)
    : QWidget(parent)
{
    setupUI();
}

void GasSupplyWidget::setupUI()
{
    // 1. 主布局
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(30, 20, 30, 20);

    // 2. 内容水平布局
    QHBoxLayout *contentLayout = new QHBoxLayout();
    contentLayout->setSpacing(40); // 组间距

    // --- 辅助 Lambda：创建参数组 ---
    // type: 0=MPa (double), 1=A (int)
    auto createGroup = [&](const QString &labelText, double val, double min, double max, int type) -> QWidget* {
        QWidget *container = new QWidget();
        QVBoxLayout *layout = new QVBoxLayout(container);
        layout->setAlignment(Qt::AlignCenter);
        layout->setSpacing(5);

        // 标签
        QLabel *label = new QLabel(labelText);
        label->setAlignment(Qt::AlignCenter);
        label->setStyleSheet("font-size: 12px; color: #333;"); // 灰色标签文字

        // 使用自定义的 SpinSlider
        SpinSlider *spin = new SpinSlider();
        spin->setFixedWidth(160);
        spin->setFixedHeight(25);
        spin->setAlignment(Qt::AlignCenter);
        spin->setRange(min, max);
        spin->setValue(val);

        if (type == 0) {
            // MPa 设置
            spin->setDecimals(2);
            spin->setSingleStep(0.05);
            spin->setSuffix(" MPa"); // 后缀
        } else {
            // A 设置
            spin->setDecimals(0);
            spin->setSingleStep(1);
            spin->setSuffix(" A");
        }

        layout->addWidget(label);
        layout->addWidget(spin);

        return container;
    };

    // 3. 创建四个参数组
    // 参数：(标签, 默认值, 最小值, 最大值, 类型)
    QWidget *group1 = createGroup("自动补气阈值", 0.40, 0.00, 1.00, 0);
    QWidget *group2 = createGroup("自动停气阈值", 0.20, 0.00, 1.00, 0);
    QWidget *group3 = createGroup("供气电流下限", 3, 0, 100, 1);
    QWidget *group4 = createGroup("供气电流上限", 4, 0, 100, 1);

    // 4. 添加到布局
    contentLayout->addStretch();
    contentLayout->addWidget(group1);
    contentLayout->addWidget(group2);
    contentLayout->addWidget(group3);
    contentLayout->addWidget(group4);
    contentLayout->addStretch();

    mainLayout->addLayout(contentLayout);
    mainLayout->addStretch();
}
