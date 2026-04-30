
#ifndef STYLELEDLABEL_H
#define STYLELEDLABEL_H
#include <QLabel>
#include <QMouseEvent>

class StyledLedLabel : public QLabel
{
    Q_OBJECT
    Q_PROPERTY(bool on READ isOn WRITE setOn)
    // 改为：禁用状态属性（替代原置灰）
    Q_PROPERTY(bool disabled READ isDisabledLed WRITE setDisabledLed)

public:
    explicit StyledLedLabel(QWidget *parent = nullptr);

    // 亮灭状态
    void setOn(bool on);
    bool isOn() const;

    // 可切换状态
    void setSwitchable(bool enable);
    bool isSwitchable() const;

    // 改为：禁用状态接口（替代原置灰）
    void setDisabledLed(bool disabled);
    bool isDisabledLed() const;

protected:
    void mousePressEvent(QMouseEvent *event) override;

private:
    bool m_on;            // 亮/灭
    bool m_isSwitchable;  // 可点击切换
    bool m_isDisabled;    // 禁用状态（替代原置灰）
    void updateStyle();   // 更新样式
};

#endif // STYLELEDLABEL_H
