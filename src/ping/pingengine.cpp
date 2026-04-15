#include "pingengine.h"
#include "dnscache.h"
#include "pingworker.h"

#include <QThreadPool>
#include <QHostAddress>
#include <QThread>
#include <QTimer>
#include <QDateTime>
#include <QMutex>
#include <QMutexLocker>
#include <QCoreApplication>
#include <QAbstractEventDispatcher>

#include <cstring>
#include <cstdlib>
#include <ctime>

#ifdef Q_OS_WIN
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
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

int PingEngine::timeoutMs() const { return m_timeoutMs; }
int PingEngine::intervalMs() const { return m_intervalMs; }
int PingEngine::packetSize() const { return m_packetSize; }
int PingEngine::maxConcurrent() const { return m_maxConcurrent; }
bool PingEngine::continuousMode() const { return m_continuousMode; }

void PingEngine::setTargets(const QStringList& targets) {
    QStringList allTargets;
    for (const QString& target : targets) {
        QStringList expanded = parseInput(target);
        allTargets.append(expanded);
    }

    if (allTargets.size() > 100000) {
        emit errorOccurred(QString("Warning: Target expansion resulted in %1 addresses. "
                                   "This may take a long time.").arg(allTargets.size()));
    }

    m_results.clear();
    m_results.reserve(allTargets.size());

    for (int i = 0; i < allTargets.size(); ++i) {
        const QString& target = allTargets[i];
        PingResult result;
        result.index = i;
        result.originalInput = target;

        QHostAddress addr;
        if (addr.setAddress(target)) {
            // It's an IP address
            result.targetIp = target;
        } else {
            // Try to resolve domain
            QList<QHostAddress> addresses = m_dnsCache->resolve(target);
            if (!addresses.isEmpty()) {
                result.resolvedDomain = target;
                result.targetIp = addresses.first().toString();
            } else {
                result.dnsFailed = true;
                result.targetIp = target;
            }
        }

        result.startTimeMs = QDateTime::currentMSecsSinceEpoch();
        result.isRunning = true;
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
    m_currentIndex = 0;

    emit started();

    if (m_continuousMode) {
        m_continuousTimer = new QTimer(this);
        m_continuousTimer->setSingleShot(false);
        connect(m_continuousTimer, &QTimer::timeout, this, &PingEngine::startBatch);
        m_continuousTimer->start(m_intervalMs);
        startBatch();
    } else {
        startBatch();
    }
}

void PingEngine::startBatch() {
    if (m_stopRequested || !m_running) return;

    int batchCount = 0;
    while (m_currentIndex < (int)m_results.size() && batchCount < m_maxConcurrent) {
        PingResult& result = m_results[m_currentIndex];

        if (result.dnsFailed) {
            result.isRunning = false;
            result.isOnline = false;
            result.lossRate = 1.0;
            result.lost = 1;
            emit resultsUpdated();
        } else {
            PingWorker* worker = new PingWorker(
                result.targetIp,
                m_timeoutMs,
                m_packetSize,
                m_currentIndex
            );
            worker->setProperty("index", m_currentIndex);
            connect(worker, &PingWorker::finished, this, &PingEngine::onWorkerFinished,
                    Qt::DirectConnection);
            worker->setAutoDelete(true);
            m_threadPool->start(worker);
            batchCount++;
        }

        m_currentIndex++;
    }

    if (m_continuousMode && m_currentIndex >= (int)m_results.size()) {
        m_currentIndex = 0;
    }
}

void PingEngine::stop() {
    if (!m_running) return;

    m_stopRequested = true;
    m_running = false;

    if (m_continuousTimer) {
        m_continuousTimer->stop();
        m_continuousTimer->deleteLater();
        m_continuousTimer = nullptr;
    }

    m_threadPool->waitForDone(5000);

    // Mark all still-running targets as stopped
    for (auto& result : m_results) {
        if (result.isRunning) {
            result.isRunning = false;
        }
    }

    emit stopped();
}

void PingEngine::clear() {
    stop();
    m_results.clear();
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

int PingEngine::totalTargets() const { return m_results.size(); }
int PingEngine::onlineCount() const {
    int count = 0;
    for (const auto& r : m_results) {
        if (r.isOnline) ++count;
    }
    return count;
}

int PingEngine::offlineCount() const {
    int count = 0;
    for (const auto& r : m_results) {
        if (!r.isOnline && !r.isRunning && !r.dnsFailed) ++count;
    }
    return count;
}

void PingEngine::onWorkerFinished(int index, bool success, qint64 rttUs) {
    if (index < 0 || index >= (int)m_results.size()) return;

    PingResult& result = m_results[index];

    result.sent++;
    result.lastRttUs = rttUs;

    if (success) {
        result.received++;
        result.isOnline = true;
        // Perform reverse DNS lookup on first successful ping
        if (result.hostname.isEmpty()) {
            result.hostname = reverseDnsLookup(result.targetIp);
        }
        if (rttUs >= 0) {
            if (result.minRttUs == 0 || rttUs < result.minRttUs) result.minRttUs = rttUs;
            if (rttUs > result.maxRttUs) result.maxRttUs = rttUs;
            result.avgRttUs = ((result.avgRttUs * (result.received - 1)) + rttUs) / result.received;
        }
    } else {
        result.lost++;
        result.isOnline = false;
    }

    result.lossRate = (result.sent > 0) ? static_cast<double>(result.lost) / result.sent : 0.0;
    result.elapsedMs = QDateTime::currentMSecsSinceEpoch() - result.startTimeMs;

    if (!m_continuousMode) {
        result.isRunning = false;
    }

    emit resultsUpdated();
}

void PingEngine::expandCidr(const QString& cidr, QStringList& out) {
    QString ip = cidr;
    int prefixLen = 32;

    if (cidr.contains('/')) {
        QStringList parts = cidr.split('/');
        ip = parts.value(0);
        prefixLen = parts.value(1).toInt();
    }

    QHostAddress addr;
    if (!addr.setAddress(ip)) return;

    quint32 ipInt = addr.toIPv4Address();
    if (prefixLen < 0 || prefixLen > 32) return;

    quint32 mask = (prefixLen == 0) ? 0 : ~((1u << (32 - prefixLen)) - 1);
    quint32 start = ipInt & mask;
    quint32 end = start | ~mask;

    quint64 count = static_cast<quint64>(end) - static_cast<quint64>(start) + 1;

    if (prefixLen < 16 && count > 65536) {
        emit errorOccurred(QString("Warning: CIDR %1 expands to %2 addresses. "
                                   "This may take a long time.").arg(cidr).arg(count));
    }

    const quint64 MAX_EXPANSION = 10000000;
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
    QList<QHostAddress> addresses = m_dnsCache->resolve(domain);
    for (const QHostAddress& addr : addresses) {
        if (addr.protocol() == QAbstractSocket::IPv4Protocol) {
            out.append(addr.toString());
        }
    }
    if (out.isEmpty()) {
        // Could not resolve - return domain itself as unresolved
        out.append(domain);
    }
}

QStringList PingEngine::parseInput(const QString& input) {
    QStringList result;
    QString trimmed = input.trimmed();
    if (trimmed.isEmpty()) return result;

    // CIDR notation
    if (trimmed.contains('/')) {
        QStringList parts = trimmed.split('/');
        bool ok;
        int prefixLen = parts.value(1).toInt(&ok);
        if (ok && prefixLen >= 0 && prefixLen <= 32) {
            expandCidr(trimmed, result);
            return result;
        }
    }

    // IP range (start-end)
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
                    quint64 count = static_cast<quint64>(endInt) - static_cast<quint64>(startInt) + 1;
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

    // Single IP
    QHostAddress addr;
    if (addr.setAddress(trimmed)) {
        result.append(trimmed);
        return result;
    }

    // Domain
    expandDomain(trimmed, result);
    return result;
}

QString PingEngine::reverseDnsLookup(const QString& ip) const {
    if (ip.isEmpty()) return QString();

#ifdef Q_OS_WIN
    struct hostent* he = nullptr;
    struct in_addr addr;
    addr.s_addr = inet_addr(ip.toLatin1().constData());
    if (addr.s_addr == INADDR_NONE) return QString();
    he = gethostbyaddr(reinterpret_cast<const char*>(&addr), sizeof(addr), AF_INET);
#else
    struct hostent* he = nullptr;
    struct in_addr addr;
    addr.s_addr = inet_addr(ip.toLatin1().constData());
    if (addr.s_addr == INADDR_NONE) return QString();
    he = gethostbyaddr(reinterpret_cast<const char*>(&addr), sizeof(addr), AF_INET);
#endif
    if (he && he->h_name) {
        return QString::fromLocal8Bit(he->h_name);
    }
    return QString();
}
