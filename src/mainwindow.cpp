#include "mainwindow.h"
#include "ping/pingengine.h"
#include "model/resulttablemodel.h"
#include "export/excelexporter.h"
#include "import/excelimporter.h"

#include <QMenuBar>
#include <QMenu>
#include <QAction>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QDebug>
#include <QTimer>
#include <QTime>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QFrame>
#include <QScrollArea>
#include <QHeaderView>
#include <QAbstractItemView>
#include <QItemSelectionModel>
#include <QItemSelection>
#include <QModelIndexList>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_pingEngine(new PingEngine(this))
    , m_tableModel(new ResultTableModel(this))
    , m_proxyModel(new QSortFilterProxyModel(this))
    , m_refreshTimer(new QTimer(this))
    , m_elapsedTimer(new QElapsedTimer)
    , m_currentTheme(Theme::Light)
    , m_isRunning(false)
    , m_onlineCount(0)
    , m_offlineCount(0)
    , m_totalTargets(0)
{
    setupUi();
    createConnections();
    setAcceptDrops(true);
}

MainWindow::~MainWindow() = default;

void MainWindow::setupUi()
{
    setWindowTitle("pingtestpp");
    resize(1200, 700);

    setupToolbar();
    setupInputArea();
    setupTableView();
    setupStatusBar();

    QWidget *centralWidget = new QWidget;
    QVBoxLayout *mainLayout = new QVBoxLayout(centralWidget);
    mainLayout->setContentsMargins(2, 2, 2, 2);
    mainLayout->setSpacing(2);
    mainLayout->addWidget(ui.inputWidget);
    mainLayout->addWidget(ui.tableView, 1);
    setCentralWidget(centralWidget);
}

void MainWindow::setupToolbar()
{
    ui.toolbar = new QToolBar("Main Toolbar");
    ui.toolbar->setMovable(false);
    addToolBar(ui.toolbar);

    ui.btnStart = new QPushButton("开始");
    ui.btnStop = new QPushButton("停止");
    ui.btnClear = new QPushButton("清除");
    ui.btnImport = new QPushButton("导入");
    ui.btnExport = new QPushButton("导出");
    ui.btnInsertResults = new QPushButton("插入结果");
    ui.btnSettings = new QPushButton("设置");
    ui.btnAbout = new QPushButton("关于");
    ui.btnToggleTheme = new QPushButton("切换主题");

    ui.btnStop->setEnabled(false);

    ui.toolbar->addWidget(ui.btnStart);
    ui.toolbar->addWidget(ui.btnStop);
    ui.toolbar->addWidget(ui.btnClear);
    ui.toolbar->addSeparator();
    ui.toolbar->addWidget(ui.btnImport);
    ui.toolbar->addWidget(ui.btnExport);
    ui.toolbar->addWidget(ui.btnInsertResults);
    ui.toolbar->addSeparator();
    ui.toolbar->addWidget(ui.btnSettings);
    ui.toolbar->addWidget(ui.btnAbout);
    ui.toolbar->addSeparator();
    ui.toolbar->addWidget(ui.btnToggleTheme);
}

void MainWindow::setupInputArea()
{
    ui.inputWidget = new QWidget;

    QVBoxLayout *inputLayout = new QVBoxLayout(ui.inputWidget);
    inputLayout->setContentsMargins(0, 0, 0, 0);

    // Target input group
    QGroupBox *targetGroup = new QGroupBox("输入目标");
    QVBoxLayout *targetLayout = new QVBoxLayout(targetGroup);
    ui.textInput = new QTextEdit;
    ui.textInput->setPlaceholderText("支持多行输入，每行一个IP/域名/CIDR，支持如：\n192.168.1.1\n10.0.0.0/24\nexample.com");
    ui.textInput->setMaximumHeight(120);
    targetLayout->addWidget(ui.textInput);

    // Config group
    QGroupBox *configGroup = new QGroupBox("配置");
    QGridLayout *configLayout = new QGridLayout(configGroup);

    // Timeout
    QLabel *labelTimeout = new QLabel("超时(ms):");
    ui.editTimeout = new QLineEdit;
    ui.editTimeout->setText(QString::number(DEFAULT_TIMEOUT_MS));
    ui.editTimeout->setMaximumWidth(100);

    // Interval
    QLabel *labelInterval = new QLabel("间隔(ms):");
    ui.spinInterval = new QDoubleSpinBox;
    ui.spinInterval->setRange(100, 60000);
    ui.spinInterval->setValue(DEFAULT_INTERVAL_SEC * 1000);
    ui.spinInterval->setSuffix(" ms");
    ui.spinInterval->setMaximumWidth(120);

    // Packet size
    QLabel *labelPacketSize = new QLabel("数据包大小:");
    ui.spinPacketSize = new QSpinBox;
    ui.spinPacketSize->setRange(8, 65500);
    ui.spinPacketSize->setValue(DEFAULT_PACKET_SIZE);
    ui.spinPacketSize->setMaximumWidth(100);

    // Concurrent
    QLabel *labelConcurrent = new QLabel("并发数:");
    ui.spinConcurrent = new QSpinBox;
    ui.spinConcurrent->setRange(10, 5000);
    ui.spinConcurrent->setValue(DEFAULT_CONCURRENT);
    ui.spinConcurrent->setMaximumWidth(100);

    // Filter
    QLabel *labelFilter = new QLabel("过滤:");
    ui.editFilter = new QLineEdit;
    ui.editFilter->setPlaceholderText("输入过滤关键字...");
    ui.editFilter->setMaximumWidth(200);

    configLayout->addWidget(labelTimeout, 0, 0);
    configLayout->addWidget(ui.editTimeout, 0, 1);
    configLayout->addWidget(labelInterval, 0, 2);
    configLayout->addWidget(ui.spinInterval, 0, 3);
    configLayout->addWidget(labelPacketSize, 1, 0);
    configLayout->addWidget(ui.spinPacketSize, 1, 1);
    configLayout->addWidget(labelConcurrent, 1, 2);
    configLayout->addWidget(ui.spinConcurrent, 1, 3);
    configLayout->addWidget(labelFilter, 2, 0);
    configLayout->addWidget(ui.editFilter, 2, 1, 1, 3);

    inputLayout->addWidget(targetGroup);
    inputLayout->addWidget(configGroup);
}

void MainWindow::setupTableView()
{
    m_proxyModel->setSourceModel(m_tableModel);
    m_proxyModel->setFilterCaseSensitivity(Qt::CaseInsensitive);

    ui.tableView = new QTableView;
    ui.tableView->setModel(m_proxyModel);
    ui.tableView->setAlternatingRowColors(true);
    ui.tableView->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui.tableView->setSortingEnabled(true);
    ui.tableView->setAcceptDrops(false);
    ui.tableView->setDropIndicatorShown(false);
    ui.tableView->setContextMenuPolicy(Qt::CustomContextMenu);

    ui.tableHeader = ui.tableView->horizontalHeader();
    ui.tableHeader->setStretchLastSection(false);
    ui.tableHeader->setSectionResizeMode(QHeaderView::Interactive);
    ui.tableHeader->setSortIndicatorShown(true);
    ui.tableHeader->setSortIndicator(0, Qt::AscendingOrder);

    // Set column widths
    ui.tableView->setColumnWidth(ResultTableModel::ColNum, 50);
    ui.tableView->setColumnWidth(ResultTableModel::ColStatus, 80);
    ui.tableView->setColumnWidth(ResultTableModel::ColTarget, 150);
    ui.tableView->setColumnWidth(ResultTableModel::ColIpAddress, 150);
    ui.tableView->setColumnWidth(ResultTableModel::ColSent, 60);
    ui.tableView->setColumnWidth(ResultTableModel::ColReceived, 70);
    ui.tableView->setColumnWidth(ResultTableModel::ColLossRate, 80);
    ui.tableView->setColumnWidth(ResultTableModel::ColMinRtt, 80);
    ui.tableView->setColumnWidth(ResultTableModel::ColMaxRtt, 80);
    ui.tableView->setColumnWidth(ResultTableModel::ColAvgRtt, 80);
    ui.tableView->setColumnWidth(ResultTableModel::ColLastRtt, 80);
    ui.tableView->setColumnWidth(ResultTableModel::ColElapsed, 100);
}

void MainWindow::setupStatusBar()
{
    ui.statusBar = new QStatusBar;
    setStatusBar(ui.statusBar);

    ui.labelOnline = new QLabel("在线: 0");
    ui.labelOffline = new QLabel("离线: 0");
    ui.labelElapsed = new QLabel("耗时: 0s");
    ui.labelProgress = new QLabel("");
    ui.progressBar = new QProgressBar;
    ui.progressBar->setMaximumWidth(200);
    ui.progressBar->setMaximum(100);
    ui.progressBar->setVisible(false);
    ui.labelStatus = new QLabel("就绪");

    ui.statusBar->addWidget(ui.labelOnline);
    ui.statusBar->addWidget(ui.labelOffline);
    ui.statusBar->addWidget(ui.labelElapsed);
    ui.statusBar->addPermanentWidget(ui.progressBar);
    ui.statusBar->addPermanentWidget(ui.labelProgress);
    ui.statusBar->addPermanentWidget(ui.labelStatus);
}

void MainWindow::createConnections()
{
    // Toolbar buttons
    connect(ui.btnStart, &QPushButton::clicked, this, &MainWindow::onStart);
    connect(ui.btnStop, &QPushButton::clicked, this, &MainWindow::onStop);
    connect(ui.btnClear, &QPushButton::clicked, this, &MainWindow::onClear);
    connect(ui.btnImport, &QPushButton::clicked, this, &MainWindow::onImport);
    connect(ui.btnExport, &QPushButton::clicked, this, &MainWindow::onExport);
    connect(ui.btnInsertResults, &QPushButton::clicked, this, &MainWindow::onInsertResults);
    connect(ui.btnSettings, &QPushButton::clicked, this, &MainWindow::onSettings);
    connect(ui.btnAbout, &QPushButton::clicked, this, &MainWindow::onAbout);
    connect(ui.btnToggleTheme, &QPushButton::clicked, this, &MainWindow::onToggleTheme);

    // Engine signals
    connect(m_pingEngine, &PingEngine::started, this, &MainWindow::onEngineStarted);
    connect(m_pingEngine, &PingEngine::stopped, this, &MainWindow::onEngineStopped);
    connect(m_pingEngine, &PingEngine::resultsUpdated, this, &MainWindow::onUpdateTable);
    connect(m_pingEngine, &PingEngine::finished, this, &MainWindow::onEngineFinished);
    connect(m_pingEngine, &PingEngine::targetCountChanged, this, &MainWindow::onTargetCountChanged);

    // Table header sort
    connect(ui.tableHeader, &QHeaderView::sortIndicatorChanged, this, &MainWindow::onSortChanged);

    // Table context menu
    connect(ui.tableView, &QTableView::customContextMenuRequested, this, &MainWindow::onContextMenu);

    // Filter
    connect(ui.editFilter, &QLineEdit::textChanged, this, &MainWindow::onFilterChanged);

    // Config changes
    connect(ui.editTimeout, &QLineEdit::editingFinished, this, &MainWindow::onConfigChanged);
    connect(ui.spinInterval, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &MainWindow::onConfigChanged);
    connect(ui.spinPacketSize, QOverload<int>::of(&QSpinBox::valueChanged), this, &MainWindow::onConfigChanged);
    connect(ui.spinConcurrent, QOverload<int>::of(&QSpinBox::valueChanged), this, &MainWindow::onConfigChanged);

    // Refresh timer
    connect(m_refreshTimer, &QTimer::timeout, this, &MainWindow::onRefreshTimer);
}

void MainWindow::onStart()
{
    QString input = ui.textInput->toPlainText().trimmed();
    if (input.isEmpty()) {
        showError("错误", "请输入目标地址");
        return;
    }

    QStringList targets = parseInput(input);
    if (targets.isEmpty()) {
        showError("错误", "未能解析有效的目标地址");
        return;
    }

    // Apply config
    int timeout = ui.editTimeout->text().toInt();
    int interval = static_cast<int>(ui.spinInterval->value());
    int packetSize = ui.spinPacketSize->value();
    int concurrent = ui.spinConcurrent->value();

    m_pingEngine->setTimeout(timeout);
    m_pingEngine->setInterval(interval);
    m_pingEngine->setPacketSize(packetSize);
    m_pingEngine->setMaxConcurrent(concurrent);

    m_pingEngine->setTargets(targets);
    m_pingEngine->start();

    m_elapsedTimer->restart();
    m_refreshTimer->start(500);
    setUiEnabled(false);
    m_isRunning = true;

    ui.labelStatus->setText("正在扫描...");
}

void MainWindow::onStop()
{
    m_pingEngine->stop();
    m_refreshTimer->stop();
    setUiEnabled(true);
    m_isRunning = false;
    ui.labelStatus->setText("已停止");
}

void MainWindow::onClear()
{
    m_pingEngine->clear();
    m_tableModel->clear();
    m_onlineCount = 0;
    m_offlineCount = 0;
    m_totalTargets = 0;
    updateCounts(0, 0);
    ui.labelElapsed->setText("耗时: 0s");
    ui.progressBar->setVisible(false);
    ui.labelProgress->setText("");
    ui.labelStatus->setText("就绪");
}

void MainWindow::onImport()
{
    QString filePath = QFileDialog::getOpenFileName(this, "导入文件", "", "Excel/CSV Files (*.xlsx *.xls *.csv);;All Files (*)");
    if (filePath.isEmpty()) return;

    ExcelImporter importer;
    auto result = importer.import(filePath);

    if (!result.error.isEmpty()) {
        showError("导入错误", result.error);
        return;
    }

    if (result.addresses.isEmpty()) {
        showInfo("导入结果", QString("共 %1 行，有效地址 %2 个").arg(result.totalRows).arg(result.validRows));
        return;
    }

    ui.textInput->setPlainText(result.addresses.join("\n"));
    showInfo("导入结果", QString("共 %1 行，有效地址 %2 个").arg(result.totalRows).arg(result.validRows));
}

void MainWindow::onExport()
{
    QString filePath = QFileDialog::getSaveFileName(this, "导出结果", "", "Excel Files (*.xlsx);;CSV Files (*.csv)");
    if (filePath.isEmpty()) return;

    QVector<PingResult> results;
    QVariantList allResults = m_pingEngine->getAllResults();
    for (const QVariant &v : allResults) {
        if (v.canConvert<PingResult>()) {
            results.append(v.value<PingResult>());
        }
    }

    ExcelExporter exporter;
    bool success = exporter.exportToFile(filePath, results);

    if (success) {
        showInfo("导出成功", QString("已导出 %1 条结果到 %2").arg(results.size()).arg(filePath));
    } else {
        showError("导出错误", "导出失败");
    }
}

void MainWindow::onInsertResults()
{
    QString filePath = QFileDialog::getOpenFileName(this, "选择要插入的文件", "", "Excel Files (*.xlsx)");
    if (filePath.isEmpty()) return;

    QVector<PingResult> results;
    QVariantList allResults = m_pingEngine->getAllResults();
    for (const QVariant &v : allResults) {
        if (v.canConvert<PingResult>()) {
            results.append(v.value<PingResult>());
        }
    }

    ExcelExporter exporter;
    bool success = exporter.insertIntoFile(filePath, "IP", results);

    if (success) {
        showInfo("插入成功", QString("已插入 %1 条结果").arg(results.size()));
    } else {
        showError("插入错误", "插入失败");
    }
}

void MainWindow::onConfigChanged()
{
    // Configuration changes are applied when start is clicked
}

void MainWindow::onSortChanged(int logicalIndex)
{
    Qt::SortOrder order = ui.tableHeader->sortIndicatorOrder();
    SortOrder sortOrder = (order == Qt::AscendingOrder) ? Ascending : Descending;
    m_tableModel->sortByColumn(logicalIndex, sortOrder);
}

void MainWindow::onFilterChanged(const QString &text)
{
    m_proxyModel->setFilterWildcard(text);
}

void MainWindow::onRefreshTimer()
{
    if (!m_isRunning) return;

    qint64 elapsed = m_elapsedTimer->elapsed();
    int seconds = elapsed / 1000;
    int minutes = seconds / 60;
    seconds = seconds % 60;

    if (minutes > 0) {
        ui.labelElapsed->setText(QString("耗时: %1m %2s").arg(minutes).arg(seconds));
    } else {
        ui.labelElapsed->setText(QString("耗时: %1s").arg(seconds));
    }
}

void MainWindow::onUpdateTable()
{
    QVariantList results = m_pingEngine->getAllResults();
    std::vector<PingResult> pingResults;

    for (const QVariant &v : results) {
        if (v.canConvert<PingResult>()) {
            pingResults.push_back(v.value<PingResult>());
        }
    }

    m_tableModel->updateResults(pingResults);

    int online = m_pingEngine->onlineCount();
    int offline = m_pingEngine->offlineCount();
    updateCounts(online, offline);

    int total = m_pingEngine->totalTargets();
    if (total > 0) {
        int progress = static_cast<int>((online + offline) * 100.0 / total);
        ui.progressBar->setValue(progress);
        ui.progressBar->setVisible(true);
        ui.labelProgress->setText(QString("%1/%2").arg(online + offline).arg(total));
    }
}

void MainWindow::onEngineStarted()
{
    setUiEnabled(false);
    m_isRunning = true;
    ui.btnStop->setEnabled(true);
    ui.btnStart->setEnabled(false);
    ui.labelStatus->setText("正在扫描...");
}

void MainWindow::onEngineStopped()
{
    setUiEnabled(true);
    m_isRunning = false;
    ui.btnStop->setEnabled(false);
    ui.btnStart->setEnabled(true);
    m_refreshTimer->stop();

    if (m_pingEngine->totalTargets() > 0) {
        ui.labelStatus->setText("扫描完成");
    } else {
        ui.labelStatus->setText("已停止");
    }
}

void MainWindow::onEngineFinished()
{
    setUiEnabled(true);
    m_isRunning = false;
    ui.btnStop->setEnabled(false);
    ui.btnStart->setEnabled(true);
    m_refreshTimer->stop();
    ui.labelStatus->setText("扫描完成");
}

void MainWindow::onTargetCountChanged(int count)
{
    m_totalTargets = count;
}

void MainWindow::onContextMenu(const QPoint &pos)
{
    QModelIndex index = ui.tableView->indexAt(pos);
    if (!index.isValid()) return;

    QMenu menu(this);

    QAction *copyIpAction = menu.addAction("复制IP");
    QAction *removeAction = menu.addAction("删除行");

    QAction *selectedAction = menu.exec(ui.tableView->mapToGlobal(pos));
    if (selectedAction == copyIpAction) {
        QModelIndex sourceIndex = m_proxyModel->mapToSource(index);
        QVariant ipData = m_tableModel->data(m_proxyModel->mapToSource(index.siblingAtColumn(ResultTableModel::ColIpAddress)), Qt::DisplayRole);
        if (ipData.isValid()) {
            QClipboard *clipboard = QApplication::clipboard();
            clipboard->setText(ipData.toString());
        }
    } else if (selectedAction == removeAction) {
        // Remove row from model
        QModelIndex sourceIndex = m_proxyModel->mapToSource(index);
        int row = sourceIndex.row();
        m_tableModel->removeRows(row, 1);
    }
}

void MainWindow::onAbout()
{
    showInfo("关于", "pingtestpp v1.0.0\n\n高性能ping测试工具\n支持IP/CIDR/域名批量检测");
}

void MainWindow::onToggleTheme()
{
    if (m_currentTheme == Theme::Light) {
        setTheme(Theme::Dark);
    } else {
        setTheme(Theme::Light);
    }
}

void MainWindow::onSettings()
{
    // Settings dialog placeholder
    showInfo("设置", "设置功能开发中...");
}

void MainWindow::onInsertSampleData()
{
    QString sampleData = "8.8.8.8\n1.1.1.1\n192.168.1.1\n10.0.0.1\nexample.com";
    ui.textInput->setPlainText(sampleData);
}

void MainWindow::setTheme(Theme theme)
{
    m_currentTheme = theme;

    if (theme == Theme::Dark) {
        qApp->setStyle("Fusion");
        QPalette darkPalette;
        darkPalette.setColor(QPalette::Window, QColor(53, 53, 53));
        darkPalette.setColor(QPalette::WindowText, Qt::white);
        darkPalette.setColor(QPalette::Base, QColor(25, 25, 25));
        darkPalette.setColor(QPalette::AlternateBase, QColor(53, 53, 53));
        darkPalette.setColor(QPalette::ToolTipBase, Qt::white);
        darkPalette.setColor(QPalette::ToolTipText, Qt::white);
        darkPalette.setColor(QPalette::Text, Qt::white);
        darkPalette.setColor(QPalette::Button, QColor(53, 53, 53));
        darkPalette.setColor(QPalette::ButtonText, Qt::white);
        darkPalette.setColor(QPalette::BrightText, Qt::red);
        darkPalette.setColor(QPalette::Link, QColor(42, 130, 218));
        darkPalette.setColor(QPalette::Highlight, QColor(42, 130, 218));
        darkPalette.setColor(QPalette::HighlightedText, Qt::black);
        qApp->setPalette(darkPalette);
    } else {
        qApp->setStyle("Fusion");
        qApp->setPalette(qApp->style()->standardPalette());
    }

    emit themeChanged(theme);
}

void MainWindow::updateStatusBar()
{
    updateCounts(m_onlineCount, m_offlineCount);
}

void MainWindow::updateCounts(int online, int offline)
{
    m_onlineCount = online;
    m_offlineCount = offline;
    ui.labelOnline->setText(QString("在线: %1").arg(online));
    ui.labelOffline->setText(QString("离线: %1").arg(offline));
}

void MainWindow::setUiEnabled(bool enabled)
{
    ui.btnStart->setEnabled(enabled);
    ui.btnStop->setEnabled(!enabled && m_isRunning);
    ui.textInput->setEnabled(enabled);
    ui.editTimeout->setEnabled(enabled);
    ui.spinInterval->setEnabled(enabled);
    ui.spinPacketSize->setEnabled(enabled);
    ui.spinConcurrent->setEnabled(enabled);
    ui.btnImport->setEnabled(enabled);
    ui.btnExport->setEnabled(enabled);
    ui.btnInsertResults->setEnabled(enabled);
    ui.btnClear->setEnabled(enabled);
}

QStringList MainWindow::parseInput(const QString &input) const
{
    return m_pingEngine->parseInput(input);
}

bool MainWindow::validateInput(const QString &input) const
{
    if (input.trimmed().isEmpty()) return false;
    QStringList lines = input.split('\n', Qt::SkipEmptyParts);
    return !lines.isEmpty();
}

void MainWindow::showError(const QString &title, const QString &message)
{
    QMessageBox::critical(this, title, message);
}

void MainWindow::showInfo(const QString &title, const QString &message)
{
    QMessageBox::information(this, title, message);
}

void MainWindow::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasUrls()) {
        QList<QUrl> urls = event->mimeData()->urls();
        if (!urls.isEmpty()) {
            QString filePath = urls.first().toLocalFile();
            if (filePath.endsWith(".xlsx") || filePath.endsWith(".csv") ||
                filePath.endsWith(".xls")) {
                event->acceptProposedAction();
                return;
            }
        }
    }
    event->ignore();
}

void MainWindow::dropEvent(QDropEvent *event)
{
    if (event->mimeData()->hasUrls()) {
        QList<QUrl> urls = event->mimeData()->urls();
        if (!urls.isEmpty()) {
            QString filePath = urls.first().toLocalFile();
            if (filePath.endsWith(".xlsx") || filePath.endsWith(".csv") ||
                filePath.endsWith(".xls")) {

                ExcelImporter importer;
                auto result = importer.import(filePath);

                if (!result.error.isEmpty()) {
                    showError("导入错误", result.error);
                    return;
                }

                if (!result.addresses.isEmpty()) {
                    ui.textInput->setPlainText(result.addresses.join("\n"));
                }

                showInfo("导入成功", QString("导入 %1 个有效地址").arg(result.validRows));
                event->acceptProposedAction();
            }
        }
    }
}
