#ifndef CONTROLLER422DIALOG_H
#define CONTROLLER422DIALOG_H

#include <QDialog>
#include <QMap>
#include <QPushButton>

struct BtnInfo {
    QString name;
    bool active;
};

namespace Ui {
class Controller422Dialog;
}

class Controller422Dialog : public QDialog
{
    Q_OBJECT

public:
    explicit Controller422Dialog(QWidget *parent = nullptr);
    ~Controller422Dialog();

private slots:
    void onMatrixButtonClicked();
    void onFrameClicked();
    void onSendClicked();

private:
    QMap<QPushButton*, BtnInfo> m_matrixButtons;
    void updateButtonStyle(QPushButton *btn);

    // --- 新增：CRC 相关 ---
    quint16 calculateCRC16(const QByteArray &data);
    static const quint16 crc16_table[256]; // CRC 表声明
    QByteArray m_currentDat;
};

#endif // CONTROLLER422DIALOG_H
