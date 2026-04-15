#pragma once
#include <QObject>
#include <QSettings>
#include <QString>
#include <QMap>
#include <QStringList>
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

    // Column visibility settings
    QMap<QString, bool> columnVisibility() const;
    void setColumnVisibility(const QMap<QString, bool>& visibility);

    // Export field settings
    QMap<QString, bool> exportFields() const;
    void setExportFields(const QMap<QString, bool>& fields);

signals:
    void configChanged();
private:
    ConfigManager(QObject* parent = nullptr);
    ~ConfigManager();
    Q_DISABLE_COPY(ConfigManager)
    std::unique_ptr<QSettings> m_settings;
};
