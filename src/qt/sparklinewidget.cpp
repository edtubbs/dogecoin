// Copyright (c) 2026
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "sparklinewidget.h"

#include <QPainter>
#include <QPaintEvent>
#include <QtMath>

SparklineWidget::SparklineWidget(QWidget* parent)
    : QWidget(parent)
{
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setMinimumHeight(34);
}

SparklineWidget::~SparklineWidget() = default;

void SparklineWidget::setData(const QVector<double>& data)
{
    m_data = data;
    update();
}

void SparklineWidget::clear()
{
    m_data.clear();
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

    // Draw line
    QPen pen(palette().color(QPalette::Text));
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
