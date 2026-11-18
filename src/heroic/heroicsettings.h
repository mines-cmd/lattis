#pragma once

#include "parsed.h"

#include <QByteArray>
#include <QString>

struct HeroicSettings
{
    bool enabled = true;
    QString path;
    // Off lists only the installed games; see docs/code/library.md.
    bool wholeLibrary = true;
    // Once a day, `legendary list-installed --check-updates` asks Epic for new builds.
    bool checkUpdates = true;

    bool operator==(const HeroicSettings &) const = default;
};

Parsed<HeroicSettings> parseHeroicSettings(const QByteArray &bytes, const QString &displayPath);
