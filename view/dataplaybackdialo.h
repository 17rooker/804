#ifndef DATAPLAYBACKDIALOG_H
#define DATAPLAYBACKDIALOG_H

#include <QDialog>
#include <QFile>
#include <QThread>
#include <QProgressBar>
#include <QPlainTextEdit>
#include <QLineEdit>
#include <QPushButton>
#include <QComboBox>
#include <QLabel>
#include <QGridLayout>
#include <QFileDialog>
#include <QMessageBox>
#include <QElapsedTimer>
#include <QWaitCondition>
#include <QMutex>
#include <QTimer>
#include <climits>

class LaunchFrameDialog;
class ControllerPanel ;
class LaunchProcessDialog;
class DataReadWorker : public QObject
{
    Q_OBJECT
public:
    explicit DataReadWorker(QObject *parent = nullptr);
    ~DataReadWorker() override;

    void setReadParams(const QString &filePath, qint64 startPos, qint64 speed, const QString &channel);
    void pauseRead();
    void resumeRead();
    void stopRead();

signals:
    void rawDataReady(const QByteArray &data);     // 发送原始二进制
    void progressUpdated(qint64 current, qint64 total);
    void readFinished();
    void readError(const QString &error);
    void workerFinished();

private slots:
    void doReadWork();

private:
    QString m_filePath;
    qint64 m_startPos = 0;
    qint64 m_speed = 0;
    QString m_channel;

    bool m_isPaused = false;
    bool m_isStopped = false;
    qint64 m_totalSize = 0;

    QMutex m_mutex;
    QWaitCondition m_cond;

    // 优化批量发送配置：减小单次批量大小，缩短批量时间间隔
    const qint64 BATCH_SIZE = 4 * 1024;    // 从8KB改为4KB，减少单次UI处理数据量
    const int BATCH_TIME_MS = 20;          // 从50ms改为20ms，更频繁但更小批量发送
};

class DataPlaybackDialog : public QDialog
{
    Q_OBJECT
    explicit DataPlaybackDialog(QWidget *parent = nullptr);
    ~DataPlaybackDialog() override;

public:
    static DataPlaybackDialog* getInstance(QWidget *parent = nullptr);
    static void destroyInstance();
    void setControllerPanelDialog(ControllerPanel *dialog);
    void setLaunchProcessDialog(LaunchProcessDialog *dialog);

    void setLaunchFrameDialog(LaunchFrameDialog *dialog);
private slots:
    // 按钮槽函数
    void onSelectFileClicked();
    void onStartClicked();
    void onPauseClicked();
    void onResumeClicked();
    void onStopClicked();
    void onQuitClicked();

    // 工作线程信号槽
    void onRawDataReady(const QByteArray &data);
//    void onProgressUpdated(qint64 current, qint64 total);
    void onReadFinished();
    void onReadError(const QString &error);
    void onWorkerFinished();

    // 进度条节流槽函数
    void updateProgressBarThrottled();
    // 新增UI数据格式化+更新槽函数
    void updateDataDisplay();

private:
    static DataPlaybackDialog* m_instance;
    static QMutex m_instanceMutex;
    QMutex m_uiDataMutex;  // 保护UI数据缓存的互斥锁
    LaunchFrameDialog *m_launchFrameDialog = nullptr;
    ControllerPanel *m_dataPanel=nullptr;
    LaunchProcessDialog*m_LaunchProcess=nullptr;
    bool m_hasSentCollectorData = false;
    // UI控件
    QLineEdit *m_fileEdit;
    QPushButton *m_fileBtn;
    QLineEdit *m_startPosEdit;
    QLineEdit *m_speedEdit;
    QComboBox *m_channelCombo;
    QPushButton *m_startBtn;
    QPushButton *m_pauseBtn;
    QPushButton *m_resumeBtn;
    QPushButton *m_stopBtn;
    QPushButton *m_quitBtn;
    QPlainTextEdit *m_dataDisplayEdit;
    QProgressBar *m_progressBar;

    // 线程管理
    QThread *m_workerThread = nullptr;
    DataReadWorker *m_readWorker = nullptr;

    // 进度条节流控制
    QTimer *m_progressTimer = nullptr;
    qint64 m_pendingProgressCurrent = 0;
    qint64 m_pendingProgressTotal = 0;

    // 新增UI更新节流控制
    QTimer *m_uiUpdateTimer = nullptr;
    QByteArray m_uiDataBuffer;  // UI层批量缓存原始二进制数据

    // 新增：限制单次UI处理的最大数据量，防止单次处理过多导致卡顿
    const qint64 MAX_UI_PROCESS_SIZE = 4 * 1024; // 单次最多处理4KB数据

    void initUI();
    void initConnections();
    void resetUI();
    void initWorkerThread();
    void destroyWorkerThread();
    // 新增：帧解析相关
      QMutex m_frameParseMutex;                // 保护帧解析缓存的互斥锁
      QByteArray m_frameParseBuffer;           // 缓存未解析完的原始数据
      static const QByteArray FRAME_HEADER;    // 帧头 0xFDB18540（静态常量）
};

#endif // DATAPLAYBACKDIALOG_H
