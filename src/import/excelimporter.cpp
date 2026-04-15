#include "excelimporter.h"
#include <QFile>
#include <QFileInfo>
#include <QTextStream>
#include <QXmlStreamReader>
#include <QRegularExpression>
#include <QSet>
#include <QDir>
#include <QDebug>
#include <QBuffer>
#include <QDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QPushButton>
#include <QLabel>
#include <QHeaderView>
#include <QScrollArea>
#include <quazip/quazip.h>
#include <quazip/quazipfile.h>
#include <quazip/quazipdir.h>

ExcelImporter::ExcelImporter(QObject* parent)
    : QObject(parent)
{
}

QList<ExcelImporter::ColumnInfo> ExcelImporter::detectIpColumns(const QString& filePath)
{
    QList<ColumnInfo> candidates;
    QFileInfo fileInfo(filePath);
    QString suffix = fileInfo.suffix().toLower();

    if (suffix == "xlsx") {
        // XLSX is a ZIP archive - use QuaZip to extract
        QuaZip zip(filePath);
        if (!zip.open(QuaZip::mdUnzip)) {
            return candidates;
        }

        QString sharedStringsXml;
        QString sheetXml;

        // Read shared strings (xl/sharedStrings.xml)
        QuaZipFile sharedFile(&zip);
        if (sharedFile.open(QIODevice::ReadOnly, "xl/sharedStrings.xml")) {
            sharedStringsXml = QString::fromUtf8(sharedFile.readAll());
            sharedFile.close();
        }

        // Read first sheet (xl/worksheets/sheet1.xml)
        QuaZipFile sheetFile(&zip);
        if (sheetFile.open(QIODevice::ReadOnly, "xl/worksheets/sheet1.xml")) {
            sheetXml = QString::fromUtf8(sheetFile.readAll());
            sheetFile.close();
        }

        zip.close();

        // Parse shared strings
        QStringList sharedStrings;
        {
            QXmlStreamReader reader(sharedStringsXml);
            while (!reader.atEnd()) {
                if (reader.readNext() == QXmlStreamReader::StartElement && reader.name().toString() == "t") {
                    sharedStrings.append(reader.readElementText());
                }
            }
        }

        // Parse sheet - extract all cell values in column order
        QVector<QStringList> rows;
        QStringList currentRow;
        int currentCol = 0;
        int maxCol = 0;

        {
            QXmlStreamReader reader(sheetXml);
            while (!reader.atEnd()) {
                if (reader.readNext() == QXmlStreamReader::StartElement) {
                    QStringView name = reader.name();
                    if (name.toString() == "c") {
                        QString ref = reader.attributes().value("r").toString();
                        // Extract column letter from ref like "A1", "B2"
                        QString colStr = ref;
                        colStr.remove(QRegularExpression("[0-9]"));
                        int col = 0;
                        for (QChar c : colStr) {
                            col = col * 26 + (c.unicode() - 'A' + 1);
                        }
                        // Fill empty columns
                        while (currentCol < col - 1) {
                            currentRow.append("");
                            ++currentCol;
                        }

                        QString type = reader.attributes().value("t").toString();
                        QString value;

                        reader.readNext();
                        if (reader.name().toString() == "v") {
                            value = reader.readElementText();
                        }

                        if (type == "s" && !value.isEmpty()) {
                            int idx = value.toInt();
                            if (idx < sharedStrings.size()) {
                                currentRow.append(sharedStrings[idx]);
                            } else {
                                currentRow.append("");
                            }
                        } else if (!value.isEmpty()) {
                            currentRow.append(value);
                        } else {
                            currentRow.append("");
                        }
                        ++currentCol;
                        maxCol = qMax(maxCol, currentCol);
                    } else if (name.toString() == "row") {
                        if (!currentRow.isEmpty()) {
                            // Pad row to maxCol
                            while (currentRow.size() < maxCol) currentRow.append("");
                            rows.append(currentRow);
                            currentRow.clear();
                            currentCol = 0;
                            maxCol = 0;
                        }
                    }
                }
            }
            // Last row
            if (!currentRow.isEmpty()) {
                while (currentRow.size() < maxCol) currentRow.append("");
                rows.append(currentRow);
            }
        }

        if (rows.isEmpty()) {
            return candidates;
        }

        // Analyze each column
        QStringList headerRow = rows.value(0, QStringList());
        for (int col = 0; col < (headerRow.isEmpty() ? 0 : headerRow.size()); ++col) {
            ColumnInfo info;
            info.columnIndex = col;
            info.header = col < headerRow.size() ? headerRow[col] : "";
            info.ipLikeCount = 0;

            // Check header for IP-related keywords
            QString lowerHeader = info.header.toLower();
            bool headerMatches = lowerHeader.contains("ip") ||
                                 lowerHeader.contains("address") ||
                                 lowerHeader.contains("host") ||
                                 lowerHeader.contains(".ip") ||
                                 lowerHeader.contains("域名") ||
                                 lowerHeader.contains("ip地址");

            // Collect samples and count IP-like values
            int sampleCount = 0;
            int maxSamples = 5;
            int maxRows = qMin(rows.size(), 100);

            for (int rowIdx = 1; rowIdx < maxRows && sampleCount < maxSamples; ++rowIdx) {
                if (rowIdx < rows.size()) {
                    QString cell = rows[rowIdx].value(col, "");
                    if (!cell.isEmpty()) {
                        if (sampleCount < maxSamples) {
                            info.samples.append(cell);
                            ++sampleCount;
                        }
                        if (isIpLike(cell) || extractAddresses(cell).size() > 0) {
                            // Only count if it looks like an IP
                            QRegularExpression ipRegex(R"(\d{1,3}\.\d{1,3}\.\d{1,3}\.\d{1,3})");
                            if (ipRegex.match(cell).hasMatch() ||
                                cell.contains(QRegularExpression(R"(\d{1,3}\.\d{1,3}\.\d{1,3}\.\d{1,3}/\d{1,2})"))) {
                                ++info.ipLikeCount;
                            }
                        }
                    }
                }
            }

            // Add as candidate if header matches or has IP-like data
            if (headerMatches || info.ipLikeCount > 0) {
                candidates.append(info);
            }
        }
    } else if (suffix == "csv" || suffix == "txt") {
        // For CSV/TXT, we treat the entire file as single column
        ColumnInfo info;
        info.columnIndex = 0;
        info.header = "";
        info.ipLikeCount = 0;

        QFile file(filePath);
        if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QTextStream in(&file);
            int lineCount = 0;
            int maxLines = 100;
            while (!in.atEnd() && lineCount < maxLines) {
                QString line = in.readLine().trimmed();
                if (!line.isEmpty()) {
                    if (lineCount < 5) {
                        info.samples.append(line);
                    }
                    if (isIpLike(line) || extractAddresses(line).size() > 0) {
                        QRegularExpression ipRegex(R"(\d{1,3}\.\d{1,3}\.\d{1,3}\.\d{1,3})");
                        if (ipRegex.match(line).hasMatch()) {
                            ++info.ipLikeCount;
                        }
                    }
                }
                ++lineCount;
            }
            file.close();
        }

        if (!info.samples.isEmpty()) {
            candidates.append(info);
        }
    }

    return candidates;
}

int ExcelImporter::selectColumn(QWidget* parent, const QList<ColumnInfo>& candidates)
{
    if (candidates.isEmpty()) return -1;
    if (candidates.size() == 1) return candidates.first().columnIndex;

    ColumnSelectDialog dialog(candidates, parent);
    if (dialog.exec() == QDialog::Accepted) {
        return dialog.selectedColumn();
    }
    return -1;
}

ExcelImporter::ImportResult ExcelImporter::import(const QString& filePath, int columnIndex)
{
    ImportResult result;
    QFileInfo fileInfo(filePath);
    QString suffix = fileInfo.suffix().toLower();

    if (suffix == "csv" || suffix == "txt") {
        // Simple line-based parsing for CSV/TXT
        QFile file(filePath);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            result.error = "Cannot open file: " + filePath;
            return result;
        }

        QTextStream in(&file);
        int lineNum = 0;
        QSet<QString> seen;
        while (!in.atEnd()) {
            QString line = in.readLine().trimmed();
            ++lineNum;
            ++result.totalRows;

            QStringList extracted = extractAddresses(line);
            for (const QString& addr : extracted) {
                if (!addr.isEmpty() && !seen.contains(addr)) {
                    seen.insert(addr);
                    result.addresses.append(addr);
                    ++result.validRows;
                }
            }
            emit progress((lineNum % 100) * 100 / qMax(lineNum, 1));
        }
        file.close();

    } else if (suffix == "xlsx") {
        // XLSX is a ZIP archive - use QuaZip to extract
        QuaZip zip(filePath);
        if (!zip.open(QuaZip::mdUnzip)) {
            result.error = "Cannot open XLSX file: " + filePath;
            return result;
        }

        QString sharedStringsXml;
        QString sheetXml;

        // Read shared strings (xl/sharedStrings.xml)
        QuaZipFile sharedFile(&zip);
        if (sharedFile.open(QIODevice::ReadOnly, "xl/sharedStrings.xml")) {
            sharedStringsXml = QString::fromUtf8(sharedFile.readAll());
            sharedFile.close();
        }

        // Read first sheet (xl/worksheets/sheet1.xml)
        QuaZipFile sheetFile(&zip);
        if (sheetFile.open(QIODevice::ReadOnly, "xl/worksheets/sheet1.xml")) {
            sheetXml = QString::fromUtf8(sheetFile.readAll());
            sheetFile.close();
        }

        zip.close();

        // Parse shared strings
        QStringList sharedStrings;
        {
            QXmlStreamReader reader(sharedStringsXml);
            while (!reader.atEnd()) {
                if (reader.readNext() == QXmlStreamReader::StartElement && reader.name().toString() == "t") {
                    sharedStrings.append(reader.readElementText());
                }
            }
        }

        // Parse sheet - extract all cell values in column order
        QVector<QStringList> rows;
        QStringList currentRow;
        int currentCol = 0;
        int maxCol = 0;

        {
            QXmlStreamReader reader(sheetXml);
            while (!reader.atEnd()) {
                if (reader.readNext() == QXmlStreamReader::StartElement) {
                    QStringView name = reader.name();
                    if (name.toString() == "c") {
                        QString ref = reader.attributes().value("r").toString();
                        // Extract column letter from ref like "A1", "B2"
                        QString colStr = ref;
                        colStr.remove(QRegularExpression("[0-9]"));
                        int col = 0;
                        for (QChar c : colStr) {
                            col = col * 26 + (c.unicode() - 'A' + 1);
                        }
                        // Fill empty columns
                        while (currentCol < col - 1) {
                            currentRow.append("");
                            ++currentCol;
                        }

                        QString type = reader.attributes().value("t").toString();
                        QString value;

                        reader.readNext();
                        if (reader.name().toString() == "v") {
                            value = reader.readElementText();
                        }

                        if (type == "s" && !value.isEmpty()) {
                            int idx = value.toInt();
                            if (idx < sharedStrings.size()) {
                                currentRow.append(sharedStrings[idx]);
                            } else {
                                currentRow.append("");
                            }
                        } else if (!value.isEmpty()) {
                            currentRow.append(value);
                        } else {
                            currentRow.append("");
                        }
                        ++currentCol;
                        maxCol = qMax(maxCol, currentCol);
                    } else if (name.toString() == "row") {
                        if (!currentRow.isEmpty()) {
                            // Pad row to maxCol
                            while (currentRow.size() < maxCol) currentRow.append("");
                            rows.append(currentRow);
                            currentRow.clear();
                            currentCol = 0;
                            maxCol = 0;
                        }
                    }
                }
            }
            // Last row
            if (!currentRow.isEmpty()) {
                while (currentRow.size() < maxCol) currentRow.append("");
                rows.append(currentRow);
            }
        }

        // Determine which column to use
        int targetCol = columnIndex;
        if (columnIndex == -1) {
            // Auto-detect
            QList<ColumnInfo> candidates = detectIpColumns(filePath);
            if (candidates.isEmpty()) {
                // Fall back to scanning all columns
                targetCol = -2; // means use all
            } else if (candidates.size() == 1) {
                targetCol = candidates.first().columnIndex;
            } else {
                // Multiple candidates - need to ask user
                // But we don't have parent widget here, so just use first best candidate
                // In practice, MainWindow will call detectIpColumns and show dialog
                targetCol = candidates.first().columnIndex;
            }
        }

        // Extract addresses from each row, column by column
        QSet<QString> seen;
        for (const QStringList& row : rows) {
            ++result.totalRows;

            if (targetCol == -2) {
                // Scan all columns
                for (const QString& cell : row) {
                    QStringList extracted = extractAddresses(cell);
                    for (const QString& addr : extracted) {
                        if (!seen.contains(addr)) {
                            seen.insert(addr);
                            result.addresses.append(addr);
                            ++result.validRows;
                        }
                    }
                }
            } else {
                // Use specific column
                if (targetCol >= 0 && targetCol < row.size()) {
                    QString cell = row[targetCol];
                    QStringList extracted = extractAddresses(cell);
                    for (const QString& addr : extracted) {
                        if (!seen.contains(addr)) {
                            seen.insert(addr);
                            result.addresses.append(addr);
                            ++result.validRows;
                        }
                    }
                }
            }
            emit progress((result.totalRows % 100) * 100 / qMax(result.totalRows, 1));
        }
    } else {
        result.error = "Unsupported file format: " + suffix;
    }

    emit progress(100);
    return result;
}

QStringList ExcelImporter::extractAddresses(const QString& text)
{
    QStringList result;
    QSet<QString> seen;

    // IPv4 pattern: \b(\d{1,3}\.\d{1,3}\.\d{1,3}\.\d{1,3})\b
    QRegularExpression ipv4Regex(R"(\b(\d{1,3}\.\d{1,3}\.\d{1,3}\.\d{1,3})\b)");
    QRegularExpressionMatchIterator it = ipv4Regex.globalMatch(text);
    while (it.hasNext()) {
        QRegularExpressionMatch match = it.next();
        QString ip = match.captured(1);
        if (!seen.contains(ip)) {
            seen.insert(ip);
            result.append(ip);
        }
    }

    // CIDR pattern: IPv4 with /prefix
    QRegularExpression cidrRegex(R"(\b(\d{1,3}\.\d{1,3}\.\d{1,3}\.\d{1,3}/\d{1,2})\b)");
    QRegularExpressionMatchIterator cidrIt = cidrRegex.globalMatch(text);
    while (cidrIt.hasNext()) {
        QRegularExpressionMatch match = cidrIt.next();
        QString cidr = match.captured(1);
        if (!seen.contains(cidr)) {
            seen.insert(cidr);
            result.append(cidr);
        }
    }

    // IPv6 pattern (simplified - hex groups with colons)
    QRegularExpression ipv6Regex(R"(\b([0-9a-fA-F]{1,4}(:[0-9a-fA-F]{1,4}){2,7})\b)");
    QRegularExpressionMatchIterator ipv6It = ipv6Regex.globalMatch(text);
    while (ipv6It.hasNext()) {
        QRegularExpressionMatch match = ipv6It.next();
        QString ip = match.captured(1);
        if (!seen.contains(ip)) {
            seen.insert(ip);
            result.append(ip);
        }
    }

    // Domain pattern: skip IPs, only match things that don't look like IPs
    QRegularExpression domainRegex(R"(\b([a-zA-Z][a-zA-Z0-9\-]*\.[a-zA-Z][a-zA-Z0-9\-.]*)\b)");
    QRegularExpressionMatchIterator domainIt = domainRegex.globalMatch(text);
    while (domainIt.hasNext()) {
        QRegularExpressionMatch match = domainIt.next();
        QString domain = match.captured(1);
        // Only add if it doesn't look like an IP and doesn't contain ".."
        if (domain.length() > 4 && !domain.contains("..") && isIpLike(domain)) {
            if (!seen.contains(domain)) {
                seen.insert(domain);
                result.append(domain);
            }
        }
    }

    return result;
}

bool ExcelImporter::isIpLike(const QString& text)
{
    // Return false if it looks like an IP address (so we don't duplicate)
    QRegularExpression ipRegex(R"(\d{1,3}\.\d{1,3}\.\d{1,3}\.\d{1,3})");
    return !ipRegex.match(text).hasMatch();
}

// Column Selection Dialog Implementation
ColumnSelectDialog::ColumnSelectDialog(const QList<ExcelImporter::ColumnInfo>& columns, QWidget* parent)
    : QDialog(parent), m_selectedColumn(-1)
{
    setWindowTitle(QStringLiteral("选择IP列"));
    setMinimumSize(600, 400);

    QVBoxLayout* mainLayout = new QVBoxLayout(this);

    QLabel* titleLabel = new QLabel(QStringLiteral("检测到多个可能的IP列，请选择要导入的列："));
    mainLayout->addWidget(titleLabel);

    QTableWidget* table = new QTableWidget;
    table->setColumnCount(4);
    table->setHorizontalHeaderLabels(QStringList() << QStringLiteral("列") << QStringLiteral("表头") << QStringLiteral("样本数据") << QStringLiteral("IP数"));
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setSelectionMode(QAbstractItemView::SingleSelection);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->horizontalHeader()->setStretchLastSection(true);

    table->setRowCount(columns.size());
    for (int i = 0; i < columns.size(); ++i) {
        const ExcelImporter::ColumnInfo& col = columns[i];

        QTableWidgetItem* colItem = new QTableWidgetItem(QString("Column %1").arg(col.columnIndex + 1));
        table->setItem(i, 0, colItem);

        QTableWidgetItem* headerItem = new QTableWidgetItem(col.header.isEmpty() ? QStringLiteral("(无表头)") : col.header);
        table->setItem(i, 1, headerItem);

        QTableWidgetItem* sampleItem = new QTableWidgetItem(col.samples.join(", "));
        table->setItem(i, 2, sampleItem);

        QTableWidgetItem* countItem = new QTableWidgetItem(QString::number(col.ipLikeCount));
        table->setItem(i, 3, countItem);

        // Select row with most IP-like values by default
        if (i == 0 || col.ipLikeCount > columns.value(i-1).ipLikeCount) {
            table->selectRow(i);
            m_selectedColumn = col.columnIndex;
        }
    }

    mainLayout->addWidget(table);

    // Buttons
    QHBoxLayout* btnLayout = new QHBoxLayout;
    btnLayout->addStretch();

    QPushButton* okBtn = new QPushButton(QStringLiteral("确定"));
    QPushButton* cancelBtn = new QPushButton(QStringLiteral("取消"));

    btnLayout->addWidget(okBtn);
    btnLayout->addWidget(cancelBtn);

    mainLayout->addLayout(btnLayout);

    connect(okBtn, &QPushButton::clicked, this, [this, table, columns]() {
        int row = table->currentRow();
        if (row >= 0 && row < columns.size()) {
            m_selectedColumn = columns[row].columnIndex;
        }
        accept();
    });

    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
    connect(table, &QTableWidget::itemDoubleClicked, this, [this, table, columns](QTableWidgetItem*) {
        int row = table->currentRow();
        if (row >= 0 && row < columns.size()) {
            m_selectedColumn = columns[row].columnIndex;
        }
        accept();
    });
}
