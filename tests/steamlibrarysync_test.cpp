#include "fakehttpserver.h"
#include "credentials.h"
#include "metadatahttp.h"
#include "steamlibrarysync.h"

#include <QDir>
#include <QDirIterator>
#include <QJsonArray>
#include <QJsonObject>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

namespace {
const QString kMe = QStringLiteral("76561198000000001");
const QByteArray kOwned = "/IPlayerService/GetOwnedGames/v1/";
const QByteArray kItems = "/IStoreBrowseService/GetItems/v1";
const QByteArray kGroupFor = "/IFamilyGroupsService/GetFamilyGroupForUser/v1/";
const QByteArray kGroup = "/IFamilyGroupsService/GetFamilyGroup/v1/";
const QByteArray kShared = "/IFamilyGroupsService/GetSharedLibraryApps/v1/";
const QByteArray kClient = "/IClientCommService/GetClientAppList/v1/";
const QByteArray kNames = "/ISteamUserOAuth/GetUserSummaries/v1/";
const QByteArray kAccess = "/IAuthenticationService/GenerateAccessTokenForApp/v1/";

QByteArray jwt(const QString &subject, qint64 expiresIn)
{
    const auto part = [](const QJsonObject &o) {
        return QJsonDocument(o).toJson(QJsonDocument::Compact).toBase64(QByteArray::Base64UrlEncoding
                                                                         | QByteArray::OmitTrailingEquals);
    };
    return part({{"alg", "none"}}) + "." + part({{"sub", subject}, {"exp", QDateTime::currentSecsSinceEpoch() + expiresIn}})
        + ".sig";
}

FakeHttpServer::Response json(const QJsonObject &response, int status = 200)
{
    return {status, QJsonDocument(QJsonObject{{"response", response}}).toJson(QJsonDocument::Compact), 0,
            "x-eresult: 1\r\n"};
}

QJsonArray ownedGames(int count, int first = 1000)
{
    QJsonArray games;
    for (int i = 0; i < count; ++i)
        games.append(QJsonObject{{"appid", first + i}, {"name", QStringLiteral("Game %1").arg(first + i)},
                                 {"playtime_forever", 10}});
    return games;
}

QStringList listing(const QString &dir)
{
    QStringList all;
    QDirIterator it(dir, QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden, QDirIterator::Subdirectories);
    while (it.hasNext())
        all.append(it.next());
    all.sort();
    return all;
}

class CountingStore : public MemorySecretStore
{
public:
    std::optional<QByteArray> read(const Attributes &attributes) override
    {
        ++reads;
        return MemorySecretStore::read(attributes);
    }
    std::atomic<int> reads{0};
};

QStringList g_logged;
QtMessageHandler g_previousHandler = nullptr;

void captureLog(QtMsgType type, const QMessageLogContext &context, const QString &message)
{
    g_logged.append(message);
    g_previousHandler(type, context, message);
}

bool anyLogged(const QString &needle)
{
    return std::any_of(g_logged.cbegin(), g_logged.cend(), [&](const QString &m) { return m.contains(needle); });
}

QByteArray fileText(const QString &path)
{
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}
}

class SteamLibrarySyncTest : public QObject
{
    Q_OBJECT

    QTemporaryDir m_dir;
    FakeHttpServer *m_server = nullptr;
    MetadataHttp *m_http = nullptr;
    std::shared_ptr<CountingStore> m_store;
    SteamLibrarySync *m_sync = nullptr;
    SteamLibrarySync::Setup m_setup;
    int m_changed = 0;

    QString data() const { return m_dir.filePath(QStringLiteral("data")); }
    QString cache() const { return m_dir.filePath(QStringLiteral("cache")); }
    QString stateFile() const { return m_dir.filePath(QStringLiteral("state/steam-library.toml")); }

    void makeSync()
    {
        m_setup = {};
        m_setup.dataDir = data();
        m_setup.cacheDir = cache();
        m_setup.cacheKey = QStringLiteral("test");
        m_setup.stateFile = stateFile();
        m_setup.steamId = [] { return kMe; };
        m_setup.held = [] { return false; };
        m_setup.http = [this] { return m_http; };
        m_setup.changed = [this] { ++m_changed; };
        m_setup.secrets = m_store;
        m_setup.authBase = m_server->url();
        m_sync = new SteamLibrarySync(m_setup);
    }

    void withKey() { QVERIFY(CredentialsFile::setValue(data(), QStringLiteral("steam"), QStringLiteral("web_key"), QStringLiteral("SECRETKEY"))); }

    void signedIn()
    {
        m_store->store(QStringLiteral("x"), SteamAuth::itemAttributes(kMe), jwt(kMe, 86400 * 100));
        m_server->on(kAccess, json({{"access_token", QString::fromUtf8(jwt(kMe, 86400))}}));
    }

    void answerEverything()
    {
        m_server->on(kOwned, json({{"game_count", 3}, {"games", ownedGames(3)}}));
        m_server->on(kItems, json({{"store_items", QJsonArray{QJsonObject{{"appid", 1000}, {"success", 1}, {"type", 0}},
                                                                QJsonObject{{"appid", 1001}, {"success", 1}, {"type", 0}},
                                                                QJsonObject{{"appid", 1002}, {"success", 1}, {"type", 4}}}}}));
        m_server->on(kGroupFor, json({{"family_groupid", "55"}, {"is_not_member_of_any_group", false}}));
        m_server->on(kGroup, json({{"members", QJsonArray{QJsonObject{{"steamid", kMe}},
                                                          QJsonObject{{"steamid", "76561198000000009"}}}}}));
        m_server->on(kNames, json({{"players", QJsonArray{QJsonObject{{"steamid", "76561198000000009"},
                                                                     {"personaname", "Alex"}}}}}));
        m_server->on(kShared, json({{"apps", QJsonArray{QJsonObject{{"appid", 2000},
                                                                   {"name", "Family Game"},
                                                                   {"owner_steamids", QJsonArray{"76561198000000009"}},
                                                                   {"app_type", 1}}}}}));
        m_server->on(kClient, json({{"apps", QJsonArray{QJsonObject{{"appid", 1000}, {"bytes_required", "4096"}}}}}));
    }

    SteamWebApi::Cache cachedLibrary()
    {
        SteamWebApi::Cache c;
        SteamWebApi::loadCache(SteamWebApi::cachePath(cache(), QStringLiteral("test")), &c);
        return c;
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
        m_store = std::make_shared<CountingStore>();
        m_changed = 0;
        m_sync = nullptr;
    }

    void cleanup()
    {
        delete m_sync;
        delete m_http;
        delete m_server;
        QDir(m_dir.path()).removeRecursively();
    }

    void noRequestWithoutASignInOrKey()
    {
        makeSync();
        m_sync->syncNow();
        QTRY_COMPARE(m_sync->state(), SteamLibrarySync::State::Idle);
        QCOMPARE(m_sync->outcome(), SteamLibrarySync::Outcome::NotConfigured);
        QVERIFY(m_server->requests.isEmpty());

        // The daily check never touches the keyring for an account that never signed in here.
        m_sync->startDaily(10);
        QTest::qWait(150);
        QVERIFY(m_server->requests.isEmpty());
        QCOMPARE(m_store->reads.load(), 1); // only the press of Sync now
        QVERIFY(!QFileInfo::exists(SteamWebApi::cachePath(cache(), QStringLiteral("test"))));
    }

    void theKeyFetchesTheOwnedGamesAndTheirTypes()
    {
        withKey();
        // 150 games: two batches of types.
        m_server->on(kOwned, json({{"game_count", 150}, {"games", ownedGames(150)}}));
        m_server->on(kItems, json({{"store_items", QJsonArray()}}));
        const QStringList before = listing(data());
        makeSync();
        m_sync->syncNow();
        QTRY_COMPARE(m_sync->state(), SteamLibrarySync::State::Idle);
        QCOMPARE(m_sync->outcome(), SteamLibrarySync::Outcome::Synced);

        QCOMPARE(m_server->requestsTo(kOwned).size(), 1);
        QCOMPARE(m_server->requestsTo(kItems).size(), 2);
        // Only the owned games: a key can't ask for the family's.
        QCOMPARE(m_server->requests.size(), 3);
        const QByteArray target = m_server->requestsTo(kOwned).first().target;
        QVERIFY(target.contains("key=SECRETKEY"));
        QVERIFY(!target.contains("access_token"));
        QVERIFY(target.contains("steamid=" + kMe.toUtf8()));

        const SteamWebApi::Cache c = cachedLibrary();
        QCOMPARE(c.owned.size(), 150);
        QCOMPARE(c.types.size(), 150);
        QCOMPARE(c.steamId, kMe);
        QVERIFY(c.fetched.isValid());
        QCOMPARE(m_changed, 1);
        QVERIFY(m_sync->lastFetch().isValid());
        QCOMPARE(m_sync->gameCount(), 150);
        QVERIFY(fileText(stateFile()).contains("last_fetch = \""));
        // The key never leaves credentials.toml, the cache never holds a secret.
        QVERIFY(!fileText(SteamWebApi::cachePath(cache(), QStringLiteral("test"))).contains("SECRETKEY"));
        QVERIFY(!fileText(stateFile()).contains("SECRETKEY"));
        QCOMPARE(listing(data()), before);

        // Types are asked once per app.
        m_server->requests.clear();
        m_sync->syncNow();
        QTRY_COMPARE(m_sync->state(), SteamLibrarySync::State::Idle);
        QCOMPARE(m_server->requestsTo(kItems).size(), 0);
        QCOMPARE(m_changed, 1);
    }

    void aSignInFetchesTheFamilyToo()
    {
        signedIn();
        answerEverything();
        makeSync();
        m_sync->syncNow();
        QTRY_COMPARE(m_sync->state(), SteamLibrarySync::State::Idle);
        QCOMPARE(m_sync->outcome(), SteamLibrarySync::Outcome::Synced);
        QVERIFY(m_sync->signedIn());

        const QByteArray target = m_server->requestsTo(kOwned).first().target;
        QVERIFY(target.contains("access_token="));
        QVERIFY(!target.contains("key="));
        for (const QByteArray &path : {kGroupFor, kGroup, kNames, kShared, kClient, kItems})
            QCOMPARE(m_server->requestsTo(path).size(), 1);
        const SteamWebApi::Cache c = cachedLibrary();
        QCOMPARE(c.owned.size(), 3);
        QCOMPARE(c.shared.size(), 1);
        QCOMPARE(c.shared.first().owners, QStringList{QStringLiteral("76561198000000009")});
        QCOMPARE(c.names.value(QStringLiteral("76561198000000009")), QStringLiteral("Alex"));
        QCOMPARE(c.sizes.value(1000), 4096);
        QCOMPARE(c.types.value(1002), 4);
        QCOMPARE(m_sync->gameCount(), 4);
        QVERIFY(fileText(stateFile()).contains("signed_in = \"" + kMe.toUtf8() + "\""));
    }

    void aFamilyOfOneHasNoSharedLibrary()
    {
        signedIn();
        answerEverything();
        m_server->clear(kGroupFor);
        m_server->on(kGroupFor, json({{"is_not_member_of_any_group", true}}));
        makeSync();
        m_sync->syncNow();
        QTRY_COMPARE(m_sync->state(), SteamLibrarySync::State::Idle);
        QCOMPARE(m_server->requestsTo(kShared).size(), 0);
        QCOMPARE(m_server->requestsTo(kGroup).size(), 0);
        QCOMPARE(cachedLibrary().owned.size(), 3);
        QVERIFY(cachedLibrary().shared.isEmpty());
    }

    void anEmptyOrFailedAnswerKeepsWhatIsCached()
    {
        withKey();
        m_server->on(kOwned, json({{"game_count", 3}, {"games", ownedGames(3)}}));
        m_server->on(kItems, json({{"store_items", QJsonArray()}}));
        makeSync();
        m_sync->syncNow();
        QTRY_COMPARE(m_sync->state(), SteamLibrarySync::State::Idle);
        const SteamWebApi::Cache first = cachedLibrary();
        QCOMPARE(first.owned.size(), 3);
        const QDateTime fetched = m_sync->lastFetch();
        QTest::qWait(1100);

        // Game details are private (or the sign-in lapsed): an answer with no games.
        m_server->clear(kOwned);
        m_server->on(kOwned, json({}));
        m_sync->syncNow();
        QTRY_COMPARE(m_sync->state(), SteamLibrarySync::State::Idle);
        QCOMPARE(m_sync->outcome(), SteamLibrarySync::Outcome::NoGames);
        QCOMPARE(cachedLibrary().owned, first.owned);
        QCOMPARE(m_sync->lastFetch(), fetched);

        // An outage.
        g_logged.clear();
        g_previousHandler = qInstallMessageHandler(captureLog);
        m_server->clear(kOwned);
        m_server->on(kOwned, {503, "busy"});
        m_sync->syncNow();
        QTRY_COMPARE(m_sync->state(), SteamLibrarySync::State::Idle);
        qInstallMessageHandler(g_previousHandler);
        // The failure is logged, and the key is nowhere in what was.
        QVERIFY(anyLogged(QStringLiteral("GetOwnedGames")));
        QVERIFY(!anyLogged(QStringLiteral("SECRETKEY")));
        QCOMPARE(m_sync->outcome(), SteamLibrarySync::Outcome::Failed);
        QVERIFY(!m_sync->error().isEmpty());
        QVERIFY(!m_sync->error().contains(QLatin1String("SECRETKEY")));
        QCOMPARE(cachedLibrary().owned, first.owned);
        QCOMPARE(cachedLibrary().fetched, first.fetched);
        QCOMPARE(m_sync->lastFetch(), fetched);
    }

    void aRefusedSignInNeedsASignInAgain()
    {
        m_store->store(QStringLiteral("x"), SteamAuth::itemAttributes(kMe), jwt(kMe, 86400 * 100));
        m_server->on(kAccess, {200, QJsonDocument(QJsonObject{{"response", QJsonObject()}}).toJson(), 0, "x-eresult: 5\r\n"});
        makeSync();
        m_sync->syncNow();
        QTRY_COMPARE(m_sync->state(), SteamLibrarySync::State::Idle);
        QCOMPARE(m_sync->outcome(), SteamLibrarySync::Outcome::SignInExpired);
        QVERIFY(m_server->requestsTo(kOwned).isEmpty());

        // With a key set, the owned games still come.
        withKey();
        m_server->on(kOwned, json({{"games", ownedGames(2)}}));
        m_server->on(kItems, json({{"store_items", QJsonArray()}}));
        m_sync->syncNow();
        QTRY_COMPARE(m_sync->state(), SteamLibrarySync::State::Idle);
        QCOMPARE(m_sync->outcome(), SteamLibrarySync::Outcome::Synced);
        QCOMPARE(cachedLibrary().owned.size(), 2);
    }

    void theDailyCheckWaitsADay()
    {
        withKey();
        m_server->on(kOwned, json({{"games", ownedGames(1)}}));
        m_server->on(kItems, json({{"store_items", QJsonArray()}}));
        QDir().mkpath(QFileInfo(stateFile()).absolutePath());
        QFile state(stateFile());
        QVERIFY(state.open(QIODevice::WriteOnly));
        state.write(("last_fetch = \"" + QDateTime::currentDateTimeUtc().addSecs(-3600).toString(Qt::ISODate) + "\"\n").toUtf8());
        state.close();
        makeSync();
        m_sync->startDaily(10);
        QTest::qWait(200);
        QVERIFY(m_server->requests.isEmpty());

        // A day and a minute later.
        delete m_sync;
        QVERIFY(state.open(QIODevice::WriteOnly | QIODevice::Truncate));
        state.write(("last_fetch = \"" + QDateTime::currentDateTimeUtc().addSecs(-86460).toString(Qt::ISODate) + "\"\n").toUtf8());
        state.close();
        makeSync();
        m_sync->startDaily(10);
        QTRY_VERIFY_WITH_TIMEOUT(!m_server->requestsTo(kOwned).isEmpty(), 3000);
        QTRY_COMPARE(m_sync->state(), SteamLibrarySync::State::Idle);
        QVERIFY(m_sync->lastFetch() > QDateTime::currentDateTimeUtc().addSecs(-60));
    }

    void theSwitchStopsTheFetch()
    {
        withKey();
        QFile settings(data() + QStringLiteral("/settings.toml"));
        QVERIFY(settings.open(QIODevice::WriteOnly));
        settings.write("[steam]\nwhole_library = false\n");
        settings.close();
        m_server->on(kOwned, json({{"games", ownedGames(1)}}));
        makeSync();
        QVERIFY(!m_sync->enabled());
        m_sync->startDaily(10);
        QTest::qWait(150);
        QVERIFY(m_server->requests.isEmpty());
    }

    void heldBySetupFetchesNothing()
    {
        withKey();
        m_server->on(kOwned, json({{"games", ownedGames(1)}}));
        makeSync();
        m_setup.held = [] { return true; };
        delete m_sync;
        m_sync = new SteamLibrarySync(m_setup);
        m_sync->startDaily(10);
        QTest::qWait(150);
        QVERIFY(m_server->requests.isEmpty());
    }

    void fixturesAnswerWithoutTheNetwork()
    {
        const QString fixtures = m_dir.filePath(QStringLiteral("fixtures"));
        QDir().mkpath(fixtures);
        const auto write = [&](const QString &name, const QByteArray &text) {
            QFile f(fixtures + u'/' + name);
            QVERIFY(f.open(QIODevice::WriteOnly));
            f.write(text);
        };
        write(QStringLiteral("signin.json"), R"({"account_name":"fixture user"})");
        write(QStringLiteral("IPlayerService-GetOwnedGames-v1.json"),
              R"({"response":{"games":[{"appid":10,"name":"One","playtime_forever":5},{"appid":11,"name":"Two"}]}})");
        write(QStringLiteral("IStoreBrowseService-GetItems-v1.json"),
              R"({"response":{"store_items":[{"appid":10,"success":1,"type":0},{"appid":11,"success":1,"type":0}]}})");
        makeSync();
        delete m_sync;
        m_setup.fixturesDir = fixtures;
        m_sync = new SteamLibrarySync(m_setup);
        QVERIFY(m_sync->signedIn());
        QCOMPARE(m_sync->accountName(), QStringLiteral("fixture user"));
        m_sync->syncNow();
        QTRY_COMPARE(m_sync->state(), SteamLibrarySync::State::Idle);
        QCOMPARE(m_sync->outcome(), SteamLibrarySync::Outcome::Synced);
        QVERIFY(m_server->requests.isEmpty());
        QCOMPARE(cachedLibrary().owned.size(), 2);
        QCOMPARE(cachedLibrary().types.size(), 2);
        QCOMPARE(m_store->reads.load(), 0);
    }
};

QTEST_GUILESS_MAIN(SteamLibrarySyncTest)

#include "steamlibrarysync_test.moc"
