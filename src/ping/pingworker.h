#pragma once
#include <QRunnable>
#include <QHostAddress>
#include <QObject>
#include <qobjectdefs.h>

class PingWorker : public QObject, public QRunnable {
    Q_OBJECT
public:
    PingWorker(const QHostAddress& target, int timeoutMs, int payloadSize);
    
    void run() override;
    
signals:
    void finished(int index, bool success, qint64 rttUs);
    
private:
    QHostAddress m_target;
    int m_timeoutMs;
    int m_payloadSize;
    int m_index = 0;
    
    int rawSocket();
    bool sendPing(int sock, const QHostAddress& addr, int payloadSize);
    bool receivePing(int sock, int timeoutMs, qint64* outRttUs);
};
