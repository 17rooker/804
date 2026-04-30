#ifndef COLLECTIONCRITERIAWIDGET_H
#define COLLECTIONCRITERIAWIDGET_H

#include <QLineEdit>
#include <QWidget>

class QLineEdit;

class CollectionCriteriaWidget : public QWidget
{
    Q_OBJECT

public:
    explicit CollectionCriteriaWidget(QWidget *parent = nullptr);
      QString getTensionUpper() const { return m_editTensionUpper->text(); }
      QString getTensionLower() const { return m_editTensionLower->text(); }
      QString getAngleUpper() const { return m_editAngleUpper->text(); }
      QString getAngleLower() const { return m_editAngleLower->text(); }
      QString getPressureUpper() const { return m_editPressureUpper->text(); }
      QString getPressureLower() const { return m_editPressureLower->text(); }
private:
    void setupUI();

    // --- 严格按照此顺序声明 ---
    QLineEdit *m_editTensionUpper;
    QLineEdit *m_editTensionLower;

    QLineEdit *m_editAngleUpper;
    QLineEdit *m_editAngleLower;

    QLineEdit *m_editPressureUpper;
    QLineEdit *m_editPressureLower;
};

#endif // COLLECTIONCRITERIAWIDGET_H
