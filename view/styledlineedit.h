#ifndef STYLEDLINEEDIT_H
#define STYLEDLINEEDIT_H

#include <QLineEdit>
#include <QColor>

class StyledLineEdit : public QLineEdit
{
    Q_OBJECT
public:
    explicit StyledLineEdit(QWidget *parent = nullptr);

    // 原有接口
    void setNormalBackground(const QColor &color);
    void setFocusBackground(const QColor &color);

    // 新增接口：启用【灰色默认+点击黑色】模式
    void setGrayInputMode(bool enable = true);

    // 新增接口：设置自定义固定宽度
    void setCustomFixedWidth(int width);
    // 新增接口：设置自定义固定高度
    void setCustomFixedHeight(int height);
    // 新增接口：同时设置自定义固定宽高
    void setCustomFixedSize(int width, int height);

protected:
    // 重写焦点事件（核心：点击切换背景）
    void focusInEvent(QFocusEvent *event) override;
    void focusOutEvent(QFocusEvent *event) override;

private:
    void updateStyleSheet();

    // 原有变量
    QColor m_normalBg;
    QColor m_focusBg;

    // 新增变量
    bool m_isGrayInputMode;
    QColor m_grayBg;
    QColor m_blackBg;
};

#endif // STYLEDLINEEDIT_H
