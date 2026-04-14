// Copyright (c) 2024 The Dogecoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_QTCOMPAT_H
#define BITCOIN_QT_QTCOMPAT_H

#include <QtGlobal>
#include <QFontMetrics>

// QFontMetrics::width() was deprecated in Qt 5.11 and removed in Qt 6.
// Use horizontalAdvance() which is available since Qt 5.11.
#if QT_VERSION >= QT_VERSION_CHECK(5, 11, 0)
inline int GUIUtil_fm_width(const QFontMetrics &fm, const QString &text)
{
    return fm.horizontalAdvance(text);
}
#else
inline int GUIUtil_fm_width(const QFontMetrics &fm, const QString &text)
{
    return fm.width(text);
}
#endif

#endif // BITCOIN_QT_QTCOMPAT_H
