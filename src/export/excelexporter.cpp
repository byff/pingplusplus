#include "excelexporter.h"
#include <xlsxwriter/xlsxwriter.h>
#include <QFile>
#include <QTextStream>

ExcelExporter::ExcelExporter(QObject* parent)
    : QObject(parent)
{
}

bool ExcelExporter::exportToFile(const QString& filePath, const QVector<PingResult>& results)
{
    lxw_workbook *workbook = workbook_new(filePath.toUtf8().constData());
    if (!workbook) {
        emit error("Failed to create workbook");
        return false;
    }

    lxw_worksheet *sheet = workbook_add_worksheet(workbook, "Ping Results");
    if (!sheet) {
        workbook_close(workbook);
        emit error("Failed to create worksheet");
        return false;
    }

    // Define formats
    lxf_format *header_format = workbook_add_format(workbook);
    format_set_bold(header_format);
    format_set_bg_color(header_format, 0xD3D3D3);

    lxf_format *percent_format = workbook_add_format(workbook);
    format_set_num_format(percent_format, "0.00%");

    // Headers: #, 状态, 目标, IP地址, 已发, 已收, 丢包率(%), 最小RTT(ms), 最大RTT(ms), 平均RTT(ms), 最新RTT(ms), 运行时间
    const char *headers[] = {
        "#", "状态", "目标", "IP地址", "已发", "已收", "丢包率(%)", "最小RTT(ms)", "最大RTT(ms)", "平均RTT(ms)", "最新RTT(ms)", "运行时间"
    };
    int num_cols = 12;

    for (int col = 0; col < num_cols; ++col) {
        worksheet_write_string(sheet, 0, col, headers[col], header_format);
    }

    // Set column widths
    worksheet_set_column(sheet, 0, 0, 5, NULL);   // #
    worksheet_set_column(sheet, 1, 1, 10, NULL);   // 状态
    worksheet_set_column(sheet, 2, 2, 25, NULL);  // 目标
    worksheet_set_column(sheet, 3, 3, 18, NULL);  // IP地址
    worksheet_set_column(sheet, 4, 4, 8, NULL);   // 已发
    worksheet_set_column(sheet, 5, 5, 8, NULL);   // 已收
    worksheet_set_column(sheet, 6, 6, 12, NULL);  // 丢包率
    worksheet_set_column(sheet, 7, 7, 12, NULL);  // 最小RTT
    worksheet_set_column(sheet, 8, 8, 12, NULL);  // 最大RTT
    worksheet_set_column(sheet, 9, 9, 12, NULL);  // 平均RTT
    worksheet_set_column(sheet, 10, 10, 12, NULL); // 最新RTT
    worksheet_set_column(sheet, 11, 11, 15, NULL); // 运行时间

    // Write data rows
    for (int i = 0; i < results.size(); ++i) {
        const PingResult &r = results[i];
        int row = i + 1;

        worksheet_write_number(sheet, row, 0, i + 1, NULL);
        worksheet_write_string(sheet, row, 1, r.statusString().toUtf8().constData(), NULL);
        worksheet_write_string(sheet, row, 2, r.target.toUtf8().constData(), NULL);
        worksheet_write_string(sheet, row, 3, r.ipAddress.toUtf8().constData(), NULL);
        worksheet_write_number(sheet, row, 4, r.packetsSent, NULL);
        worksheet_write_number(sheet, row, 5, r.packetsReceived, NULL);

        double lossRate = (r.packetsSent > 0)
            ? (double)(r.packetsSent - r.packetsReceived) / r.packetsSent
            : 0.0;
        worksheet_write_number(sheet, row, 6, lossRate * 100, NULL);

        worksheet_write_number(sheet, row, 7, r.minRttMs, NULL);
        worksheet_write_number(sheet, row, 8, r.maxRttMs, NULL);
        worksheet_write_number(sheet, row, 9, r.avgRttMs, NULL);
        worksheet_write_number(sheet, row, 10, r.lastRttMs, NULL);
        worksheet_write_string(sheet, row, 11, r.elapsedTimeString().toUtf8().constData(), NULL);

        if (i % 10 == 0) {
            emit progress((i * 100) / results.size());
        }
    }

    workbook_close(workbook);
    emit progress(100);
    return true;
}

bool ExcelExporter::insertIntoFile(const QString& filePath, const QString& ipColumnHeader, const QVector<PingResult>& results)
{
    // For insert mode, we create a new file with the results
    // In a full implementation, this would read existing file and insert data
    Q_UNUSED(ipColumnHeader);
    return exportToFile(filePath, results);
}
