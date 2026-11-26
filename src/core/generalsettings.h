#pragma once

#include "parsed.h"

#include <QByteArray>
#include <QString>
#include <QStringList>

inline const QString kAppIconMatchTheme = QStringLiteral("theme");

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
    bool toolbar = false;
    QStringList toolbarHiddenButtons;
    QStringList toolbarButtonOrder;   // of toolbarButtons(), as written: see orderedToolbarButtons()
    double clipVolume = 1.0;
    bool checkForUpdates = true;
    bool appMenuEntry = true;
    QString skippedUpdate;
    QString view = QStringLiteral("list");
    int gridCoverWidth = 220;
    bool gridPanel = true;
    bool gridShowTitles = false;
    QString appIcon = kAppIconMatchTheme;

    bool operator==(const GeneralSettings &) const = default;
};

const QStringList &railButtons();

QStringList orderedRailButtons(const QStringList &order);

const QStringList &toolbarButtons();

QStringList orderedToolbarButtons(const QStringList &order);

const QStringList &libraryViews();

constexpr int kMinGridCoverWidth = 80;
constexpr int kMaxGridCoverWidth = 600;

Parsed<GeneralSettings> parseGeneralSettings(const QByteArray &bytes, const QString &displayPath);
