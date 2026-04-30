#ifndef LEDBUTTON_H
#define LEDBUTTON_H

#include <QPushButton>

class LedButton : public QPushButton
{
    Q_OBJECT
public:
    explicit LedButton(const QString &text, QWidget *parent = nullptr);

private slots:
    void toggleState();

private:
    bool m_isActive; // 记录当前状态
    void updateStyle(); // 更新样式表
};

#endif // LEDBUTTON_H
