#include "fakehttpserver.h"
#include "credentials.h"
#include "metadatahttp.h"
#include "steamfriends.h"

#include <QDir>
#include <QJsonArray>
#include <QJsonObject>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

namespace {
const QString kMe = QStringLiteral("76561198000000001");
const QByteArray kList = "/ISteamUser/GetFriendList/v1/";
const QByteArray kSummaries = "/ISteamUser/GetPlayerSummaries/v2/";
const QByteArray kOwned = "/IPlayerService/GetOwnedGames/v1/";

QString friendId(int n)
{
    return QString::number(76561198000001000LL + n);
}

FakeHttpServer::Response answer(const QJsonObject &root, int status = 200)
{
    return {status, QJsonDocument(root).toJson(QJsonDocument::Compact), 0, {}};
}

FakeHttpServer::Response response(const QJsonObject &response)
{
    return answer({{"response", response}});
}

FakeHttpServer::Response friendList(int count)
{
    QJsonArray friends;
    for (int i = 0; i < count; ++i)
        friends.append(QJsonObject{{"steamid", friendId(i)}, {"relationship", "friend"}});
    return answer({{"friendslist", QJsonObject{{"friends", friends}}}});
}

QJsonObject player(int n, const QString &avatar = {}, qint64 gameId = 0)
{
    QJsonObject p{{"steamid", friendId(n)}, {"personaname", QStringLiteral("Friend %1").arg(n)}};
    if (!avatar.isEmpty())
        p.insert(QStringLiteral("avatarmedium"), avatar);
    if (gameId > 0) {
        p.insert(QStringLiteral("gameid"), QString::number(gameId));
        p.insert(QStringLiteral("gameextrainfo"), QStringLiteral("Game %1").arg(gameId));
    }
    return p;
}

FakeHttpServer::Response summaries(const QJsonArray &players)
{
    return response({{"players", players}});
}

FakeHttpServer::Response games(const QJsonArray &list)
{
    return response({{"game_count", list.size()}, {"games", list}});
}

QJsonObject played(qint64 appId, int total, int recent = 0)
{
    QJsonObject g{{"appid", appId}, {"playtime_forever", total}};
    if (recent > 0)
        g.insert(QStringLiteral("playtime_2weeks"), recent);
    return g;
}

void writeFile(const QString &path, const QByteArray &text)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile f(path);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write(text);
}
}

class SteamFriendsTest : public QObject
{
    Q_OBJECT

    QTemporaryDir m_dir;
    FakeHttpServer *m_server = nullptr;
    MetadataHttp *m_http = nullptr;
    SteamFriends *m_friends = nullptr;
    SteamFriends::Setup m_setup;

    QString data() const { return m_dir.filePath(QStringLiteral("data")); }
    QString cache() const { return m_dir.filePath(QStringLiteral("cache")); }
    QString cacheFile() const { return SteamWebApi::friendsCachePath(cache(), QStringLiteral("test")); }

    void makeFriends(int requestIntervalMs = 0, int playingIntervalMs = 60000)
    {
        m_setup = {};
        m_setup.dataDir = data();
        m_setup.cacheDir = cache();
        m_setup.cacheKey = QStringLiteral("test");
        m_setup.steamId = [] { return kMe; };
        m_setup.held = [] { return false; };
        m_setup.http = [this] { return m_http; };
        m_setup.requestIntervalMs = requestIntervalMs;
        m_setup.playingIntervalMs = playingIntervalMs;
        m_friends = new SteamFriends(m_setup);
    }

    void withKey()
    {
        QVERIFY(CredentialsFile::setValue(data(), QStringLiteral("steam"), QStringLiteral("web_key"), QStringLiteral("SECRETKEY")));
    }

    void fetch()
    {
        m_friends->syncNow();
        QTRY_COMPARE_WITH_TIMEOUT(m_friends->state(), SteamFriends::State::Idle, 20000);
    }

    SteamWebApi::FriendsCache cached()
    {
        SteamWebApi::FriendsCache c;
        SteamWebApi::loadFriendsCache(cacheFile(), &c);
        return c;
    }

    // Three friends: 0 played the game recently, 1 long ago, 2 owns it unplayed.
    void threeFriends()
    {
        m_server->on(kList, friendList(3));
        m_server->on(kSummaries, summaries({player(0), player(1), player(2)}));
        m_server->on(kOwned, games({played(10, 600, 90), played(11, 5)}));
        m_server->on(kOwned, games({played(10, 120), played(12, 0)}));
        m_server->on(kOwned, games({played(10, 0)}));
    }

private slots:
    void init()
    {
        QVERIFY(m_dir.isValid());
        QDir().mkpath(data());
        m_server = new FakeHttpServer;
        MetadataEndpoints endpoints;
        endpoints.steamWeb = m_server->url();
        m_http = new MetadataHttp(cache(), endpoints);
        m_friends = nullptr;
    }

    void cleanup()
    {
        delete m_friends;
        delete m_http;
        delete m_server;
        QDir(m_dir.path()).removeRecursively();
    }

    void parsesTheAnswers()
    {
        const auto list = SteamWebApi::parseFriendList(QJsonDocument::fromJson(
            R"({"friendslist":{"friends":[{"steamid":"1","relationship":"friend"},{"steamid":"2"}]}})"));
        QVERIFY(list.ok);
        QCOMPARE(list.items, (QStringList{"1", "2"}));
        QVERIFY(SteamWebApi::parseFriendList(QJsonDocument::fromJson(R"({"friendslist":{"friends":[]}})")).ok);
        QVERIFY(!SteamWebApi::parseFriendList(QJsonDocument::fromJson(R"({"response":{}})")).ok);
        QVERIFY(!SteamWebApi::parseFriendList(QJsonDocument()).ok);

        const auto people = SteamWebApi::parseFriendSummaries(QJsonDocument::fromJson(
            R"({"response":{"players":[
                {"steamid":"1","personaname":"Ann","avatarmedium":"https:\/\/x\/a_medium.jpg","gameid":"1422450","gameextrainfo":"Deadlock"},
                {"steamid":"2","personaname":"Bob"}]}})"));
        QVERIFY(people.ok);
        QCOMPARE(people.items.size(), 2);
        QCOMPARE(people.items.at(0), (SteamWebApi::FriendSummary{"1", "Ann", "https://x/a_medium.jpg", 1422450, "Deadlock"}));
        QCOMPARE(people.items.at(1).gameId, qint64(0));

        // Played recently, played previously, owned and unplayed (left out).
        const auto owned = SteamWebApi::parseFriendGames(QJsonDocument::fromJson(
            R"({"response":{"game_count":3,"games":[
                {"appid":10,"playtime_forever":600,"playtime_2weeks":90},
                {"appid":11,"playtime_forever":5},
                {"appid":12,"playtime_forever":0}]}})"));
        QVERIFY(owned.ok);
        QCOMPARE(owned.items.size(), 2);
        QCOMPARE(SteamWebApi::groupOf(owned.items.at(0)), SteamWebApi::FriendGroup::Recent);
        QCOMPARE(SteamWebApi::groupOf(owned.items.at(1)), SteamWebApi::FriendGroup::Previous);
        QCOMPARE(SteamWebApi::groupOf({12, 0, 0}), SteamWebApi::FriendGroup::None);
        // A private profile answers without games.
        const auto hidden = SteamWebApi::parseFriendGames(QJsonDocument::fromJson(R"({"response":{}})"));
        QVERIFY(hidden.ok && hidden.items.isEmpty());
    }

    void requestsKeepTheKeyInTheQuery()
    {
        const auto list = SteamWebApi::friendListRequest(kMe, "K");
        QCOMPARE(list.path, QString::fromUtf8(kList));
        QCOMPARE(list.query.queryItemValue("key"), QStringLiteral("K"));
        QCOMPARE(list.query.queryItemValue("relationship"), QStringLiteral("friend"));
        const auto owned = SteamWebApi::friendGamesRequest(friendId(1), "K");
        QCOMPARE(owned.path, QString::fromUtf8(kOwned));
        QCOMPARE(owned.query.queryItemValue("steamid"), friendId(1));
        QCOMPARE(owned.query.queryItemValue("include_played_free_games"), QStringLiteral("1"));
        QCOMPARE(owned.query.queryItemValue("skip_unvetted_apps"), QStringLiteral("0"));
        QVERIFY(!owned.query.hasQueryItem("access_token"));
    }

    void cacheRoundTrips()
    {
        SteamWebApi::FriendsCache c;
        c.steamId = kMe;
        c.fetched = QDateTime(QDate(2026, 11, 5), QTime(10, 0), QTimeZone::utc());
        c.friends = {{friendId(0), "Ann", "https://x/a_medium.jpg", {{10, 90, 600}, {11, 0, 5}}},
                     {friendId(1), "Bob", "", {}}};
        SteamWebApi::FriendsCache back;
        QVERIFY(SteamWebApi::parseFriendsCache(SteamWebApi::formatFriendsCache(c), &back));
        QCOMPARE(back, c);
        QVERIFY(!SteamWebApi::parseFriendsCache("not json", &back));
        QVERIFY(!SteamWebApi::parseFriendsCache(R"({"version":99})", &back));
    }

    void noRequestWithoutAKeyOrWithTheSwitchOff()
    {
        makeFriends();
        QVERIFY(!m_friends->enabled());
        fetch();
        QCOMPARE(m_friends->outcome(), SteamFriends::Outcome::NotConfigured);

        withKey();
        QVERIFY(m_friends->enabled());
        writeFile(data() + "/settings.toml", "[steam]\nfriends_activity = false\n");
        QVERIFY(!m_friends->enabled());
        fetch();
        QCOMPARE(m_friends->outcome(), SteamFriends::Outcome::Disabled);

        // The daily check sends nothing either.
        m_friends->startDaily(10);
        QTest::qWait(150);
        QVERIFY(m_server->requests.isEmpty());
        QVERIFY(!QFileInfo::exists(cacheFile()));
    }

    void fetchesAndGroupsByGame()
    {
        withKey();
        threeFriends();
        makeFriends();
        QSignalSpy changed(m_friends, &SteamFriends::changed);
        fetch();
        QCOMPARE(m_friends->outcome(), SteamFriends::Outcome::Synced);
        QVERIFY(changed.count() >= 1);
        QCOMPARE(m_friends->friendCount(), 3);

        // The key is the only credential, and it stays in the query.
        QCOMPARE(m_server->requestsTo(kList).size(), 1);
        QCOMPARE(m_server->requestsTo(kSummaries).size(), 1);
        QCOMPARE(m_server->requestsTo(kOwned).size(), 3);
        for (const auto &r : std::as_const(m_server->requests)) {
            QVERIFY(r.target.contains("key=SECRETKEY"));
            QVERIFY(!r.target.contains("access_token"));
        }
        QVERIFY(m_server->requestsTo(kList).first().target.contains("steamid=" + kMe.toUtf8()));

        const auto ten = m_friends->friendsOf(10);
        QCOMPARE(ten.recent.size(), 1);
        QCOMPARE(ten.recent.first().name, QStringLiteral("Friend 0"));
        QCOMPARE(ten.recent.first().recentMinutes, 90);
        QCOMPARE(ten.previous.size(), 1);
        QCOMPARE(ten.previous.first().name, QStringLiteral("Friend 1"));
        QCOMPARE(ten.previous.first().totalMinutes, 120);
        // Owned and unplayed is in neither group.
        QCOMPARE(m_friends->friendsOf(12).isEmpty(), true);
        QCOMPARE(m_friends->friendsOf(11).previous.size(), 1);
        QCOMPARE(m_friends->friendsOf(999).isEmpty(), true);

        QCOMPARE(cached().friends.size(), 3);
        QVERIFY(cached().fetched.isValid());
        // The cache lives under the cache dir and nowhere in the data dir (which backups take).
        QVERIFY(cacheFile().startsWith(cache()));
        QVERIFY(!QDir(data()).entryList(QDir::AllEntries | QDir::NoDotAndDotDot).contains("steam-friends-test.json"));

        // A new run reads the cache and a lookup sends nothing.
        delete m_friends;
        m_server->requests.clear();
        makeFriends();
        QCOMPARE(m_friends->friendsOf(10).recent.size(), 1);
        QVERIFY(m_server->requests.isEmpty());
    }

    void sortsByPlayTime()
    {
        withKey();
        m_server->on(kList, friendList(3));
        m_server->on(kSummaries, summaries({player(0), player(1), player(2)}));
        m_server->on(kOwned, games({played(10, 100, 10)}));
        m_server->on(kOwned, games({played(10, 100, 50)}));
        m_server->on(kOwned, games({played(10, 100, 30)}));
        makeFriends();
        fetch();
        const auto recent = m_friends->friendsOf(10).recent;
        QCOMPARE(recent.size(), 3);
        QCOMPARE(recent.at(0).name, QStringLiteral("Friend 1"));
        QCOMPARE(recent.at(1).name, QStringLiteral("Friend 2"));
        QCOMPARE(recent.at(2).name, QStringLiteral("Friend 0"));
    }

    void aPrivateListKeepsTheCache()
    {
        withKey();
        threeFriends();
        makeFriends();
        fetch();
        const auto before = cached();

        // A private list answers 401 (also a key Steam doesn't accept).
        m_server->clear(kList);
        m_server->on(kList, answer({}, 401));
        m_server->requests.clear();
        fetch();
        QCOMPARE(m_friends->outcome(), SteamFriends::Outcome::Failed);
        QVERIFY(!m_friends->error().isEmpty());
        QVERIFY(!m_friends->error().contains("SECRETKEY"));
        QCOMPARE(m_server->requests.size(), 1);
        QCOMPARE(cached(), before);
        QCOMPARE(m_friends->friendsOf(10).recent.size(), 1);

        // An answer that isn't a friends list is a failure too.
        m_server->clear(kList);
        m_server->on(kList, response({}));
        fetch();
        QCOMPARE(m_friends->outcome(), SteamFriends::Outcome::Failed);
        QCOMPARE(cached(), before);
    }

    void anEmptyListChangesNothing()
    {
        withKey();
        threeFriends();
        makeFriends();
        fetch();
        const auto before = cached();
        m_server->clear(kList);
        m_server->on(kList, friendList(0));
        m_server->requests.clear();
        fetch();
        QCOMPARE(m_friends->outcome(), SteamFriends::Outcome::Synced);
        QCOMPARE(m_server->requests.size(), 1);
        QCOMPARE(cached(), before);
    }

    void anEmptyOrFailedAnswerKeepsTheFriendsLastEntry()
    {
        withKey();
        threeFriends();
        makeFriends();
        fetch();
        const auto before = cached();

        // Friend 0 turns private (empty), friend 1 fails, friend 2 now has played it.
        m_server->clear(kOwned);
        m_server->on(kOwned, response({}));
        m_server->on(kOwned, answer({}, 500));
        m_server->on(kOwned, games({played(10, 30, 30)}));
        fetch();
        QCOMPARE(m_friends->outcome(), SteamFriends::Outcome::Synced);
        // The failed one is reported, and the fetch counts as incomplete: its time doesn't move.
        QVERIFY(!m_friends->error().isEmpty());
        QCOMPARE(cached().fetched, before.fetched);
        const auto ten = m_friends->friendsOf(10);
        QCOMPARE(ten.recent.size(), 2);
        QCOMPARE(ten.previous.size(), 1);
        QCOMPARE(cached().friends.at(0).games, before.friends.at(0).games);
        QCOMPARE(cached().friends.at(1).games, before.friends.at(1).games);
        QCOMPARE(cached().friends.at(2).games, (QList<SteamWebApi::FriendGame>{{10, 30, 30}}));
    }

    void aFriendWhoLeftTheListIsDropped()
    {
        withKey();
        threeFriends();
        makeFriends();
        fetch();
        m_server->clear(kList);
        m_server->on(kList, friendList(2));
        m_server->clear(kSummaries);
        m_server->on(kSummaries, summaries({player(0), player(1)}));
        fetch();
        QCOMPARE(m_friends->friendCount(), 2);
        QCOMPARE(m_friends->friendsOf(10).recent.size(), 1);
    }

    void moreThanAHundredFriendsAreAskedInBatches()
    {
        withKey();
        m_server->on(kList, friendList(250));
        const auto batch = [](int from, int to) {
            QJsonArray players;
            for (int i = from; i < to; ++i)
                players.append(player(i));
            return summaries(players);
        };
        m_server->on(kSummaries, batch(0, 100));
        m_server->on(kSummaries, batch(100, 200));
        m_server->on(kSummaries, batch(200, 250));
        m_server->on(kOwned, games({played(10, 60, 60)}));
        makeFriends();
        fetch();
        QCOMPARE(m_friends->outcome(), SteamFriends::Outcome::Synced);

        const auto asked = m_server->requestsTo(kSummaries);
        QCOMPARE(asked.size(), 3);
        const auto idCount = [](const QByteArray &target) {
            const QByteArray marker = "steamids=";
            QByteArray ids = target.mid(target.indexOf(marker) + marker.size()).split('&').first();
            return ids.replace("%2C", ",").split(',').size();
        };
        QCOMPARE(idCount(asked.at(0).target), 100);
        QCOMPARE(idCount(asked.at(1).target), 100);
        QCOMPARE(idCount(asked.at(2).target), 50);
        QCOMPARE(m_server->requestsTo(kOwned).size(), 250);
        QCOMPARE(m_friends->friendCount(), 250);
        QCOMPARE(cached().friends.last().name, QStringLiteral("Friend 249"));
        QCOMPARE(m_friends->friendsOf(10).recent.size(), 250);
    }

    void requestsAreOneAtATimeAndPaced()
    {
        withKey();
        m_server->on(kList, friendList(3));
        m_server->on(kSummaries, summaries({player(0), player(1), player(2)}));
        m_server->on(kOwned, games({played(10, 60, 60)}));
        makeFriends(120);
        fetch();
        QCOMPARE(m_server->requests.size(), 5);
        for (qsizetype i = 1; i < m_server->requests.size(); ++i)
            QVERIFY2(m_server->requests.at(i).atMs - m_server->requests.at(i - 1).atMs >= 100,
                     qPrintable(QString::number(i)));
    }

    void anotherAccountsCacheIsNotUsed()
    {
        withKey();
        threeFriends();
        makeFriends();
        fetch();
        delete m_friends;
        m_setup.steamId = [] { return QStringLiteral("76561198000000099"); };
        m_friends = new SteamFriends(m_setup);
        QVERIFY(m_friends->friendsOf(10).isEmpty());
    }

    void aSecondaryAccountWithoutIdDoesNotFetch()
    {
        withKey();
        makeFriends();
        delete m_friends;
        m_setup.steamId = [] { return QString(); };
        m_friends = new SteamFriends(m_setup);
        fetch();
        QCOMPARE(m_friends->outcome(), SteamFriends::Outcome::NoAccount);
        QVERIFY(m_server->requests.isEmpty());
    }

    void picturesAreCachedByHashAndPruned()
    {
        withKey();
        const QString first = m_server->url("/avatars/aaa111_medium.jpg").toString();
        const QString second = m_server->url("/avatars/bbb222_medium.jpg").toString();
        m_server->on("/avatars/aaa111_medium.jpg", {200, "JPEG-A", 0, {}});
        m_server->on("/avatars/bbb222_medium.jpg", {200, "JPEG-B", 0, {}});
        m_server->on(kList, friendList(2));
        m_server->on(kSummaries, summaries({player(0, first), player(1, second)}));
        m_server->on(kOwned, games({played(10, 60, 60)}));
        makeFriends();
        fetch();

        const auto people = m_friends->friendsOf(10).recent;
        QCOMPARE(people.size(), 2);
        QCOMPARE(QFileInfo(people.at(0).avatarPath).fileName(), QStringLiteral("aaa111_medium.jpg"));
        QVERIFY(people.at(0).avatarPath.startsWith(cache()));
        QCOMPARE(QFile(people.at(0).avatarPath).size(), qint64(6));
        QVERIFY(QFileInfo::exists(people.at(1).avatarPath));
        QCOMPARE(m_server->requestsTo("/avatars/aaa111_medium.jpg").size(), 1);

        // A picture already on disk isn't downloaded again; one nobody uses any more is removed.
        m_server->requests.clear();
        m_server->clear(kList);
        m_server->on(kList, friendList(1));
        m_server->clear(kSummaries);
        m_server->on(kSummaries, summaries({player(0, first)}));
        fetch();
        QVERIFY(m_server->requestsTo("/avatars/aaa111_medium.jpg").isEmpty());
        QVERIFY(QFileInfo::exists(cache() + "/steam-avatars/aaa111_medium.jpg"));
        QVERIFY(!QFileInfo::exists(cache() + "/steam-avatars/bbb222_medium.jpg"));
    }

    void anUnsafePictureAddressIsIgnored()
    {
        withKey();
        m_server->on(kList, friendList(2));
        m_server->on(kSummaries, summaries({player(0, "file:///etc/passwd.jpg"), player(1, m_server->url("/x/../..").toString())}));
        m_server->on(kOwned, games({played(10, 60, 60)}));
        makeFriends();
        fetch();
        QCOMPARE(m_friends->outcome(), SteamFriends::Outcome::Synced);
        for (const auto &p : m_friends->friendsOf(10).recent)
            QVERIFY(p.avatarPath.isEmpty());
        QCOMPARE(m_server->requests.size(), 4);
    }

    void playingNowRefreshesAtMostOnceAMinute()
    {
        withKey();
        threeFriends();
        makeFriends(0, 250);
        // Nothing is cached yet: nothing to ask.
        QVERIFY(!m_friends->refreshPlaying());
        fetch();
        QVERIFY(m_friends->playingNow().isEmpty());
        m_server->requests.clear();

        m_server->clear(kSummaries);
        m_server->on(kSummaries, summaries({player(0, {}, 10), player(1), player(2, {}, 777)}));
        QSignalSpy changed(m_friends, &SteamFriends::changed);
        QVERIFY(m_friends->refreshPlaying());
        QTRY_COMPARE(changed.count(), 1);
        QCOMPARE(m_server->requests.size(), 1);
        QVERIFY(m_server->requests.first().target.contains("key=SECRETKEY"));
        QCOMPARE(m_friends->playingNow().size(), 2);
        const auto ten = m_friends->friendsOf(10);
        QCOMPARE(ten.playing.size(), 1);
        QCOMPARE(ten.playing.first().name, QStringLiteral("Friend 0"));
        QCOMPARE(ten.playing.first().gameName, QStringLiteral("Game 10"));
        // The friend playing it is still in the group their play time puts them in.
        QCOMPARE(ten.recent.size(), 1);
        // A game that isn't in anyone's list can still be played now.
        QCOMPARE(m_friends->friendsOf(777).playing.size(), 1);

        // Within the interval: nothing is sent.
        QVERIFY(!m_friends->refreshPlaying());
        QCOMPARE(m_server->requests.size(), 1);

        // A friend who stopped.
        m_server->clear(kSummaries);
        m_server->on(kSummaries, summaries({player(0), player(1), player(2, {}, 777)}));
        QTest::qWait(300);
        QVERIFY(m_friends->refreshPlaying());
        QTRY_COMPARE(changed.count(), 2);
        QCOMPARE(m_friends->friendsOf(10).playing.size(), 0);
        QCOMPARE(m_friends->playingNow().size(), 1);

        // A fetch is not interrupted by it, and Switching the setting off stops it.
        QTest::qWait(300);
        writeFile(data() + "/settings.toml", "[steam]\nfriends_activity = false\n");
        m_server->requests.clear();
        QVERIFY(!m_friends->refreshPlaying());
        QVERIFY(m_server->requests.isEmpty());
    }

    void playingNowIsAskedInBatchesOfAHundred()
    {
        withKey();
        m_server->on(kList, friendList(120));
        m_server->on(kSummaries, summaries({player(0)}));
        m_server->on(kOwned, games({played(10, 60, 60)}));
        makeFriends();
        fetch();
        m_server->requests.clear();
        m_server->clear(kSummaries);
        m_server->on(kSummaries, summaries({player(5, {}, 10)}));
        m_server->on(kSummaries, summaries({player(110, {}, 10)}));
        QSignalSpy changed(m_friends, &SteamFriends::changed);
        QVERIFY(m_friends->refreshPlaying());
        QTRY_COMPARE(changed.count(), 1);
        QCOMPARE(m_server->requestsTo(kSummaries).size(), 2);
        QCOMPARE(m_friends->playingNow().size(), 2);
    }

    void fixturesAnswerPerFriendWithoutTheNetwork()
    {
        const QString fixtures = m_dir.filePath(QStringLiteral("fixtures"));
        writeFile(fixtures + "/ISteamUser-GetFriendList-v1.json",
                  QJsonDocument(QJsonObject{{"friendslist", QJsonObject{{"friends", QJsonArray{
                      QJsonObject{{"steamid", friendId(0)}}, QJsonObject{{"steamid", friendId(1)}}}}}}}).toJson());
        writeFile(fixtures + "/ISteamUser-GetPlayerSummaries-v2.json",
                  QJsonDocument(QJsonObject{{"response", QJsonObject{{"players", QJsonArray{
                      player(0, "https://avatars.example/aaa_medium.jpg"), player(1, "https://avatars.example/bbb_medium.jpg", 10)}}}}}).toJson());
        writeFile(fixtures + "/IPlayerService-GetOwnedGames-v1-" + friendId(0) + ".json",
                  QJsonDocument(QJsonObject{{"response", QJsonObject{{"games", QJsonArray{played(10, 60, 60)}}}}}).toJson());
        writeFile(fixtures + "/IPlayerService-GetOwnedGames-v1-" + friendId(1) + ".json",
                  QJsonDocument(QJsonObject{{"response", QJsonObject{{"games", QJsonArray{played(10, 60)}}}}}).toJson());
        withKey();
        makeFriends(5000);
        delete m_friends;
        m_setup.fixturesDir = fixtures;
        m_friends = new SteamFriends(m_setup);
        fetch();
        QCOMPARE(m_friends->outcome(), SteamFriends::Outcome::Synced);
        QVERIFY(m_server->requests.isEmpty());
        const auto ten = m_friends->friendsOf(10);
        QCOMPARE(ten.recent.size(), 1);
        QCOMPARE(ten.previous.size(), 1);
        QCOMPARE(ten.playing.size(), 1);
        // Pictures are made on the spot, with no request.
        QVERIFY(QFileInfo::exists(ten.recent.first().avatarPath));
    }
};

QTEST_GUILESS_MAIN(SteamFriendsTest)

#include "steamfriends_test.moc"
