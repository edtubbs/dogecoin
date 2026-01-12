// Copyright (c) 2026
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_DASHB0RDPAGE_H
#define BITCOIN_QT_DASHB0RDPAGE_H

#include <QVector>
#include <QWidget>

class ClientModel;
class PlatformStyle;
class QLabel;
class QTimer;
class SparklineWidget;
class WalletModel;

class Dashb0rdPage : public QWidget
{
    Q_OBJECT

public:
    explicit Dashb0rdPage(const PlatformStyle* platformStyle, QWidget* parent = nullptr);
    ~Dashb0rdPage() override;

    void setClientModel(ClientModel* model);

    // Dashb0rd container calls this. We store it for future wallet-specific stats.
    void setWalletModel(WalletModel* model);

private Q_SLOTS:
    void pollStats();

private:
    void pushSample(QVector<double>& series, SparklineWidget* spark, double value);

    ClientModel* m_clientModel;
    WalletModel* m_walletModel;
    const PlatformStyle* m_platformStyle;

    QTimer* m_pollTimer;
    QLabel* m_lastUpdated;

    // Chain
    QLabel* m_blocksValue;
    QLabel* m_headersValue;
    QLabel* m_syncValue;
    QLabel* m_ibdValue;
    QLabel* m_tipAgeValue;
    QLabel* m_warningsValue;

    SparklineWidget* m_blocksSpark;
    QVector<double> m_blocksSeries;

    // Mempool
    QLabel* m_mempoolTxValue;
    QLabel* m_mempoolBytesValue;

    SparklineWidget* m_mempoolTxSpark;
    SparklineWidget* m_mempoolBytesSpark;
    QVector<double> m_mempoolTxSeries;
    QVector<double> m_mempoolBytesSeries;

    // Network
    QLabel* m_connectionsValue;
    QLabel* m_networkActiveValue;

    SparklineWidget* m_connectionsSpark;
    QVector<double> m_connectionsSeries;
};

#endif // BITCOIN_QT_DASHB0RDPAGE_H
