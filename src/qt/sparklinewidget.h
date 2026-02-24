// Copyright (c) 2011-2016 The Bitcoin Core developers
// Copyright (c) 2021-2026 The Dogecoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_SPARKLINEWIDGET_H
#define BITCOIN_QT_SPARKLINEWIDGET_H

#include <cstdint>

#include <QString>
#include <QVector>
#include <QWidget>

class QEvent;
class QMouseEvent;

class SparklineWidget : public QWidget
{
public:
    explicit SparklineWidget(QWidget* parent = nullptr);
    ~SparklineWidget() override;

    void setData(const QVector<double>& data);
    void setPointContext(const QString& txid, const QString& blockHash = QString());
    void clear();

protected:
    void paintEvent(QPaintEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void leaveEvent(QEvent* event) override;

private:
    QVector<double> m_data;
    QVector<qint64> m_timestamps;
    QVector<QString> m_txids;
    QVector<QString> m_blockHashes;
    QString m_pointTxid;
    QString m_pointBlockHash;
};

#endif // BITCOIN_QT_SPARKLINEWIDGET_H
