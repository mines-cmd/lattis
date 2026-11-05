#include "steamwebapi.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonObject>
#include <QSaveFile>

namespace SteamWebApi {

namespace {
constexpr int kCacheVersion = 1;
constexpr int kFriendsCacheVersion = 1;

QString asText(const QJsonValue &value)
{
    if (value.isString())
        return value.toString();
    if (value.isDouble())
        return QString::number(qint64(value.toDouble()));
    return {};
}

qint64 asInteger(const QJsonValue &value)
{
    if (value.isString())
        return value.toString().toLongLong();
    return value.toInteger();
}

void authenticate(QUrlQuery *query, const Auth &auth)
{
    if (!auth.accessToken.isEmpty())
        query->addQueryItem(QStringLiteral("access_token"), auth.accessToken);
    else if (!auth.key.isEmpty())
        query->addQueryItem(QStringLiteral("key"), auth.key);
}

QJsonObject responseOf(const QJsonDocument &json, bool *shaped = nullptr)
{
    const QJsonValue response = json.object().value(QLatin1String("response"));
    if (shaped)
        *shaped = response.isObject();
    return response.toObject();
}

QJsonArray numbersToJson(const QList<qint64> &ids)
{
    QJsonArray out;
    for (const qint64 id : ids)
        out.append(QJsonObject{{QStringLiteral("appid"), id}});
    return out;
}
}

Request ownedGamesRequest(const QString &steamId, const Auth &auth)
{
    QUrlQuery query;
    authenticate(&query, auth);
    query.addQueryItem(QStringLiteral("steamid"), steamId);
    query.addQueryItem(QStringLiteral("include_appinfo"), QStringLiteral("1"));
    query.addQueryItem(QStringLiteral("include_played_free_games"), QStringLiteral("1"));
    // Otherwise some owned games are missing.
    query.addQueryItem(QStringLiteral("skip_unvetted_apps"), QStringLiteral("0"));
    return {QStringLiteral("/IPlayerService/GetOwnedGames/v1/"), query};
}

Request familyGroupForUserRequest(const QString &steamId, const QString &accessToken)
{
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("access_token"), accessToken);
    query.addQueryItem(QStringLiteral("steamid"), steamId);
    return {QStringLiteral("/IFamilyGroupsService/GetFamilyGroupForUser/v1/"), query};
}

Request familyGroupRequest(const QString &groupId, const QString &accessToken)
{
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("access_token"), accessToken);
    query.addQueryItem(QStringLiteral("family_groupid"), groupId);
    return {QStringLiteral("/IFamilyGroupsService/GetFamilyGroup/v1/"), query};
}

Request userSummariesRequest(const QStringList &steamIds, const QString &accessToken)
{
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("access_token"), accessToken);
    query.addQueryItem(QStringLiteral("steamids"), steamIds.join(u','));
    return {QStringLiteral("/ISteamUserOAuth/GetUserSummaries/v1/"), query};
}

Request playerSummariesRequest(const QStringList &steamIds, const QString &key)
{
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("key"), key);
    query.addQueryItem(QStringLiteral("steamids"), steamIds.join(u','));
    return {QStringLiteral("/ISteamUser/GetPlayerSummaries/v2/"), query};
}

Request sharedAppsRequest(const QString &steamId, const QString &groupId, const QString &accessToken)
{
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("access_token"), accessToken);
    query.addQueryItem(QStringLiteral("steamid"), steamId);
    query.addQueryItem(QStringLiteral("family_groupid"), groupId);
    query.addQueryItem(QStringLiteral("language"), QStringLiteral("english"));
    // The user's own apps too: the merge tells them apart by owner, and an app both own is the user's.
    query.addQueryItem(QStringLiteral("include_own"), QStringLiteral("1"));
    return {QStringLiteral("/IFamilyGroupsService/GetSharedLibraryApps/v1/"), query};
}

Request clientAppsRequest(const QString &accessToken)
{
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("access_token"), accessToken);
    query.addQueryItem(QStringLiteral("fields"), QStringLiteral("games"));
    return {QStringLiteral("/IClientCommService/GetClientAppList/v1/"), query};
}

Request achievementPercentagesRequest(qint64 appId)
{
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("gameid"), QString::number(appId));
    return {QStringLiteral("/ISteamUserStats/GetGlobalAchievementPercentagesForApp/v2/"), query};
}

QHash<QString, double> parseAchievementPercentages(const QJsonDocument &json)
{
    QHash<QString, double> out;
    const QJsonArray list = json.object().value(QStringLiteral("achievementpercentages")).toObject()
                                .value(QStringLiteral("achievements")).toArray();
    for (const QJsonValue &v : list) {
        const QJsonObject o = v.toObject();
        const QString name = o.value(QStringLiteral("name")).toString();
        if (!name.isEmpty())
            out.insert(name, o.value(QStringLiteral("percent")).toVariant().toDouble());
    }
    return out;
}

Request itemsRequest(const QList<qint64> &appIds)
{
    const QJsonObject input{
        {QStringLiteral("ids"), numbersToJson(appIds)},
        {QStringLiteral("context"), QJsonObject{{QStringLiteral("language"), QStringLiteral("english")},
                                                {QStringLiteral("country_code"), QStringLiteral("US")}}},
        {QStringLiteral("data_request"), QJsonObject{}},
    };
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("input_json"),
                       QString::fromUtf8(QJsonDocument(input).toJson(QJsonDocument::Compact)));
    return {QStringLiteral("/IStoreBrowseService/GetItems/v1"), query};
}

Parsed<OwnedGame> parseOwnedGames(const QJsonDocument &json)
{
    Parsed<OwnedGame> out;
    const QJsonObject response = responseOf(json, &out.ok);
    if (!out.ok)
        return out;
    for (const QJsonValue &v : response.value(QLatin1String("games")).toArray()) {
        const QJsonObject game = v.toObject();
        OwnedGame g;
        g.appId = asInteger(game.value(QLatin1String("appid")));
        if (g.appId <= 0)
            continue;
        g.name = game.value(QLatin1String("name")).toString();
        g.playMinutes = int(game.value(QLatin1String("playtime_forever")).toInt());
        g.lastPlayed = asInteger(game.value(QLatin1String("rtime_last_played")));
        out.items.append(g);
    }
    return out;
}

FamilyGroup parseFamilyGroup(const QJsonDocument &json)
{
    FamilyGroup out;
    const QJsonObject response = responseOf(json, &out.ok);
    if (!out.ok)
        return out;
    out.groupId = asText(response.value(QLatin1String("family_groupid")));
    out.member = !response.value(QLatin1String("is_not_member_of_any_group")).toBool() && !out.groupId.isEmpty()
        && out.groupId != u"0";
    return out;
}

QStringList parseFamilyMembers(const QJsonDocument &json)
{
    QStringList out;
    for (const QJsonValue &v : responseOf(json).value(QLatin1String("members")).toArray()) {
        if (const QString id = asText(v.toObject().value(QLatin1String("steamid"))); !id.isEmpty())
            out.append(id);
    }
    return out;
}

QHash<QString, QString> parseUserNames(const QJsonDocument &json)
{
    QHash<QString, QString> out;
    // ISteamUserOAuth puts players at the top level; newer services wrap them in "response".
    const QJsonObject root = json.object().contains(QLatin1String("players")) ? json.object() : responseOf(json);
    for (const QJsonValue &v : root.value(QLatin1String("players")).toArray()) {
        const QJsonObject player = v.toObject();
        const QString id = asText(player.value(QLatin1String("steamid")));
        const QString name = player.value(QLatin1String("personaname")).toString();
        if (!id.isEmpty() && !name.isEmpty())
            out.insert(id, name);
    }
    return out;
}

Parsed<SharedApp> parseSharedApps(const QJsonDocument &json)
{
    Parsed<SharedApp> out;
    const QJsonObject response = responseOf(json, &out.ok);
    if (!out.ok)
        return out;
    for (const QJsonValue &v : response.value(QLatin1String("apps")).toArray()) {
        const QJsonObject app = v.toObject();
        SharedApp a;
        a.appId = asInteger(app.value(QLatin1String("appid")));
        if (a.appId <= 0)
            continue;
        a.name = app.value(QLatin1String("name")).toString();
        for (const QJsonValue &owner : app.value(QLatin1String("owner_steamids")).toArray()) {
            if (const QString id = asText(owner); !id.isEmpty())
                a.owners.append(id);
        }
        a.appType = app.value(QLatin1String("app_type")).toInt(1);
        a.playMinutes = int(app.value(QLatin1String("rt_playtime")).toInt());
        a.lastPlayed = asInteger(app.value(QLatin1String("rt_last_played")));
        out.items.append(a);
    }
    return out;
}

QHash<qint64, qint64> parseClientApps(const QJsonDocument &json)
{
    QHash<qint64, qint64> out;
    for (const QJsonValue &v : responseOf(json).value(QLatin1String("apps")).toArray()) {
        const QJsonObject app = v.toObject();
        const qint64 id = asInteger(app.value(QLatin1String("appid")));
        const qint64 bytes = asInteger(app.value(QLatin1String("bytes_required")));
        if (id > 0 && bytes > 0)
            out.insert(id, bytes);
    }
    // The answer is not wrapped when it comes straight from the service.
    if (out.isEmpty()) {
        for (const QJsonValue &v : json.object().value(QLatin1String("apps")).toArray()) {
            const QJsonObject app = v.toObject();
            const qint64 id = asInteger(app.value(QLatin1String("appid")));
            const qint64 bytes = asInteger(app.value(QLatin1String("bytes_required")));
            if (id > 0 && bytes > 0)
                out.insert(id, bytes);
        }
    }
    return out;
}

QHash<qint64, int> parseItemTypes(const QJsonDocument &json, const QList<qint64> &requested)
{
    QHash<qint64, int> out;
    for (const QJsonValue &v : responseOf(json).value(QLatin1String("store_items")).toArray()) {
        const QJsonObject item = v.toObject();
        qint64 id = asInteger(item.value(QLatin1String("appid")));
        if (id <= 0)
            id = asInteger(item.value(QLatin1String("id")));
        if (id <= 0)
            continue;
        out.insert(id, item.value(QLatin1String("success")).toInt() == 1 && item.contains(QLatin1String("type"))
                           ? item.value(QLatin1String("type")).toInt()
                           : kTypeNoStoreData);
    }
    for (const qint64 id : requested) {
        if (!out.contains(id))
            out.insert(id, kTypeNoStoreData);
    }
    return out;
}

bool isGameType(int appType)
{
    return appType <= 2;
}

QString cachePath(const QString &cacheDir, const QString &libraryKey)
{
    return QDir(cacheDir).filePath(QStringLiteral("steam-library-%1.json").arg(libraryKey));
}

QByteArray formatCache(const Cache &cache)
{
    QJsonArray owned;
    for (const OwnedGame &g : cache.owned)
        owned.append(QJsonObject{{"a", g.appId}, {"n", g.name}, {"p", g.playMinutes}, {"l", g.lastPlayed}});
    QJsonArray shared;
    for (const SharedApp &a : cache.shared) {
        shared.append(QJsonObject{{"a", a.appId},
                                  {"n", a.name},
                                  {"o", QJsonArray::fromStringList(a.owners)},
                                  {"t", a.appType},
                                  {"p", a.playMinutes},
                                  {"l", a.lastPlayed}});
    }
    QJsonObject names;
    for (auto it = cache.names.cbegin(); it != cache.names.cend(); ++it)
        names.insert(it.key(), it.value());
    QJsonObject sizes;
    for (auto it = cache.sizes.cbegin(); it != cache.sizes.cend(); ++it)
        sizes.insert(QString::number(it.key()), it.value());
    QJsonObject types;
    for (auto it = cache.types.cbegin(); it != cache.types.cend(); ++it)
        types.insert(QString::number(it.key()), it.value());
    const QJsonObject root{{"version", kCacheVersion},
                           {"fetched", cache.fetched.isValid() ? cache.fetched.toUTC().toString(Qt::ISODate) : QString()},
                           {"steamid", cache.steamId},
                           {"owned", owned},
                           {"shared", shared},
                           {"names", names},
                           {"sizes", sizes},
                           {"types", types}};
    return QJsonDocument(root).toJson(QJsonDocument::Compact);
}

bool parseCache(const QByteArray &bytes, Cache *out)
{
    QJsonParseError error;
    const QJsonObject root = QJsonDocument::fromJson(bytes, &error).object();
    if (error.error != QJsonParseError::NoError || root.value(QLatin1String("version")).toInt() != kCacheVersion)
        return false;
    Cache cache;
    cache.fetched = QDateTime::fromString(root.value(QLatin1String("fetched")).toString(), Qt::ISODate);
    cache.steamId = root.value(QLatin1String("steamid")).toString();
    for (const QJsonValue &v : root.value(QLatin1String("owned")).toArray()) {
        const QJsonObject o = v.toObject();
        cache.owned.append({o.value("a").toInteger(), o.value("n").toString(), o.value("p").toInt(),
                            o.value("l").toInteger()});
    }
    for (const QJsonValue &v : root.value(QLatin1String("shared")).toArray()) {
        const QJsonObject o = v.toObject();
        SharedApp a{o.value("a").toInteger(), o.value("n").toString(), {}, o.value("t").toInt(1),
                    o.value("p").toInt(), o.value("l").toInteger()};
        for (const QJsonValue &owner : o.value("o").toArray())
            a.owners.append(owner.toString());
        cache.shared.append(a);
    }
    const QJsonObject names = root.value(QLatin1String("names")).toObject();
    for (auto it = names.begin(); it != names.end(); ++it)
        cache.names.insert(it.key(), it.value().toString());
    const QJsonObject sizes = root.value(QLatin1String("sizes")).toObject();
    for (auto it = sizes.begin(); it != sizes.end(); ++it)
        cache.sizes.insert(it.key().toLongLong(), it.value().toInteger());
    const QJsonObject types = root.value(QLatin1String("types")).toObject();
    for (auto it = types.begin(); it != types.end(); ++it)
        cache.types.insert(it.key().toLongLong(), it.value().toInt());
    *out = std::move(cache);
    return true;
}

bool loadCache(const QString &path, Cache *out)
{
    QFile file(path);
    return file.open(QIODevice::ReadOnly) && parseCache(file.readAll(), out);
}

bool saveCache(const QString &path, const Cache &cache)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QSaveFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write(formatCache(cache)) >= 0 && file.commit();
}

FriendGroup groupOf(const FriendGame &game)
{
    if (game.recentMinutes > 0)
        return FriendGroup::Recent;
    return game.totalMinutes > 0 ? FriendGroup::Previous : FriendGroup::None;
}

Request friendListRequest(const QString &steamId, const QString &key)
{
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("key"), key);
    query.addQueryItem(QStringLiteral("steamid"), steamId);
    query.addQueryItem(QStringLiteral("relationship"), QStringLiteral("friend"));
    return {QStringLiteral("/ISteamUser/GetFriendList/v1/"), query};
}

Request friendGamesRequest(const QString &steamId, const QString &key)
{
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("key"), key);
    query.addQueryItem(QStringLiteral("steamid"), steamId);
    query.addQueryItem(QStringLiteral("include_played_free_games"), QStringLiteral("1"));
    // Otherwise some owned games are missing.
    query.addQueryItem(QStringLiteral("skip_unvetted_apps"), QStringLiteral("0"));
    return {QStringLiteral("/IPlayerService/GetOwnedGames/v1/"), query};
}

Parsed<QString> parseFriendList(const QJsonDocument &json)
{
    Parsed<QString> out;
    const QJsonValue list = json.object().value(QLatin1String("friendslist"));
    out.ok = list.isObject();
    for (const QJsonValue &v : list.toObject().value(QLatin1String("friends")).toArray()) {
        if (const QString id = asText(v.toObject().value(QLatin1String("steamid"))); !id.isEmpty())
            out.items.append(id);
    }
    return out;
}

Parsed<FriendSummary> parseFriendSummaries(const QJsonDocument &json)
{
    Parsed<FriendSummary> out;
    const QJsonObject response = responseOf(json, &out.ok);
    for (const QJsonValue &v : response.value(QLatin1String("players")).toArray()) {
        const QJsonObject player = v.toObject();
        FriendSummary s;
        s.steamId = asText(player.value(QLatin1String("steamid")));
        if (s.steamId.isEmpty())
            continue;
        s.name = player.value(QLatin1String("personaname")).toString();
        s.avatarUrl = player.value(QLatin1String("avatarmedium")).toString();
        s.gameId = asInteger(player.value(QLatin1String("gameid")));
        s.gameName = player.value(QLatin1String("gameextrainfo")).toString();
        out.items.append(s);
    }
    return out;
}

Parsed<FriendGame> parseFriendGames(const QJsonDocument &json)
{
    Parsed<FriendGame> out;
    const QJsonObject response = responseOf(json, &out.ok);
    for (const QJsonValue &v : response.value(QLatin1String("games")).toArray()) {
        const QJsonObject game = v.toObject();
        FriendGame g;
        g.appId = asInteger(game.value(QLatin1String("appid")));
        g.recentMinutes = game.value(QLatin1String("playtime_2weeks")).toInt();
        g.totalMinutes = game.value(QLatin1String("playtime_forever")).toInt();
        if (g.appId > 0 && groupOf(g) != FriendGroup::None)
            out.items.append(g);
    }
    return out;
}

QString friendsCachePath(const QString &cacheDir, const QString &libraryKey)
{
    return QDir(cacheDir).filePath(QStringLiteral("steam-friends-%1.json").arg(libraryKey));
}

QByteArray formatFriendsCache(const FriendsCache &cache)
{
    QJsonArray friends;
    for (const Friend &f : cache.friends) {
        QJsonArray games;
        for (const FriendGame &g : f.games)
            games.append(QJsonArray{g.appId, g.recentMinutes, g.totalMinutes});
        friends.append(QJsonObject{{"id", f.steamId}, {"n", f.name}, {"a", f.avatarUrl}, {"g", games}});
    }
    const QJsonObject root{{"version", kFriendsCacheVersion},
                           {"fetched", cache.fetched.isValid() ? cache.fetched.toUTC().toString(Qt::ISODate) : QString()},
                           {"steamid", cache.steamId},
                           {"friends", friends}};
    return QJsonDocument(root).toJson(QJsonDocument::Compact);
}

bool parseFriendsCache(const QByteArray &bytes, FriendsCache *out)
{
    QJsonParseError error;
    const QJsonObject root = QJsonDocument::fromJson(bytes, &error).object();
    if (error.error != QJsonParseError::NoError || root.value(QLatin1String("version")).toInt() != kFriendsCacheVersion)
        return false;
    FriendsCache cache;
    cache.fetched = QDateTime::fromString(root.value(QLatin1String("fetched")).toString(), Qt::ISODate);
    cache.steamId = root.value(QLatin1String("steamid")).toString();
    for (const QJsonValue &v : root.value(QLatin1String("friends")).toArray()) {
        const QJsonObject o = v.toObject();
        Friend f{o.value("id").toString(), o.value("n").toString(), o.value("a").toString(), {}};
        if (f.steamId.isEmpty())
            continue;
        for (const QJsonValue &game : o.value("g").toArray()) {
            const QJsonArray a = game.toArray();
            f.games.append({a.at(0).toInteger(), a.at(1).toInt(), a.at(2).toInt()});
        }
        cache.friends.append(f);
    }
    *out = std::move(cache);
    return true;
}

bool loadFriendsCache(const QString &path, FriendsCache *out)
{
    QFile file(path);
    return file.open(QIODevice::ReadOnly) && parseFriendsCache(file.readAll(), out);
}

bool saveFriendsCache(const QString &path, const FriendsCache &cache)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QSaveFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write(formatFriendsCache(cache)) >= 0 && file.commit();
}
}
