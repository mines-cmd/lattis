#include "heroicmerge.h"
#include "heroicsettings.h"
#include "librarytestbase.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

using namespace Qt::StringLiterals;

namespace {
struct CacheGame
{
    QString appName;
    QString title;
    bool dlc = false;
};

QByteArray libraryCache(const QList<CacheGame> &games)
{
    QJsonArray list;
    for (const CacheGame &g : games) {
        list.append(QJsonObject{{u"app_name"_s, g.appName},
                                {u"title"_s, g.title},
                                {u"install"_s, QJsonObject{{u"is_dlc"_s, g.dlc}}}});
    }
    return QJsonDocument(QJsonObject{{u"library"_s, list}}).toJson(QJsonDocument::Compact);
}

QByteArray installedFile(const QStringList &appNames)
{
    QJsonObject out;
    for (const QString &app : appNames) {
        out.insert(app, QJsonObject{{u"title"_s, app},
                                    {u"install_path"_s, u"/games/"_s + app},
                                    {u"install_size"_s, 1000}});
    }
    return QJsonDocument(out).toJson(QJsonDocument::Compact);
}

const QList<CacheGame> kLibrary = {{u"HikeApp"_s, u"A Short Hike"_s},
                                   {u"FezApp"_s, u"Fez"_s},
                                   {u"HikeDlc"_s, u"A Short Hike: Hats"_s, true}};

HeroicEntry entry(const QString &id, const QString &app)
{
    return {id, id, kEpicPlatform, app};
}
}

class LibraryHeroicTest : public LibraryTestBase
{
    Q_OBJECT

private slots:
    void parsesTheIndex();
    void theIndexRoundTrips();
    void parsesTheSettings();
    void mergeNeverRemovesALine();
    void mergeWaitsWhileTheIndexIsBroken();
    void firstImportFillsTheIndex();
    void anUnchangedScanWritesNothing();
    void aGameLeavingTheCacheDropsOutAndComesBack();
    void aFileOfAGoneGameStaysHidden();
    void anInstalledGameStays();
    void anEmptyCacheRemovesNothing_data();
    void anEmptyCacheRemovesNothing();
    void anUnreadableInstalledFileRemovesNothing();
    void wholeLibraryOffListsTheInstalledOnes();
    void turnedOffKeepsTheGames();
    void aFixedTitleStays();
    void theCacheShowsTheListBeforeTheFirstScan();
    void aBrokenIndexIsNotOverwritten();
    void heroicShortcutsAreNotOffered();

private:
    QString heroicRoot() const { return m_installs.home + u"/.config/heroic"_s; }
    QString heroicIndexPath() const { return m_data + u"/library/heroic.toml"_s; }
    void writeLibrary(const QByteArray &text) { writeFile(HeroicScan::libraryPath(heroicRoot()), text); }
    void writeInstalled(const QByteArray &text) { writeFile(HeroicScan::installedPath(heroicRoot()), text); }
    void rescan(Library &library);
};

void LibraryHeroicTest::rescan(Library &library)
{
    QSignalSpy scanning(&library.store(), &LibraryStore::scanningChanged);
    library.store().refresh();
    library.store().waitForIdle();
    QVERIFY(QTest::qWaitFor([&] { return scanning.count() >= 2 && !library.store().scanning(); }, 10000));
}

void LibraryHeroicTest::parsesTheIndex()
{
    const auto r = parseHeroicIndex(R"([games]
epic-fez = { title = "Fez", store = "epic", app_name = "FezApp", heroic_minutes = 12 }
epic-hike = { title = "A Short Hike", store = "epic", app_name = "HikeApp" }
Bad_Id = { title = "x", store = "epic", app_name = "a" }
epic-no-app = { title = "x", store = "epic" }
epic-twice = { title = "Fez", store = "epic", app_name = "FezApp" }
epic-plain = "Just a title"
epic-odd = { title = "Odd", store = "epic", app_name = "Odd", heroic_minutes = -3, colour = "red" }
)", u"library/heroic.toml"_s);
    QVERIFY(r.ok);
    QCOMPARE(r.value.size(), 3);
    QCOMPARE(r.value.at(0), (HeroicEntry{u"epic-fez"_s, u"Fez"_s, u"epic"_s, u"FezApp"_s, 12}));
    QCOMPARE(r.value.at(1).heroicMinutes, -1);
    QCOMPARE(r.value.at(2).id, u"epic-odd"_s);
    QCOMPARE(r.warnings.size(), 6);

    QVERIFY(!parseHeroicIndex("[games", u"library/heroic.toml"_s).ok);
}

void LibraryHeroicTest::theIndexRoundTrips()
{
    const QList<HeroicEntry> entries = {{u"epic-z"_s, u"Quote \" and \\ slash"_s, u"epic"_s, u"z"_s, 0},
                                        {u"epic-a"_s, u"Título"_s, u"epic"_s, u"amzn1.adg.product"_s}};
    const QString text = formatHeroicIndex(u"# a comment\n"_s, entries);
    QVERIFY(text.startsWith(u"# a comment\n[games]\nepic-a = "_s));
    const auto back = parseHeroicIndex(text.toUtf8(), u"library/heroic.toml"_s);
    QVERIFY2(back.warnings.isEmpty(), qPrintable(back.warnings.join(u'\n')));
    QCOMPARE(back.value, (QList<HeroicEntry>{entries.at(1), entries.at(0)}));
}

void LibraryHeroicTest::parsesTheSettings()
{
    QVERIFY(parseHeroicSettings({}, u"settings.toml"_s).value.wholeLibrary);
    QVERIFY(parseHeroicSettings({}, u"settings.toml"_s).value.enabled);
    const auto r = parseHeroicSettings("[heroic]\nenabled = false\npath = \"~/h\"\nwhole_library = false\n",
                                       u"settings.toml"_s);
    QCOMPARE(r.value, (HeroicSettings{false, u"~/h"_s, false}));
    QVERIFY(r.warnings.isEmpty());
    const auto wrong = parseHeroicSettings("[heroic]\nwhole_library = 1\nwhole_libary = true\n", u"settings.toml"_s);
    QVERIFY(wrong.value.wholeLibrary);
    QCOMPARE(wrong.warnings.size(), 2);
}

void LibraryHeroicTest::mergeNeverRemovesALine()
{
    HeroicState state;
    state.libraryKnown = true;
    state.installedOk = true;
    state.library = {{u"Kept"_s, u"Kept"_s}};
    HeroicMerge::Input in;
    in.index = {entry(u"epic-kept"_s, u"Kept"_s), entry(u"epic-left"_s, u"Left"_s),
                {u"gog-other"_s, u"Other"_s, u"gog"_s, u"Other"_s}};
    in.state = &state;
    const HeroicMerge::Result out = HeroicMerge::merge(in);
    QCOMPARE(out.index, in.index);
    QVERIFY(out.added.isEmpty());
    // Only Epic's list speaks for Epic's games.
    QCOMPARE(out.gone, QSet<QString>{u"epic-left"_s});
}

void LibraryHeroicTest::mergeWaitsWhileTheIndexIsBroken()
{
    HeroicState state;
    state.libraryKnown = true;
    state.installedOk = true;
    state.library = {{u"New"_s, u"New"_s}};
    HeroicMerge::Input in;
    in.state = &state;
    in.canAdd = false;
    const HeroicMerge::Result out = HeroicMerge::merge(in);
    QVERIFY(out.index.isEmpty());
    QVERIFY(out.added.isEmpty());
}

void LibraryHeroicTest::firstImportFillsTheIndex()
{
    auto library = openLibrary();
    QVERIFY(!QFile::exists(heroicIndexPath()));
    QVERIFY(!readFile(m_data + u"/library/platforms.toml"_s).contains("\"epic\""));

    writeLibrary(libraryCache(kLibrary));
    writeInstalled(installedFile({u"FezApp"_s}));
    QSignalSpy added(&library->store(), &LibraryStore::gamesAdded);
    rescan(*library);

    const QByteArray index = readFile(heroicIndexPath());
    QVERIFY(index.startsWith("# Games found in Heroic"));
    QVERIFY(index.contains("epic-a-short-hike = { title = \"A Short Hike\", store = \"epic\", app_name = \"HikeApp\" }\n"));
    QVERIFY(index.contains("epic-fez = { title = \"Fez\", store = \"epic\", app_name = \"FezApp\" }\n"));
    QVERIFY(!index.contains("HikeDlc"));

    // Into the same lookup as Steam's whole library: one signal for the whole import.
    QCOMPARE(added.size(), 1);
    QStringList ids = added.first().first().toStringList();
    ids.sort();
    QCOMPARE(ids, (QStringList{u"epic-a-short-hike"_s, u"epic-fez"_s}));

    const QStringList listed = gameIds(*library);
    QVERIFY(listed.contains(u"epic-a-short-hike"_s));
    QVERIFY(listed.contains(u"epic-fez"_s));
    const LibrarySnapshot &snapshot = library->store().snapshot();
    QCOMPARE(snapshot.heroic.installed.value(u"epic-fez"_s), (HeroicInstalled{u"/games/FezApp"_s, 1000}));
    QVERIFY(!snapshot.heroic.installed.contains(u"epic-a-short-hike"_s));

    const QStringList platforms = TomlPatch::blockValues(QString::fromUtf8(readFile(m_data + u"/library/platforms.toml"_s)),
                                                         u"platform"_s, u"id"_s);
    QVERIFY(platforms.contains(u"epic"_s));
    QCOMPARE(platforms.indexOf(u"epic"_s), platforms.indexOf(kSteamPlatform) + 1);
    const auto epic = std::find_if(snapshot.content.platforms.cbegin(), snapshot.content.platforms.cend(),
                                   [](const Platform &p) { return p.id == kEpicPlatform; });
    QVERIFY(epic != snapshot.content.platforms.cend());
    QCOMPARE(igdbPlatformIds(*epic), QList<int>{6});
    QVERIFY(!anyContains(library->store().snapshot().warnings(), u"heroic"_s));
}

void LibraryHeroicTest::anUnchangedScanWritesNothing()
{
    writeLibrary(libraryCache(kLibrary));
    auto library = openLibrary();
    const QByteArray index = readFile(heroicIndexPath());
    const QDateTime written = QFileInfo(heroicIndexPath()).lastModified();
    QSignalSpy added(&library->store(), &LibraryStore::gamesAdded);
    const HeroicListing listing = library->store().snapshot().heroic;
    rescan(*library);
    rescan(*library);
    QVERIFY(added.isEmpty());
    QCOMPARE(library->store().snapshot().heroic, listing);
    QCOMPARE(readFile(heroicIndexPath()), index);
    QCOMPARE(QFileInfo(heroicIndexPath()).lastModified(), written);
    // Kept in memory, so the cache isn't parsed again.
    QCOMPARE(library->store().snapshot().heroicScan.library.size(), 2);
}

void LibraryHeroicTest::aGameLeavingTheCacheDropsOutAndComesBack()
{
    writeLibrary(libraryCache(kLibrary));
    auto library = openLibrary();
    QVERIFY(gameIds(*library).contains(u"epic-fez"_s));
    const QByteArray index = readFile(heroicIndexPath());

    writeLibrary(libraryCache({kLibrary.first()}));
    rescan(*library);
    QVERIFY(!gameIds(*library).contains(u"epic-fez"_s));
    QVERIFY(gameIds(*library).contains(u"epic-a-short-hike"_s));
    QCOMPARE(readFile(heroicIndexPath()), index);

    QSignalSpy added(&library->store(), &LibraryStore::gamesAdded);
    writeLibrary(libraryCache(kLibrary) + " ");
    rescan(*library);
    QVERIFY(gameIds(*library).contains(u"epic-fez"_s));
    QCOMPARE(readFile(heroicIndexPath()), index);
    QVERIFY(added.isEmpty()); // not new: it has its id, and whatever was looked up
}

void LibraryHeroicTest::aFileOfAGoneGameStaysHidden()
{
    writeLibrary(libraryCache(kLibrary));
    auto library = openLibrary();
    writeFile(gamePath(u"epic-fez"_s), "favorite = true\n");
    rescan(*library);
    QVERIFY(gameIds(*library).contains(u"epic-fez"_s));
    QVERIFY(library->store().snapshot().warnings().filter(u"epic-fez"_s).isEmpty());

    writeLibrary(libraryCache({kLibrary.first()}));
    rescan(*library);
    QVERIFY(!gameIds(*library).contains(u"epic-fez"_s));
    QVERIFY(QFile::exists(gamePath(u"epic-fez"_s)));
    QVERIFY(library->problems().filter(u"epic-fez"_s).isEmpty());
}

void LibraryHeroicTest::anInstalledGameStays()
{
    writeLibrary(libraryCache(kLibrary));
    writeInstalled(installedFile({u"FezApp"_s}));
    auto library = openLibrary();

    writeLibrary(libraryCache({kLibrary.first()}));
    rescan(*library);
    QVERIFY(gameIds(*library).contains(u"epic-fez"_s));
    QVERIFY(library->store().snapshot().heroic.installed.contains(u"epic-fez"_s));

    // Uninstalled in Heroic, and still not on its list: now it goes.
    writeInstalled("{}");
    rescan(*library);
    QVERIFY(!gameIds(*library).contains(u"epic-fez"_s));
}

void LibraryHeroicTest::anEmptyCacheRemovesNothing_data()
{
    QTest::addColumn<QByteArray>("cache");
    QTest::newRow("an empty list") << QByteArray(R"({"library":[]})");
    QTest::newRow("blanked by Heroic") << QByteArray("{}");
    QTest::newRow("corrupt") << QByteArray(R"({"library":[{"app_na)");
}

void LibraryHeroicTest::anEmptyCacheRemovesNothing()
{
    QFETCH(QByteArray, cache);
    writeLibrary(libraryCache(kLibrary));
    auto library = openLibrary();
    const QByteArray index = readFile(heroicIndexPath());

    writeLibrary(cache);
    rescan(*library);
    QVERIFY(gameIds(*library).contains(u"epic-fez"_s));
    QVERIFY(gameIds(*library).contains(u"epic-a-short-hike"_s));
    QVERIFY(library->store().snapshot().heroic.gone.isEmpty());
    QCOMPARE(readFile(heroicIndexPath()), index);
}

void LibraryHeroicTest::anUnreadableInstalledFileRemovesNothing()
{
    writeLibrary(libraryCache(kLibrary));
    writeInstalled(installedFile({u"FezApp"_s}));
    auto library = openLibrary();

    writeLibrary(libraryCache({kLibrary.first()}));
    writeInstalled("{ not json");
    rescan(*library);
    QVERIFY(gameIds(*library).contains(u"epic-fez"_s));
    QVERIFY(anyContains(library->store().snapshot().warnings(), u"installed.json"_s));
}

void LibraryHeroicTest::wholeLibraryOffListsTheInstalledOnes()
{
    writeLibrary(libraryCache(kLibrary));
    writeInstalled(installedFile({u"FezApp"_s}));
    auto library = openLibrary();
    const QByteArray index = readFile(heroicIndexPath());
    const QByteArray settings = demoSettings();

    writeFile(m_data + u"/settings.toml"_s, settings + "\n[heroic]\nwhole_library = false\n");
    rescan(*library);
    QVERIFY(gameIds(*library).contains(u"epic-fez"_s));
    QVERIFY(!gameIds(*library).contains(u"epic-a-short-hike"_s));
    QCOMPARE(readFile(heroicIndexPath()), index);

    writeFile(m_data + u"/settings.toml"_s, settings);
    rescan(*library);
    QVERIFY(gameIds(*library).contains(u"epic-a-short-hike"_s));
    QCOMPARE(readFile(heroicIndexPath()), index);
}

void LibraryHeroicTest::turnedOffKeepsTheGames()
{
    writeLibrary(libraryCache(kLibrary));
    writeInstalled(installedFile({u"FezApp"_s}));
    auto library = openLibrary();

    writeFile(m_data + u"/settings.toml"_s, demoSettings() + "\n[heroic]\nenabled = false\n");
    writeLibrary(libraryCache({{u"Brand"_s, u"Brand New"_s}}));
    rescan(*library);
    const LibrarySnapshot &snapshot = library->store().snapshot();
    QVERIFY(!snapshot.heroic.install.found());
    QVERIFY(snapshot.heroic.installed.isEmpty());
    QCOMPARE(snapshot.content.heroic.size(), 2);
    QVERIFY(gameIds(*library).contains(u"epic-fez"_s));
    QVERIFY(!gameIds(*library).contains(u"epic-brand-new"_s));
}

void LibraryHeroicTest::aFixedTitleStays()
{
    writeLibrary(libraryCache(kLibrary));
    auto library = openLibrary();
    QByteArray index = readFile(heroicIndexPath());
    index.replace("title = \"Fez\"", "title = \"FEZ\"");
    writeFile(heroicIndexPath(), index);
    rescan(*library);
    writeLibrary(libraryCache(kLibrary + QList<CacheGame>{{u"New"_s, u"New"_s}}));
    rescan(*library);
    const QByteArray after = readFile(heroicIndexPath());
    QVERIFY(after.contains("epic-fez = { title = \"FEZ\""));
    QVERIFY(after.contains("epic-new = "));
}

void LibraryHeroicTest::theCacheShowsTheListBeforeTheFirstScan()
{
    writeLibrary(libraryCache(kLibrary));
    writeInstalled(installedFile({u"FezApp"_s}));
    HeroicListing listing;
    {
        auto library = openLibrary();
        writeLibrary(libraryCache({{u"FezApp"_s, u"Fez"_s}}));
        rescan(*library);
        listing = library->store().snapshot().heroic;
        QVERIFY(listing.gone.contains(u"epic-a-short-hike"_s));
        library->store().forgetCache();
        QVERIFY(LibraryCache::save(LibraryCache::pathFor(m_cache, m_data), m_data, library->store().snapshot()));
    }
    const auto loaded = LibraryCache::load(LibraryCache::pathFor(m_cache, m_data), m_data);
    QVERIFY(loaded);
    QCOMPARE(loaded->heroic, listing);
    QVERIFY(loaded->heroicScan.library.isEmpty());

    // The scan after it reads Heroic's files again, and has nothing to tell.
    ScanOptions options;
    options.heroic = {m_installs.home, m_installs.home + u"/.config"_s, {}, {}};
    options.steam = {m_installs.home, {}};
    options.installs = m_installs;
    const ScanResult scan = scanLibrary(m_data, *loaded, options);
    QCOMPARE(scan.snapshot.heroic, listing);
    QCOMPARE(scan.snapshot.heroicScan.library.size(), 1);
}

void LibraryHeroicTest::aBrokenIndexIsNotOverwritten()
{
    writeLibrary(libraryCache(kLibrary));
    auto library = openLibrary();
    writeFile(heroicIndexPath(), "[games\n");
    writeLibrary(libraryCache(kLibrary + QList<CacheGame>{{u"New"_s, u"New"_s}}));
    QSignalSpy added(&library->store(), &LibraryStore::gamesAdded);
    rescan(*library);
    QCOMPARE(readFile(heroicIndexPath()), QByteArray("[games\n"));
    QVERIFY(added.isEmpty());
    QVERIFY(gameIds(*library).contains(u"epic-fez"_s));
}

void LibraryHeroicTest::heroicShortcutsAreNotOffered()
{
    const QString root = installFakeSteam();
    QFile::remove(gamePath(u"proton-diablo-iv"_s));
    QFile::remove(gamePath(u"proton-final-fantasy-xiv-online"_s));
    SteamShortcut heroic = shortcutIn(root + kShortcutsVdf, 2900000001);
    QVERIFY(heroic.appId != 0);
    heroic.launchOptions = u"--no-gui --no-sandbox \"heroic://launch?appName=HikeApp&runner=legendary\""_s;
    editShortcuts(root + kShortcutsVdf, [&](BinaryVdf::Node *doc) { QVERIFY(SteamShortcuts::update(doc, heroic)); });

    auto library = openLibrary();
    const QVariantList offered = library->steamShortcuts();
    QCOMPARE(offered.size(), 2);
    for (const QVariant &s : offered)
        QVERIFY(s.toMap().value(u"appId"_s).toLongLong() != 2900000001);

    const QStringList before = gameIds(*library);
    QCOMPARE(library->addSteamShortcuts({qint64(2900000001)}), QString());
    QCOMPARE(gameIds(*library), before);
}

QTEST_GUILESS_MAIN(LibraryHeroicTest)
#include "library_heroic_test.moc"
