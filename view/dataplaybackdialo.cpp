#include "dataplaybackdialo.h"
#include "launchframedialog.h"
#include "controllerpanel.h"
#include "launchprocessdialog.h"
#include "copyframedialog.h"
#include <QMutexLocker>
#include <QDebug>
#include <QScrollBar>
#include <QStringBuilder>
#include <QTextCursor>
#include <QProcess>

// 静态成员初始化
DataPlaybackDialog* DataPlaybackDialog::m_instance = nullptr;
QMutex DataPlaybackDialog::m_instanceMutex;
// 新增：帧头常量初始化（0xFDB18540的16进制字符串转字节数组）
const QByteArray DataPlaybackDialog::FRAME_HEADER = QByteArray::fromHex("FDB18540");

// --------------------- DataReadWorker 实现 ---------------------
DataReadWorker::DataReadWorker(QObject *parent) : QObject(parent) {}

DataReadWorker::~DataReadWorker()
{
    stopRead();
    qDebug() << "DataReadWorker 销毁";
}

void DataReadWorker::setReadParams(const QString &filePath, qint64 startPos, qint64 speed, const QString &channel)
{
    QMutexLocker locker(&m_mutex);
    m_filePath = filePath;
    m_startPos = startPos;
    m_speed = speed;
    m_channel = channel;
    m_isPaused = false;
    m_isStopped = false;
}

void DataReadWorker::pauseRead()
{
    QMutexLocker locker(&m_mutex);
    m_isPaused = true;
}

void DataReadWorker::resumeRead()
{
    QMutexLocker locker(&m_mutex);
    m_isPaused = false;
    m_cond.wakeAll();
}

void DataReadWorker::stopRead()
{
    QMutexLocker locker(&m_mutex);
    m_isStopped = true;
    m_isPaused = false;
    m_cond.wakeAll();
}

void DataReadWorker::doReadWork()
{
    QString filePath;
    qint64 startPos;
    qint64 speed;
    {
        QMutexLocker locker(&m_mutex);
        filePath = m_filePath;
        startPos = m_startPos;
        speed = m_speed;
        if (m_isStopped) {
            emit workerFinished();
            return;
        }
    }

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        emit readError("文件打开失败：" + file.errorString());
        emit workerFinished();
        return;
    }

    const qint64 totalSize = file.size();
    if (startPos < 0 || startPos >= totalSize) {
        emit readError("起始位置超出文件范围");
        file.close();
        emit workerFinished();
        return;
    }

    file.seek(startPos);
    qint64 currentPos = startPos;
    emit progressUpdated(currentPos, totalSize);

    // 优化读取块大小：起始阶段使用更小的块大小，避免初始数据爆发
    qint64 readChunk = 1024; // 初始1KB，后续恢复到4KB
    const qint64 normalReadChunk = 4096;
    int readCount = 0; // 计数，读取几次后恢复正常块大小
    const int recoverChunkCount = 5; // 读取5次后恢复正常块大小

    // 优化速度控制：使用浮点数计算保证精度，移除固定最小休眠限制
    const double speedBytesPerSec = static_cast<double>(speed);
    const double bytesPerMs = speedBytesPerSec / 1000.0; // 每毫秒允许读取的字节数

    // 批量缓存
    QByteArray batchBuffer;
    QElapsedTimer batchTimer;
    batchTimer.start();

    QElapsedTimer sleepTimer;
    while (true) {
        // 检查停止/暂停状态
        {
            QMutexLocker locker(&m_mutex);
            if (m_isStopped) break;
            while (m_isPaused && !m_isStopped) {
                m_cond.wait(&m_mutex);
            }
        }

        // 到达文件末尾
        if (currentPos >= totalSize) break;

        // 读取几次后恢复正常块大小
        if (readCount >= recoverChunkCount) {
            readChunk = normalReadChunk;
        }

        // 读取数据（严格限速）
        sleepTimer.restart();
        QByteArray data = file.read(readChunk);
        readCount++;
        if (data.isEmpty()) {
            if (file.error() != QFile::NoError) {
                emit readError("文件读取失败：" + file.errorString());
            }
            break;
        }

        // 缓存数据，达到批量阈值则发送原始二进制（严格控制批量大小）
        batchBuffer.append(data);
        currentPos += data.size();

        // 满足批量条件则发送：更小的批量+更短的时间间隔，避免UI压力
        if (batchBuffer.size() >= BATCH_SIZE || batchTimer.elapsed() >= BATCH_TIME_MS) {
            emit rawDataReady(batchBuffer);
            batchBuffer.clear();
            batchTimer.restart();
            emit progressUpdated(currentPos, totalSize);
        }

        // 精准控制读取速度：根据实际读取字节数计算目标总时间，补偿读取耗时
        const qint64 actualReadBytes = data.size();
        const double elapsedMs = static_cast<double>(sleepTimer.elapsed());
        // 计算读取actualReadBytes字节需要的目标总时间（ms）
        const double targetTotalTimeMs = bytesPerMs > 0.0 ? (static_cast<double>(actualReadBytes) / bytesPerMs) : 5.0;

        // 需要等待的时间 = 目标总时间 - 已消耗时间（确保非负）
        double waitMs = qMax(targetTotalTimeMs - elapsedMs, 0.0);

        // 高精度等待（微秒级），分批次等待以响应暂停/停止
        if (waitMs > 0.0) {
            const qint64 waitUs = static_cast<qint64>(waitMs * 1000.0); // 转换为微秒
            const qint64 batchUs = 1000; // 每次等待1ms，避免单次等待过长
            qint64 remainingUs = waitUs;
            QElapsedTimer waitTimer;
            waitTimer.start();

            while (remainingUs > 0 && !m_isStopped) {
                // 每次等待前检查暂停/停止状态
                {
                    QMutexLocker locker(&m_mutex);
                    if (m_isPaused || m_isStopped) {
                        remainingUs = 0;
                        break;
                    }
                }

                const qint64 waitBatch = qMin(remainingUs, batchUs);
                QThread::usleep(waitBatch);
                remainingUs -= waitBatch;

                // 补偿实际等待时间与预期的偏差
                const qint64 actualWaitUs = waitTimer.elapsed() * 1000;
                remainingUs = qMax(waitUs - actualWaitUs, 0LL);
            }
        }
    }

    // 发送剩余缓存数据
    if (!batchBuffer.isEmpty() && !m_isStopped) {
        emit rawDataReady(batchBuffer);
        emit progressUpdated(currentPos, totalSize);
    }

    file.close();
    emit readFinished();
    emit workerFinished();
    qDebug() << "数据读取完成，当前位置：" << currentPos;
}

// --------------------- DataPlaybackDialog 实现 ---------------------
DataPlaybackDialog::DataPlaybackDialog(QWidget *parent)
    : QDialog(parent)
    , m_launchFrameDialog(nullptr)
    ,m_dataPanel(nullptr)
    ,m_LaunchProcess(nullptr)
    , m_copyFrameDialog(nullptr)
{
    setWindowTitle("数据回放");
    setFixedSize(800, 600);
    initUI();
    initConnections();
    setAttribute(Qt::WA_DeleteOnClose, false);
}
void DataPlaybackDialog::setLaunchFrameDialog(LaunchFrameDialog *dialog)
{
    m_launchFrameDialog = dialog;
}
DataPlaybackDialog::~DataPlaybackDialog()
{
    destroyWorkerThread();
    qDebug() << "DataPlaybackDialog 销毁";
}

DataPlaybackDialog* DataPlaybackDialog::getInstance(QWidget *parent)
{
    QMutexLocker locker(&m_instanceMutex);
    if (!m_instance) {
        m_instance = new DataPlaybackDialog(parent);
    }
    return m_instance;
}

void DataPlaybackDialog::destroyInstance()
{
    QMutexLocker locker(&m_instanceMutex);
    if (m_instance) {
        m_instance->close();
        delete m_instance;
        m_instance = nullptr;
    }
}

void DataPlaybackDialog::setControllerPanelDialog(ControllerPanel *dialog)
{
    m_dataPanel=dialog;
}

void DataPlaybackDialog::setLaunchProcessDialog(LaunchProcessDialog *dialog)
{
     m_LaunchProcess=dialog;
}
void DataPlaybackDialog::setCopyFrameDialog(CopyFrameDialog *dialog)
{
    m_copyFrameDialog = dialog;
}

void DataPlaybackDialog::initUI()
{
    // 控件初始化
    QLabel *fileLabel = new QLabel("文件：");
    m_fileEdit = new QLineEdit();
    m_fileEdit->setPlaceholderText("请选择数据文件");
    m_fileBtn = new QPushButton("...");
    m_fileBtn->setFixedSize(30, 24);

    QLabel *startPosLabel = new QLabel("起始位置（字节）：");
    m_startPosEdit = new QLineEdit("0");

    QLabel *speedLabel = new QLabel("速度（字节/秒）：");
    m_speedEdit = new QLineEdit("500000");

    QLabel *channelLabel = new QLabel("回放通道：");
    m_channelCombo = new QComboBox();
    m_channelCombo->addItem("采集器1");
    m_channelCombo->addItem("采集器2");
    m_channelCombo->addItem("采集器3");
    m_channelCombo->addItem("采集器4");
    m_channelCombo->addItem("控制器1");
    m_channelCombo->addItem("控制器2");
    m_channelCombo->addItem("控制器3");
    m_channelCombo->addItem("串口服务器1");
    m_channelCombo->addItem("串口服务器2");
    m_channelCombo->addItem("串口服务器3");
    m_channelCombo->setCurrentIndex(4);
    m_startBtn = new QPushButton("开始");
    m_pauseBtn = new QPushButton("暂停");
    m_resumeBtn = new QPushButton("继续");
    m_stopBtn = new QPushButton("停止");
    m_quitBtn = new QPushButton("退出");

    m_pauseBtn->setEnabled(false);
    m_resumeBtn->setEnabled(false);
    m_stopBtn->setEnabled(false);

    // QPlainTextEdit 深度优化
    m_dataDisplayEdit = new QPlainTextEdit();
    m_dataDisplayEdit->setReadOnly(true);
    m_dataDisplayEdit->setFocusPolicy(Qt::NoFocus);
    m_dataDisplayEdit->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    m_dataDisplayEdit->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    m_dataDisplayEdit->setLineWrapMode(QPlainTextEdit::NoWrap);
    // 字体优化
    QFont font("Consolas", 9);
    font.setStyleHint(QFont::Monospace);
    font.setFixedPitch(true);
    m_dataDisplayEdit->setFont(font);
    // 限制最大行数，减少内存占用
    m_dataDisplayEdit->setMaximumBlockCount(100); // 适度增加行数，避免频繁清空

    // 进度条 - 修改格式：移除%p%，改为自定义两位小数百分比
    m_progressBar = new QProgressBar();
    m_progressBar->setFormat("已读取：%1 / %2 字节（%3%）");
    m_progressBar->setMinimum(0);
    m_progressBar->setMaximum(100);

    // 进度条节流定时器：保持50ms间隔
    m_progressTimer = new QTimer(this);
    m_progressTimer->setInterval(50);
    m_progressTimer->setSingleShot(false);

    // 优化UI更新定时器：从20ms改为50ms，减少更新频率
    m_uiUpdateTimer = new QTimer(this);
    m_uiUpdateTimer->setInterval(50);
    m_uiUpdateTimer->setSingleShot(false);

    // 布局
    QGridLayout *topLayout = new QGridLayout();
    topLayout->addWidget(fileLabel, 0, 0);
    topLayout->addWidget(m_fileEdit, 0, 1);
    topLayout->addWidget(m_fileBtn, 0, 2);
    topLayout->addWidget(startPosLabel, 1, 0);
    topLayout->addWidget(m_startPosEdit, 1, 1);
    topLayout->addWidget(speedLabel, 1, 2);
    topLayout->addWidget(m_speedEdit, 1, 3);
    topLayout->addWidget(channelLabel, 1, 4);
    topLayout->addWidget(m_channelCombo, 1, 5);
    topLayout->addWidget(m_startBtn, 2, 0);
    topLayout->addWidget(m_pauseBtn, 2, 1);
    topLayout->addWidget(m_resumeBtn, 2, 2);
    topLayout->addWidget(m_stopBtn, 2, 3);
    topLayout->addWidget(m_quitBtn, 2, 4);

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->addLayout(topLayout);
    mainLayout->addWidget(new QLabel("16进制数据（实时刷新）："));
    mainLayout->addWidget(m_dataDisplayEdit);
    mainLayout->addWidget(m_progressBar);
    setLayout(mainLayout);
}

void DataPlaybackDialog::initWorkerThread()
{
    // 确保先销毁旧的线程和worker
    destroyWorkerThread();

    m_workerThread = new QThread(this);
    m_readWorker = new DataReadWorker();
    m_readWorker->moveToThread(m_workerThread);

    // 连接线程结束信号，销毁worker
    connect(m_workerThread, &QThread::finished, m_readWorker, &QObject::deleteLater, Qt::DirectConnection);
}

void DataPlaybackDialog::destroyWorkerThread()
{
    if (m_workerThread) {
        // 停止读取
        if (m_readWorker) {
            m_readWorker->stopRead();
        }

        // 退出线程并等待
        m_workerThread->quit();
        if (!m_workerThread->wait(1000)) {
            m_workerThread->terminate();
            m_workerThread->wait(500);
        }

        // 断开所有连接，避免重复销毁
        m_workerThread->disconnect();

        // 手动删除线程（父对象是this，也可以靠父对象销毁，但显式删除更安全）
        delete m_workerThread;
        m_workerThread = nullptr;
    }

    // worker会被deleteLater，这里直接置空指针即可
    m_readWorker = nullptr;
}

void DataPlaybackDialog::initConnections()
{
    // 按钮信号
    connect(m_fileBtn, &QPushButton::clicked, this, &DataPlaybackDialog::onSelectFileClicked);
    connect(m_startBtn, &QPushButton::clicked, this, &DataPlaybackDialog::onStartClicked);
    connect(m_pauseBtn, &QPushButton::clicked, this, &DataPlaybackDialog::onPauseClicked);
    connect(m_resumeBtn, &QPushButton::clicked, this, &DataPlaybackDialog::onResumeClicked);
    connect(m_stopBtn, &QPushButton::clicked, this, &DataPlaybackDialog::onStopClicked);
    connect(m_quitBtn, &QPushButton::clicked, this, &DataPlaybackDialog::onQuitClicked);

    // 进度条节流更新
    connect(m_progressTimer, &QTimer::timeout, this, &DataPlaybackDialog::updateProgressBarThrottled);
    // UI数据更新节流
    connect(m_uiUpdateTimer, &QTimer::timeout, this, &DataPlaybackDialog::updateDataDisplay);

    // 销毁时停止读取
    connect(this, &DataPlaybackDialog::destroyed, this, &DataPlaybackDialog::destroyWorkerThread);
}

void DataPlaybackDialog::resetUI()
{
    m_startBtn->setEnabled(true);
    m_pauseBtn->setEnabled(false);
    m_resumeBtn->setEnabled(false);
    m_stopBtn->setEnabled(false);
    m_dataDisplayEdit->clear();
    m_progressBar->setValue(0);
    m_progressBar->setFormat("已读取：%1 / %2 字节（%3%）");

    m_progressTimer->stop();
    m_uiUpdateTimer->stop();

    m_pendingProgressCurrent = 0;
    m_pendingProgressTotal = 0;

    // 清空UI数据缓存
    QMutexLocker locker(&m_uiDataMutex);
    m_uiDataBuffer.clear();

    // 新增：清空帧解析缓存
    QMutexLocker frameLocker(&m_frameParseMutex);
    m_frameParseBuffer.clear();

    m_hasSentCollectorData = false;
}
void DataPlaybackDialog::onSelectFileClicked()
{
    QString filePath = QFileDialog::getOpenFileName(this, "选择数据文件", "", "所有文件 (*.*);;DAT文件 (*.dat)");
    if (!filePath.isEmpty()) {
        m_fileEdit->setText(filePath);
    }
}

void DataPlaybackDialog::onStartClicked()
{
    QString filePath = m_fileEdit->text().trimmed();
    if (filePath.isEmpty()) {
        QMessageBox::warning(this, "警告", "请先选择数据文件！");
        return;
    }

    bool ok1, ok2;
    qint64 startPos = m_startPosEdit->text().toLongLong(&ok1);
    qint64 speed = m_speedEdit->text().toLongLong(&ok2);

    if (!ok1 || startPos < 0) {
        QMessageBox::warning(this, "警告", "起始位置必须是非负整数！");
        return;
    }
    // 速度范围校验（1~10MB/s）
    if (!ok2 || speed <= 0 || speed > 10 * 1024 * 1024) {
        QMessageBox::warning(this, "警告", "速度必须是1~10485760的整数！");
        return;
    }

    // 重置UI和线程
    resetUI();
    m_playbackActive = true;
    // 初始化新的线程和worker
    initWorkerThread();

    // 重新连接worker信号（每次创建新worker都要重新连接）
    connect(m_readWorker, &DataReadWorker::rawDataReady, this, &DataPlaybackDialog::onRawDataReady, Qt::QueuedConnection);
    connect(m_readWorker, &DataReadWorker::progressUpdated, this, [this](qint64 current, qint64 total) {
        m_pendingProgressCurrent = current;
        m_pendingProgressTotal = total;
    }, Qt::QueuedConnection);
    connect(m_readWorker, &DataReadWorker::readFinished, this, &DataPlaybackDialog::onReadFinished, Qt::QueuedConnection);
    connect(m_readWorker, &DataReadWorker::readError, this, &DataPlaybackDialog::onReadError, Qt::QueuedConnection);
    connect(m_readWorker, &DataReadWorker::workerFinished, this, &DataPlaybackDialog::onWorkerFinished, Qt::QueuedConnection);

    QString channel = m_channelCombo->currentText();

    // 配置并启动工作线程
    m_readWorker->setReadParams(filePath, startPos, speed, channel);
    m_workerThread->start();
    QMetaObject::invokeMethod(m_readWorker, "doReadWork", Qt::QueuedConnection);

    // 启动定时器
    m_progressTimer->start();
    m_uiUpdateTimer->start();

    m_startBtn->setEnabled(false);
    m_pauseBtn->setEnabled(true);
    m_stopBtn->setEnabled(true);
}

void DataPlaybackDialog::onPauseClicked()
{
    if (m_readWorker) {
        m_readWorker->pauseRead();
        m_pauseBtn->setEnabled(false);
        m_resumeBtn->setEnabled(true);
    }
}

void DataPlaybackDialog::onResumeClicked()
{
    if (m_readWorker) {
        m_readWorker->resumeRead();
        m_resumeBtn->setEnabled(false);
        m_pauseBtn->setEnabled(true);
    }
}

void DataPlaybackDialog::onStopClicked()
{
    m_playbackActive = false;

    // 通知目标对话框清空缓存（通过事件队列安全调用）
    if (m_dataPanel)
        QMetaObject::invokeMethod(m_dataPanel, "clearPlaybackCache", Qt::QueuedConnection);
    if (m_launchFrameDialog)
        QMetaObject::invokeMethod(m_launchFrameDialog, "clearPlaybackCache", Qt::QueuedConnection);
    if (m_LaunchProcess)
        QMetaObject::invokeMethod(m_LaunchProcess, "clearPlaybackCache", Qt::QueuedConnection);
    if (m_copyFrameDialog)
        QMetaObject::invokeMethod(m_copyFrameDialog, "clearPlaybackCache", Qt::QueuedConnection);

    // 停止读取并销毁线程
    destroyWorkerThread();
    resetUI();
}

void DataPlaybackDialog::onQuitClicked()
{
    m_playbackActive = false;

    if (m_dataPanel)
        QMetaObject::invokeMethod(m_dataPanel, "clearPlaybackCache", Qt::QueuedConnection);
    if (m_launchFrameDialog)
        QMetaObject::invokeMethod(m_launchFrameDialog, "clearPlaybackCache", Qt::QueuedConnection);
    if (m_LaunchProcess)
        QMetaObject::invokeMethod(m_LaunchProcess, "clearPlaybackCache", Qt::QueuedConnection);
    if (m_copyFrameDialog)
        QMetaObject::invokeMethod(m_copyFrameDialog, "clearPlaybackCache", Qt::QueuedConnection);

    this->hide();
    destroyWorkerThread();
    resetUI();
}

void DataPlaybackDialog::onRawDataReady(const QByteArray &data)
{
    // 停止后拒收残留信号
    if (!m_playbackActive) {
        QMutexLocker frameLocker(&m_frameParseMutex);
        m_frameParseBuffer.clear();
        return;
    }

    // 原有：UI显示数据缓存逻辑（保留）
    QMutexLocker lockerUI(&m_uiDataMutex);
    const qint64 MAX_BUFFER_SIZE = 32 * 1024;
    if (m_uiDataBuffer.size() + data.size() > MAX_BUFFER_SIZE) {
        m_uiDataBuffer = m_uiDataBuffer.right(MAX_BUFFER_SIZE / 2);
    }
    m_uiDataBuffer.append(data);
    lockerUI.unlock();

    // ===================== 新增：帧解析核心逻辑 =====================
    if (m_launchFrameDialog == nullptr && m_dataPanel==nullptr
            && m_LaunchProcess==nullptr) {
        return; // 无目标对话框，跳过帧解析
    }

    QMutexLocker frameLocker(&m_frameParseMutex);
    // 将新数据追加到帧解析缓存
    m_frameParseBuffer.append(data);

    // 循环解析帧：至少8字节（4字节帧头 + 4字节帧长）才开始解析
    while (m_frameParseBuffer.size() >= 8) {
        // 1. 校验帧头（前4字节）
        QByteArray header = m_frameParseBuffer.left(4);
        if (header != FRAME_HEADER) {
            // 帧头不匹配，丢弃首个字节，继续解析
            m_frameParseBuffer.remove(0, 1);
            qDebug() << "帧头不匹配，丢弃无效字节，剩余缓存长度：" << m_frameParseBuffer.size();
            continue;
        }

        // 2. 解析帧长（4字节，大端序示例，根据实际字节序调整）
        QByteArray lenBytes = m_frameParseBuffer.mid(4, 4);
        qint32 frameLen = 0;
        // 大端序转换（高位在前）：0x12 0x34 0x56 0x78 → 0x12345678
        frameLen = (static_cast<quint8>(lenBytes[0]) << 24) |
                   (static_cast<quint8>(lenBytes[1]) << 16) |
                   (static_cast<quint8>(lenBytes[2]) << 8) |
                   static_cast<quint8>(lenBytes[3]);

        // 帧长合法性校验（防止异常值）
        if (frameLen <= 85 || frameLen > 1024 * 1024) { // 限制最大帧长1MB
            qWarning() << "无效帧长：" << frameLen << "，丢弃帧头";
            m_frameParseBuffer.remove(0, 4); // 丢弃帧头4字节
            continue;
        }

        // 3. 检查缓存是否包含完整帧（8字节头 + 帧长数据）
        qint64 requiredSize =frameLen;
        if (m_frameParseBuffer.size() < requiredSize) {
            qDebug() << "缓存数据不足，等待下一批（需" << requiredSize << "字节，当前" << m_frameParseBuffer.size() << "）";
            break; // 数据不足，等待下一批数据
        }

        // 4. 提取完整帧数据并发送
        QByteArray frameData = m_frameParseBuffer.left(requiredSize);
        m_frameParseBuffer.remove(0, requiredSize); // 从缓存移除已解析的帧
        if(frameLen==428)
        {
            // 发送到LaunchFrameDialog（UI线程安全调用）
            QMetaObject::invokeMethod(m_dataPanel, "appendData",
                                      Qt::QueuedConnection,
                                      Q_ARG(QByteArray, frameData));
            QMetaObject::invokeMethod(m_launchFrameDialog, "appendData",
                                      Qt::QueuedConnection,
                                      Q_ARG(QByteArray, frameData));

            QMetaObject::invokeMethod(m_LaunchProcess, "appendData",
                                      Qt::QueuedConnection,
                                      Q_ARG(QByteArray, frameData));
        }
        else
        {
            QMetaObject::invokeMethod(m_dataPanel, "appendData",
                                      Qt::QueuedConnection,
                                      Q_ARG(QByteArray, frameData));
            QMetaObject::invokeMethod(m_LaunchProcess, "appendData",
                                     Qt::QueuedConnection,
                                     Q_ARG(QByteArray, frameData));
//            if (m_copyFrameDialog)
                QMetaObject::invokeMethod(m_copyFrameDialog, "appendData",
                                          Qt::QueuedConnection,
                                          Q_ARG(QByteArray, frameData));
        }

//        qDebug() << "解析到有效帧：帧头=" << header.toHex().toUpper()
//                 << "，帧长=" << frameLen
//                 << "，发送数据长度=" << frameData.size();
    }
    frameLocker.unlock();
}

void DataPlaybackDialog::updateDataDisplay()
{
    QMutexLocker locker(&m_uiDataMutex);
    if (m_uiDataBuffer.isEmpty()) return;

    // 单次只处理最大MAX_UI_PROCESS_SIZE的数据，剩余留到下一次
    QByteArray processData = m_uiDataBuffer.left(MAX_UI_PROCESS_SIZE);
    m_uiDataBuffer = m_uiDataBuffer.mid(MAX_UI_PROCESS_SIZE);
    locker.unlock();

    // 高效16进制格式化：减少字符串操作开销
    QString hexStr = processData.toHex(' ').toUpper();
    QString formatted;
    // 预分配内存，减少内存重分配
    formatted.reserve(hexStr.size() + (hexStr.size() / 64));

    int byteCount = 0;
    const int LINE_BYTE_COUNT = 32; // 每行32字节
    for (int i = 0; i < hexStr.size(); i += 3) { // "XX " 占3个字符
        formatted += hexStr.mid(i, 3);
        byteCount++;
        // 每行末尾换行，避免超长行导致渲染卡顿
        if (byteCount % LINE_BYTE_COUNT == 0 && i + 3 < hexStr.size()) {
            formatted += '\n';
        }
    }
    if (!formatted.isEmpty()) {
        formatted += '\n'; // 最后加换行，保持格式整洁
    }

    // 优化QPlainTextEdit更新：追加内容而非覆盖，减少渲染开销
    m_dataDisplayEdit->setUpdatesEnabled(false);

    // 智能追加：避免频繁操作
    QTextCursor cursor = m_dataDisplayEdit->textCursor();
    cursor.movePosition(QTextCursor::End);
    cursor.insertText(formatted);

    // 智能滚动：仅当用户未手动滚动时滚到底部
    QScrollBar *vScroll = m_dataDisplayEdit->verticalScrollBar();
    bool isAtBottom = (vScroll->value() >= vScroll->maximum() - 10);
    if (isAtBottom) {
        m_dataDisplayEdit->moveCursor(QTextCursor::End);
    }

    m_dataDisplayEdit->setUpdatesEnabled(true);
}

void DataPlaybackDialog::updateProgressBarThrottled()
{
    if (m_pendingProgressTotal <= 0) return;

    // 计算百分比（保留两位小数）
    double percent = (static_cast<double>(m_pendingProgressCurrent) / m_pendingProgressTotal) * 100.0;
    QString percentStr = QString::number(percent, 'f', 2); // 格式化为两位小数

    // 进度条数值仍用整数（不影响显示效果），文本显示两位小数
    m_progressBar->setValue(static_cast<int>(qRound(percent)));
    m_progressBar->setFormat(QString("已读取：%1 / %2 字节（%3%）")
                             .arg(m_pendingProgressCurrent)
                             .arg(m_pendingProgressTotal)
                             .arg(percentStr));
}

void DataPlaybackDialog::onReadFinished()
{
    QMessageBox::information(this, "提示", "数据回放完成！");
    destroyWorkerThread();
    resetUI();
}

void DataPlaybackDialog::onReadError(const QString &error)
{
    QMessageBox::critical(this, "错误", error);
    destroyWorkerThread();
    resetUI();
}

void DataPlaybackDialog::onWorkerFinished()
{
    destroyWorkerThread();
    resetUI();
}
