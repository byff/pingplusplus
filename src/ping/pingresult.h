#pragma once
#include <QString>
#include <QVariant>
#include <atomic>

struct PingResult {
    int index = 0;
    QString originalInput;
    QString targetIp;
    QString resolvedDomain;
    int sent = 0;
    int received = 0;
    int lost = 0;
    double lossRate = 0.0;
    qint64 minRttUs = 0;
    qint64 maxRttUs = 0;
    qint64 avgRttUs = 0;
    qint64 lastRttUs = 0;
    bool isOnline = false;
    bool isRunning = false;
    bool dnsFailed = false;
    qint64 startTimeMs = 0;
    qint64 elapsedMs = 0;
    QString statusText() const;
    double lossRatePercent() const { return lossRate * 100.0; }
    double avgRttMs() const { return avgRttUs / 1000.0; }
    double minRttMs() const { return minRttUs / 1000.0; }
    double maxRttMs() const { return maxRttUs / 1000.0; }
    double lastRttMs() const { return lastRttUs / 1000.0; }
    QString elapsedText() const;
    QVariant toVariant() const;
    static PingResult fromVariant(const QVariant& v);
};
