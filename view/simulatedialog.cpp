#include "simulatedialog.h"
#include <QGridLayout>
#include <QLabel>
#include <QDebug>

SimulateDialog::SimulateDialog(QWidget *parent) : QDialog(parent)
{
setWindowTitle("模拟数据");
resize(1200, 800); // 窗口大小可根据需求调整
setupUI();
}

void SimulateDialog::setupUI()
{
// 主布局：网格布局（适配多行列控件）
QGridLayout *mainLayout = new QGridLayout(this);
mainLayout->setSpacing(10);
mainLayout->setContentsMargins(10, 10, 10, 10);

// ---------- 模拟“控制器发射帧解析结果”区域 ----------
QLabel *ctrlTitle = new QLabel("控制器发射帧解析结果");
ctrlTitle->setAlignment(Qt::AlignCenter);
mainLayout->addWidget(ctrlTitle, 0, 0, 1, 5); // 标题占1行5列

// 指示灯（示例：前3个为墨绿，后2个为绿色，可点击切换）
QStringList lightNames = {"火保K1", "火保K2", "火保K3", "机构1连接", "机构2连接"};
for (int i = 0; i < lightNames.size(); ++i) {
// 指示灯（QLabel模拟）
QLabel *light = new QLabel;
light->setFixedSize(20, 20);
// 初始状态：前3个墨绿，后2个绿色
QString color = (i < 3) ? "#006400" : "#00FF00";
light->setStyleSheet(QString("background-color: %1; border-radius: 10px; border: 1px solid gray;")
.arg(color));
light->setProperty("isOn", (i >= 3)); // 记录初始状态（true=绿色，false=墨绿）
connect(light, &QLabel::mousePressEvent, this, &SimulateDialog::onLightClicked); // 绑定点击事件
m_lightLabels.append(light);

// 标签文本
QLabel *nameLabel = new QLabel(lightNames[i]);
nameLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);

// 添加到布局：每行放“指示灯+标签”
int row = 1 + i / 3; // 每行3个控件
int col = (i % 3) * 2; // 每控件占2列（指示灯+标签）
mainLayout->addWidget(light, row, col);
mainLayout->addWidget(nameLabel, row, col + 1);
}

// ---------- 模拟“采集器汇总解析结果”区域（简化示例） ----------
QLabel *collectTitle = new QLabel("采集器汇总解析结果");
collectTitle->setAlignment(Qt::AlignCenter);
mainLayout->addWidget(collectTitle, 5, 0, 1, 5); // 标题占1行5列

// 添加几个指示灯（示例）
for (int i = 0; i < 4; ++i) {
QLabel *light = new QLabel;
light->setFixedSize(20, 20);
light->setStyleSheet("background-color: #00FF00; border-radius: 10px; border: 1px solid gray;");
light->setProperty("isOn", true);
connect(light, &QLabel::mousePressEvent, this, &SimulateDialog::onLightClicked);
m_lightLabels.append(light);

QLabel *nameLabel = new QLabel(QString("采集器%1").arg(i+1));
nameLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);

int row = 6 + i / 2;
int col = (i % 2) * 2;
mainLayout->addWidget(light, row, col);
mainLayout->addWidget(nameLabel, row, col + 1);
}
}

// 处理指示灯点击事件：切换墨绿（#006400）和绿色（#00FF00）
void SimulateDialog::onLightClicked()
{
QLabel *light = qobject_cast<QLabel*>(sender());
if (!light) return;

bool isOn = light->property("isOn").toBool(); // 获取当前状态
isOn = !isOn; // 切换状态
light->setProperty("isOn", isOn);

// 根据状态设置背景色
QString color = isOn ? "#00FF00" : "#006400";
light->setStyleSheet(QString("background-color: %1; border-radius: 10px; border: 1px solid gray;")
.arg(color));
}
