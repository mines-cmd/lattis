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

// An answer with no games (`{"response":{}}`) is ok with no items: it can't be told from a private profile.
Parsed<OwnedGame> parseOwnedGames(const QJsonDocument &json);
FamilyGroup parseFamilyGroup(const QJsonDocument &json);
QStringList parseFamilyMembers(const QJsonDocument &json);
QHash<QString, QString> parseUserNames(const QJsonDocument &json);
Parsed<SharedApp> parseSharedApps(const QJsonDocument &json);
QHash<qint64, qint64> parseClientApps(const QJsonDocument &json);
// The store's type of each requested app; one the answer doesn't show is kTypeNoStoreData.
QHash<qint64, int> parseItemTypes(const QJsonDocument &json, const QList<qint64> &requested);

// What the last fetches brought, kept in <cache dir>/steam-library.json. A part that failed or came back
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

QString cachePath(const QString &cacheDir);
QByteArray formatCache(const Cache &cache);
bool parseCache(const QByteArray &bytes, Cache *out);
bool loadCache(const QString &path, Cache *out);
bool saveCache(const QString &path, const Cache &cache);

// Apps of a shared list that are games or software.
bool isGameType(int appType);
}
