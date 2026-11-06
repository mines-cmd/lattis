#include "heroicmerge.h"

#include "gameid.h"

#include <utility>

namespace HeroicMerge {

Result merge(const Input &in)
{
    Result out;
    out.index = in.index;
    if (!in.state)
        return out;

    QHash<QString, QString> ids;
    for (const HeroicEntry &e : in.index) {
        if (e.store == kEpicPlatform)
            ids.insert(e.appName, e.id);
    }

    QSet<QString> taken = in.taken;
    QSet<QString> listed;
    const QList<HeroicGame> games = in.state->games();
    for (const HeroicGame &game : games) {
        if (!in.wholeLibrary && !game.installed)
            continue;
        QString id = ids.value(game.appName);
        if (id.isEmpty()) {
            if (!in.canAdd)
                continue;
            id = GameId::forTitle(kEpicPlatform, game.title, taken);
            taken.insert(id);
            out.index.append({id, game.title, kEpicPlatform, game.appName});
            out.added.append(id);
        }
        listed.insert(id);
        if (game.installed)
            out.installed.insert(id, {game.installPath, game.sizeBytes});
    }

    // Its line stays in heroic.toml: the id and the counted minutes must survive until it returns.
    const bool known = in.state->installedOk && (in.state->libraryKnown || !in.wholeLibrary);
    if (known) {
        for (const HeroicEntry &e : in.index) {
            if (e.store == kEpicPlatform && !listed.contains(e.id))
                out.gone.insert(e.id);
        }
    }
    return out;
}

QList<Played> countPlay(QList<HeroicEntry> *index, const HeroicState &state)
{
    QList<Played> out;
    if (!state.playTimeOk)
        return out;
    for (HeroicEntry &e : *index) {
        if (e.store != kEpicPlatform)
            continue;
        const HeroicPlayTime time = state.playTime.value(e.appName);
        const int before = std::exchange(e.heroicMinutes, time.minutes);
        if (before < 0 && time.minutes > 0)
            out.append({e.id, time.minutes, true, time.lastPlayed});
        else if (before >= 0 && time.minutes > before)
            out.append({e.id, time.minutes - before, false, time.lastPlayed});
    }
    return out;
}
}
