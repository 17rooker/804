#ifndef CopyFrameDialog_H
#define CopyFrameDialog_H

#include <QWidget>
#include <QMap>
#include <QMutex>
#include <QByteArray>
#include "src/Common/StructDefine.h"
class StyledLedLabel;
class StyledLineEdit;
class QTimer;
class QThread;

class FrameCopyWorker : public QObject
{
    Q_OBJECT
public slots:
    void processData(const QByteArray &data);
signals:
    void dataProcessed(const STParamInfo &param);
};

class CopyFrameDialog : public QWidget
{
    Q_OBJECT

public:
    CopyFrameDialog(QWidget *parent = nullptr);
    ~CopyFrameDialog();

public slots:
    void setParam(const STParamInfo& param);
    void appendData(const QByteArray &data);
    void clearPlaybackCache();

private:
    void setupUi();
    QWidget* createColumn1();
    QWidget* createColumn2();
    QWidget* createColumn3();
    QWidget* createColumn4();
    QWidget* createCombinedColumn();
    void updateData(const STParamInfo& param);
    void initWorkerThread();

    // UI 控件映射
    QMap<QString, StyledLedLabel*> m_ledMap;
    QMap<QString, StyledLineEdit*> m_valueMap;

    STParamInfo m_param;

    // 回放
    QByteArray   m_dataCache;
    QMutex       m_cacheMutex;
    QTimer      *m_updateTimer = nullptr;
    QThread     *m_workerThread = nullptr;
    FrameCopyWorker *m_worker = nullptr;
};

#endif // CopyFrameDialog_H
