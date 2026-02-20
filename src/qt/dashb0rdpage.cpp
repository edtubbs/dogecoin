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
#include "rpc/server.h"
#include "util.h"

#include <univalue.h>

#include <QDateTime>
#include <QFont>
#include <QFrame>
#include <QGridLayout>
#include <QLabel>
#include <QPalette>
#include <QScrollArea>
#include <QTimer>
#include <QVBoxLayout>

#include <algorithm>
#include <limits>

namespace {
static const int kPollIntervalMs = 1000;
static const int kMaxSparkPoints = 120;

static QLabel* MakeValueLabel()
{
    QLabel* l = new QLabel(QObject::tr("n/a"));
    l->setAlignment(Qt::AlignCenter);
    l->setTextInteractionFlags(Qt::TextSelectableByMouse);
    QFont f = l->font();
    f.setPointSize(f.pointSize() + 2);
    l->setFont(f);
    return l;
}

static int64_t GetInt64(const UniValue& obj, const char* key)
{
    const UniValue& v = find_value(obj, key);
    return v.isNum() ? v.get_int64() : 0;
}

static double GetDouble(const UniValue& obj, const char* key)
{
    const UniValue& v = find_value(obj, key);
    return v.isNum() ? v.get_real() : 0.0;
}

static QString GetString(const UniValue& obj, const char* key)
{
    const UniValue& v = find_value(obj, key);
    return v.isStr() ? QString::fromStdString(v.get_str()) : QString();
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
    , m_chainTipDifficultySpark(nullptr)
    , m_chainTipTimeSpark(nullptr)
    , m_chainTipBitsSpark(nullptr)
    , m_mempoolTxCountValue(nullptr)
    , m_mempoolTotalBytesValue(nullptr)
    , m_mempoolP2pkhValue(nullptr)
    , m_mempoolP2shValue(nullptr)
    , m_mempoolMultisigValue(nullptr)
    , m_mempoolOpReturnValue(nullptr)
    , m_mempoolNonstandardValue(nullptr)
    , m_mempoolOutputCountValue(nullptr)
    , m_mempoolTxCountSpark(nullptr)
    , m_mempoolTotalBytesSpark(nullptr)
    , m_mempoolP2pkhSpark(nullptr)
    , m_mempoolP2shSpark(nullptr)
    , m_mempoolMultisigSpark(nullptr)
    , m_mempoolOpReturnSpark(nullptr)
    , m_mempoolNonstandardSpark(nullptr)
    , m_mempoolOutputCountSpark(nullptr)
    , m_statsBlocksValue(nullptr)
    , m_statsTransactionsValue(nullptr)
    , m_statsTpsValue(nullptr)
    , m_statsVolumeValue(nullptr)
    , m_statsOutputsValue(nullptr)
    , m_statsBytesValue(nullptr)
    , m_statsMedianFeeValue(nullptr)
    , m_statsAvgFeeValue(nullptr)
    , m_statsBlocksSpark(nullptr)
    , m_statsTransactionsSpark(nullptr)
    , m_statsTpsSpark(nullptr)
    , m_statsVolumeSpark(nullptr)
    , m_statsOutputsSpark(nullptr)
    , m_statsBytesSpark(nullptr)
    , m_statsMedianFeeSpark(nullptr)
    , m_statsAvgFeeSpark(nullptr)
    , m_uptimeValue(nullptr)
    , m_uptimeSpark(nullptr)
{
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

    QGridLayout* grid = new QGridLayout();
    grid->setHorizontalSpacing(10);
    grid->setVerticalSpacing(10);

    int row = 0;
    int col = 0;
    const int cols = 4;

    auto addMetric = [&](const QString& label, QLabel*& value, SparklineWidget*& spark) {
        grid->addWidget(createMetricBox(label, value, spark), row, col);
        if (++col >= cols) {
            col = 0;
            ++row;
        }
    };

    addMetric(tr("Block Height"), m_chainTipHeightValue, m_chainTipHeightSpark);
    addMetric(tr("Difficulty"), m_chainTipDifficultyValue, m_chainTipDifficultySpark);
    addMetric(tr("Chain Tip Time"), m_chainTipTimeValue, m_chainTipTimeSpark);
    addMetric(tr("Bits (hex)"), m_chainTipBitsValue, m_chainTipBitsSpark);

    addMetric(tr("Mempool TX"), m_mempoolTxCountValue, m_mempoolTxCountSpark);
    addMetric(tr("Mempool Bytes"), m_mempoolTotalBytesValue, m_mempoolTotalBytesSpark);
    addMetric(tr("P2PKH Count"), m_mempoolP2pkhValue, m_mempoolP2pkhSpark);
    addMetric(tr("P2SH Count"), m_mempoolP2shValue, m_mempoolP2shSpark);
    addMetric(tr("Multisig Count"), m_mempoolMultisigValue, m_mempoolMultisigSpark);
    addMetric(tr("OP_RETURN Count"), m_mempoolOpReturnValue, m_mempoolOpReturnSpark);
    addMetric(tr("Nonstandard Count"), m_mempoolNonstandardValue, m_mempoolNonstandardSpark);
    addMetric(tr("Total Outputs"), m_mempoolOutputCountValue, m_mempoolOutputCountSpark);

    addMetric(tr("Blocks (100)"), m_statsBlocksValue, m_statsBlocksSpark);
    addMetric(tr("Transactions"), m_statsTransactionsValue, m_statsTransactionsSpark);
    addMetric(tr("TPS"), m_statsTpsValue, m_statsTpsSpark);
    addMetric(tr("Volume (DOGE)"), m_statsVolumeValue, m_statsVolumeSpark);
    addMetric(tr("Outputs"), m_statsOutputsValue, m_statsOutputsSpark);
    addMetric(tr("Bytes"), m_statsBytesValue, m_statsBytesSpark);
    addMetric(tr("Median Fee/Block"), m_statsMedianFeeValue, m_statsMedianFeeSpark);
    addMetric(tr("Avg Fee/Block"), m_statsAvgFeeValue, m_statsAvgFeeSpark);

    addMetric(tr("Uptime"), m_uptimeValue, m_uptimeSpark);

    for (int i = 0; i < cols; ++i) {
        grid->setColumnStretch(i, 1);
    }

    outer->addLayout(grid);
    outer->addStretch();

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
    (void)m_walletModel;
    pollStats();
}

QWidget* Dashb0rdPage::createMetricBox(const QString& label, QLabel*& valueLabel, SparklineWidget*& spark)
{
    QFrame* box = new QFrame(this);
    box->setFrameStyle(QFrame::StyledPanel | QFrame::Raised);

    QPalette pal = box->palette();
    pal.setColor(QPalette::Window, palette().color(QPalette::AlternateBase));
    box->setAutoFillBackground(true);
    box->setPalette(pal);

    QVBoxLayout* layout = new QVBoxLayout(box);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(6);

    QLabel* title = new QLabel(label, box);
    QFont titleFont = title->font();
    titleFont.setBold(true);
    title->setFont(titleFont);
    title->setAlignment(Qt::AlignCenter);

    valueLabel = MakeValueLabel();
    spark = new SparklineWidget(box);
    spark->setMinimumHeight(40);

    layout->addWidget(title);
    layout->addWidget(valueLabel);
    layout->addWidget(spark);

    return box;
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
        return;
    }

    try {
        JSONRPCRequest req;
        req.strMethod = "getdashboardmetrics";
        req.params = UniValue(UniValue::VARR);
        const UniValue result = tableRPC.execute(req);

        const int64_t chainTipHeight = GetInt64(result, "chain_tip_height");
        m_chainTipHeightValue->setText(QString::number(chainTipHeight));
        pushSample(m_chainTipHeightSeries, m_chainTipHeightSpark, static_cast<double>(chainTipHeight));

        const double chainTipDifficulty = GetDouble(result, "chain_tip_difficulty");
        m_chainTipDifficultyValue->setText(QString::number(chainTipDifficulty, 'f', 2));
        pushSample(m_chainTipDifficultySeries, m_chainTipDifficultySpark, chainTipDifficulty);

        const QString chainTipTime = GetString(result, "chain_tip_time");
        m_chainTipTimeValue->setText(chainTipTime);
        const qint64 chainTipTimeEpoch = static_cast<qint64>(QDateTime::fromString(chainTipTime, Qt::ISODate).toTime_t());
        pushSample(m_chainTipTimeSeries, m_chainTipTimeSpark, static_cast<double>(chainTipTimeEpoch));

        const QString chainTipBits = GetString(result, "chain_tip_bits_hex");
        m_chainTipBitsValue->setText(chainTipBits);
        bool bitsOk = false;
        const quint64 bitsValue = chainTipBits.toULongLong(&bitsOk, 0);
        pushSample(m_chainTipBitsSeries, m_chainTipBitsSpark, bitsOk ? static_cast<double>(bitsValue) : 0.0);

        const int64_t mempoolTxCount = GetInt64(result, "mempool_tx_count");
        m_mempoolTxCountValue->setText(QString::number(mempoolTxCount));
        pushSample(m_mempoolTxCountSeries, m_mempoolTxCountSpark, static_cast<double>(mempoolTxCount));

        const int64_t mempoolTotalBytes = GetInt64(result, "mempool_total_bytes");
        m_mempoolTotalBytesValue->setText(GUIUtil::formatBytes(mempoolTotalBytes));
        pushSample(m_mempoolTotalBytesSeries, m_mempoolTotalBytesSpark, static_cast<double>(mempoolTotalBytes));

        const int64_t mempoolP2pkhCount = GetInt64(result, "mempool_p2pkh_count");
        m_mempoolP2pkhValue->setText(QString::number(mempoolP2pkhCount));
        pushSample(m_mempoolP2pkhSeries, m_mempoolP2pkhSpark, static_cast<double>(mempoolP2pkhCount));

        const int64_t mempoolP2shCount = GetInt64(result, "mempool_p2sh_count");
        m_mempoolP2shValue->setText(QString::number(mempoolP2shCount));
        pushSample(m_mempoolP2shSeries, m_mempoolP2shSpark, static_cast<double>(mempoolP2shCount));

        const int64_t mempoolMultisigCount = GetInt64(result, "mempool_multisig_count");
        m_mempoolMultisigValue->setText(QString::number(mempoolMultisigCount));
        pushSample(m_mempoolMultisigSeries, m_mempoolMultisigSpark, static_cast<double>(mempoolMultisigCount));

        const int64_t mempoolOpReturnCount = GetInt64(result, "mempool_op_return_count");
        m_mempoolOpReturnValue->setText(QString::number(mempoolOpReturnCount));
        pushSample(m_mempoolOpReturnSeries, m_mempoolOpReturnSpark, static_cast<double>(mempoolOpReturnCount));

        const int64_t mempoolNonstandardCount = GetInt64(result, "mempool_nonstandard_count");
        m_mempoolNonstandardValue->setText(QString::number(mempoolNonstandardCount));
        pushSample(m_mempoolNonstandardSeries, m_mempoolNonstandardSpark, static_cast<double>(mempoolNonstandardCount));

        const int64_t mempoolOutputCount = GetInt64(result, "mempool_output_count");
        m_mempoolOutputCountValue->setText(QString::number(mempoolOutputCount));
        pushSample(m_mempoolOutputCountSeries, m_mempoolOutputCountSpark, static_cast<double>(mempoolOutputCount));

        const int64_t statsBlocks = GetInt64(result, "stats_blocks");
        m_statsBlocksValue->setText(QString::number(statsBlocks));
        pushSample(m_statsBlocksSeries, m_statsBlocksSpark, static_cast<double>(statsBlocks));

        const int64_t statsTransactions = GetInt64(result, "stats_transactions");
        m_statsTransactionsValue->setText(QString::number(statsTransactions));
        pushSample(m_statsTransactionsSeries, m_statsTransactionsSpark, static_cast<double>(statsTransactions));

        const double statsTps = GetDouble(result, "stats_tps");
        m_statsTpsValue->setText(QString::number(statsTps, 'f', 3));
        pushSample(m_statsTpsSeries, m_statsTpsSpark, statsTps);

        const double statsVolume = GetDouble(result, "stats_volume");
        m_statsVolumeValue->setText(QString::number(statsVolume, 'f', 2));
        pushSample(m_statsVolumeSeries, m_statsVolumeSpark, statsVolume);

        const int64_t statsOutputs = GetInt64(result, "stats_outputs");
        m_statsOutputsValue->setText(QString::number(statsOutputs));
        pushSample(m_statsOutputsSeries, m_statsOutputsSpark, static_cast<double>(statsOutputs));

        const int64_t statsBytes = GetInt64(result, "stats_bytes");
        m_statsBytesValue->setText(GUIUtil::formatBytes(statsBytes));
        pushSample(m_statsBytesSeries, m_statsBytesSpark, static_cast<double>(statsBytes));

        const double statsMedianFeePerBlock = GetDouble(result, "stats_median_fee_per_block");
        m_statsMedianFeeValue->setText(QString::number(statsMedianFeePerBlock, 'f', 8));
        pushSample(m_statsMedianFeeSeries, m_statsMedianFeeSpark, statsMedianFeePerBlock);

        const double statsAvgFeePerBlock = GetDouble(result, "stats_avg_fee_per_block");
        m_statsAvgFeeValue->setText(QString::number(statsAvgFeePerBlock, 'f', 8));
        pushSample(m_statsAvgFeeSeries, m_statsAvgFeeSpark, statsAvgFeePerBlock);

        const int64_t uptimeSec = GetInt64(result, "uptime_sec");
        if (uptimeSec > std::numeric_limits<int>::max()) {
            m_uptimeValue->setText(QString::number(uptimeSec) + tr(" s"));
        } else {
            m_uptimeValue->setText(GUIUtil::formatDurationStr(static_cast<int>(uptimeSec)));
        }
        pushSample(m_uptimeSeries, m_uptimeSpark, static_cast<double>(uptimeSec));
    } catch (const UniValue& objError) {
        LogPrintf("Dashboard RPC error: %s\n", objError.write().c_str());
    } catch (const std::exception& e) {
        LogPrintf("Dashboard metrics error: %s\n", e.what());
    }
}
