#include "excelexporter.h"
#include "pingresult.h"
#include <xlsxwriter.h>
#include <QFile>
#include <QTextStream>
#include <QFileInfo>

ExcelExporter::ExcelExporter(QObject* parent)
    : QObject(parent)
{
}

bool ExcelExporter::exportToFile(const QString& filePath, const QVector<PingResult>& results)
{
    lxw_workbook* workbook = workbook_new(filePath.toUtf8().constData());
    if (!workbook) {
        emit error("Failed to create workbook");
        return false;
    }

    lxw_worksheet* sheet = workbook_add_worksheet(workbook, "Ping Results");
    if (!sheet) {
        workbook_close(workbook);
        emit error("Failed to create worksheet");
        return false;
    }

    // Header format: bold, gray background
    lxw_format* header_fmt = workbook_add_format(workbook);
    format_set_bold(header_fmt);
    format_set_bg_color(header_fmt, 0xD3D3D3);

    // Headers: #, 状态, 目标, IP地址, 已发, 已收, 丢包率(%), 最小RTT, 最大RTT, 平均RTT, 最新RTT, 运行时间
    const char* headers[] = {
        "#", "状态", "目标", "IP地址", "已发", "已收",
        "丢包率(%)", "最小RTT(ms)", "最大RTT(ms)", "平均RTT(ms)",
        "最新RTT(ms)", "运行时间"
    };
    constexpr int NUM_COLS = 12;

    for (int col = 0; col < NUM_COLS; ++col) {
        worksheet_write_string(sheet, 0, col, headers[col], header_fmt);
    }

    // Column widths
    worksheet_set_column(sheet, 0, 0,  5,  NULL);   // #
    worksheet_set_column(sheet, 1, 1,  10, NULL);   // 状态
    worksheet_set_column(sheet, 2, 2,  25, NULL);   // 目标
    worksheet_set_column(sheet, 3, 3,  18, NULL);   // IP地址
    worksheet_set_column(sheet, 4, 4,   8, NULL);   // 已发
    worksheet_set_column(sheet, 5, 5,   8, NULL);   // 已收
    worksheet_set_column(sheet, 6, 6,  12, NULL);   // 丢包率
    worksheet_set_column(sheet, 7, 7,  14, NULL);   // 最小RTT
    worksheet_set_column(sheet, 8, 8,  14, NULL);   // 最大RTT
    worksheet_set_column(sheet, 9, 9,  14, NULL);   // 平均RTT
    worksheet_set_column(sheet, 10, 10, 14, NULL);   // 最新RTT
    worksheet_set_column(sheet, 11, 11, 12, NULL);   // 运行时间

    // Data rows
    for (int i = 0; i < results.size(); ++i) {
        const PingResult& r = results[i];
        int row = i + 1;

        worksheet_write_number(sheet, row, 0, i + 1, NULL);

        // Status
        worksheet_write_string(sheet, row, 1,
            r.statusText().toUtf8().constData(), NULL);

        // Target
        worksheet_write_string(sheet, row, 2,
            r.originalInput.toUtf8().constData(), NULL);

        // IP address
        worksheet_write_string(sheet, row, 3,
            r.targetIp.toUtf8().constData(), NULL);

        // Sent / Received
        worksheet_write_number(sheet, row, 4, r.sent, NULL);
        worksheet_write_number(sheet, row, 5, r.received, NULL);

        // Loss rate
        worksheet_write_number(sheet, row, 6, r.lossRatePercent(), NULL);

        // RTT values (in ms)
        worksheet_write_number(sheet, row, 7, r.minRttMs(), NULL);
        worksheet_write_number(sheet, row, 8, r.maxRttMs(), NULL);
        worksheet_write_number(sheet, row, 9, r.avgRttMs(), NULL);
        worksheet_write_number(sheet, row, 10, r.lastRttMs(), NULL);

        // Elapsed time
        worksheet_write_string(sheet, row, 11,
            r.elapsedText().toUtf8().constData(), NULL);

        if (i % 100 == 0) {
            emit progress((i * 100) / results.size());
        }
    }

    workbook_close(workbook);
    emit progress(100);
    return true;
}

bool ExcelExporter::insertIntoFile(const QString& filePath,
                                   const QString& /*ipColumnHeader*/,
                                   const QVector<PingResult>& results)
{
    Q_UNUSED(filePath);
    Q_UNUSED(results);
    // Full implementation would read existing Excel, locate IP column,
    // and insert RTT/loss columns next to it. For now, delegate to export.
    QFileInfo fi(filePath);
    QString insertPath = fi.absolutePath() + "/" + fi.baseName() + "_with_results." + fi.suffix();
    return exportToFile(insertPath, results);
}
