#pragma once
#include <QRunnable>
#include <QObject>
#include <QString>
#include <qobjectdefs.h>

class PingWorker : public QObject, public QRunnable {
    Q_OBJECT
public:
    PingWorker(const QString& targetIp, int timeoutMs, int payloadSize, int index);

    void run() override;

signals:
    void finished(int index, bool success, qint64 rttUs);

private:
    QString m_targetIp;
    int m_timeoutMs;
    int m_payloadSize;
    int m_index;
};
