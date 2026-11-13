#pragma once

#include "parsed.h"

#include <QByteArray>
#include <QString>
#include <QStringList>

// See docs/code/core.md.
struct OverviewSettings
{
    QStringList left;
    QStringList right;
    QStringList hidden;
    QStringList above = {QStringLiteral("tiles"), QStringLiteral("continue_playing")};
    QStringList below;

    bool operator==(const OverviewSettings &) const = default;
};

// The page's layout once every card has a place.
struct OverviewLayout
{
    bool automatic = true;   // no left/right: the page deals the cards into the columns itself
    QStringList left;
    QStringList right;
    QStringList above;
    QStringList below;
    QStringList hidden;

    bool operator==(const OverviewLayout &) const = default;
};

const QStringList &overviewColumnCards();
const QStringList &overviewRowCards();

Parsed<OverviewSettings> parseOverviewSettings(const QByteArray &bytes, const QString &displayPath);
OverviewLayout resolveOverview(const OverviewSettings &settings);
