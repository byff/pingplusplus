#include "pingengine.h"
#include "dnscache.h"
#include "pingworker.h"

#include <QThreadPool>
#include <QRunnable>
#include <QHostAddress>
#include <QThread>
#include <QTimer>
#include <QHostInfo>
#include <QMutex>
#include <QMutexLocker>
#include <QCoreApplication>
#include <QDateTime>
#include <QAbstractEventDispatcher>

#include <cstring>
#include <cstdlib>
#include <ctime>
#include <cmath>

#ifdef Q_OS_WIN
#include <winsock2.h>
#include <ws2tcpip.h>
#include <icmpapi.h>
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/time.h>
#endif

// PingEngine implementation
PingEngine::PingEngine(QObject* parent)
    : QObject(parent)
    , m_threadPool(new QThreadPool(this))
    , m_dnsCache(new DnsCache(this))
    , m_continuousTimer(nullptr)
{
    m_threadPool->setMaxThreadCount(m_maxConcurrent);
}

PingEngine::~PingEngine() {
    stop();
}

void PingEngine::setTimeout(int timeoutMs) {
    m_timeoutMs = qMax(100, timeoutMs);
}

void PingEngine::setInterval(int intervalMs) {
    m_intervalMs = qMax(100, intervalMs);
}

void PingEngine::setPacketSize(int size) {
    m_packetSize = qMax(8, qMin(65535, size));
}

void PingEngine::setMaxConcurrent(int max) {
    m_maxConcurrent = qMax(1, qMin(10000, max));
    if (m_threadPool) {
        m_threadPool->setMaxThreadCount(m_maxConcurrent);
    }
}

void PingEngine::setContinuousMode(bool continuous) {
    m_continuousMode = continuous;
}

int PingEngine::timeoutMs() const {
    return m_timeoutMs;
}

int PingEngine::intervalMs() const {
    return m_intervalMs;
}

int PingEngine::packetSize() const {
    return m_packetSize;
}

int PingEngine::maxConcurrent() const {
    return m_maxConcurrent;
}

bool PingEngine::continuousMode() const {
    return m_continuousMode;
}

void PingEngine::setTargets(const QStringList& targets) {
    QStringList allTargets;
    for (const QString& target : targets) {
        QStringList expanded = parseInput(target);
        allTargets.append(expanded);
    }
    
    // Limit expansion warning
    if (allTargets.size() > 100000) {
        emit errorOccurred(QString("Warning: Target expansion resulted in %1 addresses. "
                                   "This may take a long time.").arg(allTargets.size()));
    }
    
    // Clear old results and prepare new ones
    m_results.clear();
    m_results.reserve(allTargets.size());
    
    for (const QString& target : allTargets) {
        PingResult result;
        result.target = target;
        
        // Parse IP or hostname
        QHostAddress addr;
        if (addr.setAddress(target)) {
            result.address = addr;
        } else {
            // Try to resolve domain
            QList<QHostAddress> addresses = m_dnsCache->resolve(target);
            if (!addresses.isEmpty()) {
                result.address = addresses.first();
            }
        }
        
        m_results.push_back(result);
    }
    
    emit targetCountChanged(m_results.size());
}

void PingEngine::start() {
    if (m_running) return;
    
    if (m_results.empty()) {
        emit errorOccurred("No targets to ping");
        return;
    }
    
    m_running = true;
    m_stopRequested = false;
    m_onlineCount = 0;
    m_offlineCount = 0;
    m_currentIndex = 0;
    
    emit started();
    
    if (m_continuousMode) {
        // Continuous mode: start a timer for repeated batches
        m_continuousTimer = new QTimer(this);
        m_continuousTimer->setSingleShot(false);
        connect(m_continuousTimer, &QTimer::timeout, this, &PingEngine::startBatch);
        m_continuousTimer->start(m_intervalMs);
        // Start first batch immediately
        startBatch();
    } else {
        // Single pass mode - ping all targets once
        startBatch();
    }
}

void PingEngine::startBatch() {
    if (m_stopRequested || !m_running) return;
    
    int batchCount = 0;
    
    // Process targets in batches based on m_maxConcurrent
    while (m_currentIndex < (int)m_results.size() && batchCount < m_maxConcurrent) {
        PingResult& result = m_results[m_currentIndex];
        
        if (!result.address.isNull()) {
            PingWorker* worker = new PingWorker(
                result.address,
                m_timeoutMs,
                m_packetSize
            );
            worker->setProperty("index", m_currentIndex);
            connect(worker, &PingWorker::finished, this, &PingEngine::onWorkerFinished,
                    Qt::QueuedConnection);
            worker->setAutoDelete(true);
            m_threadPool->start(worker);
            batchCount++;
        } else {
            // Could not resolve - mark as offline
            result.success = false;
            result.rttUs = -1;
            result.timestampMs = QDateTime::currentMSecsSinceEpoch();
            m_offlineCount++;
            emit resultsUpdated();
        }
        
        m_currentIndex++;
    }
    
    // If in continuous mode and we've gone through all targets, reset for next round
    if (m_continuousMode && m_currentIndex >= (int)m_results.size()) {
        m_currentIndex = 0;
    }
}

void PingEngine::stop() {
    if (!m_running) return;
    
    m_stopRequested = true;
    m_running = false;
    
    // Stop continuous timer if active
    if (m_continuousTimer) {
        m_continuousTimer->stop();
        m_continuousTimer->deleteLater();
        m_continuousTimer = nullptr;
    }
    
    // Stop all pending runnables
    m_threadPool->waitForDone(5000); // Wait up to 5 seconds
    
    emit stopped();
}

void PingEngine::clear() {
    stop();
    m_results.clear();
    m_onlineCount = 0;
    m_offlineCount = 0;
    m_currentIndex = 0;
    emit targetCountChanged(0);
    emit resultsUpdated();
}

bool PingEngine::isRunning() const {
    return m_running;
}

QVariantList PingEngine::getAllResults() const {
    QVariantList list;
    list.reserve(m_results.size());
    for (const PingResult& result : m_results) {
        list.append(result.toVariant());
    }
    return list;
}

int PingEngine::totalTargets() const {
    return m_results.size();
}

int PingEngine::onlineCount() const {
    return m_onlineCount;
}

int PingEngine::offlineCount() const {
    return m_offlineCount;
}

void PingEngine::onWorkerFinished(int index, bool success, qint64 rttUs) {
    // Handle case where index comes through variant
    int idx = index;
    if (sender()) {
        bool ok;
        int propIndex = sender()->property("index").toInt(&ok);
        if (ok) idx = propIndex;
    }
    
    if (idx >= 0 && idx < (int)m_results.size()) {
        PingResult& result = m_results[idx];
        result.success = success;
        result.rttUs = rttUs;
        result.timestampMs = QDateTime::currentMSecsSinceEpoch();
        
        if (success) {
            m_onlineCount++;
        } else {
            m_offlineCount++;
        }
        
        emit resultsUpdated();
    }
}

void PingEngine::expandCidr(const QString& cidr, QStringList& out) {
    QString ip = cidr;
    int prefixLen = 32;
    
    // Parse CIDR notation
    if (cidr.contains('/')) {
        QStringList parts = cidr.split('/');
        ip = parts.value(0);
        prefixLen = parts.value(1).toInt();
    }
    
    QHostAddress addr;
    if (!addr.setAddress(ip)) {
        return;
    }
    
    quint32 ipInt = addr.toIPv4Address();
    
    if (prefixLen < 0 || prefixLen > 32) {
        return;
    }
    
    // Calculate start and end IPs
    quint32 mask = prefixLen == 0 ? 0 : ~((1u << (32 - prefixLen)) - 1);
    quint32 start = ipInt & mask;
    quint32 end = start | ~mask;
    
    // Warn if expansion is very large (> /16)
    if (prefixLen < 16) {
        quint64 count = (quint64)end - (quint64)start + 1;
        emit errorOccurred(QString("Warning: CIDR %1 expands to %2 addresses. "
                                   "This may take a long time.").arg(cidr).arg(count));
    }
    
    // Limit expansion to prevent memory issues
    const quint64 MAX_EXPANSION = 10000000; // 10 million
    quint64 count = (quint64)end - (quint64)start + 1;
    if (count > MAX_EXPANSION) {
        emit errorOccurred(QString("Error: CIDR %1 expands to %2 addresses, "
                                   "exceeding maximum of %3.").arg(cidr).arg(count).arg(MAX_EXPANSION));
        return;
    }
    
    for (quint32 i = start; i <= end; ++i) {
        QHostAddress expanded(i);
        out.append(expanded.toString());
    }
}

void PingEngine::expandDomain(const QString& domain, QStringList& out) {
    // Use DNS to resolve domain to IP addresses via DnsCache
    QList<QHostAddress> addresses = m_dnsCache->resolve(domain);
    
    for (const QHostAddress& addr : addresses) {
        // Only include IPv4 addresses for ICMP
        if (addr.protocol() == QAbstractSocket::IPv4Protocol) {
            out.append(addr.toString());
        }
    }
    
    // If no addresses found, add original domain for later resolution
    if (out.isEmpty()) {
        out.append(domain);
    }
}

QStringList PingEngine::parseInput(const QString& input) {
    QStringList result;
    QString trimmed = input.trimmed();
    
    if (trimmed.isEmpty()) {
        return result;
    }
    
    // Check if it's a CIDR notation
    if (trimmed.contains('/')) {
        QStringList parts = trimmed.split('/');
        bool ok;
        int prefixLen = parts.value(1).toInt(&ok);
        
        if (ok && prefixLen >= 0 && prefixLen <= 32) {
            expandCidr(trimmed, result);
            return result;
        }
    }
    
    // Check if it's an IP range (start-end)
    if (trimmed.contains('-')) {
        QStringList parts = trimmed.split('-');
        if (parts.size() == 2) {
            QString startIp = parts[0].trimmed();
            QString endIp = parts[1].trimmed();
            
            QHostAddress start, end;
            if (start.setAddress(startIp) && end.setAddress(endIp)) {
                quint32 startInt = start.toIPv4Address();
                quint32 endInt = end.toIPv4Address();
                
                if (endInt >= startInt) {
                    quint64 count = (quint64)endInt - (quint64)startInt + 1;
                    if (count > 100000) {
                        emit errorOccurred(QString("Warning: IP range expands to %1 addresses.").arg(count));
                    }
                    
                    for (quint32 i = startInt; i <= endInt; ++i) {
                        QHostAddress addr(i);
                        result.append(addr.toString());
                    }
                    return result;
                }
            }
        }
    }
    
    // Check if it's a single IP
    QHostAddress addr;
    if (addr.setAddress(trimmed)) {
        result.append(trimmed);
        return result;
    }
    
    // Otherwise treat as domain name
    expandDomain(trimmed, result);
    return result;
}
