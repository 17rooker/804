#include "StyleEventFilter.h"
#include <QEvent>
#include <QTimer>
#include <QFile>


void StyleEventFilter::setStyleSheet(const QString &path)
{
    QFile file(path);
    if (file.open(QFile::ReadOnly)) {
        m_styleSheet = QString::fromUtf8(file.readAll());
        // 只保存样式，不立即应用到整个应用
        file.close();
    }

}

bool StyleEventFilter::eventFilter(QObject* watched, QEvent* event) {
    if (event->type() == QEvent::Show || event->type() == QEvent::Polish) {
        if (QWidget* widget = qobject_cast<QWidget*>(watched)) {
            if (m_registeredWidgets.contains(widget)) {
                // 延迟应用样式，确保窗口已完全创建
                QTimer::singleShot(0, [this, widget]() {
                    widget->setStyleSheet(m_styleSheet);
                });
            }
        }
    }
    return QObject::eventFilter(watched, event);
}


void StyleEventFilter::registerWidget(QWidget* widget) {
    if (widget) {
        widget->installEventFilter(this);
        m_registeredWidgets.insert(widget);
    }
}
