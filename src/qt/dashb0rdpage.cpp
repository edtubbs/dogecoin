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

#include <QDateTime>
#include <QFont>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
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
    , m_blocksValue(nullptr)
    , m_headersValue(nullptr)
    , m_syncValue(nullptr)
    , m_ibdValue(nullptr)
    , m_tipAgeValue(nullptr)
    , m_warningsValue(nullptr)
    , m_blocksSpark(nullptr)
    , m_mempoolTxValue(nullptr)
    , m_mempoolBytesValue(nullptr)
    , m_mempoolTxSpark(nullptr)
    , m_mempoolBytesSpark(nullptr)
    , m_connectionsValue(nullptr)
    , m_networkActiveValue(nullptr)
    , m_connectionsSpark(nullptr)
{
    QVBoxLayout* outer = new QVBoxLayout(this);
    outer->setContentsMargins(18, 14, 18, 14);
    outer->setSpacing(12);

    QLabel* title = new QLabel(tr("Dashb0rd"));
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

    // Chain
    QGroupBox* chainBox = new QGroupBox(tr("Chain"));
    StyleSectionTitle(chainBox);
    QGridLayout* chainGrid = new QGridLayout(chainBox);
    chainGrid->setColumnStretch(0, 1);
    chainGrid->setColumnStretch(1, 0);

    AddRow(chainGrid, 0, tr("Blocks"), m_blocksValue);
    AddRow(chainGrid, 1, tr("Headers"), m_headersValue);
    AddRow(chainGrid, 2, tr("Sync"), m_syncValue);
    AddRow(chainGrid, 3, tr("IBD"), m_ibdValue);
    AddRow(chainGrid, 4, tr("Tip age"), m_tipAgeValue);
    AddRow(chainGrid, 5, tr("Warnings"), m_warningsValue);

    m_blocksSpark = new SparklineWidget(chainBox);
    m_blocksSpark->setMinimumHeight(38);
    chainGrid->addWidget(m_blocksSpark, 6, 0, 1, 2);

    topGrid->addWidget(chainBox, 0, 0);

    // Mempool
    QGroupBox* mempoolBox = new QGroupBox(tr("Mempool"));
    StyleSectionTitle(mempoolBox);
    QGridLayout* memGrid = new QGridLayout(mempoolBox);
    memGrid->setColumnStretch(0, 1);
    memGrid->setColumnStretch(1, 0);

    AddRow(memGrid, 0, tr("Transactions"), m_mempoolTxValue);
    AddRow(memGrid, 1, tr("Bytes"), m_mempoolBytesValue);

    m_mempoolTxSpark = new SparklineWidget(mempoolBox);
    m_mempoolTxSpark->setMinimumHeight(38);
    memGrid->addWidget(m_mempoolTxSpark, 2, 0, 1, 2);

    m_mempoolBytesSpark = new SparklineWidget(mempoolBox);
    m_mempoolBytesSpark->setMinimumHeight(38);
    memGrid->addWidget(m_mempoolBytesSpark, 3, 0, 1, 2);

    topGrid->addWidget(mempoolBox, 0, 1);

    // Network
    QGroupBox* netBox = new QGroupBox(tr("Network"));
    StyleSectionTitle(netBox);
    QGridLayout* netGrid = new QGridLayout(netBox);
    netGrid->setColumnStretch(0, 1);
    netGrid->setColumnStretch(1, 0);

    AddRow(netGrid, 0, tr("Connections"), m_connectionsValue);
    AddRow(netGrid, 1, tr("Network active"), m_networkActiveValue);

    m_connectionsSpark = new SparklineWidget(netBox);
    m_connectionsSpark->setMinimumHeight(38);
    netGrid->addWidget(m_connectionsSpark, 2, 0, 1, 2);

    topGrid->addWidget(netBox, 0, 2);

    topGrid->setColumnStretch(0, 1);
    topGrid->setColumnStretch(1, 1);
    topGrid->setColumnStretch(2, 1);

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
        m_blocksValue->setText(tr("n/a"));
        m_headersValue->setText(tr("n/a"));
        m_syncValue->setText(tr("n/a"));
        m_ibdValue->setText(tr("n/a"));
        m_tipAgeValue->setText(tr("n/a"));
        m_warningsValue->setText(tr("n/a"));
        m_mempoolTxValue->setText(tr("n/a"));
        m_mempoolBytesValue->setText(tr("n/a"));
        m_connectionsValue->setText(tr("n/a"));
        m_networkActiveValue->setText(tr("n/a"));
        return;
    }

    // Chain
    const int blocks = m_clientModel->getNumBlocks();
    const int headers = m_clientModel->getHeaderTipHeight();
    const double vp = m_clientModel->getVerificationProgress(nullptr);
    const bool ibd = m_clientModel->inInitialBlockDownload();

    const QDateTime lastBlockDate = m_clientModel->getLastBlockDate();
    qint64 tipAgeSecs = lastBlockDate.isValid() ? lastBlockDate.secsTo(now) : 0;
    if (tipAgeSecs < 0) tipAgeSecs = 0;

    const QString warnings = m_clientModel->getStatusBarWarnings();

    m_blocksValue->setText(QString::number(blocks));
    m_headersValue->setText(QString::number(headers));
    m_syncValue->setText(QString::number(vp * 100.0, 'f', 2) + "%");
    m_ibdValue->setText(ibd ? tr("yes") : tr("no"));
    m_tipAgeValue->setText(GUIUtil::formatNiceTimeOffset(tipAgeSecs));
    m_warningsValue->setText(warnings.isEmpty() ? tr("none") : warnings);

    pushSample(m_blocksSeries, m_blocksSpark, static_cast<double>(blocks));

    // Mempool
    const int64_t mempoolTx = m_clientModel->getMempoolSize();

    // Use dynamic usage for bytes (ClientModel does not expose getMempoolBytes() in your tree)
    const qint64 mempoolBytes = static_cast<qint64>(m_clientModel->getMempoolDynamicUsage());

    m_mempoolTxValue->setText(QString::number(mempoolTx));
    m_mempoolBytesValue->setText(GUIUtil::formatBytes(mempoolBytes));

    pushSample(m_mempoolTxSeries, m_mempoolTxSpark, static_cast<double>(mempoolTx));
    pushSample(m_mempoolBytesSeries, m_mempoolBytesSpark, static_cast<double>(mempoolBytes));

    // Network
    const int conns = m_clientModel->getNumConnections();
    const bool netActive = m_clientModel->getNetworkActive();

    m_connectionsValue->setText(QString::number(conns));
    m_networkActiveValue->setText(netActive ? tr("yes") : tr("no"));

    pushSample(m_connectionsSeries, m_connectionsSpark, static_cast<double>(conns));
}
