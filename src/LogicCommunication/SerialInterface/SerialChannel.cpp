#include "SerialChannel.h"

#include <QDebug>
#include <QTimer>
#include "src/Common/LoggerManager.h"

// ═══════════════════════════════════════════════
//  SerialChannelWorker
// ═══════════════════════════════════════════════

SerialChannelWorker::SerialChannelWorker(SerialConfig config,
                                         QString      channelId,
                                         QObject*     parent)
    : QObject(parent)
    , m_config(std::move(config))
    , m_channelId(std::move(channelId))
{}

// SerialChannel.cpp 新增实现
bool SerialChannel::updateConfig(const CommChannelConfig& newConfig) {
    // 校验配置合法性（通道类型必须是串口）
    if (newConfig.type != EChannelType::Serial) {
        LOG_WARN("system", "{} updateConfig failed: channel type not serial", newConfig.channelId.toStdString());
        return false;
    }

    // 线程安全地更新配置并重启串口
    if (m_workerThread && m_workerThread->isRunning() && m_worker) {
        // 1. 停止当前串口
        QMetaObject::invokeMethod(m_worker, "close", Qt::BlockingQueuedConnection);

        // 2. 更新配置（原子操作）
        m_config = newConfig;

        // 3. 重新打开串口
        QMetaObject::invokeMethod(m_worker, "open", Qt::QueuedConnection);

        LOG_INFO("system", "{} update serial config success: {}", m_config.channelId.toStdString(), m_config.serialCfg.portName.toStdString());
        return true;
    }

    // 通道未启动时直接更新配置
    m_config = newConfig;
    return true;
}

void SerialChannelWorker::open() {
    if (m_serial && m_serial->isOpen()) return;

    if (!m_serial) {
        m_serial = new QSerialPort(this);
        connect(m_serial, &QSerialPort::readyRead,
                this, &SerialChannelWorker::onReadyRead);
        connect(m_serial,
                QOverload<QSerialPort::SerialPortError>::of(&QSerialPort::error),
                this, &SerialChannelWorker::onError);
    }

    m_serial->setPortName(m_config.portName);
    m_serial->setBaudRate(m_config.baudRate);
    m_serial->setDataBits(static_cast<QSerialPort::DataBits>(m_config.dataBits));
    m_serial->setStopBits(static_cast<QSerialPort::StopBits>(m_config.stopBits));
    m_serial->setParity(static_cast<QSerialPort::Parity>(m_config.parity));
    m_serial->setFlowControl(
        static_cast<QSerialPort::FlowControl>(m_config.flowControl));

    if (m_serial->open(QIODevice::ReadWrite)) {

        LOG_INFO("system","{} opened:{}",m_channelId.toStdString(),m_config.portName.toStdString());
        emit stateChanged(m_channelId, EChannelState::Connected);

        // 停止重连定时器
        if (m_reconnect && m_reconnect->isActive()) {
            m_reconnect->stop();
        }
    } else {


        LOG_WARN("system","{} open failed:{}",m_channelId.toStdString(),m_serial->errorString().toStdString());

        emit stateChanged(m_channelId, EChannelState::Error);
        scheduleReconnect();
    }
}

void SerialChannelWorker::close() {
    if (m_reconnect) m_reconnect->stop();
    if (m_serial && m_serial->isOpen()) {
        m_serial->close();
    }
    emit stateChanged(m_channelId, EChannelState::Disconnected);
}

void SerialChannelWorker::sendData(const QByteArray& data) {
    if (!m_serial || !m_serial->isOpen()) return;
    m_serial->write(data);
}

void SerialChannelWorker::onReadyRead() {
    m_readBuf += m_serial->readAll();

    if (!m_readBuf.isEmpty()) {
        emit dataReceived(m_channelId, m_readBuf);
        m_readBuf.clear();
    }
}

void SerialChannelWorker::onError(QSerialPort::SerialPortError error) {
    if (error == QSerialPort::NoError) return;

    LOG_WARN("system","{} error:{} {}",m_channelId.toStdString(),static_cast<int>(error),m_serial->errorString().toStdString());

    if (m_serial->isOpen()) m_serial->close();
    emit stateChanged(m_channelId, EChannelState::Error);
    scheduleReconnect();
}

void SerialChannelWorker::scheduleReconnect() {
    if (!m_reconnect) {
        m_reconnect = new QTimer(this);
        m_reconnect->setSingleShot(false);
        connect(m_reconnect, &QTimer::timeout,
                this, &SerialChannelWorker::open);
    }
    if (!m_reconnect->isActive()) {
        m_reconnect->start(m_config.readTimeoutMs > 0
                           ? m_config.readTimeoutMs * 60   // 默认重连间隔
                           : 3000);
    }
}

// ═══════════════════════════════════════════════
//  SerialChannel
// ═══════════════════════════════════════════════

SerialChannel::SerialChannel(CommChannelConfig config)
    : QObject(nullptr)
    , m_config(std::move(config))
{}

SerialChannel::~SerialChannel() {
    stop();
}

bool SerialChannel::start() {
    if (m_workerThread) return false;

    m_workerThread = new QThread(this);
    m_worker = new SerialChannelWorker(m_config.serialCfg,
                                       m_config.channelId);
    m_worker->moveToThread(m_workerThread);

    // 线程启动时打开串口
    connect(m_workerThread, &QThread::started,
            m_worker, &SerialChannelWorker::open);

    // 串口数据 → 本对象（在主线程/调用线程回调）
    connect(m_worker, &SerialChannelWorker::dataReceived,
            this, &SerialChannel::onDataReceived,
            Qt::QueuedConnection);

    connect(m_worker, &SerialChannelWorker::stateChanged,
            this, &SerialChannel::onStateChanged,
            Qt::QueuedConnection);

    // 跨线程发送
    connect(this, &SerialChannel::requestSend,
            m_worker, &SerialChannelWorker::sendData,
            Qt::QueuedConnection);

    m_workerThread->start();
    m_state = EChannelState::Connecting;
    return true;
}

void SerialChannel::stop() {
    if (!m_workerThread) return;

    QMetaObject::invokeMethod(m_worker,
                              "close",
                              Qt::BlockingQueuedConnection);

    m_workerThread->quit();
    m_workerThread->wait(3000);

    m_worker->deleteLater();
    m_worker = nullptr;

    m_workerThread->deleteLater();
    m_workerThread = nullptr;

    m_state = EChannelState::Disconnected;
}

bool SerialChannel::send(const QByteArray& data) {
    if (!isConnected()) return false;
    emit requestSend(data);   // 队列化到工作线程
    return true;
}

bool SerialChannel::isConnected() const {
    return m_state == EChannelState::Connected;
}

EChannelState SerialChannel::state() const {
    return m_state.load();
}

QString SerialChannel::channelId() const {
    return m_config.channelId;
}


void SerialChannel::onDataReceived(const QString& /*id*/,
                                    const QByteArray& data)
{
    if (!m_dataCb || data.isEmpty()) return;

    CommRawData raw(EChannelType::Serial, m_config.channelId, data);
    m_dataCb(std::move(raw));
}

void SerialChannel::onStateChanged(const QString& id, EChannelState state) {
    m_state = state;
    if (m_stateCb) m_stateCb(id, state);
}
