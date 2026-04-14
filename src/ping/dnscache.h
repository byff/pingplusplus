#pragma once
#include <QObject>
#include <QMap>
#include <QString>
#include <QHostAddress>
#include <QMutex>

class DnsCache : public QObject {
    Q_OBJECT
public:
    explicit DnsCache(QObject* parent = nullptr);
    
    // Resolve a domain to IP addresses. Returns list of QHostAddress.
    // Uses cache if available.
    QList<QHostAddress> resolve(const QString& domain);
    
    // Clear cache
    void clear();
    
    // Pre-populate cache
    void put(const QString& domain, const QList<QHostAddress>& addresses);
    
signals:
    void resolved(const QString& domain, const QList<QHostAddress>& addresses);

private:
    QMap<QString, QList<QHostAddress>> m_cache;
    QMutex m_mutex;
    int m_maxCacheSize = 10000;
};
