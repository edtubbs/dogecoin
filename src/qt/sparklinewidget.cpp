// Copyright (c) 2011-2016 The Bitcoin Core developers
// Copyright (c) 2021-2026 The Dogecoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "sparklinewidget.h"

#include <QDateTime>
#include <QEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPaintEvent>
#include <QToolTip>
#include <QtMath>

#include <algorithm>

namespace {
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
} // namespace

SparklineWidget::SparklineWidget(QWidget* parent)
    : QWidget(parent)
{
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
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
    } else if (m_timestamps.isEmpty() || data.size() < m_timestamps.size()) {
        // Initialize (or reset) timestamps when series length changes unexpectedly.
        m_timestamps = QVector<qint64>(data.size(), now);
    } else if (data.size() > m_timestamps.size()) {
        // Append timestamps for newly added trailing samples.
        while (m_timestamps.size() < data.size()) {
            m_timestamps.push_back(now);
        }
    } else if (!m_timestamps.isEmpty()) {
        // Sliding window update: drop oldest timestamp and append current sample time.
        m_timestamps.pop_front();
        m_timestamps.push_back(now);
    }
    m_data = data;
    update();
}

void SparklineWidget::clear()
{
    m_data.clear();
    m_timestamps.clear();
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
    if (m_data.isEmpty() || m_timestamps.size() != m_data.size()) {
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

    int index = 0;
    if (n > 1) {
        // Map cursor x-position to nearest sample index.
        const double x = std::max(r.left(), std::min<double>(event->pos().x(), r.right()));
        const double ratio = (x - r.left()) / r.width();
        index = qRound(ratio * (n - 1));
        index = std::max(0, std::min(index, n - 1));
    }

    // Show timestamp and sample value for the hovered point.
    const qint64 ts = m_timestamps[index];
    const QString tsStr = QDateTime::fromTime_t(static_cast<uint>(ts)).toString(Qt::ISODate);
    const QString valueKind = property("tooltipValueKind").toString();
    const QString valueStr = FormatValueForKind(valueKind, m_data[index]);
    const QString tooltip = tr("Time: %1\nValue: %2")
        .arg(tsStr)
        .arg(valueStr);
    QToolTip::showText(event->globalPos(), tooltip, this);

    QWidget::mouseMoveEvent(event);
}

void SparklineWidget::leaveEvent(QEvent* event)
{
    QToolTip::hideText();
    QWidget::leaveEvent(event);
}
