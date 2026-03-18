// Copyright (c) 2026 The Dogecoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "platformstyletests.h"

#include "platformstyle.h"

#include <QColor>
#include <QPalette>
#include <QSet>

void PlatformStyleTests::darkPaletteLooksLikeDarkMode()
{
    const QPalette palette = PlatformStyle::createDarkModePalette();
    const QColor window = palette.color(QPalette::Window);
    const QColor text = palette.color(QPalette::WindowText);
    const QColor highlight = palette.color(QPalette::Highlight);
    const QColor highlightedText = palette.color(QPalette::HighlightedText);
    const QColor darkBorder = palette.color(QPalette::Dark);
    const QColor midBorder = palette.color(QPalette::Mid);

    QVERIFY(window.lightness() < text.lightness());
    QVERIFY(highlight.green() > highlight.red());
    QVERIFY(highlight.green() > highlight.blue());
    QVERIFY(highlightedText.lightness() < highlight.lightness());
    QVERIFY(darkBorder.lightness() < window.lightness());
    QVERIFY(midBorder.lightness() < text.lightness());
}

void PlatformStyleTests::darkTintOptionsStayGreenAndDistinct()
{
    const int originalTint = PlatformStyle::darkModeTint();
    QSet<QRgb> seenHighlights;

    for (int tint = 0; tint < PlatformStyle::darkModeTintCount(); ++tint) {
        PlatformStyle::setDarkModeTint(tint);
        const QPalette palette = PlatformStyle::createDarkModePalette();
        const QColor highlight = palette.color(QPalette::Highlight);

        QVERIFY(highlight.green() >= highlight.red());
        QVERIFY(highlight.green() >= highlight.blue());
        seenHighlights.insert(highlight.rgb());
    }

    QVERIFY(seenHighlights.size() >= 3);
    PlatformStyle::setDarkModeTint(originalTint);
}
