#pragma once
#include <QDialog>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QCheckBox>
#include <QComboBox>
#include <QLineEdit>
#include <QMap>

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

    // Column visibility settings
    QMap<QString, bool> columnVisibility() const;
    void setColumnVisibility(const QMap<QString, bool>& visibility);

    // Export field settings
    QMap<QString, bool> exportFields() const;
    void setExportFields(const QMap<QString, bool>& fields);

signals:
    void configApplied();
    void themeChangedFromDialog(const QString &theme);

private slots:
    void onApply();
    void onOk();

private:
    void setupUi();
    void loadSettings();

    QSpinBox *m_spinTimeout = nullptr;
    QSpinBox *m_spinInterval = nullptr;
    QSpinBox *m_spinPacketSize = nullptr;
    QSpinBox *m_spinConcurrent = nullptr;
    QSpinBox *m_spinRefresh = nullptr;
    QCheckBox *m_checkContinuous = nullptr;
    QCheckBox *m_checkRemember = nullptr;
    QComboBox *m_comboTheme = nullptr;

    // Column visibility
    QMap<QString, QCheckBox*> m_columnCheckboxes;

    // Export fields
    QMap<QString, QCheckBox*> m_exportFieldCheckboxes;
};
