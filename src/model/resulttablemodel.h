#pragma once
#include <QAbstractTableModel>
#include <QSortFilterProxyModel>
#include <vector>
#include "../ping/pingresult.h"

enum SortOrder { DefaultOrder, Ascending, Descending };

class ResultTableModel : public QAbstractTableModel {
    Q_OBJECT
public:
    enum Columns { ColNum=0, ColStatus, ColTarget, ColIpAddress, ColSent, ColReceived, ColLossRate, ColMinRtt, ColMaxRtt, ColAvgRtt, ColLastRtt, ColElapsed, ColCount };
    explicit ResultTableModel(QObject* parent = nullptr);
    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
    Qt::ItemFlags flags(const QModelIndex& index) const override;
    void setResults(const std::vector<PingResult>& results);
    void updateResults(const std::vector<PingResult>& results);
    void clear();
    void sortByColumn(int column, SortOrder order);
    int sortColumn() const { return m_sortColumn; }
    SortOrder sortOrder() const { return m_sortOrder; }

    // Remove rows (used by context menu delete)
    bool removeRows(int row, int count, const QModelIndex& parent = QModelIndex()) override;

signals:
    void sortChanged(int column, SortOrder order);
private:
    std::vector<PingResult> m_results;
    int m_sortColumn = ColNum;
    SortOrder m_sortOrder = DefaultOrder;
};
