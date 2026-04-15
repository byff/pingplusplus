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
    void onEngineStarted();
    void onEngineStopped();
    void onTargetCountChanged(int count);
    void onContextMenu(const QPoint &pos);
    void onAbout();
    void onToggleTheme();
    void onSettings();
    void onUpdateTable();

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
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;

    // Pimpl pattern for widget pointers
    struct PrivateUi
    {
        // Toolbar
        QToolBar *toolbar = nullptr;
        QPushButton *btnStart = nullptr;
        QPushButton *btnStop = nullptr;
        QPushButton *btnClear = nullptr;
        QPushButton *btnImport = nullptr;
        QPushButton *btnExport = nullptr;
        QPushButton *btnInsertResults = nullptr;
        QPushButton *btnSettings = nullptr;
        QPushButton *btnAbout = nullptr;
        QPushButton *btnToggleTheme = nullptr;

        // Input area
        QWidget *inputWidget = nullptr;
        QTextEdit *textInput = nullptr;
        QLineEdit *editTimeout = nullptr;
        QDoubleSpinBox *spinInterval = nullptr;
        QSpinBox *spinPacketSize = nullptr;
        QSpinBox *spinConcurrent = nullptr;
        QCheckBox *checkContinuous = nullptr;
        QLineEdit *editFilter = nullptr;

        // Table view
        QTableView *tableView = nullptr;
        QHeaderView *tableHeader = nullptr;

        // Status bar
        QStatusBar *statusBar = nullptr;
        QLabel *labelOnline = nullptr;
        QLabel *labelOffline = nullptr;
        QLabel *labelElapsed = nullptr;
        QLabel *labelProgress = nullptr;
        QProgressBar *progressBar = nullptr;
        QLabel *labelStatus = nullptr;
    };

    PrivateUi ui;

    // Core components
    PingEngine *m_pingEngine = nullptr;
    ResultTableModel *m_tableModel = nullptr;
    QSortFilterProxyModel *m_proxyModel = nullptr;

    // Timers
    QTimer *m_refreshTimer = nullptr;
    QElapsedTimer *m_elapsedTimer = nullptr;

    // State
    Theme m_currentTheme = Theme::Light;
    bool m_isRunning = false;
    int m_onlineCount = 0;
    int m_offlineCount = 0;
    int m_totalTargets = 0;

    // Configuration defaults
    static constexpr int DEFAULT_TIMEOUT_MS = 3000;
    static constexpr double DEFAULT_INTERVAL_SEC = 1.0;
    static constexpr int DEFAULT_PACKET_SIZE = 64;
    static constexpr int DEFAULT_CONCURRENT = 10;
};
