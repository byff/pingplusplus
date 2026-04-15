#include "excelimporter.h"
#include <QFile>
#include <QTextStream>
#include <QXmlStreamReader>
#include <QRegularExpression>
#include <QSet>
#include <QDir>
#include <QDebug>
#include <QBuffer>
#include <QuaZip-Qt6-1.4/quazip/quazip.h>
#include <QuaZip-Qt6-1.4/quazip/quazipfile.h>
#include <QuaZip-Qt6-1.4/quazip/quazipdir.h>

ExcelImporter::ExcelImporter(QObject* parent)
    : QObject(parent)
{
}

ExcelImporter::ImportResult ExcelImporter::import(const QString& filePath)
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
                if (reader.readNext() == QXmlStreamReader::StartElement && reader.name() == "t") {
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
                    if (name == "c") {
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
                        if (reader.name() == "v") {
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
                    } else if (name == "row") {
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

        // Extract addresses from each row, column by column
        QSet<QString> seen;
        for (const QStringList& row : rows) {
            ++result.totalRows;
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
