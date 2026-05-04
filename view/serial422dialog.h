#ifndef SERIAL422DIALOG_H
#define SERIAL422DIALOG_H

#include <QDialog>
#include <qcombobox.h>
#include <QList>
#include "src/Common/CommTypes.h"
#include "powersettingwidge.h"

// 前向声明
class CollectionCriteriaWidget;  // 新增：前向声明

// 控件组结构体
struct SerialGroup {
    QComboBox *cmbPort = nullptr;
    QLineEdit *editBaud = nullptr;
    QComboBox *cmbStop = nullptr;
    QComboBox *cmbParity = nullptr;
};

struct SerialControllerConfig {
    QString channelId;       // 通道唯一标识（如 "serial_E", "serial_F"）
    SerialConfig serialCfg;  // 串口配置
};

class QTabWidget;
class QPushButton;
class CollectionCoeffWidget;
class Serial422Dialog : public QDialog
{
    Q_OBJECT

public:
    explicit Serial422Dialog(QWidget *parent = nullptr);
    ~Serial422Dialog();

     double calculateCollectionCoeffSingleFormula(int row, int col, double x) const;
    // 获取单个控制器的配置（参数：控制器名称，如"ctrl1/ctrl2/ctrl3"）
    SerialControllerConfig getControllerConfig(const QString& ctrlName) const;

    // 获取所有控制器的配置
    QList<SerialControllerConfig> getAllControllerConfigs() const;

    // 检测串口配置是否发生了变化
    bool serialConfigChanged() const;

protected:
    // 重写QDialog的accept函数，整合配置生效逻辑
    void accept() override;

private:
    void setupUI();
    void scanPorts();

    // 辅助方法 - 将界面值转换为SerialConfig
    SerialConfig convertToSerialConfig(const SerialGroup& group) const;

    // --- 界面成员变量 ---
    QTabWidget *m_tabWidget;
    QWidget *m_tabSerial;

    // 三个控制器的数据组
    SerialGroup m_ctrl1;
    SerialGroup m_ctrl2;
    SerialGroup m_ctrl3;

    // 保存上次确认的串口配置，用于检测变化
    QList<SerialControllerConfig> m_originalSerialConfigs;

    // 按钮成员变量（移除了确认配置按钮）
    QPushButton *m_btnOk;
    QPushButton *m_btnCancel;
    CollectionCoeffWidget *m_coeffWidget;
    CollectionCriteriaWidget *m_criteriaWidget = nullptr;  // 新增：采集判据控件指针
    // 其他 Tab 占位
    QWidget *m_tabPower;
    QWidget *m_tabRemote;
    QWidget *m_tabCollectCoeff;
    QWidget *m_tabCollectJudge;
    QWidget *m_tabAirSupply;

    // 保存PowerSettingWidget实例指针
    PowerSettingWidget *m_powerWidget = nullptr;
    // 电源前缀（和main.cpp/powersettingwidge一致）
    const QList<QString> m_powerPrefixes = {"Power1", "Power2", "Power3_1", "Power4", "Power5", "Power6"};
};

#endif // SERIAL422DIALOG_H
