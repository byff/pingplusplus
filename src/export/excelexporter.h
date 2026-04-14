#pragma once
#include <QObject>
#include <QString>
#include <QVector>
#include "../ping/pingresult.h"

class ExcelExporter : public QObject {
    Q_OBJECT
public:
    explicit ExcelExporter(QObject* parent = nullptr);
    bool exportToFile(const QString& filePath, const QVector<PingResult>& results);
    bool insertIntoFile(const QString& filePath, const QString& ipColumnHeader, const QVector<PingResult>& results);
signals:
    void progress(int percent);
    void error(const QString& msg);
};
