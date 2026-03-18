// Copyright (c) 2026 The Dogecoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "platformstyletests.h"

#include "platformstyle.h"

#include <QColor>
#include <QPalette>

void PlatformStyleTests::darkPaletteLooksLikeDarkMode()
{
    const QPalette palette = PlatformStyle::createDarkModePalette();
    const QColor window = palette.color(QPalette::Window);
    const QColor text = palette.color(QPalette::WindowText);
    const QColor highlight = palette.color(QPalette::Highlight);
    const QColor highlightedText = palette.color(QPalette::HighlightedText);

    QVERIFY(window.lightness() < text.lightness());
    QVERIFY(highlight.green() > highlight.red());
    QVERIFY(highlight.green() > highlight.blue());
    QVERIFY(highlightedText.lightness() < highlight.lightness());
}
