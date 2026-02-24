// Copyright (c) 2011-2016 The Bitcoin Core developers
// Copyright (c) 2021-2026 The Dogecoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "sparklinewidget.h"

#include <QDateTime>
#include <QDialog>
#include <QDialogButtonBox>
#include <QEvent>
#include <QHeaderView>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QPaintEvent>
#include <QStringList>
#include <QToolTip>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <QtMath>

#include "rpc/client.h"
#include "rpc/server.h"

#include <univalue.h>

#include <algorithm>
#include <cctype>
#include <limits>

namespace {
static const int kDecodedTreeInitialDepth = 1;
static QString FormatValueForKind(const QString& kind, double value)
{
    if (kind == "count") {
        return QString::number(static_cast<qint64>(value));
    }
    if (kind == "bytes") {
        return QString("%1 B").arg(QString::number(static_cast<qint64>(value)));
    }
    if (kind == "doge") {
        return QString("%1 DOGE").arg(QString::number(value, 'f', 8));
    }
    if (kind == "tps") {
        return QString("%1 tx/s").arg(QString::number(value, 'f', 3));
    }
    if (kind == "epoch_time") {
        const qint64 epoch = value < 0 ? 0 : static_cast<qint64>(value);
        return QDateTime::fromTime_t(static_cast<uint>(epoch)).toString(Qt::ISODate);
    }
    if (kind == "bits_hex") {
        return QString("0x%1").arg(static_cast<qulonglong>(value), 0, 16);
    }
    if (kind == "duration_sec") {
        return QString("%1 s").arg(QString::number(static_cast<qint64>(value)));
    }
    if (kind == "difficulty") {
        return QString::number(value, 'f', 2);
    }
    return QString::number(value, 'g', 12);
}

static int SampleIndexForPos(const QPoint& pos, int width, int count)
{
    static const int kMinSampleWidth = 4;
    static const int kSamplePad = 2;
    if (count <= 1 || width <= kMinSampleWidth) {
        return 0;
    }
    const double left = kSamplePad;
    const double right = width - kSamplePad;
    const double clampedX = std::max(left, std::min<double>(pos.x(), right));
    const double ratio = (clampedX - left) / std::max(1.0, right - left);
    int index = qRound(ratio * (count - 1));
    index = std::max(0, std::min(index, count - 1));
    return index;
}

static QDateTime DateTimeFromEpochCompat(qint64 secs)
{
    if (secs < 0) {
        secs = 0;
    }
    if (secs > std::numeric_limits<uint>::max()) {
        secs = std::numeric_limits<uint>::max();
    }
    return QDateTime::fromTime_t(static_cast<uint>(secs));
}

static bool DecodeContextToUniValue(const QString& txid, const QString& blockHash, UniValue& out, QString& errorMessage)
{
    try {
        JSONRPCRequest req;
        req.fHelp = false;
        req.params = UniValue(UniValue::VARR);

        if (!txid.isEmpty() && !blockHash.isEmpty()) {
            req.strMethod = "getblock";
            req.params.push_back(UniValue(blockHash.toStdString()));
            req.params.push_back(UniValue(2));
            const UniValue blockResult = tableRPC.execute(req);
            const UniValue& txList = find_value(blockResult, "tx");
            if (!txList.isNull() && txList.isArray()) {
                const std::vector<UniValue>& txValues = txList.getValues();
                const std::string wantedTxid = txid.toStdString();
                for (const UniValue& txObj : txValues) {
                    if (!txObj.isObject()) {
                        continue;
                    }
                    const UniValue& txidValue = find_value(txObj, "txid");
                    if (txidValue.isStr() && txidValue.get_str() == wantedTxid) {
                        out = txObj;
                        return true;
                    }
                }
            }
            errorMessage = QObject::tr("Transaction %1 not found in block %2.").arg(txid).arg(blockHash);
            return false;
        } else if (!txid.isEmpty()) {
            req.strMethod = "getrawtransaction";
            req.params.push_back(UniValue(txid.toStdString()));
            req.params.push_back(UniValue(true));
        } else if (!blockHash.isEmpty()) {
            req.strMethod = "getblock";
            req.params.push_back(UniValue(blockHash.toStdString()));
            req.params.push_back(UniValue(true));
        } else {
            errorMessage = QObject::tr("No transaction or block context available for this point.");
            return false;
        }
        out = tableRPC.execute(req);
        return true;
    } catch (const std::exception& e) {
        errorMessage = QObject::tr("Unable to decode context: %1").arg(QString::fromStdString(e.what()));
    } catch (...) {
        errorMessage = QObject::tr("Unable to decode context.");
    }
    return false;
}

static void AddUniValueNode(QTreeWidgetItem* parent, const QString& key, const UniValue& value)
{
    QTreeWidgetItem* item = new QTreeWidgetItem(parent);
    item->setText(0, key);

    if (value.isObject()) {
        item->setText(1, "{...}");
        const std::vector<std::string>& keys = value.getKeys();
        const std::vector<UniValue>& values = value.getValues();
        for (size_t i = 0; i < keys.size() && i < values.size(); ++i) {
            AddUniValueNode(item, QString::fromStdString(keys[i]), values[i]);
        }
        return;
    }

    if (value.isArray()) {
        item->setText(1, QString("[%1]").arg(value.size()));
        const std::vector<UniValue>& values = value.getValues();
        for (size_t i = 0; i < values.size(); ++i) {
            AddUniValueNode(item, QString("[%1]").arg(i), values[i]);
        }
        return;
    }

    item->setText(1, QString::fromStdString(value.write()));
}

static void PopulateDecodedTree(QTreeWidget* tree, const bool decodedOk, const UniValue& decoded, const QString& decodeError)
{
    tree->clear();
    if (decodedOk) {
        if (decoded.isObject()) {
            const std::vector<std::string>& keys = decoded.getKeys();
            const std::vector<UniValue>& values = decoded.getValues();
            for (size_t i = 0; i < keys.size() && i < values.size(); ++i) {
                AddUniValueNode(tree->invisibleRootItem(), QString::fromStdString(keys[i]), values[i]);
            }
        } else {
            AddUniValueNode(tree->invisibleRootItem(), QObject::tr("context"), decoded);
        }
    } else {
        QTreeWidgetItem* err = new QTreeWidgetItem(tree->invisibleRootItem());
        err->setText(0, QObject::tr("error"));
        err->setText(1, decodeError);
    }
    tree->expandToDepth(kDecodedTreeInitialDepth);
}

static bool IsLikelyTxid(QString value)
{
    value = value.trimmed();
    if (value.size() >= 2 && value.startsWith('"') && value.endsWith('"')) {
        value = value.mid(1, value.size() - 2);
    }
    if (value.size() != 64) {
        return false;
    }
    for (int i = 0; i < value.size(); ++i) {
        if (!std::isxdigit(static_cast<unsigned char>(value.at(i).toLatin1()))) {
            return false;
        }
    }
    return true;
}
} // namespace

SparklineWidget::SparklineWidget(QWidget* parent)
    : QWidget(parent)
{
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setMinimumHeight(34);
    setMouseTracking(true);
}

SparklineWidget::~SparklineWidget() = default;

void SparklineWidget::setData(const QVector<double>& data)
{
    // Keep one timestamp per sample so hover tooltips can show point-in-time data.
    const qint64 now = static_cast<qint64>(QDateTime::currentDateTime().toTime_t());
    if (data.isEmpty()) {
        // No data means no tooltip timeline.
        m_timestamps.clear();
        m_txids.clear();
        m_blockHashes.clear();
    } else if (m_timestamps.isEmpty() || data.size() < m_timestamps.size()) {
        // Initialize (or reset) timestamps when series length changes unexpectedly.
        m_timestamps = QVector<qint64>(data.size(), now);
        m_txids = QVector<QString>(data.size(), m_pointTxid);
        m_blockHashes = QVector<QString>(data.size(), m_pointBlockHash);
    } else if (data.size() > m_timestamps.size()) {
        // Append timestamps for newly added trailing samples.
        while (m_timestamps.size() < data.size()) {
            m_timestamps.push_back(now);
            m_txids.push_back(m_pointTxid);
            m_blockHashes.push_back(m_pointBlockHash);
        }
    } else if (!m_timestamps.isEmpty()) {
        // Sliding window update: drop oldest timestamp and append current sample time.
        m_timestamps.pop_front();
        m_timestamps.push_back(now);
        m_txids.pop_front();
        m_blockHashes.pop_front();
        m_txids.push_back(m_pointTxid);
        m_blockHashes.push_back(m_pointBlockHash);
    }
    m_data = data;
    update();
}

void SparklineWidget::setPointContext(const QString& txid, const QString& blockHash)
{
    m_pointTxid = txid;
    m_pointBlockHash = blockHash;
}

void SparklineWidget::clear()
{
    m_data.clear();
    m_timestamps.clear();
    m_txids.clear();
    m_blockHashes.clear();
    m_pointTxid.clear();
    m_pointBlockHash.clear();
    update();
}

void SparklineWidget::paintEvent(QPaintEvent* /*event*/)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    // Background (use widget palette, do not hardcode colors)
    p.fillRect(rect(), palette().brush(QPalette::Base));

    if (m_data.isEmpty() || width() <= 2 || height() <= 2) {
        return;
    }

    // Compute min/max for normalization
    double minv = m_data[0];
    double maxv = m_data[0];
    for (double v : m_data) {
        if (v < minv) minv = v;
        if (v > maxv) maxv = v;
    }
    const double range = (maxv - minv);

    const int w = width();
    const int h = height();

    // Padding
    const int pad = 2;
    const QRectF r(pad, pad, w - 2.0 * pad, h - 2.0 * pad);

    // Build polyline points
    const int n = m_data.size();
    QPolygonF poly;
    poly.reserve(n);

    for (int i = 0; i < n; ++i) {
        const double x = r.left() + (n == 1 ? 0.0 : (r.width() * i / double(n - 1)));

        double norm = 0.5;
        if (range > 0.0) {
            norm = (m_data[i] - minv) / range; // 0..1
        }
        // invert Y so higher values go up
        const double y = r.bottom() - (r.height() * norm);
        poly << QPointF(x, y);
    }

    // Draw line with accent color for better visibility on dashboard
    const QColor lineColor = palette().color(QPalette::Highlight);
    QPen pen(lineColor);
    pen.setWidthF(1.2);
    p.setPen(pen);
    p.drawPolyline(poly);

    // Optional: a subtle baseline midline when flat (range == 0)
    if (range == 0.0) {
        QPen mid(palette().color(QPalette::Mid));
        mid.setWidthF(1.0);
        p.setPen(mid);
        p.drawLine(QPointF(r.left(), r.center().y()), QPointF(r.right(), r.center().y()));
    }
}

void SparklineWidget::mouseMoveEvent(QMouseEvent* event)
{
    // Tooltips require aligned value/time series data.
    if (m_data.isEmpty() || m_timestamps.size() != m_data.size() || m_txids.size() != m_data.size() || m_blockHashes.size() != m_data.size()) {
        QWidget::mouseMoveEvent(event);
        return;
    }

    const int pad = 2;
    const QRectF r(pad, pad, width() - 2.0 * pad, height() - 2.0 * pad);
    const int n = m_data.size();
    if (n <= 0 || r.width() <= 0) {
        QWidget::mouseMoveEvent(event);
        return;
    }

    const int index = SampleIndexForPos(event->pos(), width(), n);

    // Show timestamp and sample value for the hovered point.
    const qint64 ts = m_timestamps[index];
    const QString tsStr = DateTimeFromEpochCompat(ts).toString(Qt::ISODate);
    const QString valueKind = property("tooltipValueKind").toString();
    const QString valueStr = FormatValueForKind(valueKind, m_data[index]);
    const QString txid = m_txids[index];
    const QString blockHash = m_blockHashes[index];
    const bool hasTx = !txid.isEmpty();
    const QString tooltip = hasTx
        ? tr("Time: %1\nValue: %2\nTxID: %3")
              .arg(tsStr)
              .arg(valueStr)
              .arg(txid)
        : tr("Time: %1\nValue: %2\nBlock: %3")
              .arg(tsStr)
              .arg(valueStr)
              .arg(!blockHash.isEmpty() ? blockHash : tr("n/a"));
    QToolTip::showText(event->globalPos(), tooltip, this);

    QWidget::mouseMoveEvent(event);
}

void SparklineWidget::mouseDoubleClickEvent(QMouseEvent* event)
{
    if (m_data.isEmpty() || m_txids.size() != m_data.size() || m_blockHashes.size() != m_data.size()) {
        QWidget::mouseDoubleClickEvent(event);
        return;
    }
    const int index = SampleIndexForPos(event->pos(), width(), m_data.size());
    if (index < 0 || index >= m_txids.size()) {
        QWidget::mouseDoubleClickEvent(event);
        return;
    }

    const QString txid = m_txids[index];
    const QString blockHash = index < m_blockHashes.size() ? m_blockHashes[index] : QString();
    const QString tsStr = DateTimeFromEpochCompat(m_timestamps[index]).toString(Qt::ISODate);
    const QString valueStr = FormatValueForKind(property("tooltipValueKind").toString(), m_data[index]);
    UniValue decodedTx;
    QString decodeError;
    const bool decodedOk = DecodeContextToUniValue(txid, blockHash, decodedTx, decodeError);

    QDialog details(this);
    details.setWindowTitle(tr("Metric Point Details"));
    QVBoxLayout* detailsLayout = new QVBoxLayout(&details);
    detailsLayout->addWidget(new QLabel(tr("Time: %1").arg(tsStr), &details));
    detailsLayout->addWidget(new QLabel(tr("Value: %1").arg(valueStr), &details));
    if (!txid.isEmpty()) {
        detailsLayout->addWidget(new QLabel(tr("TxID: %1").arg(txid), &details));
    } else if (!blockHash.isEmpty()) {
        detailsLayout->addWidget(new QLabel(tr("Block: %1").arg(blockHash), &details));
    }

    QTreeWidget* tree = new QTreeWidget(&details);
    tree->setColumnCount(2);
    tree->setHeaderLabels(QStringList() << tr("Field") << tr("Value"));
    tree->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    tree->header()->setSectionResizeMode(1, QHeaderView::Stretch);
    PopulateDecodedTree(tree, decodedOk, decodedTx, decodeError);

    QObject::connect(tree, &QTreeWidget::itemDoubleClicked, &details, [this, blockHash](QTreeWidgetItem* item, int /*column*/) {
        if (!item) {
            return;
        }
        QString txid = item->text(1).trimmed();
        if (!IsLikelyTxid(txid)) {
            return;
        }
        if (txid.size() >= 2 && txid.startsWith('"') && txid.endsWith('"')) {
            txid = txid.mid(1, txid.size() - 2);
        }

        UniValue nestedDecoded;
        QString nestedError;
        const bool nestedOk = DecodeContextToUniValue(txid, blockHash, nestedDecoded, nestedError);

        QDialog nested(this);
        nested.setWindowTitle(tr("Decoded Transaction"));
        QVBoxLayout* nestedLayout = new QVBoxLayout(&nested);
        nestedLayout->addWidget(new QLabel(tr("TxID: %1").arg(txid), &nested));
        QTreeWidget* nestedTree = new QTreeWidget(&nested);
        nestedTree->setColumnCount(2);
        nestedTree->setHeaderLabels(QStringList() << tr("Field") << tr("Value"));
        nestedTree->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
        nestedTree->header()->setSectionResizeMode(1, QHeaderView::Stretch);
        PopulateDecodedTree(nestedTree, nestedOk, nestedDecoded, nestedError);
        nestedLayout->addWidget(nestedTree);
        QDialogButtonBox* closeNested = new QDialogButtonBox(QDialogButtonBox::Close, &nested);
        QObject::connect(closeNested, &QDialogButtonBox::rejected, &nested, &QDialog::reject);
        nestedLayout->addWidget(closeNested);
        nested.resize(760, 500);
        nested.exec();
    });
    detailsLayout->addWidget(tree);
    QDialogButtonBox* closeBox = new QDialogButtonBox(QDialogButtonBox::Close, &details);
    QObject::connect(closeBox, &QDialogButtonBox::rejected, &details, &QDialog::reject);
    detailsLayout->addWidget(closeBox);
    details.resize(760, 500);
    details.exec();
    QWidget::mouseDoubleClickEvent(event);
}

void SparklineWidget::leaveEvent(QEvent* event)
{
    QToolTip::hideText();
    QWidget::leaveEvent(event);
}
