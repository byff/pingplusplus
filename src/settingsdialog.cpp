#include "settingsdialog.h"
#include "config/configmanager.h"
#include <QDialogButtonBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QLabel>
#include <QGroupBox>
#include <QMessageBox>

SettingsDialog::SettingsDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(QStringLiteral("设置"));
    setMinimumWidth(400);
    setupUi();

    // Load current values from ConfigManager
    ConfigManager &cfg = ConfigManager::instance();
    m_spinTimeout->setValue(cfg.timeoutMs());
    m_spinInterval->setValue(cfg.intervalMs());
    m_spinPacketSize->setValue(cfg.packetSize());
    m_spinConcurrent->setValue(cfg.maxConcurrent());
    m_spinRefresh->setValue(cfg.refreshIntervalMs());
    m_checkContinuous->setChecked(cfg.continuousMode());
    m_checkRemember->setChecked(cfg.rememberAddresses());

    QString theme = cfg.theme();
    if (theme == "light") m_comboTheme->setCurrentIndex(1);
    else if (theme == "dark") m_comboTheme->setCurrentIndex(2);
    else m_comboTheme->setCurrentIndex(0);
}

SettingsDialog::~SettingsDialog() = default;

void SettingsDialog::setupUi()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);

    // Ping settings group
    QGroupBox *pingGroup = new QGroupBox(QStringLiteral("Ping 参数"));
    QFormLayout *pingLayout = new QFormLayout(pingGroup);

    m_spinTimeout = new QSpinBox;
    m_spinTimeout->setRange(100, 30000);
    m_spinTimeout->setSuffix(QStringLiteral(" ms"));
    m_spinTimeout->setToolTip(QStringLiteral("等待响应的超时时间"));
    pingLayout->addRow(QStringLiteral("超时时间:"), m_spinTimeout);

    m_spinInterval = new QSpinBox;
    m_spinInterval->setRange(100, 60000);
    m_spinInterval->setSuffix(QStringLiteral(" ms"));
    m_spinInterval->setToolTip(QStringLiteral("持续探测时的探测间隔"));
    pingLayout->addRow(QStringLiteral("探测间隔:"), m_spinInterval);

    m_spinPacketSize = new QSpinBox;
    m_spinPacketSize->setRange(8, 65500);
    m_spinPacketSize->setToolTip(QStringLiteral("ICMP 数据包payload大小"));
    pingLayout->addRow(QStringLiteral("数据包大小:"), m_spinPacketSize);

    m_spinConcurrent = new QSpinBox;
    m_spinConcurrent->setRange(10, 5000);
    m_spinConcurrent->setToolTip(QStringLiteral("同时进行的最大Ping并发数"));
    pingLayout->addRow(QStringLiteral("并发数:"), m_spinConcurrent);

    m_checkContinuous = new QCheckBox(QStringLiteral("持续探测模式"));
    m_checkContinuous->setToolTip(QStringLiteral("持续不断探测，结果实时更新"));
    pingLayout->addRow(QStringLiteral("探测模式:"), m_checkContinuous);

    mainLayout->addWidget(pingGroup);

    // UI settings group
    QGroupBox *uiGroup = new QGroupBox(QStringLiteral("界面设置"));
    QFormLayout *uiLayout = new QFormLayout(uiGroup);

    m_spinRefresh = new QSpinBox;
    m_spinRefresh->setRange(200, 5000);
    m_spinRefresh->setSuffix(QStringLiteral(" ms"));
    m_spinRefresh->setToolTip(QStringLiteral("表格刷新间隔，太短影响性能"));
    uiLayout->addRow(QStringLiteral("刷新间隔:"), m_spinRefresh);

    m_comboTheme = new QComboBox;
    m_comboTheme->addItems({QStringLiteral("跟随系统"), QStringLiteral("浅色"), QStringLiteral("深色")});
    uiLayout->addRow(QStringLiteral("主题:"), m_comboTheme);

    m_checkRemember = new QCheckBox(QStringLiteral("记住上次输入的地址"));
    uiLayout->addRow(QStringLiteral("历史记录:"), m_checkRemember);

    mainLayout->addWidget(uiGroup);

    // Buttons
    QDialogButtonBox *buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Apply | QDialogButtonBox::Cancel);
    connect(buttons, &QDialogButtonBox::accepted, this, &SettingsDialog::onOk);
    connect(buttons, &QDialogButtonBox::clicked, this, [this](QAbstractButton *btn) {
        if (btn->text().contains("Apply")) onApply();
    });
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    mainLayout->addWidget(buttons);
}

void SettingsDialog::onApply()
{
    ConfigManager &cfg = ConfigManager::instance();
    cfg.setTimeoutMs(m_spinTimeout->value());
    cfg.setIntervalMs(m_spinInterval->value());
    cfg.setPacketSize(m_spinPacketSize->value());
    cfg.setMaxConcurrent(m_spinConcurrent->value());
    cfg.setRefreshIntervalMs(m_spinRefresh->value());
    cfg.setContinuousMode(m_checkContinuous->isChecked());
    cfg.setRememberAddresses(m_checkRemember->isChecked());

    QString theme;
    switch (m_comboTheme->currentIndex()) {
        case 1: theme = "light"; break;
        case 2: theme = "dark"; break;
        default: theme = "system"; break;
    }
    cfg.setTheme(theme);

    emit configApplied();
    emit themeChangedFromDialog(theme);
}

void SettingsDialog::onOk()
{
    onApply();
    accept();
}

int SettingsDialog::timeoutMs() const { return m_spinTimeout->value(); }
int SettingsDialog::intervalMs() const { return m_spinInterval->value(); }
int SettingsDialog::packetSize() const { return m_spinPacketSize->value(); }
int SettingsDialog::maxConcurrent() const { return m_spinConcurrent->value(); }
int SettingsDialog::refreshIntervalMs() const { return m_spinRefresh->value(); }
bool SettingsDialog::continuousMode() const { return m_checkContinuous->isChecked(); }
QString SettingsDialog::theme() const { return m_comboTheme->currentText(); }
bool SettingsDialog::rememberAddresses() const { return m_checkRemember->isChecked(); }
