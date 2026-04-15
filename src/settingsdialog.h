#pragma once
#include <QDialog>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QCheckBox>
#include <QComboBox>
#include <QLineEdit>

class SettingsDialog : public QDialog {
    Q_OBJECT
public:
    explicit SettingsDialog(QWidget *parent = nullptr);
    ~SettingsDialog() override;

    int timeoutMs() const;
    int intervalMs() const;
    int packetSize() const;
    int maxConcurrent() const;
    int refreshIntervalMs() const;
    bool continuousMode() const;
    QString theme() const;
    bool rememberAddresses() const;

signals:
    void configApplied();
    void themeChangedFromDialog(const QString &theme);

private slots:
    void onApply();
    void onOk();

private:
    void setupUi();

    QSpinBox *m_spinTimeout = nullptr;
    QSpinBox *m_spinInterval = nullptr;
    QSpinBox *m_spinPacketSize = nullptr;
    QSpinBox *m_spinConcurrent = nullptr;
    QSpinBox *m_spinRefresh = nullptr;
    QCheckBox *m_checkContinuous = nullptr;
    QCheckBox *m_checkRemember = nullptr;
    QComboBox *m_comboTheme = nullptr;
};
