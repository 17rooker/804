#include "remotesettingwidget.h" // 对应的头文件

// --- 必须包含的头文件 ---
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QFileDialog>   // 用于文件对话框
#include <QDir>          // 用于路径处理
#include <QStyle>        // 【关键修复】用于 QStyle::SP_DirIcon
#include <QIntValidator> // 用于端口号输入限制

RemoteSettingWidget::RemoteSettingWidget(QWidget *parent) : QWidget(parent)
{
    setupUI();
}

RemoteSettingWidget::~RemoteSettingWidget()
{
    // 析构函数，Qt会自动清理子控件
}

void RemoteSettingWidget::setupUI()
{
    // --- 1. 主布局 ---
    // 使用垂直布局包裹所有内容，并设置外边距
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(15, 15, 15, 15);
    mainLayout->setSpacing(15);

    // --- 2. 顶部参数设置区域 (Group Box) ---
    QGroupBox *groupParam = new QGroupBox("网络参数设置", this);
    QGridLayout *gridLayout = new QGridLayout(groupParam);
    gridLayout->setHorizontalSpacing(15);
    gridLayout->setVerticalSpacing(10);
    gridLayout->setContentsMargins(15, 20, 15, 15);

    // 创建控件
    QLabel *lblServerIp = new QLabel("服务器IP地址");
    QLabel *lblServerPort = new QLabel("服务器侦听端口");
    QLabel *lblMcastIp = new QLabel("组播地址");
    QLabel *lblMcastPort = new QLabel("组播端口");

    m_editServerIp = new QLineEdit("192.168.0.49");
    m_editServerPort = new QLineEdit("7006");
    m_editMcastIp = new QLineEdit("224.168.0.23");
    m_editMcastPort = new QLineEdit("16023");

    // 端口号限制 (1-65535)
    m_editServerPort->setValidator(new QIntValidator(1, 65535, this));
    m_editMcastPort->setValidator(new QIntValidator(1, 65535, this));

    // 设置固定宽度以保持对齐美观
    m_editServerIp->setFixedWidth(120);
    m_editServerPort->setFixedWidth(120);
    m_editMcastIp->setFixedWidth(120);
    m_editMcastPort->setFixedWidth(120);

    // 将控件添加到网格布局
    gridLayout->addWidget(lblServerIp, 0, 0, Qt::AlignLeft);
    gridLayout->addWidget(m_editServerIp, 0, 1, Qt::AlignLeft);
    gridLayout->addWidget(lblServerPort, 0, 2, Qt::AlignLeft);
    gridLayout->addWidget(m_editServerPort, 0, 3, Qt::AlignLeft);

    gridLayout->addWidget(lblMcastIp, 1, 0, Qt::AlignLeft);
    gridLayout->addWidget(m_editMcastIp, 1, 1, Qt::AlignLeft);
    gridLayout->addWidget(lblMcastPort, 1, 2, Qt::AlignLeft);
    gridLayout->addWidget(m_editMcastPort, 1, 3, Qt::AlignLeft);

    // --- 3. 底部存储路径区域 ---
    QGroupBox *groupPath = new QGroupBox("数据存储路径", this);
    QHBoxLayout *pathLayout = new QHBoxLayout(groupPath);
    pathLayout->setContentsMargins(15, 20, 15, 15);

    m_editDataPath = new QLineEdit("D:/数据"); // 默认路径
    m_btnBrowse = new QPushButton();

    // 设置浏览按钮图标 (使用系统标准文件夹图标)
    m_btnBrowse->setIcon(style()->standardIcon(QStyle::SP_DirIcon));
    m_btnBrowse->setToolTip("选择文件夹");
    m_btnBrowse->setFixedSize(30, 25); // 固定按钮大小

    // 连接信号槽
    connect(m_btnBrowse, &QPushButton::clicked, this, &RemoteSettingWidget::onBrowseFolder);

    // 将路径输入框和按钮加入布局
    pathLayout->addWidget(m_editDataPath);
    pathLayout->addWidget(m_btnBrowse);

    // --- 4. 将所有组件加入主布局 ---
    mainLayout->addWidget(groupParam);
    mainLayout->addWidget(groupPath);
    mainLayout->addStretch(); // 底部留白，将内容顶上去
}

void RemoteSettingWidget::onBrowseFolder()
{
    // 获取当前路径作为起始目录
    QString currentPath = m_editDataPath->text();
    if (currentPath.isEmpty() || !QDir(currentPath).exists()) {
        currentPath = QDir::homePath(); // 如果路径无效，默认到用户主目录
    }

    // 打开文件夹选择对话框
    QString dir = QFileDialog::getExistingDirectory(this, tr("选择数据存储文件夹"),
                                                    currentPath,
                                                    QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);

    // 如果用户选择了有效路径，则更新输入框
    if (!dir.isEmpty()) {
        m_editDataPath->setText(dir);
    }
}
