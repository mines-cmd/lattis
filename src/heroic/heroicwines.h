#pragma once

#include "heroicgameconfig.h"

#include <QList>
#include <QString>
#include <QStringList>

struct HeroicWineChoice
{
    HeroicWine wine;
    // "heroic" (Heroic's own tools folder), "steam" (a compatibilitytools.d folder) or "system".
    QString source;

    bool operator==(const HeroicWineChoice &) const = default;
};

// See docs/code/heroic.md.
namespace HeroicWines {
// Heroic's Proton and Wine builds, then Steam's custom Proton builds, then the system Wine (`systemWine` is the
// path of its `wine`, empty for none), each group sorted by name.
QList<HeroicWineChoice> find(const QString &heroicRoot, const QStringList &steamToolDirs, const QString &systemWine);
}
