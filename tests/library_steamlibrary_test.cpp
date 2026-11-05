#include "librarytestbase.h"
#include "steamwebapi.h"
#include "steamwebmerge.h"

#include <QElapsedTimer>

namespace {
const QString kMe = QStringLiteral("76561198000000001");
const QString kAlex = QStringLiteral("76561198000000009");
const QString kSam = QStringLiteral("76561198000000010");

SteamWebApi::Cache baseCache()
{
    SteamWebApi::Cache c;
    c.steamId = kMe;
    c.fetched = QDateTime::currentDateTimeUtc();
    c.owned = {{1000, QStringLiteral("Owned Game"), 90, 1790000000},
               {1001, QStringLiteral("A Soundtrack"), 0, 0},
               {2001, QStringLiteral("Both Have It"), 0, 0}};
    c.types = {{1000, 0}, {1001, 6}, {2001, 0}};
    c.shared = {{2000, QStringLiteral("Family Game"), {kAlex}, 1, 30, 1780000000},
                {2001, QStringLiteral("Both Have It"), {kMe, kAlex}, 1, 0, 0},
                {2002, QStringLiteral("Two Lenders"), {kAlex, kSam}, 1, 0, 0},
                {2003, QStringLiteral("A Video"), {kAlex}, 4, 0, 0}};
    c.names = {{kAlex, QStringLiteral("Alex")}, {kSam, QStringLiteral("Sam")}};
    c.sizes = {{1000, 5000000000LL}};
    return c;
}
}

// The whole Steam library: the Web API's cache merged into steam.toml by the scan.
class LibrarySteamLibraryTest : public LibraryTestBase
{
    Q_OBJECT

    std::unique_ptr<Library> openWith(const SteamWebApi::Cache *cache)
    {
        if (cache)
            SteamWebApi::saveCache(SteamWebApi::cachePath(m_cache, QStringLiteral("test")), *cache);
        auto library = std::make_unique<Library>(m_data, m_cache);
        library->store().setInstallLocations(m_installs);
        library->store().setSteamLocations({m_installs.home, {}});
        library->store().setStateDir(m_state);
        library->store().setSteamWebCache(SteamWebApi::cachePath(m_cache, QStringLiteral("test")));
        library->setUndoDir(m_state + QStringLiteral("/undo"));
        QSignalSpy scanning(&library->store(), &LibraryStore::scanningChanged);
        library->startRefresh();
        library->store().waitForIdle();
        if (!QTest::qWaitFor([&] { return scanning.count() >= 2 && !library->store().scanning(); }, 10000))
            qWarning("the first scan's result never arrived");
        return library;
    }

    void rescan(Library &library)
    {
        library.store().refresh();
        library.store().waitForIdle();
        QTest::qWait(30);
        QCoreApplication::processEvents();
        library.store().waitForIdle();
    }

    QVariantMap game(Library &library, const QString &id) { return library.gameAt(library.rowOfGame(id)); }

    // Shown rows (not-installed Steam games are hidden by default).
    static void showAll(Library &library) { library.setInstalledOnly(false); }

private slots:
    void parsesTheAnswers();
    void parsesAnEmptyAnswerAndAFamilyOfOne();
    void requestsKeepTheSecretsInTheQuery();
    void cacheRoundTrips();
    void newGamesJoinTheIndex();
    void installedLaterIsTheSameGame();
    void ownedAndSharedIsOnlyOwned();
    void gamesThatLeaveGo();
    void anEmptyCacheRemovesNothing();
    void anotherAccountsCacheIsIgnored();
    void blockedAndNonGamesAreSkipped();
    void turningItOffRemovesWhatTheApiBrought();
    void playTimeAndSizeComeFromTheApi();
    void aFileAloneIsHiddenOnceTheApiIsInvolved();
    void theIndexFormatReadsBothForms();
    void nothingChangesWithoutACache();
    void aLargeLibraryIsFast();
};

void LibrarySteamLibraryTest::parsesTheAnswers()
{
    const auto owned = SteamWebApi::parseOwnedGames(QJsonDocument::fromJson(
        R"({"response":{"game_count":2,"games":[
            {"appid":620,"name":"Portal 2","playtime_forever":120,"rtime_last_played":1700000000},
            {"appid":730,"name":"Counter-Strike 2","playtime_forever":0}]}})"));
    QVERIFY(owned.ok);
    QCOMPARE(owned.items.size(), 2);
    QCOMPARE(owned.items.first(), (SteamWebApi::OwnedGame{620, "Portal 2", 120, 1700000000}));

    const auto shared = SteamWebApi::parseSharedApps(QJsonDocument::fromJson(
        R"({"response":{"owner_steamid":"1","apps":[
            {"appid":10,"name":"Solo","owner_steamids":["76561198000000009"],"app_type":1,"rt_playtime":5,"rt_last_played":9},
            {"appid":20,"name":"Two Lenders","owner_steamids":["76561198000000009","76561198000000010"],"app_type":1}]}})"));
    QVERIFY(shared.ok);
    QCOMPARE(shared.items.size(), 2);
    QCOMPARE(shared.items.at(0).owners, QStringList{kAlex});
    QCOMPARE(shared.items.at(0).playMinutes, 5);
    QCOMPARE(shared.items.at(1).owners, (QStringList{kAlex, kSam}));

    const auto group = SteamWebApi::parseFamilyGroup(QJsonDocument::fromJson(
        R"({"response":{"family_groupid":"123456","is_not_member_of_any_group":false,"role":1}})"));
    QVERIFY(group.ok && group.member);
    QCOMPARE(group.groupId, QStringLiteral("123456"));
    QCOMPARE(SteamWebApi::parseFamilyMembers(QJsonDocument::fromJson(
                 R"({"response":{"members":[{"steamid":"76561198000000009"},{"steamid":"76561198000000010"}]}})")),
             (QStringList{kAlex, kSam}));
    QCOMPARE(SteamWebApi::parseUserNames(QJsonDocument::fromJson(
                 R"({"response":{"players":[{"steamid":"76561198000000009","personaname":"Alex"}]}})")),
             (QHash<QString, QString>{{kAlex, QStringLiteral("Alex")}}));

    const auto sizes = SteamWebApi::parseClientApps(QJsonDocument::fromJson(
        R"({"response":{"apps":[{"appid":620,"bytes_required":"1234567890"},{"appid":730,"bytes_required":"0"}]}})"));
    QCOMPARE(sizes, (QHash<qint64, qint64>{{620, 1234567890LL}}));

    // A game the store no longer shows has no type to read.
    const auto types = SteamWebApi::parseItemTypes(QJsonDocument::fromJson(
        R"({"response":{"store_items":[{"appid":620,"success":1,"type":0},{"appid":5,"success":1,"type":4},
            {"appid":6,"success":15}]}})"), {620, 5, 6, 7});
    QCOMPARE(types.value(620), 0);
    QCOMPARE(types.value(5), 4);
    QCOMPARE(types.value(6), SteamWebApi::kTypeNoStoreData);
    QCOMPARE(types.value(7), SteamWebApi::kTypeNoStoreData);
}

void LibrarySteamLibraryTest::parsesAnEmptyAnswerAndAFamilyOfOne()
{
    // A private profile and a failure look alike; the parser only says the answer had the shape of one.
    const auto empty = SteamWebApi::parseOwnedGames(QJsonDocument::fromJson(R"({"response":{}})"));
    QVERIFY(empty.ok);
    QVERIFY(empty.items.isEmpty());
    QVERIFY(!SteamWebApi::parseOwnedGames(QJsonDocument::fromJson("[]")).ok);
    QVERIFY(!SteamWebApi::parseOwnedGames(QJsonDocument()).ok);

    const auto alone = SteamWebApi::parseFamilyGroup(QJsonDocument::fromJson(
        R"({"response":{"is_not_member_of_any_group":true}})"));
    QVERIFY(alone.ok);
    QVERIFY(!alone.member);
    const auto noApps = SteamWebApi::parseSharedApps(QJsonDocument::fromJson(R"({"response":{}})"));
    QVERIFY(noApps.ok);
    QVERIFY(noApps.items.isEmpty());
}

void LibrarySteamLibraryTest::requestsKeepTheSecretsInTheQuery()
{
    const auto owned = SteamWebApi::ownedGamesRequest(kMe, {QStringLiteral("TOKEN"), {}});
    QCOMPARE(owned.path, QStringLiteral("/IPlayerService/GetOwnedGames/v1/"));
    QVERIFY(!owned.path.contains(QLatin1String("TOKEN")));
    QCOMPARE(owned.query.queryItemValue(QStringLiteral("access_token")), QStringLiteral("TOKEN"));
    QCOMPARE(owned.query.queryItemValue(QStringLiteral("skip_unvetted_apps")), QStringLiteral("0"));
    QCOMPARE(owned.query.queryItemValue(QStringLiteral("include_played_free_games")), QStringLiteral("1"));
    const auto withKey = SteamWebApi::ownedGamesRequest(kMe, {{}, QStringLiteral("KEY")});
    QCOMPARE(withKey.query.queryItemValue(QStringLiteral("key")), QStringLiteral("KEY"));
    QVERIFY(!withKey.query.hasQueryItem(QStringLiteral("access_token")));
    QCOMPARE(SteamWebApi::sharedAppsRequest(kMe, QStringLiteral("9"), QStringLiteral("T")).path,
             QStringLiteral("/IFamilyGroupsService/GetSharedLibraryApps/v1/"));
    QCOMPARE(SteamWebApi::clientAppsRequest(QStringLiteral("T")).query.queryItemValue(QStringLiteral("fields")),
             QStringLiteral("games"));
    const auto items = SteamWebApi::itemsRequest({620, 730});
    QVERIFY(items.query.queryItemValue(QStringLiteral("input_json"), QUrl::FullyDecoded).contains(QLatin1String("730")));
}

void LibrarySteamLibraryTest::cacheRoundTrips()
{
    const SteamWebApi::Cache cache = baseCache();
    SteamWebApi::Cache back;
    QVERIFY(SteamWebApi::parseCache(SteamWebApi::formatCache(cache), &back));
    QCOMPARE(back.owned, cache.owned);
    QCOMPARE(back.shared, cache.shared);
    QCOMPARE(back.names, cache.names);
    QCOMPARE(back.sizes, cache.sizes);
    QCOMPARE(back.types, cache.types);
    QCOMPARE(back.steamId, cache.steamId);
    QVERIFY(!SteamWebApi::parseCache("not json", &back));
    QVERIFY(!SteamWebApi::parseCache(R"({"version":99})", &back));
}

void LibrarySteamLibraryTest::newGamesJoinTheIndex()
{
    installFakeSteam();
    const SteamWebApi::Cache cache = baseCache();
    auto library = openWith(&cache);
    QSignalSpy added(&library->store(), &LibraryStore::gamesAdded);
    rescan(*library);
    QVERIFY(added.isEmpty()); // looking them up is the lookup's own step

    const QByteArray index = readFile(steamIndexPath());
    QVERIFY(index.contains("1000 = { title = \"Owned Game\", owned = true, from_api = true, type = 0 }\n"));
    QVERIFY(index.contains("2000 = { title = \"Family Game\", shared_by = [\"" + kAlex.toUtf8() + "\"], from_api = true }\n"));
    QVERIFY(!index.contains("1001"));
    QVERIFY(!index.contains("2003"));
    // The installed ones keep their plain lines.
    QVERIFY(index.contains("2231450 = \"Pizza Tower\"\n"));
    QVERIFY(index.startsWith("# Steam games found by the import"));

    showAll(*library);
    const QVariantMap owned = game(*library, QStringLiteral("steam-1000"));
    QCOMPARE(owned.value("title").toString(), QStringLiteral("Owned Game"));
    QCOMPARE(owned.value("installed").toBool(), false);
    QCOMPARE(owned.value("steamOwnership").toString(), QStringLiteral("owned"));
    QVERIFY(owned.value("sharedBy").toStringList().isEmpty());
    const QVariantMap family = game(*library, QStringLiteral("steam-2000"));
    QCOMPARE(family.value("steamOwnership").toString(), QStringLiteral("family"));
    QCOMPARE(family.value("sharedBy").toStringList(), QStringList{QStringLiteral("Alex")});
    QCOMPARE(game(*library, QStringLiteral("steam-2002")).value("sharedBy").toStringList(),
             (QStringList{QStringLiteral("Alex"), QStringLiteral("Sam")}));
    QVERIFY(game(*library, QStringLiteral("steam-2231450")).value("steamOwnership").toString().isEmpty());

    // The next start (from the library cache) shows the same.
    library.reset();
    library = openWith(nullptr);
    showAll(*library);
    QCOMPARE(game(*library, QStringLiteral("steam-2000")).value("steamOwnership").toString(), QStringLiteral("family"));
    QCOMPARE(readFile(steamIndexPath()), index);
}

void LibrarySteamLibraryTest::installedLaterIsTheSameGame()
{
    const QString root = installFakeSteam();
    SteamWebApi::Cache cache = baseCache();
    auto library = openWith(&cache);
    showAll(*library);
    QVERIFY(!game(*library, QStringLiteral("steam-1000")).value("installed").toBool());

    writeFile(root + QStringLiteral("/steamapps/appmanifest_1000.acf"),
              "\"AppState\"\n{\n\t\"appid\"\t\t\"1000\"\n\t\"name\"\t\t\"Owned Game\"\n\t\"StateFlags\"\t\t\"4\"\n}\n");
    rescan(*library);
    const QVariantMap installed = game(*library, QStringLiteral("steam-1000"));
    QVERIFY(installed.value("installed").toBool());
    QCOMPARE(installed.value("steamOwnership").toString(), QStringLiteral("owned"));
    // One line, not two.
    QCOMPARE(readFile(steamIndexPath()).count("\n1000 = "), 1);

    // The account no longer lists it: installed, so it stays, without the API's mark of ownership.
    cache.owned.removeFirst();
    SteamWebApi::saveCache(SteamWebApi::cachePath(m_cache, QStringLiteral("test")), cache);
    rescan(*library);
    const QVariantMap kept = game(*library, QStringLiteral("steam-1000"));
    QVERIFY(kept.value("installed").toBool());
    QVERIFY(kept.value("steamOwnership").toString().isEmpty());
    QVERIFY(readFile(steamIndexPath()).contains("1000 = { title = \"Owned Game\", from_api = true"));
}

void LibrarySteamLibraryTest::ownedAndSharedIsOnlyOwned()
{
    installFakeSteam();
    const SteamWebApi::Cache cache = baseCache();
    auto library = openWith(&cache);
    showAll(*library);
    const QVariantMap both = game(*library, QStringLiteral("steam-2001"));
    QCOMPARE(both.value("steamOwnership").toString(), QStringLiteral("owned"));
    QVERIFY(both.value("sharedBy").toStringList().isEmpty());
    QVERIFY(readFile(steamIndexPath()).contains("2001 = { title = \"Both Have It\", owned = true, from_api = true, type = 0 }\n"));
}

void LibrarySteamLibraryTest::gamesThatLeaveGo()
{
    installFakeSteam();
    SteamWebApi::Cache cache = baseCache();
    auto library = openWith(&cache);
    showAll(*library);
    QVERIFY(library->rowOfGame(QStringLiteral("steam-1000")) >= 0);
    QVERIFY(library->rowOfGame(QStringLiteral("steam-2000")) >= 0);

    // Refunded, and a family member left: both come back from a successful answer without them.
    cache.owned = {{1001, QStringLiteral("A Soundtrack"), 0, 0}, {3000, QStringLiteral("Another"), 0, 0}};
    cache.types.insert(3000, 0);
    cache.shared = {{2001, QStringLiteral("Both Have It"), {kMe, kAlex}, 1, 0, 0}};
    SteamWebApi::saveCache(SteamWebApi::cachePath(m_cache, QStringLiteral("test")), cache);
    rescan(*library);
    showAll(*library);
    QCOMPARE(library->rowOfGame(QStringLiteral("steam-1000")), -1);
    QCOMPARE(library->rowOfGame(QStringLiteral("steam-2000")), -1);
    QVERIFY(library->rowOfGame(QStringLiteral("steam-3000")) >= 0);
    const QByteArray index = readFile(steamIndexPath());
    QVERIFY(!index.contains("1000 = "));
    QVERIFY(!index.contains("2000 = "));
    // The installed games from the manifests are never touched.
    QVERIFY(index.contains("2231450 = \"Pizza Tower\""));
    QVERIFY(index.contains("8000 = \"Tomb Raider: Anniversary\""));
}

void LibrarySteamLibraryTest::anEmptyCacheRemovesNothing()
{
    installFakeSteam();
    SteamWebApi::Cache cache = baseCache();
    auto library = openWith(&cache);
    const QByteArray before = readFile(steamIndexPath());
    QVERIFY(before.contains("1000 = "));

    // An empty or failed answer, or a privacy setting, leaves the cache with nothing: no games go.
    SteamWebApi::Cache empty;
    empty.steamId = kMe;
    SteamWebApi::saveCache(SteamWebApi::cachePath(m_cache, QStringLiteral("test")), empty);
    rescan(*library);
    QCOMPARE(readFile(steamIndexPath()), before);
    QFile::remove(SteamWebApi::cachePath(m_cache, QStringLiteral("test")));
    rescan(*library);
    QCOMPARE(readFile(steamIndexPath()), before);
    // Only the family list empty: the owned games still decide, and the family's stay.
    cache.shared.clear();
    SteamWebApi::saveCache(SteamWebApi::cachePath(m_cache, QStringLiteral("test")), cache);
    rescan(*library);
    QCOMPARE(readFile(steamIndexPath()), before);
}

void LibrarySteamLibraryTest::anotherAccountsCacheIsIgnored()
{
    installFakeSteam();
    SteamWebApi::Cache cache = baseCache();
    cache.steamId = QStringLiteral("76561198000000002");
    const QByteArray before = readFile(steamIndexPath());
    auto library = openWith(&cache);
    QVERIFY(!readFile(steamIndexPath()).contains("1000 = "));
    Q_UNUSED(before)
}

void LibrarySteamLibraryTest::blockedAndNonGamesAreSkipped()
{
    installFakeSteam();
    SteamWebApi::Cache cache = baseCache();
    cache.owned = {{228980, QStringLiteral("Steamworks Common Redistributables"), 0, 0},
                   {1070560, QStringLiteral("Steam Linux Runtime 1.0 (scout)"), 0, 0},
                   {4000, QStringLiteral("Proton 9.0"), 0, 0},
                   {4001, QStringLiteral("A Demo"), 0, 0},
                   {4002, QStringLiteral("Delisted Game"), 0, 0},
                   {4003, QStringLiteral("Not Asked Yet"), 0, 0},
                   {4004, QStringLiteral("Real Game"), 0, 0}};
    cache.types = {{228980, 0}, {1070560, 0}, {4000, 0}, {4001, 3}, {4002, SteamWebApi::kTypeNoStoreData}, {4004, 0}};
    cache.shared = {{4100, QStringLiteral("Family Tool"), {kAlex}, 3, 0, 0}};
    auto library = openWith(&cache);
    const QByteArray index = readFile(steamIndexPath());
    QVERIFY(!index.contains("228980"));
    QVERIFY(!index.contains("1070560"));
    QVERIFY(!index.contains("4000 = "));
    QVERIFY(!index.contains("4001 = "));
    QVERIFY(!index.contains("4003 = ")); // its type waits for the next fetch
    QVERIFY(!index.contains("4100 = "));
    QVERIFY(index.contains("4002 = "));  // the store no longer shows it, but it is owned
    QVERIFY(index.contains("4004 = "));
}

void LibrarySteamLibraryTest::turningItOffRemovesWhatTheApiBrought()
{
    const QString root = installFakeSteam();
    const SteamWebApi::Cache cache = baseCache();
    writeFile(root + QStringLiteral("/steamapps/appmanifest_1000.acf"),
              "\"AppState\"\n{\n\t\"appid\"\t\t\"1000\"\n\t\"name\"\t\t\"Owned Game\"\n\t\"StateFlags\"\t\t\"4\"\n}\n");
    auto library = openWith(&cache);
    showAll(*library);
    QVERIFY(library->rowOfGame(QStringLiteral("steam-2000")) >= 0);

    writeFile(m_data + QStringLiteral("/settings.toml"), "[steam]\nwhole_library = false\n");
    rescan(*library);
    QTRY_COMPARE_WITH_TIMEOUT(library->rowOfGame(QStringLiteral("steam-2000")), -1, 3000);
    const QByteArray index = readFile(steamIndexPath());
    QVERIFY(!index.contains("2000 = "));
    QVERIFY(!index.contains("2001 = "));
    // Installed: it stays, as a plain game.
    QVERIFY(index.contains("1000 = \"Owned Game\"\n"));
    QVERIFY(index.contains("2231450 = \"Pizza Tower\"\n"));
    QVERIFY(game(*library, QStringLiteral("steam-1000")).value("steamOwnership").toString().isEmpty());

    // And it stays off while the cache is still there.
    rescan(*library);
    QCOMPARE(readFile(steamIndexPath()), index);
}

void LibrarySteamLibraryTest::playTimeAndSizeComeFromTheApi()
{
    installFakeSteam();
    SteamWebApi::Cache cache = baseCache();
    // 2231450 is installed and played here: the API's numbers don't replace Steam's own.
    cache.owned.append({2231450, QStringLiteral("Pizza Tower"), 9999, 1790000000});
    cache.types.insert(2231450, 0);
    auto library = openWith(&cache);
    showAll(*library);
    const QVariantMap owned = game(*library, QStringLiteral("steam-1000"));
    QCOMPARE(owned.value("playMinutes").toInt(), 90);
    QVERIFY(!owned.value("sizeText").toString().isEmpty());
    QVERIFY(!owned.value("lastPlayedText").toString().isEmpty());
    const QVariantMap local = game(*library, QStringLiteral("steam-2231450"));
    QVERIFY(local.value("playMinutes").toInt() != 9999);
}

void LibrarySteamLibraryTest::aFileAloneIsHiddenOnceTheApiIsInvolved()
{
    installFakeSteam();
    auto library = openWith(nullptr);
    // Without the API, a game file alone is a game, as it always was.
    writeFile(gamePath(QStringLiteral("steam-777")), "title = \"Only A File\"\nplatform = \"steam\"\n");
    rescan(*library);
    showAll(*library);
    QVERIFY(library->rowOfGame(QStringLiteral("steam-777")) >= 0);

    const SteamWebApi::Cache cache = baseCache();
    SteamWebApi::saveCache(SteamWebApi::cachePath(m_cache, QStringLiteral("test")), cache);
    rescan(*library);
    showAll(*library);
    QCOMPARE(library->rowOfGame(QStringLiteral("steam-777")), -1);
    QVERIFY(QFileInfo::exists(gamePath(QStringLiteral("steam-777"))));
}

void LibrarySteamLibraryTest::theIndexFormatReadsBothForms()
{
    const auto parsed = parseSteamIndex(
        "[games]\n1 = \"Plain\"\n2 = { title = \"Table\", owned = true, shared_by = [\"7\", \"8\"], from_api = true, type = 0 }\n"
        "3 = { title = \"Odd\", mystery = 1 }\n",
        QStringLiteral("steam.toml"));
    QVERIFY(parsed.ok);
    QCOMPARE(parsed.value.size(), 3);
    QCOMPARE(parsed.value.at(0), (IndexEntry{1, QStringLiteral("Plain")}));
    IndexEntry table;
    table.appId = 2;
    table.title = QStringLiteral("Table");
    table.owned = true;
    table.sharedBy = {QStringLiteral("7"), QStringLiteral("8")};
    table.fromApi = true;
    table.storeType = 0;
    QCOMPARE(parsed.value.at(1), table);
    QVERIFY(anyContains(parsed.warnings, QStringLiteral("mystery")));
    // What is written reads back the same.
    const QString text = formatSteamIndex(QStringLiteral("# header\n"), {table, parsed.value.at(0)});
    QVERIFY(text.startsWith(QStringLiteral("# header\n[games]\n1 = \"Plain\"\n2 = { title = \"Table\"")));
    const auto again = parseSteamIndex(text.toUtf8(), QStringLiteral("steam.toml"));
    QCOMPARE(again.value.size(), 2);
    QCOMPARE(again.value.at(1), table);
    QVERIFY(again.warnings.isEmpty());
}

void LibrarySteamLibraryTest::nothingChangesWithoutACache()
{
    installFakeSteam();
    auto library = openWith(nullptr);
    const QByteArray before = readFile(steamIndexPath());
    QVERIFY(!before.contains("from_api"));
    const SteamWebApi::Cache cache = baseCache();
    SteamWebApi::saveCache(SteamWebApi::cachePath(m_cache, QStringLiteral("test")), cache);
    rescan(*library);
    // Written once, then left alone.
    const QDateTime modified = QFileInfo(steamIndexPath()).lastModified();
    const QByteArray after = readFile(steamIndexPath());
    QVERIFY(after != before);
    QTest::qWait(20);
    rescan(*library);
    QCOMPARE(readFile(steamIndexPath()), after);
    QCOMPARE(QFileInfo(steamIndexPath()).lastModified(), modified);
}

void LibrarySteamLibraryTest::aLargeLibraryIsFast()
{
    installFakeSteam();
    SteamWebApi::Cache cache = baseCache();
    cache.owned.clear();
    cache.shared.clear();
    for (int i = 0; i < 1500; ++i) {
        cache.owned.append({100000 + i, QStringLiteral("Game number %1").arg(i), i, 1700000000 + i});
        cache.types.insert(100000 + i, 0);
        cache.sizes.insert(100000 + i, 1000000LL * i);
    }
    SteamWebApi::saveCache(SteamWebApi::cachePath(m_cache, QStringLiteral("test")), cache);
    QElapsedTimer timer;
    timer.start();
    auto library = openWith(nullptr);
    qInfo() << "first scan with 1500 games from the API:" << timer.elapsed() << "ms";
    QVERIFY(readFile(steamIndexPath()).count("from_api = true") == 1500);
    timer.restart();
    rescan(*library);
    qInfo() << "a scan with nothing new:" << timer.elapsed() << "ms";
    QVERIFY(timer.elapsed() < 2000);
    showAll(*library);
    QCOMPARE(library->gameCount(), 28 + 1500);
}

QTEST_GUILESS_MAIN(LibrarySteamLibraryTest)
#include "library_steamlibrary_test.moc"
