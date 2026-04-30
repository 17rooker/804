#ifndef POWERSETTINGWIDGE_H
#define POWERSETTINGWIDGE_H

#include <QWidget>
#include <QLineEdit>
#include <vector>
#include "src/Common/ConfigHelper.h"

// 电源配置结构体（用于存储IP/TCP端口/组播信息）
struct PowerConfig {
    QString ip;
    int tcpPort;
    QString mcastIp;
    int mcastPort;

    // 重载相等运算符，用于对比配置是否变化
    bool operator==(const PowerConfig& other) const {
        return ip == other.ip
               && tcpPort == other.tcpPort
               && mcastIp == other.mcastIp
               && mcastPort == other.mcastPort;
    }
    bool operator!=(const PowerConfig& other) const {
        return !(*this == other);
    }
};

// 用于存储数据的结构体
struct PowerRowData {
    QLineEdit *ipEdit;
    QLineEdit *tcpEdit;
    QLineEdit *mcastIpEdit;
    QLineEdit *mcastPortEdit;
};

class PowerSettingWidget : public QWidget
{
    Q_OBJECT

public:
    explicit PowerSettingWidget(QWidget *parent = nullptr);
    ~PowerSettingWidget();

    // 获取当前界面配置
    std::vector<PowerConfig> getCurrentConfigs() const;
    // 判断配置是否有改动
    bool hasConfigChanged() const;

private:
    // 1. 声明 setupUI
    void setupUI();

    // 2. 声明创建单行控件的函数 (不需要传递宽度参数了)
    QWidget* createStyledRow(const QString &ip, int tcp, const QString &mcastIp, int mcastPort);

    // 加载初始配置（从Info.ini读取）
    void loadOriginalConfigs();

    // 存储控件指针以便后续获取数据
    std::vector<PowerRowData> m_rows;
    // 存储初始配置（用于对比是否改动）
    std::vector<PowerConfig> m_originalConfigs;
    // 电源前缀（和main.cpp中的powerTcpPrefixes对应）
    const QList<QString> m_powerPrefixes = {"Power1", "Power2", "Power3_1", "Power4", "Power5", "Power6"};
};

#endif // POWERSETTINGWIDGE_H
