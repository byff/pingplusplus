#pragma once
#include <QObject>
#include <QString>
#include <QStringList>
#include <QDialog>

class ExcelImporter : public QObject {
    Q_OBJECT
public:
    explicit ExcelImporter(QObject* parent = nullptr);
    struct ImportResult {
        QStringList addresses;  // extracted IP/CIDR/domain strings
        int totalRows = 0;
        int validRows = 0;
        QString error;
    };

    // Column info for Excel column selection dialog
    struct ColumnInfo {
        int columnIndex;
        QString header;
        QStringList samples;  // First few values
        int ipLikeCount;       // Number of IP-like values
    };

    // Detect best IP column(s) from Excel and return candidate columns
    static QList<ColumnInfo> detectIpColumns(const QString& filePath);

    // Show column selection dialog and return selected column index (-1 if cancelled)
    static int selectColumn(QWidget* parent, const QList<ColumnInfo>& candidates);

    // Import with specific column index (-1 = auto-detect, -2 = all columns)
    ImportResult import(const QString& filePath, int columnIndex = -1);
    static QStringList extractAddresses(const QString& text);
    static bool isIpLike(const QString& text);
signals:
    void progress(int percent);
};

// Column selection dialog
class ColumnSelectDialog : public QDialog {
    Q_OBJECT
public:
    explicit ColumnSelectDialog(const QList<ExcelImporter::ColumnInfo>& columns, QWidget* parent = nullptr);
    int selectedColumn() const { return m_selectedColumn; }

private:
    int m_selectedColumn;
};
