#ifndef STYLEEVENTFILTER_H
#define STYLEEVENTFILTER_H

#include <QObject>
#include <QWidget>
#include <QSet>


class StyleEventFilter : public QObject {
    Q_OBJECT
public:

    static StyleEventFilter& getInstance()
    {
        static StyleEventFilter s_ins;

        return s_ins;
    }
    void setStyleSheet(const QString& path);
    void registerWidget(QWidget* widget);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:

    QString m_styleSheet;
    QSet<QWidget*> m_registeredWidgets;
};

#endif // STYLEEVENTFILTER_H
