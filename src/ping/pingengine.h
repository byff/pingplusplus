#pragma once
#include <QObject>
#include <QAbstractItemModel>
#include <QHostAddress>
#include <QUrl>
#include <QVariant>
#include <QTimer>
#include <memory>
#include <vector>
#include <atomic>
#include "pingresult.h"

class DnsCache;
class QThreadPool;
class PingWorker;

class PingEngine : public QObject {
    Q_OBJECT

public:
    explicit PingEngine(QObject* parent = nullptr);
    ~PingEngine();

    // Configuration
    void setTimeout(int timeoutMs);
    void setInterval(int intervalMs);
    void setPacketSize(int size);
    void setMaxConcurrent(int max);
    void setContinuousMode(bool continuous);

    int timeoutMs() const;
    int intervalMs() const;
    int packetSize() const;
    int maxConcurrent() const;
    bool continuousMode() const;

    // Targets
    Q_INVOKABLE void setTargets(const QStringList& targets);
    Q_INVOKABLE void start();
    Q_INVOKABLE void stop();
    Q_INVOKABLE void clear();
    Q_INVOKABLE bool isRunning() const;

    // Results - returns a flat list of all results
    Q_INVOKABLE QVariantList getAllResults() const;

    // Statistics
    Q_INVOKABLE int totalTargets() const;
    Q_INVOKABLE int onlineCount() const;
    Q_INVOKABLE int offlineCount() const;

signals:
    void started();
    void stopped();
    void resultsUpdated();
    void targetCountChanged(int count);
    void errorOccurred(const QString& error);

private slots:
    void onWorkerFinished(int index, bool success, qint64 rttUs);

private:
    void expandCidr(const QString& cidr, QStringList& out);
    void expandDomain(const QString& domain, QStringList& out);

public:
    QStringList parseInput(const QString& input);
    void startBatch();

    std::unique_ptr<DnsCache> m_dnsCache;
    QThreadPool* m_threadPool = nullptr;
    QTimer* m_continuousTimer = nullptr;

    std::vector<PingResult> m_results;
    std::atomic<bool> m_running{false};
    std::atomic<bool> m_stopRequested{false};

    int m_timeoutMs = 3000;
    int m_intervalMs = 1000;
    int m_packetSize = 56;
    int m_maxConcurrent = 500;
    bool m_continuousMode = false;

    int m_currentIndex = 0;
};
