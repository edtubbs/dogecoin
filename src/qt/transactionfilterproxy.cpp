// Copyright (c) 2011-2016 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "transactionfilterproxy.h"

#include "transactiontablemodel.h"
#include "transactionrecord.h"

#include <cstdlib>

#include <QDateTime>

// Earliest date that can be represented (far in the past)
const QDateTime TransactionFilterProxy::MIN_DATE = QDateTime::fromTime_t(0);
// Last date that can be represented (far in the future)
const QDateTime TransactionFilterProxy::MAX_DATE = QDateTime::fromTime_t(0xFFFFFFFF);

TransactionFilterProxy::TransactionFilterProxy(QObject *parent) :
    QSortFilterProxyModel(parent),
    dateFrom(MIN_DATE),
    dateTo(MAX_DATE),
    addrPrefix(),
    typeFilter(ALL_TYPES),
    watchOnlyFilter(WatchOnlyFilter_All),
    minAmount(0),
    limitRows(-1),
    showInactive(true)
{
}

bool TransactionFilterProxy::filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const
{
    QModelIndex index = sourceModel()->index(sourceRow, 0, sourceParent);

    int type = index.data(TransactionTableModel::TypeRole).toInt();
    QDateTime datetime = index.data(TransactionTableModel::DateRole).toDateTime();
    bool involvesWatchAddress = index.data(TransactionTableModel::WatchonlyRole).toBool();
    QString address = index.data(TransactionTableModel::AddressRole).toString();
    QString label = index.data(TransactionTableModel::LabelRole).toString();
    qint64 amount = llabs(index.data(TransactionTableModel::AmountRole).toLongLong());
    int status = index.data(TransactionTableModel::StatusRole).toInt();

    if(!showInactive && status == TransactionStatus::Conflicted)
        return false;
    if(!(TYPE(type) & typeFilter))
        return false;
    if (involvesWatchAddress && watchOnlyFilter == WatchOnlyFilter_No)
        return false;
    if (!involvesWatchAddress && watchOnlyFilter == WatchOnlyFilter_Yes)
        return false;
    if(datetime < dateFrom || datetime > dateTo)
        return false;
    if (!address.contains(addrPrefix, Qt::CaseInsensitive) && !label.contains(addrPrefix, Qt::CaseInsensitive))
        return false;
    if(amount < minAmount)
        return false;

    return true;
}

void TransactionFilterProxy::setDateRange(const QDateTime &from, const QDateTime &to)
{
    this->dateFrom = from;
    this->dateTo = to;
    invalidateFilter();
}

void TransactionFilterProxy::setAddressPrefix(const QString &_addrPrefix)
{
    this->addrPrefix = _addrPrefix;
    invalidateFilter();
}

void TransactionFilterProxy::setTypeFilter(quint32 modes)
{
    this->typeFilter = modes;
    invalidateFilter();
}

void TransactionFilterProxy::setMinAmount(const CAmount& minimum)
{
    this->minAmount = minimum;
    invalidateFilter();
}

void TransactionFilterProxy::setWatchOnlyFilter(WatchOnlyFilter filter)
{
    this->watchOnlyFilter = filter;
    invalidateFilter();
}

void TransactionFilterProxy::setLimit(int limit)
{
    this->limitRows = limit;
}

void TransactionFilterProxy::setShowInactive(bool _showInactive)
{
    this->showInactive = _showInactive;
    invalidateFilter();
}

int TransactionFilterProxy::rowCount(const QModelIndex &parent) const
{
    if(limitRows != -1)
    {
        return std::min(QSortFilterProxyModel::rowCount(parent), limitRows);
    }
    else
    {
        return QSortFilterProxyModel::rowCount(parent);
    }
}

bool TransactionFilterProxy::lessThan(const QModelIndex &left, const QModelIndex &right) const
{
    // For Date and Status column sorts, use status.sortKey as the sole
    // comparison key.  The sortKey encodes (blockHeight, coinbase,
    // sort_time, sort_idx) where sort_time is the stable block timestamp
    // for confirmed transactions (identical for all tx in the same block)
    // and sort_idx carries a +1000 boost for PQC Commitment (TX_C) records
    // so they always appear above their paired PQC Reveal (TX_R) records.
    //
    // Using sortKey directly — rather than rec->time as a primary key —
    // ensures that the Transactions screen and the Overview (Wow) screen
    // produce the same ordering both before and after a wallet restart,
    // regardless of differences in nTimeSmart computation paths.
    if (left.column() == TransactionTableModel::Date ||
        left.column() == TransactionTableModel::Status)
    {
        QString leftKey  = left.sibling(left.row(),  TransactionTableModel::Status).data(Qt::EditRole).toString();
        QString rightKey = right.sibling(right.row(), TransactionTableModel::Status).data(Qt::EditRole).toString();
        return leftKey < rightKey;
    }

    // For all other columns (Amount, Type, …) use the column value as the
    // primary key with sortKey as a tiebreaker.
    QVariant leftData  = left.data(Qt::EditRole);
    QVariant rightData = right.data(Qt::EditRole);

    if (leftData != rightData)
        return QSortFilterProxyModel::lessThan(left, right);

    QString leftKey  = left.sibling(left.row(),  TransactionTableModel::Status).data(Qt::EditRole).toString();
    QString rightKey = right.sibling(right.row(), TransactionTableModel::Status).data(Qt::EditRole).toString();
    return leftKey < rightKey;
}
