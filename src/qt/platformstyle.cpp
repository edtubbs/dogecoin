// Copyright (c) 2015-2016 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "platformstyle.h"

#include "guiconstants.h"

#include <QApplication>
#include <QColor>
#include <QCoreApplication>
#include <QIcon>
#include <QImage>
#include <QPalette>
#include <QPixmap>
#include <QSettings>
#include <QStyle>
#include <QStyleFactory>

static const struct {
    const char *platformId;
    /** Show images on push buttons */
    const bool imagesOnButtons;
    /** Colorize single-color icons */
    const bool colorizeIcons;
    /** Extra padding/spacing in transactionview */
    const bool useExtraSpacing;
} platform_styles[] = {
    {"macosx", false, false, true},
    {"windows", true, false, false},
    /* Other: linux, unix, ... */
    {"other", true, true, false}
};
static const unsigned platform_styles_count = sizeof(platform_styles)/sizeof(*platform_styles);
static const char* DARK_MODE_SETTING = "fUseDarkMode";

namespace {
QColor DarkModeBorderColor()
{
    return QColor(46, 58, 51);
}
}

namespace {
/* Local functions for colorizing single-color images */

void MakeSingleColorImage(QImage& img, const QColor& colorbase)
{
    img = img.convertToFormat(QImage::Format_ARGB32);
    for (int x = img.width(); x--; )
    {
        for (int y = img.height(); y--; )
        {
            const QRgb rgb = img.pixel(x, y);
            img.setPixel(x, y, qRgba(colorbase.red(), colorbase.green(), colorbase.blue(), qAlpha(rgb)));
        }
    }
}

QIcon ColorizeIcon(const QIcon& ico, const QColor& colorbase)
{
    QIcon new_ico;
    QSize sz;
    Q_FOREACH(sz, ico.availableSizes())
    {
        QImage img(ico.pixmap(sz).toImage());
        MakeSingleColorImage(img, colorbase);
        new_ico.addPixmap(QPixmap::fromImage(img));
    }
    return new_ico;
}

QImage ColorizeImage(const QString& filename, const QColor& colorbase)
{
    QImage img(filename);
    MakeSingleColorImage(img, colorbase);
    return img;
}

QIcon ColorizeIcon(const QString& filename, const QColor& colorbase)
{
    return QIcon(QPixmap::fromImage(ColorizeImage(filename, colorbase)));
}

}


PlatformStyle::PlatformStyle(const QString &_name, bool _imagesOnButtons, bool _colorizeIcons, bool _useExtraSpacing):
    name(_name),
    imagesOnButtons(_imagesOnButtons),
    colorizeIcons(_colorizeIcons),
    useExtraSpacing(_useExtraSpacing),
    singleColor(0,0,0),
    textColor(0,0,0)
{
    // Determine icon highlighting color
    if (colorizeIcons) {
        const QColor colorHighlightBg(QApplication::palette().color(QPalette::Highlight));
        const QColor colorHighlightFg(QApplication::palette().color(QPalette::HighlightedText));
        const QColor colorText(QApplication::palette().color(QPalette::WindowText));
        const int colorTextLightness = colorText.lightness();
        QColor colorbase;
        if (abs(colorHighlightBg.lightness() - colorTextLightness) < abs(colorHighlightFg.lightness() - colorTextLightness))
            colorbase = colorHighlightBg;
        else
            colorbase = colorHighlightFg;
        singleColor = colorbase;
    }
    // Determine text color
    textColor = QColor(QApplication::palette().color(QPalette::WindowText));
}

QImage PlatformStyle::SingleColorImage(const QString& filename) const
{
    if (!colorizeIcons)
        return QImage(filename);
    return ColorizeImage(filename, SingleColor());
}

QIcon PlatformStyle::SingleColorIcon(const QString& filename) const
{
    if (!colorizeIcons)
        return QIcon(filename);
    return ColorizeIcon(filename, SingleColor());
}

QIcon PlatformStyle::SingleColorIcon(const QIcon& icon) const
{
    if (!colorizeIcons)
        return icon;
    return ColorizeIcon(icon, SingleColor());
}

QIcon PlatformStyle::TextColorIcon(const QString& filename) const
{
    return ColorizeIcon(filename, TextColor());
}

QIcon PlatformStyle::TextColorIcon(const QIcon& icon) const
{
    return ColorizeIcon(icon, TextColor());
}

const PlatformStyle *PlatformStyle::instantiate(const QString &platformId)
{
    for (unsigned x=0; x<platform_styles_count; ++x)
    {
        if (platformId == platform_styles[x].platformId)
        {
            return new PlatformStyle(
                    platform_styles[x].platformId,
                    platform_styles[x].imagesOnButtons,
                    platform_styles[x].colorizeIcons,
                    platform_styles[x].useExtraSpacing);
        }
    }
    return 0;
}

QPalette PlatformStyle::createDarkModePalette()
{
    QPalette darkPalette;

    const QColor windowColor(26, 32, 28);
    const QColor baseColor(18, 24, 20);
    const QColor alternateBaseColor(32, 40, 35);
    const QColor textColor(214, 232, 220);
    const QColor mutedTextColor(137, 161, 146);
    const QColor buttonColor(35, 44, 38);
    const QColor highlightColor(74, 163, 111);
    const QColor highlightedTextColor(10, 24, 16);
    const QColor borderColor = DarkModeBorderColor();

    darkPalette.setColor(QPalette::Window, windowColor);
    darkPalette.setColor(QPalette::WindowText, textColor);
    darkPalette.setColor(QPalette::Base, baseColor);
    darkPalette.setColor(QPalette::AlternateBase, alternateBaseColor);
    darkPalette.setColor(QPalette::ToolTipBase, alternateBaseColor);
    darkPalette.setColor(QPalette::ToolTipText, textColor);
    darkPalette.setColor(QPalette::Text, textColor);
    darkPalette.setColor(QPalette::Button, buttonColor);
    darkPalette.setColor(QPalette::ButtonText, textColor);
    darkPalette.setColor(QPalette::Mid, borderColor);
    darkPalette.setColor(QPalette::Dark, QColor(16, 20, 18));
    darkPalette.setColor(QPalette::Shadow, QColor(8, 10, 9));
    darkPalette.setColor(QPalette::Light, QColor(45, 56, 49));
    darkPalette.setColor(QPalette::Link, highlightColor);
    darkPalette.setColor(QPalette::Highlight, highlightColor);
    darkPalette.setColor(QPalette::HighlightedText, highlightedTextColor);
    darkPalette.setColor(QPalette::BrightText, QColor(255, 128, 128));
    darkPalette.setColor(QPalette::Disabled, QPalette::Text, mutedTextColor);
    darkPalette.setColor(QPalette::Disabled, QPalette::ButtonText, mutedTextColor);
    darkPalette.setColor(QPalette::Disabled, QPalette::WindowText, mutedTextColor);

    return darkPalette;
}

bool PlatformStyle::isDarkModeEnabled()
{
    QSettings settings(QAPP_ORG_NAME, QAPP_APP_NAME_DEFAULT);
    if (!settings.contains(DARK_MODE_SETTING)) {
        settings.setValue(DARK_MODE_SETTING, true);
    }
    return settings.value(DARK_MODE_SETTING, true).toBool();
}

void PlatformStyle::applyTheme(bool darkModeEnabled)
{
    if (!qobject_cast<QApplication*>(QCoreApplication::instance())) {
        return;
    }

    QStyle* fusionStyle = QStyleFactory::create("Fusion");
    if (fusionStyle) {
        QApplication::setStyle(fusionStyle);
    }

    if (darkModeEnabled) {
        QApplication::setPalette(createDarkModePalette());
        const QString borderColor = DarkModeBorderColor().name();
        qApp->setStyleSheet(
            QString("QDialog, QMessageBox { border: 1px solid %1; }"
                    "QMenu { border: 1px solid %1; }").arg(borderColor));
    } else {
        const QStyle* currentStyle = QApplication::style();
        QApplication::setPalette(currentStyle ? currentStyle->standardPalette() : QPalette());
        qApp->setStyleSheet("");
    }
}

void PlatformStyle::setDarkModeEnabled(bool enabled)
{
    QSettings settings(QAPP_ORG_NAME, QAPP_APP_NAME_DEFAULT);
    settings.setValue(DARK_MODE_SETTING, enabled);
    applyTheme(enabled);
}
