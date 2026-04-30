#ifndef TOPSTATUSBAR_H
#define TOPSTATUSBAR_H

#include <QWidget>
#include <QTimer>
#include <QLabel>
#include <QDateTime>
#include <QHBoxLayout>

class TopStatusBar : public QWidget
{
    Q_OBJECT
public:
    TopStatusBar(QWidget *parent = nullptr) : QWidget(parent) {
        // UI布局
        m_beijingTimeLabel = new QLabel(this);
        m_testTimeLabel = new QLabel("00:00:00", this);
        m_statusLabel = new QLabel("机构准备好", this); // 对应右上角

        // 样式设置（模拟截图风格）
        setStyleSheet("QLabel { color: #00FF00; font-weight: bold; font-size: 16px; background: black; border: 1px solid gray; padding: 5px; }");
        m_statusLabel->setStyleSheet("QLabel { color: white; background: transparent; }");

        QHBoxLayout *layout = new QHBoxLayout(this);
        layout->addWidget(new QLabel("北京时间"));
        layout->addWidget(m_beijingTimeLabel);
        layout->addStretch();
        layout->addWidget(new QLabel("测试时间"));
        layout->addWidget(m_testTimeLabel);
        layout->addWidget(m_statusLabel);
        setLayout(layout);

        // 定时器
        // 1. 北京时间定时器 (1秒)
        QTimer *timeTimer = new QTimer(this);
        connect(timeTimer, &QTimer::timeout, this, [=](){
            QString timeStr = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss");
            m_beijingTimeLabel->setText(timeStr);
        });
        timeTimer->start(1000);

        // 2. 测试计时器 (1秒)
        m_testSeconds = 0;
        QTimer *testTimer = new QTimer(this);
        connect(testTimer, &QTimer::timeout, this, [=](){
            m_testSeconds++;
            QTime t(0,0,0);
            t = t.addSecs(m_testSeconds);
            m_testTimeLabel->setText(t.toString("hh:mm:ss"));
        });
        testTimer->start(1000);
    }

private:
    QLabel *m_beijingTimeLabel;
    QLabel *m_testTimeLabel;
    QLabel *m_statusLabel;
    int m_testSeconds;
};

#endif // TOPSTATUSBAR_H
