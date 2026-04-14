#include "dnscache.h"

#include <QThread>
#include <cstring>

#ifdef Q_OS_WIN
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#endif

DnsCache::DnsCache(QObject* parent)
    : QObject(parent)
{
}

QList<QHostAddress> DnsCache::resolve(const QString& domain)
{
    if (domain.isEmpty()) {
        return QList<QHostAddress>();
    }

    QMutexLocker locker(&m_mutex);

    // Check cache first
    if (m_cache.contains(domain)) {
        return m_cache.value(domain);
    }

    QList<QHostAddress> addresses;

    // Use getaddrinfo for DNS resolution (supports both IPv4 and IPv6)
    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;    // Both IPv4 and IPv6
    hints.ai_socktype = SOCK_STREAM;

    struct addrinfo* result = nullptr;
    int ret = getaddrinfo(domain.toLocal8Bit().constData(), nullptr, &hints, &result);

    if (ret == 0 && result != nullptr) {
        struct addrinfo* rp = result;
        for (rp = result; rp != nullptr; rp = rp->ai_next) {
            if (rp->ai_family == AF_INET) {
                // IPv4
                struct sockaddr_in* sockaddr_in = reinterpret_cast<struct sockaddr_in*>(rp->ai_addr);
                char ipstr[INET_ADDRSTRLEN];
                inet_ntop(AF_INET, &(sockaddr_in->sin_addr), ipstr, INET_ADDRSTRLEN);
                addresses.append(QHostAddress(QString::fromLocal8Bit(ipstr)));
            } else if (rp->ai_family == AF_INET6) {
                // IPv6
                struct sockaddr_in6* sockaddr_in6 = reinterpret_cast<struct sockaddr_in6*>(rp->ai_addr);
                char ipstr[INET6_ADDRSTRLEN];
                inet_ntop(AF_INET6, &(sockaddr_in6->sin6_addr), ipstr, INET6_ADDRSTRLEN);
                addresses.append(QHostAddress(QString::fromLocal8Bit(ipstr)));
            }
        }
        freeaddrinfo(result);
    }

    // Enforce cache size limit
    if (m_cache.size() >= m_maxCacheSize && !m_cache.contains(domain)) {
        // Remove first entry (oldest)
        if (!m_cache.isEmpty()) {
            m_cache.remove(m_cache.firstKey());
        }
    }

    // Cache the result (even if empty, to avoid repeated lookups)
    m_cache.insert(domain, addresses);

    locker.unlock();
    emit resolved(domain, addresses);

    return addresses;
}

void DnsCache::clear()
{
    QMutexLocker locker(&m_mutex);
    m_cache.clear();
}

void DnsCache::put(const QString& domain, const QList<QHostAddress>& addresses)
{
    QMutexLocker locker(&m_mutex);

    // Enforce cache size limit before inserting
    if (m_cache.size() >= m_maxCacheSize && !m_cache.contains(domain)) {
        if (!m_cache.isEmpty()) {
            m_cache.remove(m_cache.firstKey());
        }
    }

    m_cache.insert(domain, addresses);
}
