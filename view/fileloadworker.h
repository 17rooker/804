#ifndef FILELOADWORKER_H
#define FILELOADWORKER_H

#include <QObject>
#include <QStringList>
#include <QMap>
#include <QVector>

class FileLoadWorker : public QObject
{
    Q_OBJECT
public:
    QStringList m_headers;
    QMap<QString, QVector<double>> m_plotData;

public slots:
    void loadFile(const QString &filePath);

signals:
    void progressChanged(int percent, const QString &status);
    void finished();
};

#endif // FILELOADWORKER_H
