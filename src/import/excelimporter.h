#pragma once
#include <QObject>
#include <QString>
#include <QStringList>

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
    ImportResult import(const QString& filePath);
    static QStringList extractAddresses(const QString& text);
    static bool isIpLike(const QString& text);
signals:
    void progress(int percent);
};
