#include "resulttablemodel.h"
#include <QColor>
#include <QFont>

namespace {
QString formatRttMs(double ms)
{
    if (ms <= 0) {
        return QStringLiteral("-");
    }
    return QStringLiteral("%1 ms").arg(ms, 0, 'f', 2);
}
}

ResultTableModel::ResultTableModel(QObject* parent)
    : QAbstractTableModel(parent)
{
}

int ResultTableModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return static_cast<int>(m_results.size());
}

int ResultTableModel::columnCount(const QModelIndex& parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return ColCount;
}

QVariant ResultTableModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= rowCount()) {
        return QVariant();
    }

    const PingResult& result = m_results[index.row()];

    if (role == Qt::DisplayRole) {
        switch (index.column()) {
        case ColNum:
            return result.index + 1;
        case ColStatus:
            return result.statusText();
        case ColTarget:
            return result.originalInput;
        case ColHostname:
            return result.hostname;
        case ColIpAddress:
            if (!result.resolvedDomain.isEmpty() && result.resolvedDomain != result.targetIp) {
                return QStringLiteral("%1 → %2").arg(result.resolvedDomain).arg(result.targetIp);
            }
            return result.targetIp;
        case ColSent:
            return result.sent;
        case ColReceived:
            return result.received;
        case ColLossRate:
            return QStringLiteral("%1%").arg(result.lossRatePercent(), 0, 'f', 1);
        case ColMinRtt:
            return formatRttMs(result.minRttMs());
        case ColMaxRtt:
            return formatRttMs(result.maxRttMs());
        case ColAvgRtt:
            return formatRttMs(result.avgRttMs());
        case ColLastRtt:
            return formatRttMs(result.lastRttMs());
        case ColElapsed:
            return result.elapsedText();
        default:
            return QVariant();
        }
    }

    if (role == Qt::ForegroundRole) {
        // Red for loss rate > 5%
        if (index.column() == ColLossRate) {
            if (result.lossRatePercent() > 5.0) {
                return QColor(Qt::red);
            }
        }
        // Red for status offline or DNS failed
        if (index.column() == ColStatus) {
            if (result.statusText() == QStringLiteral("离线") || result.dnsFailed) {
                return QColor(Qt::red);
            }
            if (result.statusText() == QStringLiteral("在线")) {
                return QColor(Qt::darkGreen);
            }
        }
        // Red for RTT > 100ms in any RTT column
        if (index.column() == ColMinRtt || index.column() == ColMaxRtt ||
            index.column() == ColAvgRtt || index.column() == ColLastRtt) {
            double rttMs = 0;
            switch (index.column()) {
                case ColMinRtt: rttMs = result.minRttMs(); break;
                case ColMaxRtt: rttMs = result.maxRttMs(); break;
                case ColAvgRtt: rttMs = result.avgRttMs(); break;
                case ColLastRtt: rttMs = result.lastRttMs(); break;
            }
            if (rttMs > 100.0) {
                return QColor(Qt::red);
            }
        }
    }

    return QVariant();
}

QVariant ResultTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role != Qt::DisplayRole) {
        return QVariant();
    }

    if (orientation != Qt::Horizontal) {
        return QVariant();
    }

    static const QString headers[] = {
        QStringLiteral("#"),
        QStringLiteral("状态"),
        QStringLiteral("目标"),
        QStringLiteral("主机名"),
        QStringLiteral("IP 地址"),
        QStringLiteral("已发"),
        QStringLiteral("已收"),
        QStringLiteral("丢包率"),
        QStringLiteral("最小时延"),
        QStringLiteral("最大时延"),
        QStringLiteral("平均时延"),
        QStringLiteral("最后时延"),
        QStringLiteral("耗时")
    };

    if (section >= 0 && section < ColCount) {
        return headers[section];
    }

    return QVariant();
}

Qt::ItemFlags ResultTableModel::flags(const QModelIndex& index) const
{
    if (!index.isValid()) {
        return Qt::ItemIsEnabled;
    }
    return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
}

void ResultTableModel::setResults(const std::vector<PingResult>& results)
{
    beginResetModel();
    m_results = results;
    sortByColumn(m_sortColumn, m_sortOrder);
    endResetModel();
}

void ResultTableModel::updateResults(const std::vector<PingResult>& results)
{
    beginResetModel();
    m_results = results;
    sortByColumn(m_sortColumn, m_sortOrder);
    endResetModel();
}

void ResultTableModel::clear()
{
    beginResetModel();
    m_results.clear();
    endResetModel();
}

void ResultTableModel::sortByColumn(int column, SortOrder order)
{
    m_sortColumn = column;
    m_sortOrder = order;

    if (order == DefaultOrder || m_results.empty()) {
        return;
    }

    std::sort(m_results.begin(), m_results.end(),
        [column, order](const PingResult& a, const PingResult& b) {
            bool ascending = (order == Ascending);

            switch (column) {
            case ColNum:
                return ascending ? (a.index < b.index) : (a.index > b.index);
            case ColStatus: {
                int statusA = 0, statusB = 0;
                if (a.isRunning) statusA = 3;
                else if (a.dnsFailed) statusA = 0;
                else if (a.isOnline) statusA = 2;
                else statusA = 1;
                if (b.isRunning) statusB = 3;
                else if (b.dnsFailed) statusB = 0;
                else if (b.isOnline) statusB = 2;
                else statusB = 1;
                return ascending ? (statusA < statusB) : (statusA > statusB);
            }
            case ColTarget:
                return ascending ? (a.originalInput < b.originalInput) : (a.originalInput > b.originalInput);
            case ColHostname:
                return ascending ? (a.hostname < b.hostname) : (a.hostname > b.hostname);
            case ColIpAddress:
                return ascending ? (a.targetIp < b.targetIp) : (a.targetIp > b.targetIp);
            case ColSent:
                return ascending ? (a.sent < b.sent) : (a.sent > b.sent);
            case ColReceived:
                return ascending ? (a.received < b.received) : (a.received > b.received);
            case ColLossRate:
                return ascending ? (a.lossRate < b.lossRate) : (a.lossRate > b.lossRate);
            case ColMinRtt:
                return ascending ? (a.minRttUs < b.minRttUs) : (a.minRttUs > b.minRttUs);
            case ColMaxRtt:
                return ascending ? (a.maxRttUs < b.maxRttUs) : (a.maxRttUs > b.maxRttUs);
            case ColAvgRtt:
                return ascending ? (a.avgRttUs < b.avgRttUs) : (a.avgRttUs > b.avgRttUs);
            case ColLastRtt:
                return ascending ? (a.lastRttUs < b.lastRttUs) : (a.lastRttUs > b.lastRttUs);
            case ColElapsed:
                return ascending ? (a.elapsedMs < b.elapsedMs) : (a.elapsedMs > b.elapsedMs);
            default:
                return false;
            }
        });

    emit sortChanged(column, order);
}

bool ResultTableModel::removeRows(int row, int count, const QModelIndex& parent)
{
    if (parent.isValid()) return false;
    if (row < 0 || row + count > rowCount()) return false;

    beginRemoveRows(parent, row, row + count - 1);
    m_results.erase(m_results.begin() + row, m_results.begin() + row + count);
    endRemoveRows();
    return true;
}
