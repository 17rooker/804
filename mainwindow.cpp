#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "view/controllerpanel.h"
#include "src/CommManager.h"
#include <QHBoxLayout>
#include <QMenu>
#include <QVBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QFrame>
#include <QFont>
#include <QTextEdit>
#include <QTimer>


MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_testSeconds(0)
    // 初始化全局唯一的422指令弹窗实例（堆创建，父对象为MainWindow）
    , m_controller422Dialog(new Controller422Dialog(this))
    // 初始化全局唯一的422设置弹窗实例
    , m_serial422Dialog(new Serial422Dialog(this))
    // 初始化三个控制器唯一实例
    , m_emissionTab1(new EmissionTab(this, m_controller422Dialog, m_serial422Dialog, "控制器1"))
    , m_emissionTab2(new EmissionTab(this, m_controller422Dialog, m_serial422Dialog, "控制器2"))
    , m_emissionTab3(new EmissionTab(this, m_controller422Dialog, m_serial422Dialog, "控制器3"))

{
//    StyleEventFilter::getInstance().setStyleSheet(qApp->applicationDirPath() + "/style/style.qss");
//    StyleEventFilter::getInstance().registerWidget(this);

    this->setWindowTitle("牵制释放终端软件");
    this->setMinimumSize(1600, 900);

    initUi();

}

MainWindow::~MainWindow()
{
    // 释放三个控制器实例
    delete m_emissionTab1;
    delete m_emissionTab2;
    delete m_emissionTab3;

    delete ui;
}

void MainWindow::updateTime()
{
    m_lblSysTime->setText(QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss"));
}

void MainWindow::updateTestData()
{
    m_testSeconds++;
    int h = m_testSeconds / 3600;
    int m = (m_testSeconds % 3600) / 60;
    int s = m_testSeconds % 60;
    m_lblTestTimeVal->setText(QString("%1:%2:%3")
                              .arg(h, 2, 10, QChar('0'))
                              .arg(m, 2, 10, QChar('0'))
                              .arg(s, 2, 10, QChar('0')));
}

void MainWindow::updateSystemData()
{
    // 这里调用 m_dataPanel->updateData(...)
}


void MainWindow::initUi()
{
    // 2. 创建核心数据面板 (虽然还没放，先实例化)
    m_dataPanel = new ControllerPanel(this);
    m_LaunchProcess =new LaunchProcessDialog(this);

    // 3. 构建顶部黑色标题栏
    QWidget *topBar = new QWidget(this);
    topBar->setFixedHeight(100);
    topBar->setStyleSheet("background-color: black;");

    QLabel *logoLabel = new QLabel(topBar);
    QPixmap pixmap(":/logo.png");
    if (!pixmap.isNull()) {
        logoLabel->setPixmap(pixmap.scaledToHeight(60, Qt::SmoothTransformation));
    } else {
        logoLabel->setText("LOGO");
        logoLabel->setStyleSheet("color: white; font-size: 20px;");
    }

    logoLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    logoLabel->setFixedWidth(120);

    // 主水平布局：LOGO + 左侧时间 + 中间标题 + 右侧状态
    QHBoxLayout *topLayout = new QHBoxLayout(topBar);
    topLayout->setContentsMargins(10, 10, 30, 10);
    topLayout->setSpacing(20); // 控件间距

    // 左侧：北京时间
    QVBoxLayout *leftLayout = new QVBoxLayout();
    leftLayout->setAlignment(Qt::AlignLeft | Qt::AlignBottom);

    QLabel *lblBeijingTitle = new QLabel("北京时间", topBar);
    lblBeijingTitle->setStyleSheet("color: white; font-size: 14px; font-family: 'SimHei';");

    m_lblSysTime = new QLabel("14:36:16", topBar);
    m_lblSysTime->setStyleSheet("color: #00FF00; font-size: 20px; font-weight: bold; font-family: 'Courier';");

    leftLayout->addWidget(lblBeijingTitle);
    leftLayout->addWidget(m_lblSysTime);

    // 中间：主标题
    QLabel *mainTitle = new QLabel("牵制释放终端软件", topBar);
    mainTitle->setAlignment(Qt::AlignCenter);
    mainTitle->setStyleSheet("color: #00FF00; font-size: 36px; font-weight: bold; font-family: 'SimHei'; letter-spacing: 5px;");

    // 右侧：测试时间 + 状态灯
    QVBoxLayout *rightLayout = new QVBoxLayout();
    rightLayout->setAlignment(Qt::AlignRight | Qt::AlignBottom);
    rightLayout->setSpacing(5);

    QHBoxLayout *testTimeLayout = new QHBoxLayout();
    testTimeLayout->setAlignment(Qt::AlignRight);
    testTimeLayout->setSpacing(5);

    QLabel *lblTestTitle = new QLabel("测试时间", topBar);
    lblTestTitle->setStyleSheet("color: white; font-size: 14px; font-family: 'SimHei';");

    m_lblTestTimeVal = new QLabel("04:22:52", topBar);
    m_lblTestTimeVal->setStyleSheet("color: #00FF00; font-size: 20px; font-weight: bold; font-family: 'Courier';");

    testTimeLayout->addWidget(lblTestTitle);
    testTimeLayout->addWidget(m_lblTestTimeVal);

    QHBoxLayout *statusLayout = new QHBoxLayout();
    statusLayout->setAlignment(Qt::AlignRight);
    statusLayout->setSpacing(15);

    // 1. 测发控状态
    QHBoxLayout *cecLayout = new QHBoxLayout();
    cecLayout->setSpacing(5);
    QLabel *lblCEC = new QLabel("测发控", topBar);
    lblCEC->setStyleSheet("color: white; font-size: 14px; font-family: 'SimHei';");
    m_lblCecLight = new QLabel(topBar);
    m_lblCecLight->setFixedSize(12, 12);
    m_lblCecLight->setStyleSheet("border-radius: 6px; background-color: #888888; border: 1px solid white;");
    cecLayout->addWidget(lblCEC);
    cecLayout->addWidget(m_lblCecLight);

    // 2. 机构准备好状态
    QHBoxLayout *readyLayout = new QHBoxLayout();
    readyLayout->setSpacing(5);
    QLabel *lblReady = new QLabel("机构准备好", topBar);
    lblReady->setStyleSheet("color: white; font-size: 14px; font-family: 'SimHei';");
    m_lblStatusLight = new QLabel(topBar);
    m_lblStatusLight->setFixedSize(12, 12);
    m_lblStatusLight->setStyleSheet("border-radius: 6px; background-color: #00FF00; border: 1px solid white;");
    readyLayout->addWidget(lblReady);
    readyLayout->addWidget(m_lblStatusLight);

    statusLayout->addLayout(cecLayout);
    statusLayout->addLayout(readyLayout);
    rightLayout->addLayout(testTimeLayout);
    rightLayout->addLayout(statusLayout);


    topLayout->addWidget(logoLabel);    // 第一：LOGO贴左
    topLayout->addLayout(leftLayout, 1); // 第二：北京时间
    topLayout->addWidget(mainTitle, 3);  // 第三：主标题（拉伸占比最大）
    topLayout->addLayout(rightLayout, 1); // 第四：右侧状态

    QWidget *btnBar = new QWidget(this);
    btnBar->setFixedHeight(45); // 增加高度以适应更大的按钮
    btnBar->setStyleSheet("background-color: #E0E0E0;"); // 浅灰背景，模拟原图
    QHBoxLayout *btnLayout = new QHBoxLayout(btnBar);
    btnLayout->setContentsMargins(15, 8, 15, 8); // 增加内边距
    btnLayout->setSpacing(12); // 增加按钮间距

    QStringList btnNames = {"控制器1", "控制器2", "控制器3", "模拟数据", "事后分析",
                            "DA输出", "422指令", "422开启", "422设置", "程序电源",
                            "退出"};

    for (const QString &name : btnNames) {
        QPushButton *btn = new QPushButton(name, btnBar);
        btn->setMinimumWidth(90); // 增加最小宽度
        btn->setMinimumHeight(30); // 增加最小高度
        // 按钮样式：扁平化，带边框，增加鼠标悬浮效果
        btn->setStyleSheet(
            "QPushButton {"
            "   border: 1px solid #8f8f91;"
            "   border-radius: 4px;"
            "   padding: 4px 8px;"
            "   background-color: qlineargradient(x1: 0, y1: 0, x2: 0, y2: 1, stop: 0 #f6f7fa, stop: 1 #dadbde);"
            "   font-size: 13px;"
            "}"
            "QPushButton:hover {"
            "   background-color: qlineargradient(x1: 0, y1: 0, x2: 0, y2: 1, stop: 0 #e6e7ea, stop: 1 #cadcde);"
            "}"
            "QPushButton:pressed {"
            "   background-color: #dadbde;"
            "}"
        );
        btnLayout->addWidget(btn);

        // 422指令按钮绑定
        if (name == "422指令") {
            connect(btn, &QPushButton::clicked, this, [=]() {
                // 复用全局唯一实例，不再新建
                m_controller422Dialog->exec();
            });
        }
        // 422设置按钮绑定
        else if (name == "422设置") {
            connect(btn, &QPushButton::clicked, this, [=]() {
                // 复用全局唯一的422设置弹窗实例
                m_serial422Dialog->exec();
            });
        }
        // 模拟数据按钮绑定
        else if(name == "模拟数据") {
            connect(btn, &QPushButton::clicked, this, [=]() {
                // 创建并显示模拟数据窗口
                simulateddata dialog(this);
                dialog.exec();
            });
        }
        // 控制器1按钮绑定 - 复用唯一实例
        else if (name == "控制器1") {
            connect(btn, &QPushButton::clicked, this, [=]() {
               m_emissionTab1->setControllerPanelDialog(m_dataPanel);
               m_emissionTab1->setLaunchProcessDialog(m_LaunchProcess);
               m_emissionTab1->exec();
            });
        }
        // 控制器2按钮绑定 - 复用唯一实例
        else if (name == "控制器2") {
            connect(btn, &QPushButton::clicked, this, [=]() {
                m_emissionTab2->exec();
            });
        }
        // 控制器3按钮绑定 - 复用唯一实例
        else if (name == "控制器3") {
            connect(btn, &QPushButton::clicked, this, [=]() {
                m_emissionTab3->exec();
            });
        }
        // 退出按钮绑定
        else if (name == "退出") {
            connect(btn, &QPushButton::clicked, this, &QWidget::close);
        }
    }
    btnLayout->addStretch();

    QWidget *centerWidget = new QWidget(this);
    QHBoxLayout *centerLayout = new QHBoxLayout(centerWidget);
    centerLayout->setContentsMargins(10, 10, 10, 10);
    centerLayout->setSpacing(15);

    // 左侧：数据面板 (占据 75%)
    centerLayout->addWidget(m_dataPanel, 4);

    // 右侧：运控记录 (占据 25%)
    QFrame *logFrame = new QFrame(centerWidget);
    logFrame->setFrameShape(QFrame::Box);
    logFrame->setLineWidth(1);
    logFrame->setStyleSheet("border: 1px solid #888;");
    QVBoxLayout *logLayout = new QVBoxLayout(logFrame);

    QLabel *logTitle = new QLabel("运控记录", logFrame);
    logTitle->setAlignment(Qt::AlignCenter);
    logTitle->setFont(QFont("SimHei", 11, QFont::Bold));
    logTitle->setStyleSheet("background-color: #DDDDDD;"); // 标题背景

    m_logText = new QTextEdit(logFrame);
    m_logText->setReadOnly(true);
    m_logText->setFont(QFont("Consolas", 9));
    m_logText->append("[系统启动]");

    QPushButton *launchBtn = new QPushButton("发射界面", logFrame);
    launchBtn->setFixedHeight(40);
    launchBtn->setFont(QFont("SimHei", 10, QFont::Bold));
    launchBtn->setStyleSheet("background-color: #E0E0E0; border: 1px solid gray;");

    // 修改点击事件连接
    connect(launchBtn, &QPushButton::clicked, this, [=]() {
//        LaunchProcessDialog dialog(this);
        m_LaunchProcess->exec(); // 模态弹出
    });

    logLayout->addWidget(logTitle);
    logLayout->addWidget(m_logText);
    logLayout->addWidget(launchBtn);

    centerLayout->addWidget(logFrame, 1);

    QVBoxLayout *mainLayout = new QVBoxLayout();
    mainLayout->setSpacing(0);
    mainLayout->setContentsMargins(0, 0, 0, 0);

    mainLayout->addWidget(topBar);      // 1. 黑色顶栏
    mainLayout->addWidget(btnBar);      // 2. 按钮栏
    mainLayout->addWidget(centerWidget);// 3. 核心内容

    QWidget *centralWidget = new QWidget(this);
    centralWidget->setLayout(mainLayout);
    this->setCentralWidget(centralWidget);

    m_timerSys = new QTimer(this);
    connect(m_timerSys, &QTimer::timeout, this, &MainWindow::updateTime);
    m_timerSys->start(1000);
    updateTime();

    m_timerTest = new QTimer(this);
    connect(m_timerTest, &QTimer::timeout, this, &MainWindow::updateTestData);
    m_timerTest->start(1000);

    // 监听测发控TCP连接状态（仅在状态切换时打印日志）
    QString tcpChannelId = ConfigHelper::getInstance().getValue("Communication/TCP_Name_ServerRemote", "tcp_device_serverRemote").toString();
    connect(&CommManager::instance(), &CommManager::channelStateChanged,
        this, [this, tcpChannelId](const QString& channelId, EChannelState state) {
            if (channelId != tcpChannelId) return;
            static EChannelState prevState = EChannelState::Disconnected;

            QString timeStr = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss");
            if (state == EChannelState::Connected && prevState != EChannelState::Connected) {
                m_lblCecLight->setStyleSheet("border-radius: 6px; background-color: #00FF00; border: 1px solid white;");
                if (m_logText) m_logText->append(QString("[%1] 与测发控建立连接").arg(timeStr));
            } else if ((state == EChannelState::Disconnected || state == EChannelState::Error) && prevState == EChannelState::Connected) {
                m_lblCecLight->setStyleSheet("border-radius: 6px; background-color: #888888; border: 1px solid white;");
                if (m_logText) m_logText->append(QString("[%1] 与测发控断开连接").arg(timeStr));
            }
            prevState = state;
        });

    // 运控记录支持右键清除
    if (m_logText) {
        m_logText->setContextMenuPolicy(Qt::CustomContextMenu);
        connect(m_logText, &QWidget::customContextMenuRequested, this, [this](const QPoint& pos) {
            QMenu menu;
            menu.addAction("清除记录", this, [this]() {
                if (m_logText) m_logText->clear();
            });
            menu.exec(m_logText->mapToGlobal(pos));
        });
    }
}
