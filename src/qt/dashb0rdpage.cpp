// Copyright (c) 2026
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#if defined(HAVE_CONFIG_H)
#include "config/bitcoin-config.h"
#endif

#include "dashb0rdpage.h"

#include "clientmodel.h"
#include "guiutil.h"
#include "platformstyle.h"
#include "sparklinewidget.h"

#include "rpc/client.h"
#include "rpc/protocol.h"
#include "util.h"
#include "utilstrencodings.h"

#include <univalue.h>

#include <QDateTime>
#include <QFont>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QScrollArea>
#include <QTimer>
#include <QVBoxLayout>

namespace {
static const int kPollIntervalMs = 1000;
static const int kMaxSparkPoints = 120;

static QLabel* MakeKeyLabel(const QString& txt)
{
    QLabel* l = new QLabel(txt);
    l->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    l->setTextInteractionFlags(Qt::TextSelectableByMouse);
    return l;
}

static QLabel* MakeValueLabel()
{
    QLabel* l = new QLabel(QObject::tr("n/a"));
    l->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    l->setTextInteractionFlags(Qt::TextSelectableByMouse);
    l->setMinimumWidth(140);
    return l;
}

static void AddRow(QGridLayout* grid, int row, const QString& key, QLabel*& outValue)
{
    grid->addWidget(MakeKeyLabel(key), row, 0);
    outValue = MakeValueLabel();
    grid->addWidget(outValue, row, 1);
}

static void StyleSectionTitle(QGroupBox* box)
{
    QFont f = box->font();
    f.setBold(true);
    box->setFont(f);
}
} // namespace

Dashb0rdPage::Dashb0rdPage(const PlatformStyle* platformStyle, QWidget* parent)
    : QWidget(parent)
    , m_clientModel(nullptr)
    , m_walletModel(nullptr)
    , m_platformStyle(platformStyle)
    , m_pollTimer(new QTimer(this))
    , m_lastUpdated(nullptr)
    , m_chainTipHeightValue(nullptr)
    , m_chainTipDifficultyValue(nullptr)
    , m_chainTipTimeValue(nullptr)
    , m_chainTipBitsValue(nullptr)
    , m_chainTipHeightSpark(nullptr)
    , m_mempoolTxCountValue(nullptr)
    , m_mempoolTotalBytesValue(nullptr)
    , m_mempoolP2pkhValue(nullptr)
    , m_mempoolP2shValue(nullptr)
    , m_mempoolMultisigValue(nullptr)
    , m_mempoolOpReturnValue(nullptr)
    , m_mempoolNonstandardValue(nullptr)
    , m_mempoolOutputCountValue(nullptr)
    , m_mempoolTxSpark(nullptr)
    , m_mempoolBytesSpark(nullptr)
    , m_statsBlocksValue(nullptr)
    , m_statsTransactionsValue(nullptr)
    , m_statsTpsValue(nullptr)
    , m_statsVolumeValue(nullptr)
    , m_statsOutputsValue(nullptr)
    , m_statsBytesValue(nullptr)
    , m_statsMedianFeeValue(nullptr)
    , m_statsAvgFeeValue(nullptr)
    , m_statsTpsSpark(nullptr)
    , m_uptimeValue(nullptr)
    , m_connectionsValue(nullptr)
    , m_networkActiveValue(nullptr)
    , m_connectionsSpark(nullptr)
{
    // Create scroll area to fit all metrics
    QScrollArea* scrollArea = new QScrollArea(this);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    
    QWidget* scrollContent = new QWidget();
    QVBoxLayout* outer = new QVBoxLayout(scrollContent);
    outer->setContentsMargins(18, 14, 18, 14);
    outer->setSpacing(12);

    QLabel* title = new QLabel(tr("Dashb0rd - All Metrics"));
    QFont tf = title->font();
    tf.setPointSize(tf.pointSize() + 8);
    tf.setBold(true);
    title->setFont(tf);
    outer->addWidget(title);

    m_lastUpdated = new QLabel(tr("Last updated: n/a"));
    m_lastUpdated->setTextInteractionFlags(Qt::TextSelectableByMouse);
    outer->addWidget(m_lastUpdated);

    QGridLayout* topGrid = new QGridLayout();
    topGrid->setHorizontalSpacing(14);
    topGrid->setVerticalSpacing(12);
    outer->addLayout(topGrid);

    // Chain Tip Metrics Section
    QGroupBox* chainTipBox = new QGroupBox(tr("Chain Tip"));
    StyleSectionTitle(chainTipBox);
    QGridLayout* chainTipGrid = new QGridLayout(chainTipBox);
    chainTipGrid->setColumnStretch(0, 1);
    chainTipGrid->setColumnStretch(1, 0);

    AddRow(chainTipGrid, 0, tr("Height"), m_chainTipHeightValue);
    AddRow(chainTipGrid, 1, tr("Difficulty"), m_chainTipDifficultyValue);
    AddRow(chainTipGrid, 2, tr("Time"), m_chainTipTimeValue);
    AddRow(chainTipGrid, 3, tr("Bits (hex)"), m_chainTipBitsValue);

    m_chainTipHeightSpark = new SparklineWidget(chainTipBox);
    m_chainTipHeightSpark->setMinimumHeight(38);
    chainTipGrid->addWidget(m_chainTipHeightSpark, 4, 0, 1, 2);

    topGrid->addWidget(chainTipBox, 0, 0);

    // Mempool Metrics Section
    QGroupBox* mempoolBox = new QGroupBox(tr("Mempool"));
    StyleSectionTitle(mempoolBox);
    QGridLayout* memGrid = new QGridLayout(mempoolBox);
    memGrid->setColumnStretch(0, 1);
    memGrid->setColumnStretch(1, 0);

    AddRow(memGrid, 0, tr("Transactions"), m_mempoolTxCountValue);
    AddRow(memGrid, 1, tr("Total Bytes"), m_mempoolTotalBytesValue);
    AddRow(memGrid, 2, tr("P2PKH Count"), m_mempoolP2pkhValue);
    AddRow(memGrid, 3, tr("P2SH Count"), m_mempoolP2shValue);
    AddRow(memGrid, 4, tr("Multisig Count"), m_mempoolMultisigValue);
    AddRow(memGrid, 5, tr("OP_RETURN Count"), m_mempoolOpReturnValue);
    AddRow(memGrid, 6, tr("Nonstandard Count"), m_mempoolNonstandardValue);
    AddRow(memGrid, 7, tr("Output Count"), m_mempoolOutputCountValue);

    m_mempoolTxSpark = new SparklineWidget(mempoolBox);
    m_mempoolTxSpark->setMinimumHeight(38);
    memGrid->addWidget(m_mempoolTxSpark, 8, 0, 1, 2);

    m_mempoolBytesSpark = new SparklineWidget(mempoolBox);
    m_mempoolBytesSpark->setMinimumHeight(38);
    memGrid->addWidget(m_mempoolBytesSpark, 9, 0, 1, 2);

    topGrid->addWidget(mempoolBox, 0, 1);

    // Rolling Statistics Section
    QGroupBox* statsBox = new QGroupBox(tr("Rolling Statistics (Last 100 Blocks)"));
    StyleSectionTitle(statsBox);
    QGridLayout* statsGrid = new QGridLayout(statsBox);
    statsGrid->setColumnStretch(0, 1);
    statsGrid->setColumnStretch(1, 0);

    AddRow(statsGrid, 0, tr("Blocks Analyzed"), m_statsBlocksValue);
    AddRow(statsGrid, 1, tr("Total Transactions"), m_statsTransactionsValue);
    AddRow(statsGrid, 2, tr("TPS"), m_statsTpsValue);
    AddRow(statsGrid, 3, tr("Volume (DOGE)"), m_statsVolumeValue);
    AddRow(statsGrid, 4, tr("Outputs"), m_statsOutputsValue);
    AddRow(statsGrid, 5, tr("Bytes"), m_statsBytesValue);
    AddRow(statsGrid, 6, tr("Median Fee/Block"), m_statsMedianFeeValue);
    AddRow(statsGrid, 7, tr("Avg Fee/Block"), m_statsAvgFeeValue);

    m_statsTpsSpark = new SparklineWidget(statsBox);
    m_statsTpsSpark->setMinimumHeight(38);
    statsGrid->addWidget(m_statsTpsSpark, 8, 0, 1, 2);

    topGrid->addWidget(statsBox, 1, 0);

    // Network & Uptime Section
    QGroupBox* networkBox = new QGroupBox(tr("Network & Uptime"));
    StyleSectionTitle(networkBox);
    QGridLayout* netGrid = new QGridLayout(networkBox);
    netGrid->setColumnStretch(0, 1);
    netGrid->setColumnStretch(1, 0);

    AddRow(netGrid, 0, tr("Connections"), m_connectionsValue);
    AddRow(netGrid, 1, tr("Network Active"), m_networkActiveValue);
    AddRow(netGrid, 2, tr("Uptime"), m_uptimeValue);

    m_connectionsSpark = new SparklineWidget(networkBox);
    m_connectionsSpark->setMinimumHeight(38);
    netGrid->addWidget(m_connectionsSpark, 3, 0, 1, 2);

    topGrid->addWidget(networkBox, 1, 1);

    topGrid->setColumnStretch(0, 1);
    topGrid->setColumnStretch(1, 1);

    scrollArea->setWidget(scrollContent);
    
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->addWidget(scrollArea);

    connect(m_pollTimer, SIGNAL(timeout()), this, SLOT(pollStats()));
    m_pollTimer->setInterval(kPollIntervalMs);
    m_pollTimer->start();

    pollStats();
}

Dashb0rdPage::~Dashb0rdPage() = default;

void Dashb0rdPage::setClientModel(ClientModel* model)
{
    m_clientModel = model;
    pollStats();
}

void Dashb0rdPage::setWalletModel(WalletModel* model)
{
    m_walletModel = model;
    (void)m_walletModel; // silence unused for now
    pollStats();
}

void Dashb0rdPage::pushSample(QVector<double>& series, SparklineWidget* spark, double value)
{
    series.push_back(value);
    if (series.size() > kMaxSparkPoints) {
        const int extra = series.size() - kMaxSparkPoints;
        series.erase(series.begin(), series.begin() + extra);
    }
    if (spark) {
        spark->setData(series);
    }
}

void Dashb0rdPage::pollStats()
{
    const QDateTime now = QDateTime::currentDateTime();
    m_lastUpdated->setText(tr("Last updated: %1").arg(now.toString(Qt::ISODate)));

    if (!m_clientModel) {
        // Set all to n/a
        if (m_chainTipHeightValue) m_chainTipHeightValue->setText(tr("n/a"));
        if (m_chainTipDifficultyValue) m_chainTipDifficultyValue->setText(tr("n/a"));
        if (m_chainTipTimeValue) m_chainTipTimeValue->setText(tr("n/a"));
        if (m_chainTipBitsValue) m_chainTipBitsValue->setText(tr("n/a"));
        if (m_mempoolTxCountValue) m_mempoolTxCountValue->setText(tr("n/a"));
        if (m_mempoolTotalBytesValue) m_mempoolTotalBytesValue->setText(tr("n/a"));
        if (m_connectionsValue) m_connectionsValue->setText(tr("n/a"));
        if (m_networkActiveValue) m_networkActiveValue->setText(tr("n/a"));
        if (m_uptimeValue) m_uptimeValue->setText(tr("n/a"));
        return;
    }

    // Call getdashboardmetrics RPC
    try {
        UniValue params(UniValue::VARR);
        UniValue result = m_clientModel->getChainTipBlockHash(); // We'll use a different approach
        
        // For now, get metrics directly from ClientModel
        // In a production implementation, you'd call the RPC through the client
        
        // Network stats (available from ClientModel)
        const int conns = m_clientModel->getNumConnections();
        const bool netActive = m_clientModel->getNetworkActive();
        
        m_connectionsValue->setText(QString::number(conns));
        m_networkActiveValue->setText(netActive ? tr("yes") : tr("no"));
        pushSample(m_connectionsSeries, m_connectionsSpark, static_cast<double>(conns));
        
        // Chain tip from ClientModel
        const int blocks = m_clientModel->getNumBlocks();
        m_chainTipHeightValue->setText(QString::number(blocks));
        pushSample(m_chainTipHeightSeries, m_chainTipHeightSpark, static_cast<double>(blocks));
        
        // Get difficulty, etc. - would need to call RPC
        // For now, show basic info
        m_chainTipDifficultyValue->setText(tr("RPC call required"));
        m_chainTipTimeValue->setText(m_clientModel->getLastBlockDate().toString(Qt::ISODate));
        m_chainTipBitsValue->setText(tr("RPC call required"));
        
        // Mempool from ClientModel
        const int64_t mempoolTx = m_clientModel->getMempoolSize();
        const qint64 mempoolBytes = static_cast<qint64>(m_clientModel->getMempoolDynamicUsage());
        
        m_mempoolTxCountValue->setText(QString::number(mempoolTx));
        m_mempoolTotalBytesValue->setText(GUIUtil::formatBytes(mempoolBytes));
        pushSample(m_mempoolTxSeries, m_mempoolTxSpark, static_cast<double>(mempoolTx));
        pushSample(m_mempoolBytesSeries, m_mempoolBytesSpark, static_cast<double>(mempoolBytes));
        
        // Mempool output types - would need RPC call
        m_mempoolP2pkhValue->setText(tr("RPC call required"));
        m_mempoolP2shValue->setText(tr("RPC call required"));
        m_mempoolMultisigValue->setText(tr("RPC call required"));
        m_mempoolOpReturnValue->setText(tr("RPC call required"));
        m_mempoolNonstandardValue->setText(tr("RPC call required"));
        m_mempoolOutputCountValue->setText(tr("RPC call required"));
        
        // Rolling stats - would need RPC call
        m_statsBlocksValue->setText(tr("RPC call required"));
        m_statsTransactionsValue->setText(tr("RPC call required"));
        m_statsTpsValue->setText(tr("RPC call required"));
        m_statsVolumeValue->setText(tr("RPC call required"));
        m_statsOutputsValue->setText(tr("RPC call required"));
        m_statsBytesValue->setText(tr("RPC call required"));
        m_statsMedianFeeValue->setText(tr("RPC call required"));
        m_statsAvgFeeValue->setText(tr("RPC call required"));
        
        // Uptime - would need RPC call  
        m_uptimeValue->setText(tr("RPC call required"));
        
    } catch (const std::exception& e) {
        // Error handling
        LogPrintf("Dashboard metrics error: %s\n", e.what());
    }
}
