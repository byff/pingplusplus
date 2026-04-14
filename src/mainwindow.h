#pragma once

#include <QMainWindow>
#include <QTimer>
#include <QElapsedTimer>
#include <QTextEdit>
#include <QLineEdit>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QTableView>
#include <QStandardItemModel>
#include <QToolBar>
#include <QStatusBar>
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>
#include <QComboBox>
#include <QCheckBox>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QWidget>
#include <QSortFilterProxyModel>
#include <QHeaderView>
#include <QFileDialog>
#include <QMessageBox>
#include <QApplication>
#include <QStyleFactory>

class PingEngine;
class ResultTableModel;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

    enum class Theme { Light, Dark };

    void setTheme(Theme theme);
    Theme currentTheme() const { return m_currentTheme; }

signals:
    void themeChanged(Theme theme);
    void pingResultAdded(const QString &host, bool online, int latencyMs);
    void progressChanged(int current, int total);

private slots:
    void onStart();
    void onStop();
    void onClear();
    void onImport();
    void onExport();
    void onInsertResults();
    void onConfigChanged();
    void onSortChanged(int logicalIndex);
    void onFilterChanged(const QString &text);
    void onRefreshTimer();
    void onPingResultReady(const QString &host, bool online, int latencyMs, const QString &timestamp);
    void onEngineFinished();
    void onAbout();
    void onExportResults();
    void onImportResults();
    void onToggleTheme();
    void onSettings();
    void onInsertSampleData();

private:
    void setupUi();
    void setupToolbar();
    void setupInputArea();
    void setupTableView();
    void setupStatusBar();
    void createConnections();
    void updateStatusBar();
    void updateCounts(int online, int offline);
    void setUiEnabled(bool enabled);
    QStringList parseInput(const QString &input) const;
    bool validateInput(const QString &input) const;
    void showError(const QString &title, const QString &message);
    void showInfo(const QString &title, const QString &message);

    // Pimpl pattern for widget pointers
    struct PrivateUi
    {
        // Toolbar
        QToolBar *toolbar;
        QPushButton *btnStart;
        QPushButton *btnStop;
        QPushButton *btnClear;
        QPushButton *btnImport;
        QPushButton *btnExport;
        QPushButton *btnInsertResults;
        QPushButton *btnSettings;
        QPushButton *btnAbout;
        QPushButton *btnToggleTheme;

        // Input area
        QWidget *inputWidget;
        QTextEdit *textInput;
        QLineEdit *editTimeout;
        QDoubleSpinBox *spinInterval;
        QSpinBox *spinPacketSize;
        QSpinBox *spinConcurrent;
        QLineEdit *editFilter;

        // Table view
        QTableView *tableView;
        QHeaderView *tableHeader;

        // Status bar
        QStatusBar *statusBar;
        QLabel *labelOnline;
        QLabel *labelOffline;
        QLabel *labelElapsed;
        QLabel *labelProgress;
        QProgressBar *progressBar;
        QLabel *labelStatus;
    };

    PrivateUi ui;

    // Core components
    PingEngine *m_pingEngine;
    ResultTableModel *m_tableModel;
    QSortFilterProxyModel *m_proxyModel;

    // Timers
    QTimer *m_refreshTimer;
    QElapsedTimer *m_elapsedTimer;

    // State
    Theme m_currentTheme;
    bool m_isRunning;
    int m_onlineCount;
    int m_offlineCount;
    int m_totalTargets;

    // Configuration defaults
    static constexpr int DEFAULT_TIMEOUT_MS = 3000;
    static constexpr double DEFAULT_INTERVAL_SEC = 1.0;
    static constexpr int DEFAULT_PACKET_SIZE = 64;
    static constexpr int DEFAULT_CONCURRENT = 10;
};

#endif // MAINWINDOW_H
