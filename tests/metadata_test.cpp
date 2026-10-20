#include "bulklookup.h"
#include "credentials.h"
#include "fakehttpserver.h"
#include "igdb.h"
#include "metadataclient.h"
#include "metadatahttp.h"
#include "metadatasettings.h"
#include "scanlookup.h"
#include "steamgriddb.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QSignalSpy>

#include <QDir>
#include <QFile>
#include <QBuffer>
#include <QImage>
#include <QImageReader>
#include <QTemporaryDir>
#include <QTest>

namespace {

bool anyContains(const QStringList &list, const QString &part)
{
    return std::any_of(list.cbegin(), list.cend(), [&](const QString &s) { return s.contains(part); });
}

QString readAll(const QString &path)
{
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? QString::fromUtf8(file.readAll()) : QString();
}

// Readable and writable by its owner, and nothing for anyone else.
bool ownerOnly(const QString &path)
{
    const QFile::Permissions p = QFile::permissions(path);
    const QFile::Permissions others = QFile::ReadGroup | QFile::WriteGroup | QFile::ExeGroup | QFile::ReadOther
        | QFile::WriteOther | QFile::ExeOther;
    return p.testFlags(QFile::ReadOwner | QFile::WriteOwner) && !(p & others);
}

} // namespace

class MetadataTest : public QObject
{
    Q_OBJECT

private slots:
    void credentialsParse();
    void credentialsBadValuesWarn();
    void credentialsMissingFile();
    void credentialsCreateOwnerOnly();
    void credentialsKeepCommentsAndOrder();
    void credentialsEmptyRemovesKey();
    void credentialsReadableByOthersWarns();

    void noNetworkBeforeFirstRequest();
    void igdbTokenFetchedOnce();
    void igdbTokenRenewedAfter401();
    void igdbWrongKeys();
    void errorMessages_data();
    void errorMessages();
    void timeoutAndUnreachable();
    void igdbRateLimit();
    void destroyedContextNotCalled();

    void igdbSearchQuery();
    void igdbPlatforms();
    void igdbStars();
    void igdbParse();
    void igdbYearPerPlatform();
    void igdbClearMatch();
    void igdbFacets();
    void igdbFittingTags();
    void igdbGamesQuery();

    void sgdbSearch();
    void sgdbArtRequest();
    void sgdbImages();
    void downloads();
    void downloadsConvertIcons();

    void clientLookup();
    void clientLookupIds();
    void clientLookupIdsBatches();
    void clientArt();
    void clientCancelAndKeys();

    void bulkChoosing();
    void bulkChoiceCounts();
    void bulkStateCounts();
    void bulkResultFilter();
    void bulkUnmarkNotOnIgdb();
    void bulkLookUp();
    void bulkStopKeepsResults();
    void bulkArtDefaults();
    void bulkApply();
    void bulkArtForGameNotOnIgdb();
    void bulkReplaceExisting();
    void bulkReplaceArt();
    void bulkTags();
    void bulkIncludeComplete();
    void bulkFetchedAhead();
    void bulkFetchedAheadBatches();
    void scanLookup();
    void metadataSettings();
    void idListModel();
};

void MetadataTest::credentialsParse()
{
    const auto r = CredentialsFile::parse(R"(# keys
[igdb]
client_id = "abcd1234"
client_secret = " efgh5678 "

[steamgriddb]
api_key = "ijkl9012"
)",
                                          "credentials.toml");
    QVERIFY(r.ok);
    QVERIFY2(r.warnings.isEmpty(), qPrintable(r.warnings.join(u'\n')));
    QCOMPARE(r.value, (Credentials{"abcd1234", "efgh5678", "ijkl9012"}));
    QVERIFY(r.value.hasIgdb());
    QVERIFY(r.value.hasSteamGridDb());
    QVERIFY(!(Credentials{"abcd1234", "", ""}.hasIgdb()));
}

void MetadataTest::credentialsBadValuesWarn()
{
    const auto r = CredentialsFile::parse(R"([igdb]
client_id = 1234
clientsecret = "x"

[steamgrid]
api_key = "y"

[steamgriddb]
api_key = "ijkl9012"
)",
                                          "credentials.toml");
    QVERIFY(r.ok);
    QCOMPARE(r.value, (Credentials{"", "", "ijkl9012"}));
    QCOMPARE(r.warnings.size(), 3);
    QVERIFY(anyContains(r.warnings, QStringLiteral("credentials.toml:2: client_id must be text in quotes")));
    QVERIFY(anyContains(r.warnings, QStringLiteral("credentials.toml:3: unknown key clientsecret")));
    QVERIFY(anyContains(r.warnings, QStringLiteral("credentials.toml:5: unknown section [steamgrid]")));

    const auto broken = CredentialsFile::parse("[igdb\n", "credentials.toml");
    QVERIFY(!broken.ok);
    QCOMPARE(broken.warnings.size(), 1);
}

void MetadataTest::credentialsMissingFile()
{
    QTemporaryDir dir;
    const auto r = CredentialsFile::load(dir.path());
    QVERIFY(r.ok);
    QVERIFY(r.warnings.isEmpty());
    QCOMPARE(r.value, Credentials());
    // Clearing a key doesn't create the file.
    QVERIFY(CredentialsFile::setValue(dir.path(), "igdb", "client_id", ""));
    QVERIFY(!QFile::exists(CredentialsFile::path(dir.path())));
}

void MetadataTest::credentialsCreateOwnerOnly()
{
    QTemporaryDir dir;
    QVERIFY(CredentialsFile::setValue(dir.path(), "steamgriddb", "api_key", "ijkl9012"));
    const QString file = CredentialsFile::path(dir.path());
    QVERIFY(ownerOnly(file));
    QCOMPARE(readAll(file), QStringLiteral("# API keys for looking up games. Keep this file private: it's readable "
                                           "only by you.\n\n[steamgriddb]\napi_key = \"ijkl9012\"\n"));

    // Still owner-only after the atomic rewrite, even when something widened it.
    QFile::setPermissions(file, QFile::permissions(file) | QFile::ReadGroup | QFile::ReadOther);
    QVERIFY(CredentialsFile::setValue(dir.path(), "igdb", "client_id", "abcd1234"));
    QVERIFY(ownerOnly(file));
    const auto r = CredentialsFile::load(dir.path());
    QVERIFY(r.warnings.isEmpty());
    QCOMPARE(r.value, (Credentials{"abcd1234", "", "ijkl9012"}));
}

void MetadataTest::credentialsKeepCommentsAndOrder()
{
    QTemporaryDir dir;
    const QString file = CredentialsFile::path(dir.path());
    const QByteArray original = R"(# My keys
[igdb]
client_id = "old"   # from dev.twitch.tv
client_secret = "secret"

[steamgriddb]
api_key = "key"
)";
    {
        QFile f(file);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write(original);
    }
    QVERIFY(CredentialsFile::setValue(dir.path(), "igdb", "client_id", " new "));
    QCOMPARE(readAll(file), QString::fromUtf8(original).replace("\"old\"", "\"new\""));
    // An unchanged value doesn't rewrite the file.
    QVERIFY(CredentialsFile::setValue(dir.path(), "igdb", "client_id", "new"));
    QVERIFY(ownerOnly(file));
}

void MetadataTest::credentialsEmptyRemovesKey()
{
    QTemporaryDir dir;
    QVERIFY(CredentialsFile::setValue(dir.path(), "igdb", "client_id", "abcd1234"));
    QVERIFY(CredentialsFile::setValue(dir.path(), "igdb", "client_secret", "efgh5678"));
    QVERIFY(CredentialsFile::setValue(dir.path(), "igdb", "client_id", "  "));
    const QString text = readAll(CredentialsFile::path(dir.path()));
    QVERIFY(!text.contains(QStringLiteral("client_id")));
    QVERIFY(text.contains(QStringLiteral("client_secret = \"efgh5678\"")));
}

void MetadataTest::credentialsReadableByOthersWarns()
{
    QTemporaryDir dir;
    const QString file = CredentialsFile::path(dir.path());
    {
        QFile f(file);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("[steamgriddb]\napi_key = \"key\"\n");
    }
    QFile::setPermissions(file, QFile::ReadOwner | QFile::WriteOwner | QFile::ReadGroup | QFile::ReadOther);
    const auto r = CredentialsFile::load(dir.path());
    QCOMPARE(r.value.steamGridDbKey, QStringLiteral("key"));
    QCOMPARE(r.warnings.size(), 1);
    QVERIFY(anyContains(r.warnings, QStringLiteral("other users can read your API keys")));
}

namespace {

const Credentials kKeys{"client-id", "client-secret", "grid-key"};
const QByteArray kToken1 = R"({"access_token":"token-1","expires_in":5000000,"token_type":"bearer"})";
const QByteArray kToken2 = R"({"access_token":"token-2","expires_in":5000000,"token_type":"bearer"})";

MetadataEndpoints endpoints(const FakeHttpServer &server)
{
    return {server.url(QStringLiteral("/oauth2/token")), server.url(QStringLiteral("/v4")),
            server.url(QStringLiteral("/api/v2"))};
}

// Runs an IGDB query and waits for its answer.
MetadataReply igdbQuery(MetadataHttp &http, const QByteArray &query = "fields id;")
{
    MetadataReply out;
    bool done = false;
    http.igdb(QStringLiteral("games"), query, &http, [&](const MetadataReply &r) {
        out = r;
        done = true;
    });
    [&] { QTRY_VERIFY_WITH_TIMEOUT(done, 5000); }();
    return out;
}

MetadataReply gridQuery(MetadataHttp &http, const QString &path)
{
    MetadataReply out;
    bool done = false;
    http.steamGridDb(path, {}, &http, [&](const MetadataReply &r) {
        out = r;
        done = true;
    });
    [&] { QTRY_VERIFY_WITH_TIMEOUT(done, 5000); }();
    return out;
}

} // namespace

void MetadataTest::noNetworkBeforeFirstRequest()
{
    QTemporaryDir cache;
    MetadataHttp http(cache.path());
    http.setCredentials(kKeys);
    QVERIFY(!http.hasNetworkManager());
}

void MetadataTest::igdbTokenFetchedOnce()
{
    QTemporaryDir cache;
    FakeHttpServer server;
    server.on("/oauth2/token", {200, kToken1});
    server.on("/v4/games", {200, "[{\"id\":1942}]"});
    {
        MetadataHttp http(cache.path(), endpoints(server));
        http.setCredentials(kKeys);
        QCOMPARE(igdbQuery(http).json.array().first().toObject().value("id").toInt(), 1942);
        QVERIFY(igdbQuery(http).ok());
        QCOMPARE(http.tokenFetches(), 1);
    }
    const auto token = server.requestsTo("/oauth2/token");
    QCOMPARE(token.size(), 1);
    QCOMPARE(token.first().method, QByteArray("POST"));
    QVERIFY(token.first().body.contains("client_secret=client-secret"));
    QVERIFY(!token.first().target.contains("secret")); // never in the URL
    const auto games = server.requestsTo("/v4/games");
    QCOMPARE(games.size(), 2);
    QCOMPARE(games.first().headers.value("client-id"), QByteArray("client-id"));
    QCOMPARE(games.first().headers.value("authorization"), QByteArray("Bearer token-1"));
    QCOMPARE(games.first().body, QByteArray("fields id;"));

    // Kept in the cache dir, readable only by the user, and used by the next start.
    QVERIFY(ownerOnly(cache.filePath("igdb-token")));
    MetadataHttp next(cache.path(), endpoints(server));
    next.setCredentials(kKeys);
    QVERIFY(igdbQuery(next).ok());
    QCOMPARE(next.tokenFetches(), 0);

    // Another client ID doesn't use it.
    MetadataHttp other(cache.path(), endpoints(server));
    other.setCredentials({"other-id", "other-secret", ""});
    QVERIFY(igdbQuery(other).ok());
    QCOMPARE(other.tokenFetches(), 1);
}

void MetadataTest::igdbTokenRenewedAfter401()
{
    QTemporaryDir cache;
    FakeHttpServer server;
    server.on("/oauth2/token", {200, kToken1});
    server.on("/oauth2/token", {200, kToken2});
    server.on("/v4/games", {401, "{}"});
    server.on("/v4/games", {200, "[]"});
    MetadataHttp http(cache.path(), endpoints(server));
    http.setCredentials(kKeys);
    const MetadataReply reply = igdbQuery(http);
    QVERIFY2(reply.ok(), qPrintable(reply.error));
    QCOMPARE(http.tokenFetches(), 2);
    const auto games = server.requestsTo("/v4/games");
    QCOMPARE(games.size(), 2);
    QCOMPARE(games.last().headers.value("authorization"), QByteArray("Bearer token-2"));

    // A second 401 in a row is the keys' fault.
    server.clear("/v4/games");
    server.on("/v4/games", {401, "{}"});
    QCOMPARE(igdbQuery(http).error, QStringLiteral("IGDB didn't accept your keys"));
    QCOMPARE(http.tokenFetches(), 3);
}

void MetadataTest::igdbWrongKeys()
{
    QTemporaryDir cache;
    FakeHttpServer server;
    server.on("/oauth2/token", {403, R"({"status":403,"message":"invalid client secret"})"});
    MetadataHttp http(cache.path(), endpoints(server));
    http.setCredentials(kKeys);
    QCOMPARE(igdbQuery(http).error, QStringLiteral("IGDB didn't accept your keys"));
    QVERIFY(server.requestsTo("/v4/games").isEmpty());

    QString error = QStringLiteral("not called");
    http.testIgdb(&http, [&](const QString &e) { error = e; });
    QTRY_COMPARE(error, QStringLiteral("IGDB didn't accept your keys"));

    MetadataHttp noKeys(cache.path(), endpoints(server));
    QCOMPARE(igdbQuery(noKeys).error, QStringLiteral("Add your IGDB keys in Settings → Metadata"));
    QVERIFY(!noKeys.hasNetworkManager());
}

void MetadataTest::errorMessages_data()
{
    QTest::addColumn<int>("status");
    QTest::addColumn<QByteArray>("body");
    QTest::addColumn<QString>("error");
    QTest::newRow("401") << 401 << QByteArray("{}") << "SteamGridDB didn't accept your keys";
    QTest::newRow("429") << 429 << QByteArray("{}") << "SteamGridDB is busy; try again in a minute";
    QTest::newRow("500") << 500 << QByteArray("{}") << "SteamGridDB answered with an error (500)";
    QTest::newRow("malformed JSON") << 200 << QByteArray("{\"success\": tr") << "SteamGridDB sent an answer the launcher couldn't read";
    QTest::newRow("ok") << 200 << QByteArray("{\"success\": true, \"data\": []}") << "";
}

void MetadataTest::errorMessages()
{
    QFETCH(int, status);
    QFETCH(QByteArray, body);
    QFETCH(QString, error);
    QTemporaryDir cache;
    FakeHttpServer server;
    server.on("/api/v2/search/autocomplete/hades", {status, body});
    MetadataHttp http(cache.path(), endpoints(server));
    http.setCredentials(kKeys);
    QCOMPARE(gridQuery(http, "/search/autocomplete/hades").error, error);
    QCOMPARE(server.requests.first().headers.value("authorization"), QByteArray("Bearer grid-key"));
}

void MetadataTest::timeoutAndUnreachable()
{
    QTemporaryDir cache;
    FakeHttpServer server;
    server.on("/api/v2/search/autocomplete/slow", {200, "{}", 2000});
    MetadataHttp http(cache.path(), endpoints(server));
    http.setCredentials(kKeys);
    http.setTimeout(200);
    QCOMPARE(gridQuery(http, "/search/autocomplete/slow").error,
             QStringLiteral("Couldn't reach SteamGridDB (no answer in 0.2 s)"));

    MetadataEndpoints closed = endpoints(server);
    closed.steamGridDb = QUrl(QStringLiteral("http://127.0.0.1:1/api/v2"));
    MetadataHttp unreachable(cache.path(), closed);
    unreachable.setCredentials(kKeys);
    QCOMPARE(gridQuery(unreachable, "/search/autocomplete/hades").error, QStringLiteral("Couldn't reach SteamGridDB"));

    MetadataHttp noKey(cache.path(), endpoints(server));
    QCOMPARE(gridQuery(noKey, "/search/autocomplete/hades").error,
             QStringLiteral("Add your SteamGridDB key in Settings → Metadata"));
}

void MetadataTest::igdbRateLimit()
{
    QTemporaryDir cache;
    FakeHttpServer server;
    server.on("/oauth2/token", {200, kToken1});
    server.on("/v4/games", {200, "[]"});
    MetadataHttp http(cache.path(), endpoints(server));
    http.setCredentials(kKeys);
    http.setIgdbInterval(100);
    int done = 0;
    for (int i = 0; i < 5; ++i)
        http.igdb(QStringLiteral("games"), "fields id;", &http, [&](const MetadataReply &) { ++done; });
    QTRY_COMPARE(done, 5);
    const auto games = server.requestsTo("/v4/games");
    QCOMPARE(games.size(), 5);
    for (qsizetype i = 1; i < games.size(); ++i)
        QVERIFY2(games[i].atMs - games[i - 1].atMs >= 90, qPrintable(QString::number(games[i].atMs - games[i - 1].atMs)));
}

void MetadataTest::destroyedContextNotCalled()
{
    QTemporaryDir cache;
    FakeHttpServer server;
    server.on("/api/v2/search/autocomplete/hades", {200, "{}", 100});
    server.on("/api/v2/search/autocomplete/portal", {200, "{\"success\": true, \"data\": []}"});
    MetadataHttp http(cache.path(), endpoints(server));
    http.setCredentials(kKeys);
    bool called = false;
    auto *context = new QObject;
    http.steamGridDb(QStringLiteral("/search/autocomplete/hades"), {}, context, [&](const MetadataReply &) { called = true; });
    delete context;
    QString error = QStringLiteral("not called");
    http.testSteamGridDb(&http, [&](const QString &e) { error = e; });
    QTRY_COMPARE(error, QString());
    QTest::qWait(200);
    QVERIFY(!called);
}

namespace {

QJsonDocument fixture(const QString &name)
{
    QFile file(QStringLiteral(METADATA_DATA_DIR "/%1.json").arg(name));
    if (!file.open(QIODevice::ReadOnly))
        qFatal("missing fixture %s", qPrintable(name));
    return QJsonDocument::fromJson(file.readAll());
}

int indexOfId(const QList<IgdbGame> &games, int id)
{
    for (qsizetype i = 0; i < games.size(); ++i) {
        if (games.at(i).id == id)
            return int(i);
    }
    return -1;
}

} // namespace

void MetadataTest::igdbStars()
{
    // Members' 0-100 to 0-5 stars, to a tenth; many ratings leave it nearly as is.
    QCOMPARE(Igdb::stars(93, 5000), 4.6);
    QCOMPARE(Igdb::stars(73, 10000), 3.6);
    QCOMPARE(Igdb::stars(0, 10000), 0.0);
    QCOMPARE(Igdb::stars(100, 10000), 5.0);
    // Few ratings pull it toward 3.5: Valheim's 84 on 309 ratings, and 100 on 20.
    QCOMPARE(Igdb::stars(84, 309), 4.1);
    QCOMPARE(Igdb::stars(100, 20), 4.1);
    // Too few ratings to say.
    QCOMPARE(Igdb::stars(100, 19), -1.0);
}

void MetadataTest::igdbSearchQuery()
{
    QCOMPARE(Igdb::searchQuery(QStringLiteral("Mario Tennis Aces"), {130}),
             QByteArray("search \"Mario Tennis Aces\"; where platforms = (130) & version_parent = null & game_type = "
                        "(0,4,8,9,10,11,12); fields name, alternative_names.name, first_release_date, "
                        "release_dates.date, release_dates.platform, release_dates.status, platforms, summary, "
                        "rating, rating_count, cover.image_id, external_games.uid, external_games.external_game_source, genres, "
                        "themes, game_modes, keywords.slug; limit 10;"));
    const QByteArray any = Igdb::searchQuery(QStringLiteral(" The \"Best\"\\ Game  "), {});
    QVERIFY2(any.startsWith("search \"The Best Game\"; where version_parent = null &"), any.constData());
    QVERIFY(Igdb::searchQuery(QStringLiteral("Pokémon"), {37, 137}).contains("search \"Pokémon\"; where platforms = (37,137) &"));
    QVERIFY(Igdb::searchQuery(QStringLiteral("Twilight Princess"), {}, 50).endsWith("keywords.slug; limit 50;"));
    QVERIFY(Igdb::searchQuery(QStringLiteral("Twilight Princess"), {3, 6}, 50).contains("where platforms = (3,6) &"));
    QVERIFY(Igdb::gameQuery(81145).startsWith("where id = 81145; fields name,"));
}

void MetadataTest::igdbPlatforms()
{
    QSet<int> ids;
    QSet<QString> names;
    for (const IgdbPlatform &p : Igdb::platforms()) {
        QVERIFY(p.id > 0);
        QVERIFY(!p.name.isEmpty());
        ids.insert(p.id);
        names.insert(p.name);
    }
    QCOMPARE(ids.size(), Igdb::platforms().size());
    QCOMPARE(names.size(), Igdb::platforms().size());
    QCOMPARE(Igdb::platformName(21), QStringLiteral("GameCube"));
    QCOMPARE(Igdb::platformName(999999), QString());

    // QML's list is sorted by name.
    QTemporaryDir data;
    QTemporaryDir cache;
    MetadataClient client(data.path(), cache.path());
    const QVariantList list = client.igdbPlatforms();
    QCOMPARE(list.size(), Igdb::platforms().size());
    for (qsizetype i = 1; i < list.size(); ++i)
        QVERIFY(list.at(i - 1).toMap().value("name").toString().localeAwareCompare(list.at(i).toMap().value("name").toString()) < 0);
}

void MetadataTest::igdbParse()
{
    QString error;
    const QList<IgdbGame> games = Igdb::parseGames(fixture("igdb-search-mario-tennis-aces"), {130}, &error);
    QVERIFY(error.isEmpty());
    QCOMPARE(games.size(), 1);
    const IgdbGame &g = games.first();
    QCOMPARE(g.id, 81145);
    QCOMPARE(g.name, QStringLiteral("Mario Tennis Aces"));
    QCOMPARE(g.year, 2018);
    QCOMPARE(g.platforms, QList<int>{130});
    QVERIFY(g.summary.startsWith(QStringLiteral("A new Mario Tennis game")));
    QVERIFY(g.alternativeNames.contains(QStringLiteral("Mario Tennis: Aces")));
    QCOMPARE(g.steamAppId, 0);
    QVERIFY(g.coverThumbnail().toString().startsWith(
        QStringLiteral("https://images.igdb.com/igdb/image/upload/t_cover_small/")));
    QVERIFY(g.coverThumbnail().toString().endsWith(QStringLiteral(".jpg")));

    const QList<IgdbGame> hades = Igdb::parseGames(fixture("igdb-search-hades"), {6});
    const int modern = indexOfId(hades, 113112);
    QVERIFY(modern >= 0);
    QCOMPARE(hades.at(modern).steamAppId, 1145360);
    QCOMPARE(hades.at(indexOfId(hades, 80529)).year, 1995);

    QCOMPARE(Igdb::parseGames(fixture("igdb-search-none"), {130}).size(), 0);
    QCOMPARE(Igdb::parseGames(fixture("igdb-game-81145"), {130}).first().id, 81145);

    Igdb::parseGames(QJsonDocument::fromJson("{\"message\": \"x\"}"), {}, &error);
    QCOMPARE(error, QStringLiteral("IGDB sent an answer the launcher couldn't read"));
}

void MetadataTest::igdbYearPerPlatform()
{
    const QJsonDocument json = fixture("igdb-search-hades");
    const auto year = [&](const QList<int> &platforms) {
        const QList<IgdbGame> games = Igdb::parseGames(json, platforms);
        return games.at(indexOfId(games, 113112)).year;
    };
    QCOMPARE(year({6}), 2020); // not the 2018 early access
    QCOMPARE(year({48}), 2021); // PS4
    QCOMPARE(year({130}), 2020);
    QCOMPARE(year({}), 2020); // first_release_date
    QCOMPARE(year({37}), 2020); // not released there: first_release_date
}

void MetadataTest::igdbClearMatch()
{
    const QList<IgdbGame> mario = Igdb::parseGames(fixture("igdb-search-mario-tennis-aces"), {130});
    QCOMPARE(Igdb::clearMatch(mario, QStringLiteral("MARIO TENNIS ACES")), 0);
    QCOMPARE(Igdb::clearMatch(mario, QStringLiteral("Mario Tennis: Aces")), 0); // alternative name
    QCOMPARE(Igdb::clearMatch(mario, QStringLiteral("Mario Tennis")), -1);
    QCOMPARE(Igdb::clearMatch(mario, QString()), -1);

    const QList<IgdbGame> pokemon = Igdb::parseGames(fixture("igdb-search-pokemon-ultra-sun"), {37, 137});
    QCOMPARE(Igdb::clearMatch(pokemon, QStringLiteral("Pokemon Ultra Sun")), 0); // accents folded
    QCOMPARE(Igdb::clearMatch(pokemon, QStringLiteral("Pokémon Ultrasol")), 0);

    const QList<IgdbGame> hades = Igdb::parseGames(fixture("igdb-search-hades"), {6});
    QCOMPARE(Igdb::clearMatch(hades, QStringLiteral("Hades")), -1); // 1995 and 2020
    QCOMPARE(Igdb::clearMatch(hades, QStringLiteral("Hades"), 1145360), indexOfId(hades, 113112));
    QCOMPARE(Igdb::clearMatch(hades, QStringLiteral("Some other title"), 1145360), indexOfId(hades, 113112));
    QCOMPARE(Igdb::clearMatch(hades, QStringLiteral("Hades 2")), -1); // a name and an alternative name
    QCOMPARE(Igdb::clearMatch(hades, QStringLiteral("Hades"), 999), -1);

    const QList<IgdbGame> metroid = Igdb::parseGames(fixture("igdb-search-metroid-dread"), {});
    QCOMPARE(Igdb::clearMatch(metroid, QStringLiteral("Metroid Dread")), -1); // three platforms
    QCOMPARE(Igdb::clearMatch({}, QStringLiteral("Metroid Dread")), -1);
}

void MetadataTest::igdbFacets()
{
    const QList<IgdbGame> hades = Igdb::parseGames(fixture("igdb-search-hades"), {6});
    QCOMPARE(hades.at(indexOfId(hades, 113112)).facets,
             (QStringList{"genre:12", "genre:25", "genre:31", "genre:32", "theme:1", "theme:17", "mode:1",
                          "keyword:roguelike", "keyword:roguelite", "keyword:greek-mythology", "keyword:hack-and-slash"}));
    QVERIFY(hades.at(indexOfId(hades, 80529)).facets.isEmpty()); // none sent
    const QList<IgdbGame> dread = Igdb::parseGames(fixture("igdb-search-metroid-dread"), {});
    QVERIFY(dread.at(indexOfId(dread, 15698)).facets.contains(QStringLiteral("keyword:metroidvania")));

    // Expanded fields (objects with an id) read the same; zeros, and keywords without a slug, don't count.
    const QList<IgdbGame> expanded = Igdb::parseGames(QJsonDocument::fromJson(R"([{"id": 1, "name": "A",
        "genres": [{"id": 8, "name": "Platform"}, 0], "game_modes": [{"id": 2}], "keywords": [{"id": 3}, {"slug": ""}]}])"),
                                                      {});
    QCOMPARE(expanded.first().facets, (QStringList{"genre:8", "mode:2"}));
}

void MetadataTest::igdbFittingTags()
{
    const QList<std::pair<QString, QStringList>> tags{
        {"Roguelike", {"keyword:roguelike", "keyword:roguelite"}},
        {"Platformer", {"genre:8"}},
        {"Horror", {"theme:19"}},
        {"Couch co-op", {"mode:4"}},
        {"Adventure", {}},
    };
    // Each kind.
    QCOMPARE(Igdb::fittingTags({"genre:8"}, tags, {}), QStringList{"Platformer"});
    QCOMPARE(Igdb::fittingTags({"theme:19"}, tags, {}), QStringList{"Horror"});
    QCOMPARE(Igdb::fittingTags({"mode:4"}, tags, {}), QStringList{"Couch co-op"});
    QCOMPARE(Igdb::fittingTags({"keyword:roguelike"}, tags, {}), QStringList{"Roguelike"});
    // A variant fits too; several at once come in the tags' order.
    QCOMPARE(Igdb::fittingTags({"keyword:roguelite"}, tags, {}), QStringList{"Roguelike"});
    QCOMPARE(Igdb::fittingTags({"mode:4", "theme:19", "genre:8", "keyword:roguelite", "keyword:roguelike"}, tags, {}),
             (QStringList{"Roguelike", "Platformer", "Horror", "Couch co-op"}));
    // A tag the game has isn't suggested.
    QCOMPARE(Igdb::fittingTags({"genre:8", "theme:19"}, tags, {"Platformer"}), QStringList{"Horror"});
    // The same id of another kind doesn't fit; a tag with [] never does.
    QVERIFY(Igdb::fittingTags({"theme:8", "genre:19", "genre:31"}, tags, {}).isEmpty());
    QVERIFY(Igdb::fittingTags({}, tags, {}).isEmpty());
    QVERIFY(Igdb::fittingTags({"genre:8"}, {}, {}).isEmpty());
}

void MetadataTest::igdbGamesQuery()
{
    QVERIFY(Igdb::gamesQuery({81145, 113112}).startsWith("where id = (81145,113112); fields name,"));
    QVERIFY(Igdb::gamesQuery({81145}).endsWith("keywords.slug; limit 500;"));

    QList<int> ids;
    for (int i = 1; i <= 1201; ++i)
        ids.append(i);
    const QList<QList<int>> batches = Igdb::batches(ids);
    QCOMPARE(batches.size(), 3);
    QCOMPARE(batches.at(0).size(), 500);
    QCOMPARE(batches.at(1).first(), 501);
    QCOMPARE(batches.at(2).size(), 201);
    QCOMPARE(batches.at(2).last(), 1201);
    QVERIFY(Igdb::batches({}).isEmpty());
}

void MetadataTest::sgdbSearch()
{
    QCOMPARE(SteamGridDb::searchPath(QStringLiteral(" Mario  Tennis Aces ")),
             QStringLiteral("/search/autocomplete/Mario%20Tennis%20Aces"));
    QCOMPARE(SteamGridDb::steamPath(1145360), QStringLiteral("/games/steam/1145360"));

    // The title reaches the server as one path segment, slash and all.
    QTemporaryDir cache;
    FakeHttpServer server;
    MetadataHttp http(cache.path(), endpoints(server));
    http.setCredentials(kKeys);
    gridQuery(http, SteamGridDb::searchPath(QStringLiteral("Pokémon X/Y: 100%")));
    QCOMPARE(server.requests.first().target,
             QByteArray("/api/v2/search/autocomplete/Pok%C3%A9mon%20X%2FY%3A%20100%25"));

    QString error;
    const QList<SgdbGame> mario = SteamGridDb::parseGames(fixture("sgdb-search-mario-tennis-aces"), &error);
    QVERIFY(error.isEmpty());
    QCOMPARE(mario.size(), 10);
    QCOMPARE(mario.first(), (SgdbGame{5254520, "Mario Tennis Aces", 2018}));
    QCOMPARE(SteamGridDb::clearMatch(mario, QStringLiteral("Mario Tennis Aces"), 2018), 0);
    QCOMPARE(SteamGridDb::clearMatch(mario, QStringLiteral("Mario Tennis Aces"), 0), 0);
    QCOMPARE(SteamGridDb::clearMatch(mario, QStringLiteral("Mario Tennis Ace"), 2018), -1);

    const QList<SgdbGame> steam = SteamGridDb::parseGames(fixture("sgdb-steam-1145360"));
    QCOMPARE(steam, (QList<SgdbGame>{{4997889, "Hades", 2019}}));
    QCOMPARE(SteamGridDb::clearMatch(SteamGridDb::parseGames(fixture("sgdb-search-hades")), QStringLiteral("Hades"), 2020), 0);

    // The year only breaks ties.
    const QList<SgdbGame> twins = {{1, "Doom", 1993}, {2, "DOOM", 2016}};
    QCOMPARE(SteamGridDb::clearMatch(twins, QStringLiteral("Doom"), 2016), 1);
    QCOMPARE(SteamGridDb::clearMatch(twins, QStringLiteral("Doom"), 0), -1);
    QCOMPARE(SteamGridDb::clearMatch(twins, QStringLiteral("Doom"), 2020), -1);

    // Sequel numbers match as digits or Roman numerals.
    const QList<SgdbGame> hades = SteamGridDb::parseGames(fixture("sgdb-search-hades"));
    QCOMPARE(hades.at(3).name, QStringLiteral("Hades II"));
    QCOMPARE(SteamGridDb::clearMatch(hades, QStringLiteral("Hades 2"), 0), 3);
    QCOMPARE(SteamGridDb::clearMatch(hades, QStringLiteral("hades ii"), 0), 3);
    QCOMPARE(SteamGridDb::clearMatch(hades, QStringLiteral("Hades 3"), 0), -1);
    QCOMPARE(SteamGridDb::otherNumerals(QStringLiteral("Mortal Shell 2")), QStringLiteral("Mortal Shell II"));
    QCOMPARE(SteamGridDb::otherNumerals(QStringLiteral("Hades II: Early Access")), QStringLiteral("Hades 2: Early Access"));
    QCOMPARE(SteamGridDb::otherNumerals(QStringLiteral("Final Fantasy XIV")), QStringLiteral("Final Fantasy 14"));
    QCOMPARE(SteamGridDb::otherNumerals(QStringLiteral("Mix up")), QString()); // not a numeral
    QCOMPARE(SteamGridDb::otherNumerals(QStringLiteral("I Am Bread")), QString()); // "I" is the word
    QCOMPARE(SteamGridDb::otherNumerals(QStringLiteral("1943")), QString());
    QCOMPARE(SteamGridDb::otherNumerals(QStringLiteral("Hades")), QString());

    SteamGridDb::parseGames(QJsonDocument::fromJson("{\"success\": false, \"errors\": [\"x\"]}"), &error);
    QCOMPARE(error, QStringLiteral("SteamGridDB sent an answer the launcher couldn't read"));
}

void MetadataTest::sgdbArtRequest()
{
    auto [path, query] = SteamGridDb::artRequest(5254520, QStringLiteral("cover"), {}, 0);
    QCOMPARE(path, QStringLiteral("/grids/game/5254520"));
    QCOMPARE(query.toString(QUrl::FullyDecoded),
             QStringLiteral("dimensions=600x900,342x482,660x930&types=static&nsfw=false&humor=false"));
    std::tie(path, query) = SteamGridDb::artRequest(7, QStringLiteral("logo"), QStringLiteral("white"), 2);
    QCOMPARE(path, QStringLiteral("/logos/game/7"));
    QCOMPARE(query.toString(QUrl::FullyDecoded), QStringLiteral("styles=white&types=static&nsfw=false&humor=false&page=2"));
    QCOMPARE(SteamGridDb::artRequest(7, QStringLiteral("hero"), {}, 0).first, QStringLiteral("/heroes/game/7"));
    QCOMPARE(SteamGridDb::artRequest(7, QStringLiteral("icon"), {}, 0).first, QStringLiteral("/icons/game/7"));

    QCOMPARE(SteamGridDb::styles(QStringLiteral("cover")).size(), 5);
    QCOMPARE(SteamGridDb::styles(QStringLiteral("hero")),
             (QStringList{"alternate", "blurred", "material"}));
    QCOMPARE(SteamGridDb::styles(QStringLiteral("logo")), (QStringList{"official", "white", "black", "custom"}));
    QCOMPARE(SteamGridDb::styles(QStringLiteral("icon")), (QStringList{"official", "custom"}));
}

void MetadataTest::sgdbImages()
{
    // SteamGridDB's order is kept: its scores are all 0.
    const QJsonDocument grids = fixture("sgdb-grids-5254520");
    QList<int> order;
    for (const QJsonValue &v : grids.object().value("data").toArray())
        order.append(v.toObject().value("id").toInt());
    QString error;
    const QList<SgdbImage> covers = SteamGridDb::parseImages(grids, QStringLiteral("cover"), &error);
    QVERIFY(error.isEmpty());
    QCOMPARE(covers.size(), 25);
    QList<int> parsed;
    for (const SgdbImage &image : covers)
        parsed.append(image.id);
    QCOMPARE(parsed, order);
    const SgdbImage &first = covers.first();
    QVERIFY(first.url.toString().startsWith(QStringLiteral("https://cdn2.steamgriddb.com/grid/")));
    QVERIFY(first.thumbnail.toString().startsWith(QStringLiteral("https://cdn2.steamgriddb.com/thumb/")));
    QCOMPARE(first.style, QStringLiteral("alternate"));
    QVERIFY(!first.author.isEmpty());

    QCOMPARE(SteamGridDb::parseImages(fixture("sgdb-heroes-5254520"), QStringLiteral("hero")).size(), 6);
    QCOMPARE(SteamGridDb::parseImages(fixture("sgdb-logos-5254520"), QStringLiteral("logo")).size(), 4);
    const QList<SgdbImage> icons = SteamGridDb::parseImages(fixture("sgdb-icons-5254520"), QStringLiteral("icon"));
    QCOMPARE(icons.size(), 2);
    QVERIFY(std::any_of(icons.cbegin(), icons.cend(), [](const SgdbImage &i) { return i.mime == "image/vnd.microsoft.icon"; }));

    // Paging: full pages, then an empty one.
    QCOMPARE(SteamGridDb::parseImages(fixture("sgdb-grids-4997889-page0"), QStringLiteral("cover")).size(), 50);
    QCOMPARE(SteamGridDb::parseImages(fixture("sgdb-grids-4997889-page4"), QStringLiteral("cover")).size(), 49);
    QVERIFY(SteamGridDb::parseImages(fixture("sgdb-grids-4997889-page5"), QStringLiteral("cover"), &error).isEmpty());
    QVERIFY(error.isEmpty());

    // Whatever slips through the API's filters is dropped here.
    const QJsonDocument mixed = QJsonDocument::fromJson(R"({"success": true, "data": [
        {"id": 1, "url": "https://x/1.png", "width": 600, "height": 900, "mime": "image/png"},
        {"id": 2, "url": "https://x/2.png", "width": 600, "height": 900, "mime": "image/png", "nsfw": true},
        {"id": 3, "url": "https://x/3.png", "width": 600, "height": 900, "mime": "image/png", "humor": true},
        {"id": 4, "url": "https://x/4.gif", "width": 600, "height": 900, "mime": "image/gif"},
        {"id": 5, "url": "https://x/5.webp", "width": 600, "height": 900, "mime": "image/webp"},
        {"id": 6, "url": "https://x/6.png", "width": 920, "height": 430, "mime": "image/png"},
        {"id": 7, "url": "https://x/7.png", "width": 342, "height": 482, "mime": "image/png"}]})");
    QList<int> kept;
    for (const SgdbImage &image : SteamGridDb::parseImages(mixed, QStringLiteral("cover")))
        kept.append(image.id);
    QCOMPARE(kept, (QList<int>{1, 7}));
    QCOMPARE(SteamGridDb::parseImages(mixed, QStringLiteral("logo")).size(), 3); // any size

    SteamGridDb::parseImages(QJsonDocument::fromJson("{\"success\": true, \"data\": {}}"), QStringLiteral("cover"), &error);
    QCOMPARE(error, QStringLiteral("SteamGridDB sent an answer the launcher couldn't read"));
}

void MetadataTest::downloads()
{
    QTemporaryDir cache;
    FakeHttpServer server;
    server.on("/grid/abc.png", {200, "not really a png"});
    MetadataHttp http(cache.path(), endpoints(server));
    QString path;
    QString error = QStringLiteral("not called");
    http.download(server.url(QStringLiteral("/grid/abc.png")), QStringLiteral("cover-1.png"), &http,
                  [&](const QString &p, const QString &e) {
                      path = p;
                      error = e;
                  });
    QTRY_COMPARE(error, QString());
    QCOMPARE(path, QDir(http.downloadsDir()).filePath(QStringLiteral("cover-1.png")));
    QCOMPARE(readAll(path), QStringLiteral("not really a png"));
    // No key needed: the images are on SteamGridDB's CDN.
    QVERIFY(!server.requests.first().headers.contains("authorization"));

    error = QStringLiteral("not called");
    http.download(server.url(QStringLiteral("/grid/missing.png")), QStringLiteral("hero-2.png"), &http,
                  [&](const QString &, const QString &e) { error = e; });
    QTRY_COMPARE(error, QStringLiteral("SteamGridDB answered with an error (404)"));
    QVERIFY(!QFile::exists(QDir(http.downloadsDir()).filePath(QStringLiteral("hero-2.png"))));

    http.removeDownloads();
    QVERIFY(!QFile::exists(path));
}

void MetadataTest::downloadsConvertIcons()
{
    QByteArray ico;
    {
        QBuffer buffer(&ico);
        buffer.open(QIODevice::WriteOnly);
        QImage image(64, 64, QImage::Format_ARGB32);
        image.fill(Qt::red);
        QVERIFY(image.save(&buffer, "ICO"));
    }
    QTemporaryDir cache;
    FakeHttpServer server;
    server.on("/icon/a.ico", {200, ico});
    server.on("/icon/broken.ico", {200, "not an image"});
    MetadataHttp http(cache.path(), endpoints(server));
    QString path;
    QString error = QStringLiteral("not called");
    http.download(server.url(QStringLiteral("/icon/a.ico")), QStringLiteral("icon-1.ico"), &http,
                  [&](const QString &p, const QString &e) {
                      path = p;
                      error = e;
                  });
    QTRY_COMPARE(error, QString());
    // The library takes png, jpg and webp art.
    QVERIFY(path.endsWith(QStringLiteral("icon-1.png")));
    QImageReader reader(path);
    QCOMPARE(reader.format(), QByteArray("png"));
    QCOMPARE(reader.size(), QSize(64, 64));

    error = QStringLiteral("not called");
    http.download(server.url(QStringLiteral("/icon/broken.ico")), QStringLiteral("icon-2.ico"), &http,
                  [&](const QString &, const QString &e) { error = e; });
    QTRY_COMPARE(error, QStringLiteral("SteamGridDB sent an image the launcher can't read"));
}

namespace {

// Waits for request `id`'s result.
QVariantMap result(MetadataClient &client, int id)
{
    QVariantMap out;
    bool done = false;
    const auto connection = QObject::connect(&client, &MetadataClient::finished, &client,
                                             [&](int request, const QVariantMap &r) {
                                                 if (request == id) {
                                                     out = r;
                                                     done = true;
                                                 }
                                             });
    [&] { QTRY_VERIFY_WITH_TIMEOUT(done, 5000); }();
    QObject::disconnect(connection);
    return out;
}

} // namespace

void MetadataTest::clientLookup()
{
    QTemporaryDir data;
    QTemporaryDir cache;
    MetadataClient client(data.path(), cache.path());
    client.http()->setFixturesDir(QStringLiteral(METADATA_DATA_DIR));
    client.http()->setFixtureDelay(0);

    QVariantMap r = result(client, client.lookup(QStringLiteral("Mario Tennis Aces"), {130}));
    QCOMPARE(r.value("error").toString(), QString());
    QCOMPARE(r.value("match").toInt(), 0);
    const QVariantMap mario = r.value("candidates").toList().first().toMap();
    QCOMPARE(mario.value("igdb").toInt(), 81145);
    QCOMPARE(mario.value("name").toString(), QStringLiteral("Mario Tennis Aces"));
    QCOMPARE(mario.value("year").toInt(), 2018);
    QCOMPARE(mario.value("platforms").toStringList(), QStringList{"Switch"});
    QVERIFY(!mario.value("summary").toString().isEmpty());
    // Fixtures: the cover thumbnail is a placeholder file.
    const QUrl cover(mario.value("cover").toString());
    QVERIFY(cover.isLocalFile());
    QVERIFY(!QImage(cover.toLocalFile()).isNull());

    r = result(client, client.lookup(QStringLiteral("Hades"), {6}));
    QCOMPARE(r.value("candidates").toList().size(), 10);
    QCOMPARE(r.value("match").toInt(), -1);
    r = result(client, client.lookup(QStringLiteral("Hades"), {6}, 0, 1145360));
    const QVariantMap hades = r.value("candidates").toList().at(r.value("match").toInt()).toMap();
    QCOMPARE(hades.value("igdb").toInt(), 113112);
    QVERIFY(hades.value("facets").toStringList().contains(QStringLiteral("keyword:roguelike")));

    // A remembered id goes straight to the game.
    r = result(client, client.lookup(QStringLiteral("Whatever the title says"), {130}, 81145));
    QCOMPARE(r.value("match").toInt(), 0);
    QCOMPARE(r.value("candidates").toList().first().toMap().value("igdb").toInt(), 81145);

    r = result(client, client.lookup(QStringLiteral("Not A Real Game"), {130}));
    QCOMPARE(r.value("error").toString(), QString());
    QVERIFY(r.value("candidates").toList().isEmpty());
    QCOMPARE(r.value("match").toInt(), -1);

    // A game on other platforms: none on Linux and PC (that answer's own file), found on
    // every platform (the file without ids).
    r = result(client, client.lookup(QStringLiteral("Twilight Princess"), {3, 6}, 0, 0, 50));
    QVERIFY(r.value("candidates").toList().isEmpty());
    r = result(client, client.lookup(QStringLiteral("Twilight Princess"), {}, 0, 0, 50));
    QCOMPARE(r.value("candidates").toList().size(), 2);
    QCOMPARE(r.value("match").toInt(), -1);
    const QVariantMap tp = r.value("candidates").toList().first().toMap();
    QCOMPARE(tp.value("igdb").toInt(), 1030);
    QCOMPARE(tp.value("platforms").toStringList(), (QStringList{"Wii", "GameCube"}));
    QCOMPARE(tp.value("year").toInt(), 2006);
    QVERIFY(QUrl(tp.value("coverLarge").toString()).isLocalFile());
    QVERIFY(tp.value("coverLarge").toString() != tp.value("cover").toString());

    QVERIFY(!client.http()->hasNetworkManager());
}

void MetadataTest::clientLookupIds()
{
    QTemporaryDir data;
    QTemporaryDir cache;
    MetadataClient client(data.path(), cache.path());
    client.http()->setFixturesDir(QStringLiteral(METADATA_DATA_DIR));
    client.http()->setFixtureDelay(0);

    // The fixtures answer several ids with their files' games; 42 has none.
    QVariantMap r = result(client, client.lookupIds({{81145, {130}}, {113112, {48}}, {15698, {130}}, {42, {}}}));
    QCOMPARE(r.value("error").toString(), QString());
    const QVariantMap games = r.value("games").toMap();
    QCOMPARE(games.keys(), (QStringList{"113112", "15698", "81145"}));
    const QVariantMap hades = games.value("113112").toMap();
    QCOMPARE(hades.value("name").toString(), QStringLiteral("Hades"));
    QCOMPARE(hades.value("year").toInt(), 2021); // on PS4, its own platform
    QVERIFY(hades.value("facets").toStringList().contains(QStringLiteral("keyword:roguelite")));
    QVERIFY(games.value("15698").toMap().value("facets").toStringList().contains(QStringLiteral("genre:8")));
    QCOMPARE(games.value("81145").toMap().value("facets").toStringList().first(), QStringLiteral("genre:14"));

    r = result(client, client.lookupIds({}));
    QCOMPARE(r.value("error").toString(), QString());
    QVERIFY(r.value("games").toMap().isEmpty());
}

void MetadataTest::clientLookupIdsBatches()
{
    QTemporaryDir data;
    QTemporaryDir cache;
    FakeHttpServer server;
    server.on("/oauth2/token", {200, kToken1});
    server.on("/v4/games", {200, R"([{"id": 2, "name": "Two", "genres": [8]}, {"id": 999999, "name": "Not asked"}])"});
    MetadataClient client(data.path(), cache.path(), endpoints(server));
    client.http()->setCredentials(kKeys);
    client.http()->setIgdbInterval(100);

    // 1201 games: three requests, one per 500, kept to the rate limit.
    QHash<int, QList<int>> platforms;
    for (int id = 1; id <= 1201; ++id)
        platforms.insert(id, {});
    QVariantMap r = result(client, client.lookupIds(platforms));
    QCOMPARE(r.value("error").toString(), QString());
    const auto games = server.requestsTo("/v4/games");
    QCOMPARE(games.size(), 3);
    QVERIFY(games.at(0).body.startsWith("where id = (1,2,3,"));
    QVERIFY(games.at(0).body.contains(",500); fields"));
    QVERIFY(games.at(1).body.startsWith("where id = (501,"));
    QVERIFY(games.at(2).body.startsWith("where id = (1001,"));
    QVERIFY(games.at(2).body.endsWith(",1201); " + Igdb::gamesQuery({}).mid(Igdb::gamesQuery({}).indexOf("fields"))));
    for (qsizetype i = 1; i < games.size(); ++i)
        QVERIFY2(games[i].atMs - games[i - 1].atMs >= 90, qPrintable(QString::number(games[i].atMs - games[i - 1].atMs)));
    // The games asked for that IGDB sent; one it wasn't asked for is left out.
    const QVariantMap found = r.value("games").toMap();
    QCOMPARE(found.keys(), QStringList{"2"});
    QCOMPARE(found.value("2").toMap().value("facets").toStringList(), QStringList{"genre:8"});

    // One batch of two failing fails the lookup.
    server.clear("/v4/games");
    server.on("/v4/games", {200, "[]"});
    server.on("/v4/games", {500, "{}"});
    platforms.removeIf([](auto it) { return it.key() > 600; });
    r = result(client, client.lookupIds(platforms));
    QVERIFY(!r.value("error").toString().isEmpty());
    QVERIFY(!r.contains("games"));
}

void MetadataTest::clientArt()
{
    QTemporaryDir data;
    QTemporaryDir cache;
    MetadataClient client(data.path(), cache.path());
    client.http()->setFixturesDir(QStringLiteral(METADATA_DATA_DIR));
    client.http()->setFixtureDelay(0);

    QVariantMap r = result(client, client.findArt(QStringLiteral("Mario Tennis Aces"), 2018));
    QCOMPARE(r.value("steamgriddb").toInt(), 5254520);
    QCOMPARE(r.value("name").toString(), QStringLiteral("Mario Tennis Aces"));
    QCOMPARE(result(client, client.findArt(QStringLiteral("Hades"), 2020, 1145360)).value("steamgriddb").toInt(), 4997889);
    // SteamGridDB doesn't know the app: the title is tried.
    QCOMPARE(result(client, client.findArt(QStringLiteral("Hades"), 2020, 999)).value("steamgriddb").toInt(), 4997889);
    r = result(client, client.findArt(QStringLiteral("Mario Tennis"), 0));
    QCOMPARE(r.value("steamgriddb").toInt(), 0);
    // Nothing for "Mortal Shell 2": "Mortal Shell II" is searched, and it's the match.
    r = result(client, client.findArt(QStringLiteral("Mortal Shell 2"), 2026));
    QCOMPARE(r.value("steamgriddb").toInt(), 5491529);
    QCOMPARE(r.value("name").toString(), QStringLiteral("Mortal Shell II"));

    r = result(client, client.artChoices(5254520, QStringLiteral("cover"), {}, 0));
    const QVariantList covers = r.value("images").toList();
    QCOMPARE(covers.size(), 25);
    QVERIFY(r.value("end").toBool()); // 25 of at most 50: the last page
    QVERIFY(!result(client, client.artChoices(4997889, QStringLiteral("cover"), {}, 0)).value("end").toBool());
    QVERIFY(result(client, client.artChoices(4997889, QStringLiteral("cover"), {}, 4)).value("end").toBool()); // 49
    const QVariantMap first = covers.first().toMap();
    QVERIFY(first.value("url").toString().startsWith(QStringLiteral("https://cdn2.steamgriddb.com/grid/")));
    QVERIFY(QUrl(first.value("thumbnail").toString()).isLocalFile());
    QCOMPARE(first.value("width").toInt(), 600);
    QVERIFY(result(client, client.artChoices(5254520, QStringLiteral("cover"), {}, 1)).value("end").toBool()); // empty

    const QVariantList material =
        result(client, client.artChoices(4997889, QStringLiteral("cover"), QStringLiteral("material"), 0)).value("images").toList();
    QVERIFY(!material.isEmpty());
    for (const QVariant &image : material)
        QCOMPARE(image.toMap().value("style").toString(), QStringLiteral("material"));

    r = result(client, client.download(first.value("url").toString(), QStringLiteral("cover")));
    const QString path = QUrl(r.value("path").toString()).toLocalFile();
    QVERIFY(path.startsWith(client.http()->downloadsDir()));
    QVERIFY(path.endsWith(QStringLiteral(".png")));
    QCOMPARE(QImage(path).size(), QSize(300, 450));

    QCOMPARE(client.styles(QStringLiteral("hero")).first().toMap().value("name").toString(), QStringLiteral("Alternate"));
    QVERIFY(!client.http()->hasNetworkManager());
}

void MetadataTest::clientCancelAndKeys()
{
    QTemporaryDir data;
    QTemporaryDir cache;
    MetadataClient client(data.path(), cache.path());
    client.http()->setFixturesDir(QStringLiteral(METADATA_DATA_DIR));
    client.http()->setFixtureDelay(0);

    int finished = 0;
    connect(&client, &MetadataClient::finished, &client, [&] { ++finished; });
    client.cancel(client.lookup(QStringLiteral("Mario Tennis Aces"), {130}));
    const int kept = client.lookup(QStringLiteral("Mario Tennis Aces"), {130});
    result(client, kept);
    QTest::qWait(100);
    QCOMPARE(finished, 1);

    // Keys: written to credentials.toml and read back.
    MetadataClient real(data.path(), cache.path());
    QVERIFY(!real.hasSteamGridDbKey());
    int changes = 0;
    connect(&real, &MetadataClient::keysChanged, &real, [&] { ++changes; });
    QCOMPARE(real.setKey(QStringLiteral("steamgriddb"), QStringLiteral("api_key"), QStringLiteral("grid-key")), QString());
    QVERIFY(real.hasSteamGridDbKey());
    QCOMPARE(real.keys().value("steamGridDbKey").toString(), QStringLiteral("grid-key"));
    QCOMPARE(changes, 1);
    QCOMPARE(real.setKey(QStringLiteral("steamgriddb"), QStringLiteral("api_key"), QString()), QString());
    QVERIFY(!real.hasSteamGridDbKey());
    QCOMPARE(real.http()->credentials(), Credentials());

    // Without keys, a lookup says where to add them, and nothing reaches the network.
    const QVariantMap r = result(real, real.lookup(QStringLiteral("Mario Tennis Aces"), {130}));
    QCOMPARE(r.value("error").toString(), QStringLiteral("Add your IGDB keys in Settings → Metadata"));
    QVERIFY(!real.http()->hasNetworkManager());

    // Leftover downloads are removed when the client starts.
    QDir().mkpath(real.http()->downloadsDir());
    QFile leftover(QDir(real.http()->downloadsDir()).filePath(QStringLiteral("cover-x.png")));
    QVERIFY(leftover.open(QIODevice::WriteOnly));
    leftover.close();
    MetadataClient next(data.path(), cache.path());
    QVERIFY(!leftover.exists());
}

namespace {

// The games of the bulk tests: what the canned answers know, and some they don't.
QList<LookupGame> bulkGames()
{
    LookupGame mario;
    mario.id = QStringLiteral("switch-mario-tennis-aces");
    mario.title = QStringLiteral("Mario Tennis Aces");
    mario.platform = QStringLiteral("switch");
    mario.platformName = QStringLiteral("Switch");
    mario.tags = {QStringLiteral("Party")};
    mario.igdbPlatforms = {130};

    LookupGame hades;
    hades.id = QStringLiteral("steam-1145360");
    hades.title = QStringLiteral("Hades");
    hades.platform = QStringLiteral("steam");
    hades.platformName = QStringLiteral("Steam");
    hades.hasDescription = true;
    hades.hasYear = true;
    hades.art = {QStringLiteral("cover")};
    hades.tags = {QStringLiteral("Roguelike")};
    hades.steamAppId = 1145360;
    hades.igdbPlatforms = {6};

    LookupGame metroid;
    metroid.id = QStringLiteral("switch-metroid-dread");
    metroid.title = QStringLiteral("Metroid Dread");
    metroid.platform = QStringLiteral("switch");
    metroid.platformName = QStringLiteral("Switch");
    metroid.hasDescription = true;
    metroid.art = {QStringLiteral("cover"), QStringLiteral("hero"), QStringLiteral("logo"), QStringLiteral("icon")};
    metroid.igdbPlatforms = {130};

    LookupGame homebrew = metroid;
    homebrew.id = QStringLiteral("switch-homebrew");
    homebrew.title = QStringLiteral("Qwzx Homebrew Thing");
    homebrew.hasDescription = false;
    homebrew.hasYear = true;

    LookupGame skipped = homebrew;
    skipped.id = QStringLiteral("3ds-not-on-igdb");
    skipped.platform = QStringLiteral("3ds");
    skipped.platformName = QStringLiteral("3DS");
    skipped.igdb = 0;

    LookupGame complete = metroid;
    complete.id = QStringLiteral("switch-complete");
    complete.hasYear = true;
    complete.hasRating = true;
    complete.rating = 4;

    return {mario, hades, metroid, homebrew, skipped, complete};
}

QStringList shownIds(const BulkLookup &bulk)
{
    QStringList out;
    for (const QVariant &g : bulk.games())
        out.append(g.toMap().value("id").toString());
    return out;
}

QVariantMap resultFor(const BulkLookup &bulk, const QString &id)
{
    for (const QVariant &r : bulk.results()) {
        if (r.toMap().value("id").toString() == id)
            return r.toMap();
    }
    return {};
}

QVariantMap artOf(const QVariantMap &result, const QString &kind)
{
    for (const QVariant &a : result.value("art").toList()) {
        if (a.toMap().value("kind").toString() == kind)
            return a.toMap();
    }
    return {};
}

// A fixtures client, the bulk games, and writers that record what they're given.
struct BulkSetup
{
    QTemporaryDir data;
    QTemporaryDir cache;
    MetadataClient client{data.path(), cache.path()};
    QList<LookupGame> games = bulkGames();
    QList<std::pair<QString, QVariantMap>> written;
    QList<std::array<QString, 3>> art;
    QHash<QString, QString> failValues; // id → error the writer answers
    QList<std::pair<QString, QStringList>> tagMappings; // none: no tags suggested
    int valueBatches = 0;
    int artBatches = 0;
    BulkLookup bulk{&client, [this] { return games; },
                    [this](const QList<std::pair<QString, QVariantMap>> &g) {
                        ++valueBatches;
                        written += g;
                        QHash<QString, QString> errors;
                        for (const auto &[id, values] : g) {
                            if (failValues.contains(id))
                                errors.insert(id, failValues.value(id));
                        }
                        return errors;
                    },
                    [this](const QList<std::array<QString, 3>> &a) {
                        ++artBatches;
                        art += a;
                        return QHash<QString, QString>();
                    },
                    [this] { return tagMappings; }};

    BulkSetup()
    {
        client.http()->setFixturesDir(QStringLiteral(METADATA_DATA_DIR));
        client.http()->setFixtureDelay(0);
        bulk.reload(); // with the fixtures' keys
    }
};

} // namespace

void MetadataTest::bulkChoosing()
{
    BulkSetup s;
    BulkLookup &bulk = s.bulk;
    // Missing something, and not marked as not on IGDB.
    QCOMPARE(shownIds(bulk), (QStringList{"switch-mario-tennis-aces", "steam-1145360", "switch-metroid-dread", "switch-homebrew"}));
    QCOMPARE(bulk.missingCount(), 4);
    QCOMPARE(bulk.games().first().toMap().value("missing").toStringList(),
             (QStringList{"description", "year", "rating", "cover", "hero", "logo", "icon"}));
    bulk.setIncludeNotOnIgdb(true);
    QCOMPARE(bulk.games().size(), 5);
    bulk.setIncludeNotOnIgdb(false);

    // Each filter alone…
    bulk.setPlatform(QStringLiteral("switch"));
    QCOMPARE(shownIds(bulk), (QStringList{"switch-mario-tennis-aces", "switch-metroid-dread", "switch-homebrew"}));
    bulk.setPlatform({});
    bulk.setMissing({QStringLiteral("year")});
    QCOMPARE(shownIds(bulk), (QStringList{"switch-mario-tennis-aces", "switch-metroid-dread"}));
    bulk.setMissing({QStringLiteral("art")});
    QCOMPARE(shownIds(bulk), (QStringList{"switch-mario-tennis-aces", "steam-1145360"}));
    bulk.setMissing({QStringLiteral("description"), QStringLiteral("year"), QStringLiteral("art")});
    bulk.setTags({QStringLiteral("Party"), QStringLiteral("Roguelike")});
    QCOMPARE(shownIds(bulk), (QStringList{"switch-mario-tennis-aces", "steam-1145360"}));
    bulk.setTags({});
    bulk.setSearch(QStringLiteral("HADES"));
    QCOMPARE(shownIds(bulk), QStringList{"steam-1145360"});
    bulk.setSearch({});
    // …and together.
    bulk.setPlatform(QStringLiteral("switch"));
    bulk.setMissing({QStringLiteral("art")});
    QCOMPARE(shownIds(bulk), QStringList{"switch-mario-tennis-aces"});
    bulk.clearFilters();
    QCOMPARE(bulk.games().size(), 4);

    QCOMPARE(bulk.checkedCount(), 4);
    bulk.setChecked(QStringLiteral("switch-homebrew"), false);
    QCOMPARE(bulk.checkedCount(), 3);
    bulk.setPlatform(QStringLiteral("steam"));
    bulk.setAllChecked(false);
    QCOMPARE(bulk.checkedCount(), 0);
    bulk.clearFilters();
    QCOMPARE(bulk.checkedCount(), 2); // Mario Tennis and Metroid Dread
    QCOMPARE(bulk.platforms().size(), 3);
    QCOMPARE(bulk.allTags(), (QStringList{"Party", "Roguelike"}));
}

namespace {

QMap<QString, int> platformCounts(const BulkLookup &bulk)
{
    QMap<QString, int> out;
    for (const QVariant &p : bulk.platforms())
        out.insert(p.toMap().value("id").toString(), p.toMap().value("count").toInt());
    return out;
}

QMap<QString, int> tagCountsOf(const BulkLookup &bulk)
{
    QMap<QString, int> out;
    const QVariantMap counts = bulk.tagCounts();
    for (auto it = counts.cbegin(); it != counts.cend(); ++it)
        out.insert(it.key(), it.value().toInt());
    return out;
}

int stateCount(const BulkLookup &bulk, const QString &state)
{
    return bulk.stateCounts().value(state).toInt();
}

// The pick row's candidate on the Switch.
int switchCandidate(const BulkLookup &bulk)
{
    const QVariantList candidates = resultFor(bulk, "switch-metroid-dread").value("candidates").toList();
    return int(std::find_if(candidates.cbegin(), candidates.cend(),
                            [](const QVariant &c) { return c.toMap().value("igdb").toInt() == 15698; })
               - candidates.cbegin());
}

} // namespace

void MetadataTest::bulkChoiceCounts()
{
    BulkSetup s;
    BulkLookup &bulk = s.bulk;
    using Counts = QMap<QString, int>;
    QCOMPARE(platformCounts(bulk), (Counts{{"switch", 3}, {"steam", 1}, {"3ds", 0}}));
    QCOMPARE(tagCountsOf(bulk), (Counts{{"Party", 1}, {"Roguelike", 1}}));
    QCOMPARE(bulk.notOnIgdbCount(), 1);

    // A section's counts follow the other sections, not their own.
    bulk.setPlatform(QStringLiteral("steam"));
    QCOMPARE(platformCounts(bulk), (Counts{{"switch", 3}, {"steam", 1}, {"3ds", 0}}));
    QCOMPARE(tagCountsOf(bulk), (Counts{{"Party", 0}, {"Roguelike", 1}}));
    QCOMPARE(bulk.notOnIgdbCount(), 0); // the one not on IGDB is a 3DS game
    bulk.setPlatform({});
    bulk.setTags({QStringLiteral("Party")});
    QCOMPARE(platformCounts(bulk), (Counts{{"switch", 1}, {"steam", 0}, {"3ds", 0}}));
    QCOMPARE(tagCountsOf(bulk), (Counts{{"Party", 1}, {"Roguelike", 1}}));
    QCOMPARE(bulk.notOnIgdbCount(), 0);
    bulk.setTags({});

    bulk.setMissing({QStringLiteral("art")});
    QCOMPARE(platformCounts(bulk), (Counts{{"switch", 1}, {"steam", 1}, {"3ds", 0}}));
    bulk.setMissing({QStringLiteral("year")});
    QCOMPARE(platformCounts(bulk), (Counts{{"switch", 2}, {"steam", 0}, {"3ds", 0}}));
    bulk.clearFilters();

    // The search narrows the list, not the numbers beside the choices.
    bulk.setSearch(QStringLiteral("hades"));
    QCOMPARE(bulk.games().size(), 1);
    QCOMPARE(platformCounts(bulk), (Counts{{"switch", 3}, {"steam", 1}, {"3ds", 0}}));
    QCOMPARE(tagCountsOf(bulk), (Counts{{"Party", 1}, {"Roguelike", 1}}));
    bulk.setSearch({});

    // Not on IGDB: listed on request, and still counted when it isn't.
    bulk.setIncludeNotOnIgdb(true);
    QCOMPARE(platformCounts(bulk), (Counts{{"switch", 3}, {"steam", 1}, {"3ds", 1}}));
    QCOMPARE(bulk.notOnIgdbCount(), 1);
    bulk.setPlatform(QStringLiteral("switch"));
    QCOMPARE(bulk.notOnIgdbCount(), 0);
    bulk.clearFilters();

    // Games that aren't missing anything, with "All games" or "Replace existing".
    bulk.setIncludeComplete(true);
    QCOMPARE(platformCounts(bulk), (Counts{{"switch", 4}, {"steam", 1}, {"3ds", 0}}));
    bulk.setIncludeComplete(false);
    bulk.setReplaceExisting(true);
    QCOMPARE(platformCounts(bulk), (Counts{{"switch", 4}, {"steam", 1}, {"3ds", 0}}));
    bulk.setMissing({QStringLiteral("art")});
    QCOMPARE(platformCounts(bulk), (Counts{{"switch", 4}, {"steam", 1}, {"3ds", 0}})); // art is replaced too
}

void MetadataTest::bulkStateCounts()
{
    BulkSetup s;
    BulkLookup &bulk = s.bulk;
    QCOMPARE(stateCount(bulk, "all"), 0);
    bulk.start();
    QTRY_VERIFY(!bulk.running());
    QCOMPARE(stateCount(bulk, "all"), 4);
    QCOMPARE(stateCount(bulk, "matched"), 2);
    QCOMPARE(stateCount(bulk, "pick"), 1);
    QCOMPARE(stateCount(bulk, "notFound"), 1);
    QCOMPARE(stateCount(bulk, "error"), 0);
    QCOMPARE(stateCount(bulk, "notOnIgdb"), 0);
    QCOMPARE(stateCount(bulk, "running"), 0);

    bulk.pick("switch-metroid-dread", switchCandidate(bulk));
    QTRY_COMPARE(stateCount(bulk, "matched"), 3);
    QCOMPARE(stateCount(bulk, "pick"), 0);

    bulk.retry("switch-homebrew", QStringLiteral("Mario Tennis Aces"));
    QCOMPARE(stateCount(bulk, "running"), 1);
    QCOMPARE(stateCount(bulk, "notFound"), 0);
    QTRY_COMPARE(stateCount(bulk, "matched"), 4);

    bulk.markNotOnIgdb(QStringLiteral("switch-homebrew"));
    QCOMPARE(stateCount(bulk, "matched"), 3);
    QCOMPARE(stateCount(bulk, "notOnIgdb"), 1);
    QCOMPARE(stateCount(bulk, "all"), 4);
    bulk.unmarkNotOnIgdb(QStringLiteral("switch-homebrew"));
    QTRY_COMPARE(stateCount(bulk, "matched"), 4);
    QCOMPARE(stateCount(bulk, "notOnIgdb"), 0);
}

void MetadataTest::bulkResultFilter()
{
    BulkSetup s;
    BulkLookup &bulk = s.bulk;
    QSignalSpy changed(&bulk, &BulkLookup::resultFilterChanged);
    bulk.start();
    QTRY_VERIFY(!bulk.running());
    QCOMPARE(bulk.resultModel()->count(), 4);

    // The model follows the filter; the results, the counts and what Apply writes don't.
    bulk.setResultFilter(QStringLiteral("matched"));
    QCOMPARE(changed.size(), 1);
    QCOMPARE(bulk.resultModel()->count(), 2);
    bulk.setResultFilter(QStringLiteral("pick"));
    QCOMPARE(bulk.resultModel()->count(), 1);
    QCOMPARE(bulk.resultModel()->data(bulk.resultModel()->index(0), IdListModel::IdRole).toString(),
             QStringLiteral("switch-metroid-dread"));
    QCOMPARE(bulk.results().size(), 4);
    QCOMPARE(bulk.applyCount(), 2);
    QCOMPARE(stateCount(bulk, "all"), 4);
    QCOMPARE(stateCount(bulk, "matched"), 2);

    // A result that changes state leaves the filtered list, one that arrives joins it.
    bulk.pick("switch-metroid-dread", switchCandidate(bulk));
    QCOMPARE(bulk.resultModel()->count(), 0);
    bulk.setResultFilter(QStringLiteral("matched"));
    QTRY_COMPARE(bulk.resultModel()->count(), 3);
    bulk.setResultFilter({});
    QCOMPARE(bulk.resultModel()->count(), 4);

    // A new lookup starts from all of them.
    bulk.setResultFilter(QStringLiteral("notFound"));
    QCOMPARE(bulk.resultModel()->count(), 1);
    bulk.start();
    QCOMPARE(bulk.resultFilter(), QString());
    QTRY_VERIFY(!bulk.running());
    QCOMPARE(bulk.resultModel()->count(), 4);
}

void MetadataTest::bulkUnmarkNotOnIgdb()
{
    BulkSetup s;
    BulkLookup &bulk = s.bulk;
    bulk.start();
    QTRY_VERIFY(!bulk.running());
    const QStringList candidates = [&] {
        QStringList out;
        for (const QVariant &c : resultFor(bulk, "switch-metroid-dread").value("candidates").toList())
            out.append(c.toMap().value("name").toString());
        return out;
    }();
    QVERIFY(!candidates.isEmpty());

    // A pick and a not found come back as they were, the pick with its candidates.
    bulk.markNotOnIgdb(QStringLiteral("switch-metroid-dread"));
    bulk.markNotOnIgdb(QStringLiteral("switch-homebrew"));
    QCOMPARE(resultFor(bulk, "switch-metroid-dread").value("state").toString(), QStringLiteral("notOnIgdb"));
    QCOMPARE(bulk.applyCount(), 4);
    bulk.unmarkNotOnIgdb(QStringLiteral("switch-metroid-dread"));
    bulk.unmarkNotOnIgdb(QStringLiteral("switch-homebrew"));
    QCOMPARE(resultFor(bulk, "switch-metroid-dread").value("state").toString(), QStringLiteral("pick"));
    QCOMPARE(resultFor(bulk, "switch-homebrew").value("state").toString(), QStringLiteral("notFound"));
    QCOMPARE(bulk.applyCount(), 2);
    QStringList after;
    for (const QVariant &c : resultFor(bulk, "switch-metroid-dread").value("candidates").toList())
        after.append(c.toMap().value("name").toString());
    QCOMPARE(after, candidates);

    // Marking twice keeps the first state; unmarking a row that isn't marked changes nothing.
    bulk.markNotOnIgdb(QStringLiteral("switch-homebrew"));
    bulk.markNotOnIgdb(QStringLiteral("switch-homebrew"));
    bulk.unmarkNotOnIgdb(QStringLiteral("switch-homebrew"));
    QCOMPARE(resultFor(bulk, "switch-homebrew").value("state").toString(), QStringLiteral("notFound"));
    bulk.unmarkNotOnIgdb(QStringLiteral("switch-homebrew"));
    QCOMPARE(resultFor(bulk, "switch-homebrew").value("state").toString(), QStringLiteral("notFound"));

    // A match is filled in again.
    bulk.markNotOnIgdb(QStringLiteral("switch-mario-tennis-aces"));
    QVERIFY(resultFor(bulk, "switch-mario-tennis-aces").value("fields").toList().isEmpty());
    bulk.unmarkNotOnIgdb(QStringLiteral("switch-mario-tennis-aces"));
    QTRY_COMPARE(resultFor(bulk, "switch-mario-tennis-aces").value("state").toString(), QStringLiteral("matched"));
    QCOMPARE(resultFor(bulk, "switch-mario-tennis-aces").value("fields").toList().size(), 3);
    QVERIFY(artOf(resultFor(bulk, "switch-mario-tennis-aces"), "cover").value("checked").toBool());
}

void MetadataTest::bulkLookUp()
{
    BulkSetup s;
    BulkLookup &bulk = s.bulk;
    bulk.start();
    QVERIFY(bulk.running());
    QCOMPARE(bulk.total(), 4);
    QTRY_VERIFY(!bulk.running());
    QCOMPARE(bulk.done(), 4);
    QCOMPARE(bulk.results().size(), 4);

    // A clear match: its missing fields, checked, and the first image of each kind it lacks.
    const QVariantMap mario = resultFor(bulk, "switch-mario-tennis-aces");
    QCOMPARE(mario.value("state").toString(), QStringLiteral("matched"));
    QCOMPARE(mario.value("match").toMap().value("igdb").toInt(), 81145);
    const QVariantList fields = mario.value("fields").toList();
    QCOMPARE(fields.size(), 3);
    QCOMPARE(fields.at(1).toMap().value("value").toInt(), 2018);
    QVERIFY(fields.at(1).toMap().value("checked").toBool());
    QCOMPARE(fields.at(2).toMap().value("value").toDouble(), 3.6); // rating 72.35 on 400 ratings
    QVERIFY(fields.at(2).toMap().value("checked").toBool());
    const QVariantMap cover = artOf(mario, "cover");
    QVERIFY(cover.value("checked").toBool());
    QCOMPARE(cover.value("image").toMap().value("id").toInt(), 421524); // SteamGridDB's first
    QVERIFY(!cover.value("changed").toBool());
    QVERIFY(artOf(mario, "icon").value("checked").toBool());
    QCOMPARE(bulk.steamGridDbId("switch-mario-tennis-aces"), 5254520);

    // Hades: matched by its Steam app id; SteamGridDB has no heroes, logos or icons in the fixtures.
    const QVariantMap hades = resultFor(bulk, "steam-1145360");
    QCOMPARE(hades.value("state").toString(), QStringLiteral("matched"));
    QVERIFY(hades.value("fields").toList().isEmpty());
    QVERIFY(artOf(hades, "hero").value("none").toBool());
    QVERIFY(!artOf(hades, "hero").value("checked").toBool());
    QVERIFY(artOf(hades, "cover").isEmpty()); // it has one

    QCOMPARE(resultFor(bulk, "switch-metroid-dread").value("state").toString(), QStringLiteral("pick"));
    QCOMPARE(resultFor(bulk, "switch-homebrew").value("state").toString(), QStringLiteral("notFound"));
    QCOMPARE(bulk.applyCount(), 2);

    // Picking, retrying with another title, and marking as not on IGDB.
    const QVariantList candidates = resultFor(bulk, "switch-metroid-dread").value("candidates").toList();
    const int onSwitch = int(std::find_if(candidates.cbegin(), candidates.cend(),
                                          [](const QVariant &c) { return c.toMap().value("igdb").toInt() == 15698; })
                             - candidates.cbegin());
    bulk.pick("switch-metroid-dread", onSwitch);
    QTRY_COMPARE(resultFor(bulk, "switch-metroid-dread").value("state").toString(), QStringLiteral("matched"));
    QCOMPARE(resultFor(bulk, "switch-metroid-dread").value("fields").toList().first().toMap().value("value").toInt(), 2021);
    QCOMPARE(bulk.applyCount(), 3);

    // Detailed lookup's choice, found on other platforms under another name.
    QCOMPARE(resultFor(bulk, "switch-homebrew").value("igdbPlatforms").toList(), QVariantList{130});
    const QVariantMap tp = result(s.client, s.client.lookup(QStringLiteral("Twilight Princess"), {}, 0, 0, 50))
                               .value("candidates").toList().first().toMap();
    bulk.choose("switch-homebrew", tp, QStringLiteral("Twilight Princess"));
    QTRY_COMPARE(resultFor(bulk, "switch-homebrew").value("state").toString(), QStringLiteral("matched"));
    const QVariantMap chosen = resultFor(bulk, "switch-homebrew");
    QCOMPARE(chosen.value("match").toMap().value("igdb").toInt(), 1030);
    QCOMPARE(chosen.value("searched").toString(), QStringLiteral("Twilight Princess"));
    const QVariantList chosenFields = chosen.value("fields").toList();
    QVERIFY(std::any_of(chosenFields.cbegin(), chosenFields.cend(), [](const QVariant &f) {
        return f.toMap().value("key").toString() == QLatin1String("description")
               && f.toMap().value("value").toString().startsWith(QLatin1String("Link, a young ranch hand"));
    }));
    QCOMPARE(bulk.applyCount(), 4);

    bulk.retry("switch-homebrew", QStringLiteral("Mario Tennis Aces"));
    QTRY_COMPARE(resultFor(bulk, "switch-homebrew").value("state").toString(), QStringLiteral("matched"));
    QCOMPARE(resultFor(bulk, "switch-homebrew").value("searched").toString(), QStringLiteral("Mario Tennis Aces"));
    bulk.markNotOnIgdb("switch-homebrew");
    QCOMPARE(resultFor(bulk, "switch-homebrew").value("state").toString(), QStringLiteral("notOnIgdb"));
}

void MetadataTest::bulkStopKeepsResults()
{
    BulkSetup s;
    s.client.http()->setFixtureDelay(50);
    BulkLookup &bulk = s.bulk;
    bulk.start();
    // The first games finish; stopping drops the ones still running and the rest of the queue.
    QTRY_VERIFY(bulk.done() >= 1);
    bulk.stop();
    QVERIFY(!bulk.running());
    const int kept = int(bulk.results().size());
    QCOMPARE(kept, bulk.done());
    QVERIFY(kept < 4);
    for (const QVariant &r : bulk.results())
        QVERIFY(r.toMap().value("state").toString() != QLatin1String("running"));
    QTest::qWait(200); // nothing comes in late
    QCOMPARE(bulk.results().size(), kept);
}

void MetadataTest::bulkArtDefaults()
{
    BulkSetup s;
    BulkLookup &bulk = s.bulk;
    bulk.start();
    QTRY_VERIFY(!bulk.running());
    const QString id = QStringLiteral("switch-mario-tennis-aces");

    // Another image replaces the default, and says so.
    bulk.setArt(id, "cover", {{"id", 7}, {"url", "https://cdn2.steamgriddb.com/grid/other.png"}});
    QVariantMap cover = artOf(resultFor(bulk, id), "cover");
    QCOMPARE(cover.value("image").toMap().value("id").toInt(), 7);
    QVERIFY(cover.value("changed").toBool());
    // Unselecting leaves the kind out; checking it again brings back the default.
    bulk.setArt(id, "logo", {});
    QVERIFY(!artOf(resultFor(bulk, id), "logo").value("checked").toBool());
    bulk.setArtChecked(id, "logo", true);
    QVERIFY(artOf(resultFor(bulk, id), "logo").value("checked").toBool());
    QVERIFY(!artOf(resultFor(bulk, id), "logo").value("changed").toBool());
    bulk.setArtChecked(id, "hero", false);
    QVERIFY(!artOf(resultFor(bulk, id), "hero").value("checked").toBool());
    // A kind without images can't be checked.
    bulk.setArtChecked("steam-1145360", "hero", true);
    QVERIFY(!artOf(resultFor(bulk, "steam-1145360"), "hero").value("checked").toBool());
}

void MetadataTest::bulkApply()
{
    BulkSetup s;
    BulkLookup &bulk = s.bulk;
    bulk.start();
    QTRY_VERIFY(!bulk.running());
    const QString mario = QStringLiteral("switch-mario-tennis-aces");
    bulk.setFieldChecked(mario, "year", false);
    bulk.setArtChecked(mario, "hero", false);
    bulk.pick("switch-metroid-dread", 0);
    QTRY_COMPARE(resultFor(bulk, "switch-metroid-dread").value("state").toString(), QStringLiteral("matched"));
    bulk.markNotOnIgdb("switch-homebrew");
    // Since the lookup, Mario Tennis got a description and a logo by hand: they're left alone.
    s.games[0].hasDescription = true;
    s.games[0].art = {QStringLiteral("logo")};
    s.failValues.insert(QStringLiteral("switch-metroid-dread"), QStringLiteral("Not saved: read-only"));

    QSignalSpy applied(&bulk, &BulkLookup::applied);
    bulk.apply();
    QTRY_COMPARE(applied.size(), 1);
    QVERIFY(!bulk.applying());
    QCOMPARE(s.valueBatches, 1);

    QHash<QString, QVariantMap> written;
    for (const auto &[id, values] : s.written)
        written.insert(id, values);
    QCOMPARE(written.value(mario), (QVariantMap{{"igdb", 81145}, {"rating", 3.6}, {"rating_source", "igdb"}, {"steamgriddb", 5254520}}));
    QCOMPARE(written.value("steam-1145360"), (QVariantMap{{"igdb", 113112}, {"steamgriddb", 4997889}}));
    QCOMPARE(written.value("switch-homebrew"), (QVariantMap{{"igdb", 0}}));
    QVERIFY(written.contains("switch-metroid-dread"));

    // Mario Tennis's cover and icon: downloaded, set in one batch. No hero (unchecked), no logo (it has one now).
    QStringList kinds;
    for (const auto &[id, kind, file] : s.art) {
        QCOMPARE(id, mario);
        QVERIFY(QUrl(file).isLocalFile());
        kinds.append(kind);
    }
    kinds.sort();
    QCOMPARE(kinds, (QStringList{"cover", "icon"}));
    QCOMPARE(s.artBatches, 1);

    // Written games leave the results; the one that failed stays, with why.
    QCOMPARE(applied.first().first().toInt(), 3);
    QCOMPARE(bulk.results().size(), 1);
    QCOMPARE(resultFor(bulk, "switch-metroid-dread").value("applyError").toString(), QStringLiteral("Not saved: read-only"));
}

void MetadataTest::bulkArtForGameNotOnIgdb()
{
    BulkSetup s;
    LookupGame hack = s.games.at(2);
    hack.id = QStringLiteral("switch-star-road-hack");
    hack.title = QStringLiteral("Star Road Hack");
    hack.art = {};
    s.games = {hack};
    s.bulk.reload();
    s.bulk.start();
    QTRY_VERIFY(!s.bulk.running());
    const QString id = hack.id;

    // IGDB has nothing, SteamGridDB does: its art is offered, and Apply saves it without marking the game.
    QCOMPARE(resultFor(s.bulk, id).value("state").toString(), QStringLiteral("notFound"));
    QVERIFY(artOf(resultFor(s.bulk, id), "cover").value("checked").toBool());
    QVERIFY(s.bulk.hasArt());
    QCOMPARE(s.bulk.applyCount(), 1);
    s.bulk.setArtChecked(id, "hero", false);
    s.bulk.setArtChecked(id, "logo", false);
    s.bulk.setArtChecked(id, "icon", false);
    QCOMPARE(s.bulk.applyCount(), 1);
    s.bulk.setArtChecked(id, "cover", false);
    QCOMPARE(s.bulk.applyCount(), 0);
    s.bulk.setArtChecked(id, "cover", true);

    // Marked as not on IGDB, the art stays.
    s.bulk.markNotOnIgdb(id);
    QVERIFY(artOf(resultFor(s.bulk, id), "cover").value("checked").toBool());
    QVERIFY(!s.bulk.running());

    QSignalSpy applied(&s.bulk, &BulkLookup::applied);
    s.bulk.apply();
    QTRY_COMPARE(applied.size(), 1);
    QCOMPARE(s.written.size(), 1);
    QCOMPARE(s.written.first().second, (QVariantMap{{"igdb", 0}, {"steamgriddb", 5254520}}));
    QCOMPARE(s.art.size(), 1);
    QCOMPARE(s.art.first()[1], QStringLiteral("cover"));
}

namespace {

QStringList resultTags(const BulkLookup &bulk, const QString &id, bool checkedOnly = false)
{
    QStringList out;
    for (const QVariant &t : resultFor(bulk, id).value("tags").toList()) {
        if (!checkedOnly || t.toMap().value("checked").toBool())
            out.append(t.toMap().value("name").toString());
    }
    return out;
}

} // namespace

void MetadataTest::bulkTags()
{
    BulkSetup s;
    BulkLookup &bulk = s.bulk;
    s.tagMappings = {{"Roguelike", {"keyword:roguelike", "keyword:roguelite"}},
                     {"Platformer", {"genre:8"}},
                     {"Sports", {"genre:14"}},
                     {"Hack and slash", {"genre:25"}},
                     {"Horror", {"theme:19"}}};
    QVERIFY(bulk.suggestTags());
    bulk.start();
    QTRY_VERIFY(!bulk.running());
    const QString mario = QStringLiteral("switch-mario-tennis-aces");
    const QString hades = QStringLiteral("steam-1145360");
    const QString dread = QStringLiteral("switch-metroid-dread");
    // The tags that fit each match, checked, without those the game has (Hades's Roguelike).
    QCOMPARE(resultTags(bulk, mario, true), QStringList{"Sports"});
    QCOMPARE(resultTags(bulk, hades, true), QStringList{"Hack and slash"});
    bulk.pick(dread, 2);
    QTRY_COMPARE(resultFor(bulk, dread).value("state").toString(), QStringLiteral("matched"));
    QCOMPARE(resultTags(bulk, dread, true), QStringList{"Platformer"});

    // Unchecked, Hades's is left out; it still has its row's other values.
    bulk.setTagChecked(hades, "Hack and slash", false);
    QCOMPARE(resultTags(bulk, hades), QStringList{"Hack and slash"});
    QVERIFY(resultTags(bulk, hades, true).isEmpty());
    // Since the lookup, Mario Tennis got Sports by hand, and Metroid Dread got another tag.
    s.games[0].tags = {"Party", "Sports"};
    s.games[2].tags = {"Favorite of mine"};
    QCOMPARE(bulk.applyCount(), 3);

    QSignalSpy applied(&bulk, &BulkLookup::applied);
    bulk.apply();
    QTRY_COMPARE(applied.size(), 1);
    QHash<QString, QVariantMap> written;
    for (const auto &[id, values] : s.written)
        written.insert(id, values);
    // Added to the tags the game has then: none doubled, none removed.
    QVERIFY(!written.value(mario).contains("tags"));
    QVERIFY(!written.value(hades).contains("tags"));
    QCOMPARE(written.value(dread).value("tags").toStringList(), (QStringList{"Favorite of mine", "Platformer"}));

    // Off: none suggested for the games looked up then.
    BulkSetup off;
    off.tagMappings = s.tagMappings;
    off.bulk.setSuggestTags(false);
    off.bulk.start();
    QTRY_VERIFY(!off.bulk.running());
    QVERIFY(resultFor(off.bulk, mario).value("tags").toList().isEmpty());
}

void MetadataTest::bulkIncludeComplete()
{
    BulkSetup s;
    BulkLookup &bulk = s.bulk;
    QVERIFY(!bulk.includeComplete());
    QVERIFY(!shownIds(bulk).contains("switch-complete"));
    bulk.setIncludeComplete(true);
    // Listed, unchecked; the Missing filter hides nothing then.
    QVERIFY(shownIds(bulk).contains("switch-complete"));
    bulk.setMissing({"art"});
    QCOMPARE(shownIds(bulk), (QStringList{"switch-mario-tennis-aces", "steam-1145360", "switch-metroid-dread",
                                          "switch-homebrew", "switch-complete"}));
    QCOMPARE(bulk.checkedCount(), 4);
    bulk.setAllChecked(true);
    QCOMPARE(bulk.checkedCount(), 5);
    bulk.setIncludeComplete(false);
    QVERIFY(!shownIds(bulk).contains("switch-complete"));
    QCOMPARE(shownIds(bulk), (QStringList{"switch-mario-tennis-aces", "steam-1145360"})); // missing art
}

void MetadataTest::bulkFetchedAhead()
{
    // Metroid Dread and Hades by their ids, Mario Tennis searched, and an id IGDB doesn't know.
    BulkSetup s;
    BulkLookup &bulk = s.bulk;
    s.games[1].igdb = 113112;
    s.games[2].igdb = 15698;
    LookupGame gone = s.games[0];
    gone.id = QStringLiteral("switch-gone");
    gone.igdb = 42;
    s.games.append(gone);
    s.tagMappings = {{"Platformer", {"genre:8"}}};
    bulk.reload();
    bulk.setAllChecked(true);
    bulk.start();
    QTRY_VERIFY(!bulk.running());
    QCOMPARE(bulk.done(), 5);
    // Metroid Dread's title finds three games; its id, the one.
    QCOMPARE(resultFor(bulk, "switch-metroid-dread").value("state").toString(), QStringLiteral("matched"));
    QCOMPARE(resultFor(bulk, "switch-metroid-dread").value("match").toMap().value("igdb").toInt(), 15698);
    QCOMPARE(resultTags(bulk, "switch-metroid-dread"), QStringList{"Platformer"});
    QCOMPARE(resultFor(bulk, "steam-1145360").value("match").toMap().value("igdb").toInt(), 113112);
    QCOMPARE(resultFor(bulk, "switch-mario-tennis-aces").value("state").toString(), QStringLiteral("matched"));
    QCOMPARE(resultFor(bulk, "switch-gone").value("state").toString(), QStringLiteral("notFound"));
}

void MetadataTest::bulkFetchedAheadBatches()
{
    QTemporaryDir data;
    QTemporaryDir cache;
    FakeHttpServer server;
    server.on("/oauth2/token", {200, kToken1});
    server.on("/v4/games", {200, R"([{"id": 7, "name": "Seven", "summary": "Found by id"}])"});
    MetadataClient client(data.path(), cache.path(), endpoints(server));
    client.http()->setCredentials(kKeys);
    client.http()->setIgdbInterval(100);

    // 1001 games with ids, missing only a description, and two without, searched.
    QList<LookupGame> games;
    const auto game = [](int n, int igdb) {
        LookupGame g;
        g.id = QStringLiteral("game-%1").arg(n);
        g.title = QStringLiteral("Game %1").arg(n);
        g.hasYear = g.hasRating = true;
        g.art = {QStringLiteral("cover"), QStringLiteral("hero"), QStringLiteral("logo"), QStringLiteral("icon")};
        g.igdb = igdb;
        return g;
    };
    games.append(game(0, -1));
    for (int n = 1; n <= 1001; ++n)
        games.append(game(n, n));
    games.append(game(1002, -1));
    BulkLookup bulk(&client, [&] { return games; }, {}, {});
    QCOMPARE(bulk.checkedCount(), 1003);
    bulk.start();
    QCOMPARE(bulk.total(), 1003);
    QTRY_VERIFY_WITH_TIMEOUT(!bulk.running(), 30000);
    QCOMPARE(bulk.done(), 1003);

    // Three requests fetch the 1001, the two others are searched: kept to the rate limit.
    const auto requests = server.requestsTo("/v4/games");
    QCOMPARE(requests.size(), 5);
    int byId = 0;
    for (const auto &r : requests)
        byId += r.body.startsWith("where id = (") ? 1 : 0;
    QCOMPARE(byId, 3);
    // On average: the server shares this thread, busy with 1003 results, so one arrival can
    // be read late and the next on time.
    const qint64 span = requests.last().atMs - requests.first().atMs;
    QVERIFY2(span >= 4 * 90, qPrintable(QString::number(span)));

    const QVariantMap seven = resultFor(bulk, "game-7");
    QCOMPARE(seven.value("state").toString(), QStringLiteral("matched"));
    QCOMPARE(seven.value("fields").toList().first().toMap().value("value").toString(), QStringLiteral("Found by id"));
    QCOMPARE(resultFor(bulk, "game-8").value("state").toString(), QStringLiteral("notFound")); // not sent
    QCOMPARE(resultFor(bulk, "game-0").value("state").toString(), QStringLiteral("pick")); // searched: "Seven"
}

void MetadataTest::bulkReplaceExisting()
{
    BulkSetup s;
    s.games[1].description = QStringLiteral("My own words"); // Hades has a description and a year
    s.games[1].year = 1990;
    BulkLookup &bulk = s.bulk;
    bulk.reload();
    QVERIFY(!shownIds(bulk).contains("switch-complete"));

    // A selection stays as it was: the games "Replace existing" adds start unchecked.
    bulk.setAllChecked(false);
    bulk.setChecked(QStringLiteral("switch-metroid-dread"), true);
    bulk.setReplaceExisting(true);
    QCOMPARE(bulk.checkedCount(), 1);
    bulk.setReplaceExisting(false);
    bulk.setAllChecked(true);
    bulk.setReplaceExisting(true);
    QCOMPARE(bulk.checkedCount(), 4); // not the complete game
    bulk.setReplaceExisting(false);

    // Every game is listed, and "description" also finds the ones that have one.
    bulk.setReplaceExisting(true);
    QVERIFY(shownIds(bulk).contains("switch-complete"));
    QCOMPARE(bulk.missingCount(), 4); // still the ones missing something
    bulk.setMissing({QStringLiteral("description")});
    QVERIFY(shownIds(bulk).contains("steam-1145360"));
    bulk.clearFilters();

    bulk.setAllChecked(false);
    bulk.setChecked(QStringLiteral("steam-1145360"), true);
    bulk.start();
    QTRY_VERIFY(!bulk.running());

    // IGDB's description and year are offered in place of the game's own, unchecked.
    const QVariantList fields = resultFor(bulk, "steam-1145360").value("fields").toList();
    QCOMPARE(fields.size(), 2);
    for (const QVariant &f : fields) {
        QVERIFY(f.toMap().value("replaces").toBool());
        QVERIFY(!f.toMap().value("checked").toBool());
    }
    QCOMPARE(fields.at(0).toMap().value("current").toString(), QStringLiteral("My own words"));
    QCOMPARE(fields.at(1).toMap().value("current").toInt(), 1990);
    const QString summary = fields.at(0).toMap().value("value").toString();
    QVERIFY(!summary.isEmpty());

    const auto tally = [&](const char *category) { return bulk.categories().value(category).toMap(); };
    const auto described = [&] { return resultFor(bulk, "steam-1145360").value("fields").toList(); };

    // Each category on its own: the description's replacement, not the year's. The year was
    // edited since the lookup, so once applied only the description is replaced.
    QCOMPARE(tally("description").value("total").toInt(), 1);
    QCOMPARE(tally("description").value("on").toInt(), 0);
    QCOMPARE(tally("year").value("total").toInt(), 1);
    bulk.setCategoryChecked(QStringLiteral("description"), true);
    QCOMPARE(tally("description").value("on").toInt(), 1);
    QVERIFY(described().at(0).toMap().value("checked").toBool());
    QVERIFY(!described().at(1).toMap().value("checked").toBool());
    bulk.setCategoryChecked(QStringLiteral("description"), false);
    QCOMPARE(tally("description").value("on").toInt(), 0);
    bulk.setCategoryChecked(QStringLiteral("year"), true);
    QCOMPARE(tally("year").value("on").toInt(), 1);
    QCOMPARE(tally("description").value("on").toInt(), 0);
    bulk.setCategoryChecked(QStringLiteral("year"), false);
    QCOMPARE(tally("rating").value("total").toInt(), 0); // Hades has no rating to replace

    // Tags and art check and uncheck everything offered.
    const int tags = tally("tags").value("total").toInt();
    QCOMPARE(tally("tags").value("on").toInt(), tags); // suggested tags start checked
    bulk.setCategoryChecked(QStringLiteral("tags"), false);
    QCOMPARE(tally("tags").value("on").toInt(), 0);
    for (const QVariant &t : resultFor(bulk, "steam-1145360").value("tags").toList())
        QVERIFY(!t.toMap().value("checked").toBool());
    bulk.setCategoryChecked(QStringLiteral("tags"), true);
    QCOMPARE(tally("tags").value("on").toInt(), tags);

    // The cover Hades has is offered a replacement, unchecked; "art" leaves it alone.
    const QVariantMap cover = artOf(resultFor(bulk, "steam-1145360"), "cover");
    QVERIFY(cover.value("replaces").toBool());
    QVERIFY(!cover.value("checked").toBool());
    QVERIFY(!artOf(resultFor(bulk, "steam-1145360"), "hero").value("replaces").toBool());
    QCOMPARE(tally("replaceArt").value("total").toInt(), 1);
    QCOMPARE(tally("replaceArt").value("on").toInt(), 0);
    bulk.setCategoryChecked(QStringLiteral("art"), true);
    QVERIFY(!artOf(resultFor(bulk, "steam-1145360"), "cover").value("checked").toBool());
    bulk.setCategoryChecked(QStringLiteral("replaceArt"), true);
    QVERIFY(artOf(resultFor(bulk, "steam-1145360"), "cover").value("checked").toBool());

    bulk.setCategoryChecked(QStringLiteral("description"), true);
    bulk.setCategoryChecked(QStringLiteral("year"), true);
    for (const QVariant &f : described())
        QVERIFY(f.toMap().value("checked").toBool());
    s.games[1].year = 2001;
    QSignalSpy applied(&bulk, &BulkLookup::applied);
    bulk.apply();
    QTRY_COMPARE(applied.size(), 1);
    QCOMPARE(s.written.size(), 1);
    const QVariantMap values = s.written.first().second;
    QCOMPARE(values.value("description").toString(), summary);
    QVERIFY(!values.contains("year"));
    QCOMPARE(s.art.size(), 1);
    QCOMPARE(s.art.first()[1], QStringLiteral("cover"));
}

void MetadataTest::bulkReplaceArt()
{
    BulkSetup s;
    s.games[1].artUrls = {{QStringLiteral("cover"), QStringLiteral("file:///art/cover.png?v=1")}};
    BulkLookup &bulk = s.bulk;
    bulk.setReplaceExisting(true);
    bulk.setMissing({QStringLiteral("description")});
    bulk.setAllChecked(false);
    bulk.setChecked(QStringLiteral("steam-1145360"), true);
    bulk.start();
    QTRY_VERIFY(!bulk.running());

    // Art isn't looked for, so none is replaced; the missing kinds are still offered.
    QVERIFY(!artOf(resultFor(bulk, "steam-1145360"), "cover").contains("kind"));
    bulk.clearResults();

    bulk.setMissing({QStringLiteral("art")});
    bulk.start();
    QTRY_VERIFY(!bulk.running());
    bulk.setCategoryChecked(QStringLiteral("replaceArt"), true);

    // Art set since the lookup isn't replaced.
    s.games[1].artUrls = {{QStringLiteral("cover"), QStringLiteral("file:///art/cover.png?v=2")}};
    QSignalSpy applied(&bulk, &BulkLookup::applied);
    bulk.apply();
    QTRY_COMPARE(applied.size(), 1);
    QVERIFY(std::none_of(s.art.cbegin(), s.art.cend(), [](const auto &a) { return a[1] == QLatin1String("cover"); }));
}

void MetadataTest::idListModel()
{
    const auto item = [](const QString &id, int v = 0) { return QVariantMap{{"id", id}, {"v", v}}; };
    const auto ids = [](const IdListModel &m) {
        QStringList out;
        for (int i = 0; i < m.rowCount(); ++i)
            out.append(m.data(m.index(i), IdListModel::IdRole).toString());
        return out;
    };
    IdListModel model;
    QSignalSpy inserted(&model, &QAbstractItemModel::rowsInserted);
    QSignalSpy removed(&model, &QAbstractItemModel::rowsRemoved);
    QSignalSpy moved(&model, &QAbstractItemModel::rowsMoved);
    QSignalSpy changed(&model, &QAbstractItemModel::dataChanged);
    QSignalSpy reset(&model, &QAbstractItemModel::modelReset);

    model.setItems({item("a"), item("b"), item("c")});
    QCOMPARE(ids(model), QStringList({"a", "b", "c"}));
    QCOMPARE(inserted.size(), 1); // one run

    // One row changed: one row signalled.
    model.setItems({item("a"), item("b", 1), item("c")});
    QCOMPARE(changed.size(), 1);
    QCOMPARE(changed.first().at(0).toModelIndex().row(), 1);
    QCOMPARE(model.data(model.index(1), IdListModel::ItemRole).toMap().value("v").toInt(), 1);

    // The same again: nothing.
    model.setItems({item("a"), item("b", 1), item("c")});
    QCOMPARE(changed.size(), 1);

    // Appended, removed, reordered, and inserted in the middle.
    model.setItems({item("a"), item("b", 1), item("c"), item("d")});
    QCOMPARE(inserted.size(), 2);
    model.setItems({item("a"), item("c"), item("d")});
    QCOMPARE(removed.size(), 1);
    model.setItems({item("d"), item("a"), item("c")});
    QCOMPARE(ids(model), QStringList({"d", "a", "c"}));
    QVERIFY(moved.size() >= 1);
    model.setItems({item("d"), item("x"), item("y"), item("a"), item("c")});
    QCOMPARE(ids(model), QStringList({"d", "x", "y", "a", "c"}));
    QCOMPARE(inserted.size(), 3);
    model.setItems({});
    QCOMPARE(model.count(), 0);
    QCOMPARE(reset.size(), 0);
}

void MetadataTest::scanLookup()
{
    BulkSetup s;
    ScanLookup scan([&s] { return s.games; },
                    [&s](const QList<std::pair<QString, QVariantMap>> &g) {
                        s.written += g;
                        return QHash<QString, QString>();
                    },
                    [&s](const QList<std::array<QString, 3>> &a) {
                        s.art += a;
                        return QHash<QString, QString>();
                    },
                    s.data.filePath("settings.toml"), &s.client);
    QSignalSpy finished(&scan, &ScanLookup::finished);
    QSignalSpy message(&scan, &ScanLookup::message);

    // Mario Tennis is a clear match, Metroid Dread needs a pick, and the game marked as not on
    // IGDB is left alone; games not added aren't looked up.
    scan.add({"switch-mario-tennis-aces", "switch-metroid-dread", "3ds-not-on-igdb"});
    QVERIFY(scan.running());
    QTRY_COMPARE(finished.size(), 1);
    QVERIFY(!scan.running());
    QCOMPARE(finished.first().at(0).toInt(), 1);
    QCOMPARE(finished.first().at(1).toInt(), 1);
    QCOMPARE(s.written.size(), 1);
    QCOMPARE(s.written.first().first, QStringLiteral("switch-mario-tennis-aces"));
    QCOMPARE(s.written.first().second.value("year").toInt(), 2018);
    QCOMPARE(s.written.first().second.value("igdb").toInt(), 81145);
    QVERIFY(!s.written.first().second.contains("tags")); // never without showing them
    QStringList kinds;
    for (const auto &[id, kind, file] : s.art)
        kinds.append(kind);
    kinds.sort();
    QCOMPARE(kinds, (QStringList{"cover", "hero", "icon", "logo"}));
    QCOMPARE(message.size(), 1);
    QVERIFY(message.first().at(1).toBool()); // Metroid Dread needs a look

    // Stopped: its progress starts over.
    QCOMPARE(scan.progressTotal(), 0);
    QCOMPARE(scan.progressDone(), 0);

    // Games that lack nothing: done without a message.
    scan.add({"switch-complete"});
    QTRY_COMPARE(finished.size(), 2);
    QCOMPARE(finished.last().at(0).toInt(), 0);
    QCOMPARE(message.size(), 1);

    // Games added while a batch runs wait for the next one: it keeps running, and its progress
    // counts both batches until it stops.
    QList<std::pair<bool, int>> atFinished; // running, progressTotal
    connect(&scan, &ScanLookup::finished, this,
            [&] { atFinished.append({scan.running(), scan.progressTotal()}); });
    scan.add({"switch-mario-tennis-aces"});
    QVERIFY(scan.running());
    scan.add({"switch-metroid-dread"});
    QCOMPARE(scan.progressTotal(), 2);
    QTRY_COMPARE(finished.size(), 4);
    QCOMPARE(atFinished, (QList<std::pair<bool, int>>{{true, 2}, {false, 0}}));
    QVERIFY(!scan.running());

    // Without IGDB's keys nothing is looked up.
    QTemporaryDir empty;
    MetadataClient keyless(empty.path(), empty.path());
    ScanLookup none([&s] { return s.games; }, {}, {}, {}, &keyless);
    none.add({"switch-mario-tennis-aces"});
    QVERIFY(!none.running());

    // Turned off: written to settings.toml, and new games aren't looked up. On again removes the key.
    QVERIFY(scan.enabled());
    QCOMPARE(scan.setEnabled(false), QString());
    QVERIFY(!scan.enabled());
    QFile settings(s.data.filePath("settings.toml"));
    QVERIFY(settings.open(QIODevice::ReadOnly));
    QVERIFY(settings.readAll().contains("[metadata]\nlook_up_new_games = false"));
    settings.close();
    scan.add({"switch-mario-tennis-aces"});
    QVERIFY(!scan.running());
    QCOMPARE(scan.setEnabled(true), QString());
    QVERIFY(scan.enabled());
    QVERIFY(settings.open(QIODevice::ReadOnly));
    QVERIFY(!settings.readAll().contains("look_up_new_games"));
}

void MetadataTest::metadataSettings()
{
    const auto r = parseMetadataSettings("[metadata]\nlook_up_new_games = false\n", "settings.toml");
    QVERIFY(r.ok);
    QVERIFY(!r.value.lookUpNewGames);
    QVERIFY(r.warnings.isEmpty());
    QVERIFY(parseMetadataSettings("[launch]\nwhile_running = \"stay\"\n", "settings.toml").value.lookUpNewGames);

    const auto bad = parseMetadataSettings("[metadata]\nlook_up_new_games = \"no\"\nlook_up_new_game = true\n",
                                           "settings.toml");
    QVERIFY(bad.value.lookUpNewGames);
    QCOMPARE(bad.warnings,
             (QStringList{"settings.toml:3: unknown key look_up_new_game — did you mean \"look_up_new_games\"?",
                          "settings.toml:2: look_up_new_games must be true or false"}));
}

QTEST_GUILESS_MAIN(MetadataTest)
#include "metadata_test.moc"
