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
    void loadConfig();

    QLineEdit *m_editServerIp;
    QLineEdit *m_editServerPort;
    QLineEdit *m_editMcastIp;
    QLineEdit *m_editMcastPort;
    QLineEdit *m_editDataPath;
    QPushButton *m_btnBrowse;
    QPushButton *m_btnApplyTcp;
    QPushButton *m_btnApplyUdp;
};

#endif // REMOTESETTINGWIDGET_H
