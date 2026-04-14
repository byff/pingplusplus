#include "configmanager.h"
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>

ConfigManager::ConfigManager(QObject* parent)
    : QObject(parent)
{
    QString configPath;

#ifdef Q_OS_WIN
    // Windows: portable (next to executable)
    configPath = QCoreApplication::applicationDirPath() + "/config.ini";
#else
    // Linux: ~/.config/pingtestpp/config.ini
    configPath = QDir::homePath() + "/.config/pingtestpp/config.ini";
    QDir configDir = QFileInfo(configPath).absoluteDir();
    if (!configDir.exists()) {
        configDir.mkpath(".");
    }
#endif

    m_settings = std::make_unique<QSettings>(configPath, QSettings::IniFormat);

    // Set default values if not already set
    if (!m_settings->contains("timeoutMs")) m_settings->setValue("timeoutMs", 3000);
    if (!m_settings->contains("intervalMs")) m_settings->setValue("intervalMs", 1000);
    if (!m_settings->contains("packetSize")) m_settings->setValue("packetSize", 56);
    if (!m_settings->contains("maxConcurrent")) m_settings->setValue("maxConcurrent", 500);
    if (!m_settings->contains("continuousMode")) m_settings->setValue("continuousMode", false);
    if (!m_settings->contains("refreshIntervalMs")) m_settings->setValue("refreshIntervalMs", 1000);
    if (!m_settings->contains("theme")) m_settings->setValue("theme", "system");
    if (!m_settings->contains("rememberAddresses")) m_settings->setValue("rememberAddresses", true);
}

ConfigManager::~ConfigManager() = default;

ConfigManager& ConfigManager::instance() {
    static ConfigManager instance;
    return instance;
}

int ConfigManager::timeoutMs() const {
    return m_settings->value("timeoutMs", 3000).toInt();
}

void ConfigManager::setTimeoutMs(int ms) {
    m_settings->setValue("timeoutMs", ms);
    emit configChanged();
}

int ConfigManager::intervalMs() const {
    return m_settings->value("intervalMs", 1000).toInt();
}

void ConfigManager::setIntervalMs(int ms) {
    m_settings->setValue("intervalMs", ms);
    emit configChanged();
}

int ConfigManager::packetSize() const {
    return m_settings->value("packetSize", 56).toInt();
}

void ConfigManager::setPacketSize(int size) {
    m_settings->setValue("packetSize", size);
    emit configChanged();
}

int ConfigManager::maxConcurrent() const {
    return m_settings->value("maxConcurrent", 500).toInt();
}

void ConfigManager::setMaxConcurrent(int max) {
    m_settings->setValue("maxConcurrent", max);
    emit configChanged();
}

bool ConfigManager::continuousMode() const {
    return m_settings->value("continuousMode", false).toBool();
}

void ConfigManager::setContinuousMode(bool continuous) {
    m_settings->setValue("continuousMode", continuous);
    emit configChanged();
}

int ConfigManager::refreshIntervalMs() const {
    return m_settings->value("refreshIntervalMs", 1000).toInt();
}

void ConfigManager::setRefreshIntervalMs(int ms) {
    m_settings->setValue("refreshIntervalMs", ms);
    emit configChanged();
}

QString ConfigManager::theme() const {
    return m_settings->value("theme", "system").toString();
}

void ConfigManager::setTheme(const QString& theme) {
    m_settings->setValue("theme", theme);
    emit configChanged();
}

bool ConfigManager::rememberAddresses() const {
    return m_settings->value("rememberAddresses", true).toBool();
}

void ConfigManager::setRememberAddresses(bool remember) {
    m_settings->setValue("rememberAddresses", remember);
    emit configChanged();
}

QStringList ConfigManager::lastAddresses() const {
    return m_settings->value("lastAddresses", QStringList()).toStringList();
}

void ConfigManager::setLastAddresses(const QStringList& addresses) {
    m_settings->setValue("lastAddresses", addresses);
    emit configChanged();
}

QString ConfigManager::configFilePath() const {
    return m_settings->fileName();
}
