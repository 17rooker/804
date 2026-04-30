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
    void onBrowseFolder(); // 槽函数：处理文件夹选择

private:
    void setupUI(); // 构建界面

    // 控件指针
    QLineEdit *m_editServerIp;
    QLineEdit *m_editServerPort;
    QLineEdit *m_editMcastIp;
    QLineEdit *m_editMcastPort;
    QLineEdit *m_editDataPath;
    QPushButton *m_btnBrowse;
};

#endif // REMOTESETTINGWIDGET_H
