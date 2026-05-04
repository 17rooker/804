#ifndef REMOTESETTINGWIDGET_H
#define REMOTESETTINGWIDGET_H

#include <QWidget>
#include "src/CustomMessage/IMessage.h"

class QLineEdit;
class QPushButton;
class QLabel;

class RemoteSettingWidget : public QWidget, public IMessage
{
    Q_OBJECT

public:
    explicit RemoteSettingWidget(QWidget *parent = nullptr);
    ~RemoteSettingWidget();
    bool condition() override { return isVisible(); }
    void onMessage(IEvent* pEvent) override;

private slots:
    void onBrowseFolder();
    void onApplyTcp();
    void onApplyUdp();

private:
    void setupUI();
    void setupDisplayUI(QWidget *parent);
    void loadConfig();

    QLineEdit *m_editServerIp;
    QLineEdit *m_editServerPort;
    QLineEdit *m_editMcastIp;
    QLineEdit *m_editMcastPort;
    QLineEdit *m_editDataPath;
    QPushButton *m_btnBrowse;
    QPushButton *m_btnApplyTcp;
    QPushButton *m_btnApplyUdp;

    // TCP运控数据显示
    QLabel *m_lblTcpFrameType  = nullptr;
    QLabel *m_lblTcpFrameCount = nullptr;
    QLabel *m_lblTcpWordCount  = nullptr;
    QLabel *m_lblTcpWordType   = nullptr;
    QLabel *m_lblTcpSrc       = nullptr;
    QLabel *m_lblTcpDst       = nullptr;
    QLabel *m_lblTcpTime      = nullptr;
    QLabel *m_lblTcpDate      = nullptr;
};

#endif // REMOTESETTINGWIDGET_H
