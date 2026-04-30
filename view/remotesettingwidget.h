#ifndef REMOTESETTINGWIDGET_H
#define REMOTESETTINGWIDGET_H

#include <QWidget>

class QLineEdit;
class QPushButton;

class RemoteSettingWidget : public QWidget
{
    Q_OBJECT

public:
    explicit RemoteSettingWidget(QWidget *parent = nullptr);
    ~RemoteSettingWidget();

private slots:
    void onBrowseFolder();          // 选择数据存储路径
    void onApplyTcp();              // 应用TCP配置
    void onApplyUdp();              // 应用UDP组播配置

private:
    void setupUI();                 // 构建界面
    void loadConfig();              // 从Info.ini加载当前值

    // 控件指针
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
