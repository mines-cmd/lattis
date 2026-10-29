#include "steamwebmerge.h"
#include "steamscan.h"

#include <QSet>

#include <algorithm>

namespace SteamWebMerge {

namespace {
struct Wanted
{
    QString name;
    bool owned = false;
    QStringList sharedBy;
    int type = -1;
    bool shared = false;
    int playMinutes = -1;
    qint64 lastPlayed = 0;
};

bool involved(const IndexEntry &e)
{
    return e.owned || !e.sharedBy.isEmpty();
}
}

Result merge(const Input &in)
{
    Result out;
    out.index = in.index;

    if (!in.wholeLibrary) {
        // Switched off: what the API brought goes, unless it is installed.
        QList<IndexEntry> kept;
        for (IndexEntry e : std::as_const(out.index)) {
            if (e.fromApi && !in.installed.contains(e.appId)) {
                out.changed = true;
                continue;
            }
            if (involved(e) || e.fromApi) {
                e.owned = false;
                e.sharedBy.clear();
                e.fromApi = false;
                e.storeType = -1;
                out.changed = true;
            }
            kept.append(e);
        }
        out.index = kept;
        return out;
    }
    if (!in.cache || in.cache->isEmpty())
        return out;

    const SteamWebApi::Cache &cache = *in.cache;
    out.active = true;
    out.names = cache.names;

    // The sources can only take games away when they brought some.
    const bool ownedKnown = !cache.owned.isEmpty();
    const bool sharedKnown = !cache.shared.isEmpty();

    QHash<qint64, Wanted> wanted;
    for (const SteamWebApi::OwnedGame &g : cache.owned) {
        Wanted &w = wanted[g.appId];
        w.name = g.name;
        w.owned = true;
        w.type = cache.types.value(g.appId, -1);
        w.playMinutes = g.playMinutes;
        w.lastPlayed = g.lastPlayed;
    }
    for (const SteamWebApi::SharedApp &a : cache.shared) {
        if (!SteamWebApi::isGameType(a.appType))
            continue;
        Wanted &w = wanted[a.appId];
        if (w.name.isEmpty())
            w.name = a.name;
        QStringList others;
        for (const QString &owner : a.owners) {
            if (owner == in.steamId)
                w.owned = true;
            else
                others.append(owner);
        }
        if (!w.owned)
            w.sharedBy = others;
        w.shared = true;
        if (w.playMinutes < 0 || (!w.owned && a.playMinutes > 0)) {
            w.playMinutes = a.playMinutes;
            w.lastPlayed = a.lastPlayed;
        }
    }

    QHash<qint64, qsizetype> position;
    for (qsizetype i = 0; i < out.index.size(); ++i)
        position.insert(out.index.at(i).appId, i);

    QList<IndexEntry> added;
    for (auto it = wanted.cbegin(); it != wanted.cend(); ++it) {
        const qint64 appId = it.key();
        const Wanted &w = it.value();
        if (SteamScan::isBlocked(appId, w.name))
            continue;
        if (const auto at = position.constFind(appId); at != position.cend()) {
            IndexEntry &e = out.index[*at];
            const IndexEntry before = e;
            e.owned = w.owned;
            e.sharedBy = w.owned ? QStringList() : w.sharedBy;
            if (w.type >= 0)
                e.storeType = w.type;
            if (e != before)
                out.changed = true;
        } else {
            // An owned app whose type the store hasn't answered yet waits for the next fetch: it could be a tool.
            const bool game = w.type == SteamWebApi::kTypeGame || w.type == SteamWebApi::kTypeNoStoreData
                || (w.type < 0 && w.shared);
            if (!game)
                continue;
            IndexEntry e;
            e.appId = appId;
            e.title = w.name.isEmpty() ? QStringLiteral("App %1").arg(appId) : w.name;
            e.owned = w.owned;
            e.sharedBy = w.owned ? QStringList() : w.sharedBy;
            e.fromApi = true;
            e.storeType = w.type >= 0 ? w.type : -1;
            added.append(e);
        }
    }
    std::sort(added.begin(), added.end(), [](const IndexEntry &a, const IndexEntry &b) { return a.appId < b.appId; });
    if (!added.isEmpty())
        out.changed = true;

    QList<IndexEntry> kept;
    for (IndexEntry e : std::as_const(out.index)) {
        const bool stillWanted = wanted.contains(e.appId) && !SteamScan::isBlocked(e.appId, wanted.value(e.appId).name);
        if (!stillWanted && involved(e)) {
            const bool wasOwned = e.owned;
            if ((wasOwned && !ownedKnown) || (!wasOwned && !sharedKnown)) {
                kept.append(e);
                continue;
            }
            out.changed = true;
            if (e.fromApi && !in.installed.contains(e.appId))
                continue;
            e.owned = false;
            e.sharedBy.clear();
        }
        kept.append(e);
    }
    out.index = kept + added;
    for (const IndexEntry &e : std::as_const(added))
        out.added.append(e.appId);

    for (auto it = wanted.cbegin(); it != wanted.cend(); ++it) {
        SteamWebApp app;
        app.sizeBytes = cache.sizes.value(it.key(), 0);
        app.playMinutes = it->playMinutes;
        app.lastPlayed = it->lastPlayed;
        out.web.insert(it.key(), app);
    }
    return out;
}
}
