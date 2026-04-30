#ifndef SERIALCHANNEL_H
#define SERIALCHANNEL_H

#include "src/LogicCommunication/ICommChannel.h"

#include <QObject>
#include <QSerialPort>
#include <QThread>
#include <QTimer>
#include <atomic>
#include <memory>

/**
 * @brief SerialChannelWorker  串口工作对象（运行在独立线程）
 *
 * 将 QSerialPort 运行在子线程以避免阻塞主线程，
 * 通过 Qt 信号槽跨线程传递数据。
 */
class SerialChannelWorker : public QObject {
    Q_OBJECT

public:
    explicit SerialChannelWorker(SerialConfig config, QString channelId,
                                 QObject* parent = nullptr);

private:
    void scheduleReconnect();
public slots:
    void open();
    void close();
    void sendData(const QByteArray& data);

signals:
    void dataReceived(const QString& channelId, const QByteArray& data);
    void stateChanged(const QString& channelId, EChannelState state);

private slots:
    void onReadyRead();
    void onError(QSerialPort::SerialPortError error);

private:
    SerialConfig        m_config;
    QString             m_channelId;
    QSerialPort*        m_serial   = nullptr;
    QTimer*             m_reconnect = nullptr;
    QByteArray          m_readBuf;
};

// ─────────────────────────────────────────────

/**
 * @brief SerialChannel  串口通信通道
 *
 * 使用 Qt QSerialPort 实现，内部独立 QThread，
 * 对外与 TCP/UDP 保持相同的 ICommChannel 接口。
 */
class SerialChannel : public QObject  {
    Q_OBJECT
    // 接收消息的回调函数
    using SerialDataReceivedCallback = std::function<void(const CommRawData&)>;
public:
    explicit SerialChannel(CommChannelConfig config);
    ~SerialChannel() ;

    bool  start()      ;
    void  stop()       ;
    bool  send(const QByteArray& data) ;
    bool updateConfig(const CommChannelConfig& newConfig);
    bool          isConnected() const ;
    EChannelState state()       const ;
    QString       channelId()   const ;
    EChannelType  channelType() const  { return EChannelType::Serial; }

    // ── 回调注册（必须在 start() 之前调用）────────
    void setDataCallback(SerialDataReceivedCallback cb) {
        m_dataCb = std::move(cb);
    }
    void setStateCallback(ChannelStateCallback cb) {
        m_stateCb = std::move(cb);
    }

signals:
    void requestSend(const QByteArray& data);   // 转发到工作线程

private slots:
    void onDataReceived(const QString& id, const QByteArray& data);
    void onStateChanged(const QString& id, EChannelState state);

private:
    CommChannelConfig          m_config;
    std::atomic<EChannelState> m_state { EChannelState::Disconnected };

    QThread*             m_workerThread = nullptr;
    SerialChannelWorker* m_worker       = nullptr;

    SerialDataReceivedCallback m_dataCb;
    ChannelStateCallback m_stateCb;
};

#endif // SERIALCHANNEL_H
