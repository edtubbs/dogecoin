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

    // Chain Tip Metrics
    QLabel* m_chainTipHeightValue;
    QLabel* m_chainTipDifficultyValue;
    QLabel* m_chainTipTimeValue;
    QLabel* m_chainTipBitsValue;
    SparklineWidget* m_chainTipHeightSpark;
    QVector<double> m_chainTipHeightSeries;

    // Mempool Metrics
    QLabel* m_mempoolTxCountValue;
    QLabel* m_mempoolTotalBytesValue;
    QLabel* m_mempoolP2pkhValue;
    QLabel* m_mempoolP2shValue;
    QLabel* m_mempoolMultisigValue;
    QLabel* m_mempoolOpReturnValue;
    QLabel* m_mempoolNonstandardValue;
    QLabel* m_mempoolOutputCountValue;
    SparklineWidget* m_mempoolTxSpark;
    SparklineWidget* m_mempoolBytesSpark;
    QVector<double> m_mempoolTxSeries;
    QVector<double> m_mempoolBytesSeries;

    // Rolling Stats Metrics
    QLabel* m_statsBlocksValue;
    QLabel* m_statsTransactionsValue;
    QLabel* m_statsTpsValue;
    QLabel* m_statsVolumeValue;
    QLabel* m_statsOutputsValue;
    QLabel* m_statsBytesValue;
    QLabel* m_statsMedianFeeValue;
    QLabel* m_statsAvgFeeValue;
    SparklineWidget* m_statsTpsSpark;
    QVector<double> m_statsTpsSeries;

    // Uptime
    QLabel* m_uptimeValue;

    // Network (for comparison with old metrics)
    QLabel* m_connectionsValue;
    QLabel* m_networkActiveValue;
    SparklineWidget* m_connectionsSpark;
    QVector<double> m_connectionsSeries;
};

#endif // BITCOIN_QT_DASHB0RDPAGE_H
