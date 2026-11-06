#pragma once

#include "heroicscan.h"
#include "libraryfiles.h"

#include <QHash>
#include <QList>
#include <QSet>
#include <QStringList>

// See docs/code/library.md.
namespace HeroicMerge {
struct Input
{
    QList<HeroicEntry> index;
    const HeroicState *state = nullptr;
    // [heroic] whole_library
    bool wholeLibrary = true;
    // False while heroic.toml has an error: an id that can't be saved would change at the next start.
    bool canAdd = true;
    QSet<QString> taken;
};

struct Result
{
    QList<HeroicEntry> index;
    // Ids the merge brought into the index.
    QStringList added;
    QHash<QString, HeroicInstalled> installed;
    QSet<QString> gone;
};

Result merge(const Input &input);

struct Played
{
    QString id;
    // Heroic's whole total on a first count, what it grew by after that.
    int minutes = 0;
    bool first = false;
    qint64 lastPlayed = 0;

    bool operator==(const Played &) const = default;
};

// Moves each entry's heroic_minutes to Heroic's total, and says what that adds to the games.
QList<Played> countPlay(QList<HeroicEntry> *index, const HeroicState &state);
}
