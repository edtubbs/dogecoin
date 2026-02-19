// Copyright (c) 2026
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_SPARKLINEWIDGET_H
#define BITCOIN_QT_SPARKLINEWIDGET_H

#include <QVector>
#include <QWidget>

class SparklineWidget : public QWidget
{
public:
    explicit SparklineWidget(QWidget* parent = nullptr);
    ~SparklineWidget() override;

    void setData(const QVector<double>& data);
    void clear();

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    QVector<double> m_data;
};

#endif // BITCOIN_QT_SPARKLINEWIDGET_H
