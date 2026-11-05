#pragma once

#include <QByteArray>
#include <QDateTime>
#include <QHash>
#include <QJsonDocument>
#include <QList>
#include <QString>
#include <QStringList>
#include <QUrlQuery>

// See docs/code/steam.md.
namespace SteamWebApi {

// The store type of an app the store has nothing on (delisted); kept as a game.
constexpr int kTypeNoStoreData = -2;
// The store's type for a game.
constexpr int kTypeGame = 0;

struct Request
{
    QString path;
    QUrlQuery query;
};

// How a request proves who asks: a signed-in session's access token, or the user's own Web API key.
struct Auth
{
    QString accessToken;
    QString key;

    bool isEmpty() const { return accessToken.isEmpty() && key.isEmpty(); }
};

struct OwnedGame
{
    qint64 appId = 0;
    QString name;
    int playMinutes = 0;
    qint64 lastPlayed = 0;

    bool operator==(const OwnedGame &) const = default;
};

struct SharedApp
{
    qint64 appId = 0;
    QString name;
    QStringList owners;
    int appType = 1;
    int playMinutes = 0;
    qint64 lastPlayed = 0;

    bool operator==(const SharedApp &) const = default;
};

struct FamilyGroup
{
    bool ok = false;
    bool member = false;
    QString groupId;
};

template<typename T>
struct Parsed
{
    bool ok = false; // the answer had the shape of an answer
    QList<T> items;
};

Request ownedGamesRequest(const QString &steamId, const Auth &auth);
Request familyGroupForUserRequest(const QString &steamId, const QString &accessToken);
Request familyGroupRequest(const QString &groupId, const QString &accessToken);
Request userSummariesRequest(const QStringList &steamIds, const QString &accessToken);
Request playerSummariesRequest(const QStringList &steamIds, const QString &key);
Request sharedAppsRequest(const QString &steamId, const QString &groupId, const QString &accessToken);
Request clientAppsRequest(const QString &accessToken);
// `appIds` is at most itemsBatch() long.
Request itemsRequest(const QList<qint64> &appIds);
constexpr int itemsBatch() { return 100; }
// Needs no key.
Request achievementPercentagesRequest(qint64 appId);

// An answer with no games (`{"response":{}}`) is ok with no items: it can't be told from a private profile.
Parsed<OwnedGame> parseOwnedGames(const QJsonDocument &json);
FamilyGroup parseFamilyGroup(const QJsonDocument &json);
QStringList parseFamilyMembers(const QJsonDocument &json);
QHash<QString, QString> parseUserNames(const QJsonDocument &json);
Parsed<SharedApp> parseSharedApps(const QJsonDocument &json);
QHash<qint64, qint64> parseClientApps(const QJsonDocument &json);
// The share of all players (0–100) that unlocked each achievement, by API name.
QHash<QString, double> parseAchievementPercentages(const QJsonDocument &json);
// The store's type of each requested app; one the answer doesn't show is kTypeNoStoreData.
QHash<qint64, int> parseItemTypes(const QJsonDocument &json, const QList<qint64> &requested);

// What the last fetches brought, kept in <cache dir>/steam-library-<data dir key>.json, one per library. A part that failed or came back
// empty keeps what the fetch before it brought.
struct Cache
{
    QDateTime fetched;
    QString steamId;
    QList<OwnedGame> owned;
    QList<SharedApp> shared;
    QHash<QString, QString> names;
    QHash<qint64, qint64> sizes;
    QHash<qint64, int> types;

    bool isEmpty() const { return owned.isEmpty() && shared.isEmpty(); }
    bool operator==(const Cache &) const = default;
};

QString cachePath(const QString &cacheDir, const QString &libraryKey);
// The cache was one shared steam-library.json before it was kept per library; a library without its own yet
// starts from a copy, since the family games and lenders' names can't be fetched without the sign-in.
void adoptLegacyCache(const QString &cacheDir, const QString &libraryKey);
QByteArray formatCache(const Cache &cache);
bool parseCache(const QByteArray &bytes, Cache *out);
bool loadCache(const QString &path, Cache *out);
bool saveCache(const QString &path, const Cache &cache);

// Apps of a shared list that are games or software.
bool isGameType(int appType);

// A friend's game with some play time; games owned and never played are not kept.
struct FriendGame
{
    qint64 appId = 0;
    int recentMinutes = 0; // the last two weeks
    int totalMinutes = 0;

    bool operator==(const FriendGame &) const = default;
};

enum class FriendGroup { None, Recent, Previous };
FriendGroup groupOf(const FriendGame &game);

struct FriendSummary
{
    QString steamId;
    QString name;
    QString avatarUrl;
    // The game being played now; 0 = none. Not an app of the store for a non-Steam game.
    qint64 gameId = 0;
    QString gameName;
    // Any state but offline; a friend set to Invisible looks offline.
    bool online = false;

    bool operator==(const FriendSummary &) const = default;
};

struct Friend
{
    QString steamId;
    QString name;
    QString avatarUrl;
    QList<FriendGame> games;

    bool operator==(const Friend &) const = default;
};

// Needs the user's Web API key.
Request friendListRequest(const QString &steamId, const QString &key);
// `steamIds` is at most friendSummariesBatch() long.
constexpr int friendSummariesBatch() { return 100; }
// Without the games' names, which a friend's list doesn't need.
Request friendGamesRequest(const QString &steamId, const QString &key);

// A list with no friends is ok with no items: it can't be told from one that hides them.
Parsed<QString> parseFriendList(const QJsonDocument &json);
Parsed<FriendSummary> parseFriendSummaries(const QJsonDocument &json);
// An answer with no games (`{"response":{}}`) is ok with no items: it can't be told from a private profile.
Parsed<FriendGame> parseFriendGames(const QJsonDocument &json);

// <cache dir>/steam-friends-<data dir key>.json, one per library. Other people's activity: never in a backup.
struct FriendsCache
{
    QDateTime fetched;
    QString steamId;
    QList<Friend> friends;

    bool isEmpty() const { return friends.isEmpty(); }
    bool operator==(const FriendsCache &) const = default;
};

QString friendsCachePath(const QString &cacheDir, const QString &libraryKey);
QByteArray formatFriendsCache(const FriendsCache &cache);
bool parseFriendsCache(const QByteArray &bytes, FriendsCache *out);
bool loadFriendsCache(const QString &path, FriendsCache *out);
bool saveFriendsCache(const QString &path, const FriendsCache &cache);
}
