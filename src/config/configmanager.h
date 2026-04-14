#pragma once
#include <QObject>
#include <QString>
#include <QSettings>
#include <memory>

class ConfigManager : public QObject {
    Q_OBJECT
public:
    static ConfigManager& instance();
    int timeoutMs() const; void setTimeoutMs(int ms);
    int intervalMs() const; void setIntervalMs(int ms);
    int packetSize() const; void setPacketSize(int size);
    int maxConcurrent() const; void setMaxConcurrent(int max);
    bool continuousMode() const; void setContinuousMode(bool continuous);
    int refreshIntervalMs() const; void setRefreshIntervalMs(int ms);
    QString theme() const; void setTheme(const QString& theme);
    bool rememberAddresses() const; void setRememberAddresses(bool remember);
    QStringList lastAddresses() const; void setLastAddresses(const QStringList& addresses);
    QString configFilePath() const;
signals:
    void configChanged();
private:
    ConfigManager(QObject* parent = nullptr);
    ~ConfigManager();
    Q_DISABLE_COPY(ConfigManager)
    std::unique_ptr<QSettings> m_settings;
};
