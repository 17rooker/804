#include "emissiontab.h"
#include "launchframedialog.h" // 包含新窗口的头文件
#include "copyframedialog.h"
#include "framestatisticswidget.h"
#include "dataplaybackdialo.h"
#include "launchprocessdialog.h"

#include "wavechart.h"
#include "bookbindingwgt.h"
#include "view/controller422dialog.h"
#include "view/serial422dialog.h"
#include "controllerpanel.h"
#include "src/CustomMessage/DataInteractionManager.h"
#include "src/Common/LoggerManager.h"
#include "src/StyleEventFilter.h"
#include "InitiativeMsgEvent.h"
#include <QGroupBox>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QLineEdit>
#include <QFrame>
#include <QMessageBox>
#include <QDesktopServices>
#include <QUrl>
#include <QFileInfo>
#include <QDir>

// 修改构造函数：接收422弹窗实例参数 + 控制器名称
EmissionTab::EmissionTab(QWidget *parent,
                         Controller422Dialog *controller422Dialog,
                         Serial422Dialog *serial422Dialog,
                         const QString &controllerName)
    : QDialog(parent)
    , m_launchFrameDialog(nullptr) // 初始化指针
    , m_controller422Dialog(controller422Dialog) // 初始化422指令弹窗实例
    , m_serial422Dialog(serial422Dialog) // 初始化422设置弹窗实例
    , m_controllerName(controllerName) // 初始化控制器名称
{
    setupUI();
}

EmissionTab::~EmissionTab()
{
    // Qt 的父子对象机制会自动删除 m_launchFrameDialog，
    // 但如果是手动 new 且没有 parent，需要手动 delete。
    // 这里我们在 show 时传入了 this 作为 parent，所以可以不手动 delete。
}

void EmissionTab::onMessage(IEvent *pEvent)
{
    if (!pEvent) return;

    switch (pEvent->getType()) {

    case EventType::E_InitiativeMsg: {
        // 设备数据（TCP / UDP / 串口 解析后投递）
        auto* pInit = static_cast<InitiativeMsgEvent*>(pEvent);
        const STParamInfo& param = pInit->getParamData(); // 引用避免拷贝

        // 1. 先过滤控制器和通道ID（保留原有逻辑）
        if(m_controllerName=="控制器1")
        {
            if(param.channelId!="serial_E")
                return;
        }
        else if(m_controllerName=="控制器2")
        {
            if(param.channelId!="serial_F")
                return;
        }
        else
        {
            if(param.channelId!="serial_G")
                return;
        }

        // 2. 核心逻辑：根据eDataType分发参数
        if(param.eDataType == EDataType::E_ServerControl_BrakeRelease_A5)
        {
            // 发给发射帧界面：先检查指针非空
            if (m_launchFrameDialog) {
                m_launchFrameDialog->setParam(param); // 传const引用，高性能
            }
        }
        else
        {
            // 发给测试帧 + 波形图界面：均做非空检查
            if (m_copyFrameDialog) {
                m_copyFrameDialog->setParam(param);
            }
            if (m_waveChart) {
                m_waveChart->setParam(param);
            }
        }
        break;
    }

    case EventType::E_InternalMsg: {
        break;
    }

    default:
        break;
    }
}

void EmissionTab::setControllerPanelDialog(ControllerPanel *dialog)
{
    m_dataPanel=dialog;
}

void EmissionTab::setLaunchProcessDialog(LaunchProcessDialog *dialog)
{
    m_LaunchProcessDialog=dialog;
}

double EmissionTab::calculateCollectionCoeff(int row, int col, double x)
{

    if (m_serial422Dialog) {
        return m_serial422Dialog->calculateCollectionCoeffSingleFormula(row, col, x);
    }

    return 0.0;

}

void EmissionTab::handleCalculateCollectionCoeff(int row, int col, double x)
{  // 调用原有计算逻辑
    double result = calculateCollectionCoeff(row, col, x);
    // 发射结果信号，返回给LaunchFrameDialog
    emit sendCollectionCoeffResult(result);

}
void EmissionTab::setupUI()
{
    // --- 1. 窗口基本属性 ---
    // 根据控制器名称动态设置窗口标题
    auto* handle = DataInteractionManager::getInstance().getMsgHandle();

    // 订阅实时数据
    handle->subMessage(this, ESubDataType::E_RealTimeData);
    setWindowTitle(tr("串口状态显示_%1.vi").arg(m_controllerName));
    resize(1200, 800);

    // 样式表代码
    setStyleSheet(R"(
                  QDialog { background-color: #D6D6D6; font-size: 12px; }
                  QGroupBox { border: 1px solid gray; border-radius: 5px; margin-top: 10px; font-weight: bold; color: black; }
                  QGroupBox::title { subcontrol-origin: margin; left: 10px; padding: 0 3px 0 3px; }
                  QPushButton { background-color: #E0E0E0; border: 1px solid #8f8f91; border-radius: 4px; padding: 4px 8px; }
                  QPushButton:hover { background-color: #F0F0F0; }
                  QPushButton:pressed { background-color: #D0D0D0; }
                  QLabel { color: black; }
                  .LedLabel { background-color: #4CAF50; border: 1px solid #333; border-radius: 3px; qproperty-alignment: AlignCenter; color: white; }
                  )");

    // --- 2. 顶部区域 ---
    QWidget *topWidget = new QWidget(this);
    QHBoxLayout *topLayout = new QHBoxLayout(topWidget);
    topLayout->setContentsMargins(10, 10, 10, 10);

    QLabel *pathLabel = new QLabel(tr("数据文件存储路径"), this);
    m_pathLineEdit = new QLineEdit(this);
    m_pathLineEdit->setText("D:/数据");
    QPushButton *folderBtn = new QPushButton(tr("..."), this);
    folderBtn->setFixedWidth(30);
    connect(folderBtn, &QPushButton::clicked, this, &EmissionTab::onOpenFolder);

    QLabel *queue1Lbl = new QLabel(tr("队列1"), this);
    QLabel *queue1Val = new QLabel("0", this);
    queue1Val->setFrameStyle(QFrame::Panel | QFrame::Sunken);
    queue1Val->setMinimumWidth(40);
    queue1Val->setAlignment(Qt::AlignCenter);

    QLabel *queue2Lbl = new QLabel(tr("队列2"), this);
    QLabel *queue2Val = new QLabel("0", this);
    queue2Val->setFrameStyle(QFrame::Panel | QFrame::Sunken);
    queue2Val->setMinimumWidth(40);
    queue2Val->setAlignment(Qt::AlignCenter);

    QPushButton *btnCmd = new QPushButton(tr("指令"), this);
    QPushButton *btnBind = new QPushButton(tr("装订"), this);
    QPushButton *btnSet = new QPushButton(tr("设置"), this);
    QPushButton *btnClear = new QPushButton(tr("清零"), this);
    QPushButton *btnReplay = new QPushButton(tr("回放"), this);
    QPushButton *btnExit = new QPushButton(tr("退出"), this);
    connect(btnExit, &QPushButton::clicked, this, &QDialog::close);

    // 绑定指令按钮点击事件，复用全局422指令弹窗实例
    connect(btnCmd, &QPushButton::clicked, this, [=]() {
        if (m_controller422Dialog) {
            m_controller422Dialog->exec();
        } else {
            QMessageBox::warning(this, tr("警告"), tr("422指令弹窗实例未初始化！"));
        }
    });

    // 修改回放按钮点击事件，添加设置LaunchFrameDialog指针的逻辑
    connect(btnReplay, &QPushButton::clicked, this, [=]() {
        DataPlaybackDialog* playbackDlg = DataPlaybackDialog::getInstance(this);
        // 新增：将当前EmissionTab的m_launchFrameDialog关联到回放窗口
        playbackDlg->setLaunchFrameDialog(m_launchFrameDialog);
        playbackDlg->setLaunchProcessDialog(m_LaunchProcessDialog);
        playbackDlg->setControllerPanelDialog(m_dataPanel);
        playbackDlg->setCopyFrameDialog(m_copyFrameDialog);

        playbackDlg->show();
        playbackDlg->raise();
        playbackDlg->activateWindow();
    });
    // 绑定设置按钮点击事件，复用全局422设置弹窗实例
    connect(btnSet, &QPushButton::clicked, this, [=]() {
        if (m_serial422Dialog) {
            m_serial422Dialog->exec();
        } else {
            QMessageBox::warning(this, tr("警告"), tr("422设置弹窗实例未初始化！"));
        }
    });

    // 新增：绑定清零按钮事件 - 重置发射帧界面
    connect(btnClear, &QPushButton::clicked, this, [=]() {
        if (m_launchFrameDialog) {
            m_launchFrameDialog->resetUI();
            // 可选：重置队列数值显示
            queue1Val->setText("0");
            queue2Val->setText("0");
            //            QMessageBox::information(this, tr("提示"), tr("已重置发射帧界面至初始状态！"));
        } else {
            QMessageBox::warning(this, tr("警告"), tr("发射帧界面实例未初始化！"));
        }
    });




    bookbindingwgt*bookwgt=new bookbindingwgt(this);

    connect(btnBind, &QPushButton::clicked, this, [=]() {
        if (bookwgt) {
            bookwgt->exec();
        }
    });
    topLayout->addWidget(pathLabel);
    topLayout->addWidget(m_pathLineEdit);
    topLayout->addWidget(folderBtn);
    topLayout->addSpacing(20);
    topLayout->addWidget(queue1Lbl);
    topLayout->addWidget(queue1Val);
    topLayout->addWidget(queue2Lbl);
    topLayout->addWidget(queue2Val);
    topLayout->addStretch();
    topLayout->addWidget(btnCmd);
    topLayout->addWidget(btnBind);
    topLayout->addWidget(btnSet);
    topLayout->addWidget(btnClear);
    topLayout->addWidget(btnReplay);
    topLayout->addWidget(btnExit);

    // --- 3. 中部区域 (TabWidget) ---
    m_tabWidget = new QTabWidget(this);

    // 创建标签页
    FrameStatisticsWidget *tabReceive = new FrameStatisticsWidget(this);
    if (!m_launchFrameDialog) {
        m_launchFrameDialog = new LaunchFrameDialog(this);
    }
    // 新增：将局部变量改为成员变量赋值
    m_copyFrameDialog = new CopyFrameDialog(this);
    m_tabWidget->addTab(tabReceive, tr("接收状态"));
    m_tabWidget->addTab(m_launchFrameDialog, tr("发射帧"));
    m_tabWidget->addTab(m_copyFrameDialog, tr("测试帧"));

    // 新增：将局部变量改为成员变量赋值
    m_waveChart = new wavechart(this);
    m_tabWidget->addTab(m_waveChart, tr("波形图"));
    // --- 5. 主布局组装 ---
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->addWidget(topWidget);
    mainLayout->addWidget(m_tabWidget);
    m_tabWidget->setCurrentIndex(1);
    setLayout(mainLayout);
}

// 打开文件夹实现
void EmissionTab::onOpenFolder()
{
    QString path = m_pathLineEdit->text();
#ifdef Q_OS_LINUX
    if (path.contains("D:")) {
        path = QDir::homePath();
    }
#endif
    QFileInfo fileInfo(path);
    if (fileInfo.isDir()) {
        QUrl url = QUrl::fromLocalFile(fileInfo.absoluteFilePath());
        if (!QDesktopServices::openUrl(url)) {
            QMessageBox::warning(this, tr("错误"), tr("无法打开该文件夹：") + path);
        }
    } else {
        QDir dir(path);
        if (!dir.exists()) {
            QMessageBox::information(this, tr("提示"), tr("路径不存在，将尝试打开父目录或根目录。"));
            QDesktopServices::openUrl(QUrl::fromLocalFile(QDir::rootPath()));
        } else {
            QDesktopServices::openUrl(QUrl::fromLocalFile(dir.absolutePath()));
        }
    }
}
