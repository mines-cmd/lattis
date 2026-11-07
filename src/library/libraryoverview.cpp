#include "libraryoverview.h"

#include <QHash>
#include <QSet>

#include <algorithm>

namespace LibraryOverview {
namespace {
// What a game or a group of linked copies counts as: one entry, under its main copy or else the first shown one.
struct Unit
{
    const Game *shown = nullptr;
    bool grouped = false;
    bool installed = false;
};
} // namespace

Overview overview(const QList<Game> &games)
{
    QList<Unit> units;
    QHash<QString, int> groupUnit;
    QSet<QString> platforms;
    for (const Game &g : games) {
        if (g.hidden)
            continue;
        platforms.insert(g.platform);
        if (g.groupId.isEmpty()) {
            units.append({&g, false, g.installed});
            continue;
        }
        const auto it = groupUnit.constFind(g.groupId);
        if (it == groupUnit.cend()) {
            groupUnit.insert(g.groupId, int(units.size()));
            units.append({&g, true, g.installed});
            continue;
        }
        Unit &unit = units[*it];
        unit.installed = unit.installed || g.installed;
        if (g.id == g.groupId)
            unit.shown = &g;
    }

    Overview out;
    out.totals.games = int(units.size());
    out.totals.platforms = int(platforms.size());
    for (const Unit &unit : units) {
        const Game &g = *unit.shown;
        if (unit.installed)
            ++out.totals.installed;
        out.totals.minutes += unit.grouped ? g.groupPlayMinutes : g.playMinutes;
        if (g.status == QLatin1String("not_started"))
            ++out.totals.backlog;
        if (g.status == QLatin1String("playing"))
            out.continuePlaying.append({g.id, g.title, g.coverUrl, unit.grouped ? g.groupLastPlayed : g.lastPlayed});
    }

    // Never-played ones last, then by title, so the order doesn't depend on the library's.
    std::sort(out.continuePlaying.begin(), out.continuePlaying.end(), [](const Counted &a, const Counted &b) {
        if (a.lastPlayed.isValid() != b.lastPlayed.isValid())
            return a.lastPlayed.isValid();
        if (a.lastPlayed != b.lastPlayed)
            return a.lastPlayed > b.lastPlayed;
        if (const int byTitle = a.title.compare(b.title, Qt::CaseInsensitive))
            return byTitle < 0;
        return a.id < b.id;
    });
    if (out.continuePlaying.size() > kContinueLimit)
        out.continuePlaying.resize(kContinueLimit);
    return out;
}

QVariantMap toVariantMap(const Overview &overview)
{
    QVariantList playing;
    for (const Counted &c : overview.continuePlaying)
        playing.append(QVariantMap{{QStringLiteral("id"), c.id},
                                   {QStringLiteral("title"), c.title},
                                   {QStringLiteral("coverUrl"), c.coverUrl},
                                   {QStringLiteral("lastPlayed"), c.lastPlayed}});
    const Totals &t = overview.totals;
    return {{QStringLiteral("games"), t.games},
            {QStringLiteral("installed"), t.installed},
            {QStringLiteral("platforms"), t.platforms},
            {QStringLiteral("minutes"), t.minutes},
            {QStringLiteral("backlog"), t.backlog},
            {QStringLiteral("continuePlaying"), playing}};
}
} // namespace LibraryOverview
