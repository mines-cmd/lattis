#pragma once

#include "parsed.h"

#include <QByteArray>
#include <QString>
#include <QStringList>

// See docs/code/core.md.
struct GeneralSettings
{
    bool startFullscreen = false;
    bool downloadBarCollapsed = false;
    bool systemLocale = true;
    bool reduceMotion = false;
    bool railHidden = false;
    QStringList railHiddenButtons;
    QStringList railButtonOrder;   // of railButtons(), as written: see orderedRailButtons()
    double clipVolume = 1.0;
    bool checkForUpdates = true;
    bool appMenuEntry = true;
    QString skippedUpdate;

    bool operator==(const GeneralSettings &) const = default;
};

const QStringList &railButtons();

QStringList orderedRailButtons(const QStringList &order);

Parsed<GeneralSettings> parseGeneralSettings(const QByteArray &bytes, const QString &displayPath);
