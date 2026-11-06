#include "heroicmerge.h"
#include "heroicsettings.h"
#include "librarytestbase.h"

#include <QCryptographicHash>
#include <QImage>
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
    void countsOnlyWhatGrew();
    void anUnreadableTimestampCountsNothing();
    void firstImportStartsFromHeroicsTotal();
    void firstCountKeepsAFileWithPlayTime();
    void growthIsAddedToAnEdit();
    void aDroppedTotalRestartsTheCount();
    void aRestoredIndexAddsNothing();
    void aSessionUnderAMinuteChangesNothing();
    void aBrokenGameFileIsCountedOnceFixed();
    void aNewTimestampFileIsNoticed();
    void firstImportCopiesHeroicsCovers();
    void aRemovedCoverStaysRemoved();
    void aBrokenCachedImageIsSkipped();
    void aGameSaysWhoRunsIt();
    void settingsShowWhatWasFound();
    void settingsSaveTheirValues();

private:
    QString heroicRoot() const { return m_installs.home + u"/.config/heroic"_s; }
    QString heroicIndexPath() const { return m_data + u"/library/heroic.toml"_s; }
    void writeLibrary(const QByteArray &text) { writeFile(HeroicScan::libraryPath(heroicRoot()), text); }
    void writeInstalled(const QByteArray &text) { writeFile(HeroicScan::installedPath(heroicRoot()), text); }
    void writePlayTime(int minutes, const QString &lastPlayed = u"2026-11-01T10:00:00.000Z"_s)
    {
        writeFile(HeroicScan::playTimePath(heroicRoot()),
                  QJsonDocument(QJsonObject{{u"FezApp"_s, QJsonObject{{u"lastPlayed"_s, lastPlayed}, {u"totalPlayed"_s, minutes}}}})
                      .toJson(QJsonDocument::Compact));
    }
    // The list as these tests read it: with the games that aren't installed.
    std::unique_ptr<Library> openLibrary()
    {
        auto library = LibraryTestBase::openLibrary();
        library->setInstalledOnly(false);
        return library;
    }
    // Fez and A Short Hike with square art; Heroic has shown only the images written with writeCached().
    QByteArray libraryWithArt() const
    {
        const auto game = [](const QString &app, const QString &title) {
            return QJsonObject{{u"app_name"_s, app}, {u"title"_s, title}, {u"art_square"_s, squareUrl(app)},
                               {u"art_logo"_s, squareUrl(app + u"Logo"_s)},
                               {u"install"_s, QJsonObject{{u"is_dlc"_s, false}}}};
        };
        return QJsonDocument(QJsonObject{{u"library"_s, QJsonArray{game(u"FezApp"_s, u"Fez"_s),
                                                                   game(u"HikeApp"_s, u"A Short Hike"_s)}}})
            .toJson(QJsonDocument::Compact);
    }
    static QString squareUrl(const QString &app) { return u"https://cdn.test/"_s + app + u".jpg"_s; }
    QString cachedPath(const QString &app) const
    {
        const QByteArray asked = squareUrl(app).toUtf8() + "?h=400&resize=1&w=300";
        return heroicRoot() + u"/images-cache/"_s
               + QString::fromLatin1(QCryptographicHash::hash(asked, QCryptographicHash::Sha256).toHex());
    }
    void writeCached(const QString &app, const char *format)
    {
        QDir().mkpath(heroicRoot() + u"/images-cache"_s);
        QImage image(30, 40, QImage::Format_RGB32);
        image.fill(Qt::darkCyan);
        QVERIFY(image.save(cachedPath(app), format));
    }
    QByteArray fezFile() const { return readFile(m_data + u"/library/games/epic-fez.toml"_s); }
    QList<PlayHistory::Session> fezHistory() const { return PlayHistory::read(m_data, u"epic-fez"_s).value.sessions; }
    std::unique_ptr<Library> openWithFez(int minutes);
    void rescan(Library &library);
};

std::unique_ptr<Library> LibraryHeroicTest::openWithFez(int minutes)
{
    writeLibrary(libraryCache(kLibrary));
    writeInstalled(installedFile({u"FezApp"_s}));
    writePlayTime(minutes);
    return openLibrary();
}

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
    QVERIFY(index.contains("epic-a-short-hike = { title = \"A Short Hike\", store = \"epic\", app_name = \"HikeApp\", heroic_minutes = 0 }\n"));
    QVERIFY(index.contains("epic-fez = { title = \"Fez\", store = \"epic\", app_name = \"FezApp\", heroic_minutes = 0 }\n"));
    QVERIFY(!index.contains("HikeDlc"));
    QVERIFY(library->game(u"epic-fez"_s)->installed);
    QVERIFY(!library->game(u"epic-a-short-hike"_s)->installed);

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

void LibraryHeroicTest::countsOnlyWhatGrew()
{
    HeroicState state;
    state.playTimeOk = true;
    state.playTime = {{u"New"_s, {30, 100}}, {u"Grew"_s, {50, 200}}, {u"Same"_s, {7, 300}}, {u"Dropped"_s, {5, 400}}};
    QList<HeroicEntry> index = {entry(u"epic-new"_s, u"New"_s),
                                {u"epic-grew"_s, u"Grew"_s, kEpicPlatform, u"Grew"_s, 40},
                                {u"epic-same"_s, u"Same"_s, kEpicPlatform, u"Same"_s, 7},
                                {u"epic-dropped"_s, u"Dropped"_s, kEpicPlatform, u"Dropped"_s, 60},
                                entry(u"epic-unplayed"_s, u"Unplayed"_s),
                                {u"gog-other"_s, u"Other"_s, u"gog"_s, u"New"_s}};
    const QList<HeroicMerge::Played> played = HeroicMerge::countPlay(&index, state);
    QCOMPARE(played, (QList<HeroicMerge::Played>{{u"epic-new"_s, 30, true, 100}, {u"epic-grew"_s, 10, false, 200}}));
    QCOMPARE(index.at(0).heroicMinutes, 30);
    QCOMPARE(index.at(1).heroicMinutes, 50);
    QCOMPARE(index.at(3).heroicMinutes, 5);
    // Counted as 0, so what it's played from now on is growth.
    QCOMPARE(index.at(4).heroicMinutes, 0);
    QCOMPARE(index.at(5).heroicMinutes, -1);
}

void LibraryHeroicTest::anUnreadableTimestampCountsNothing()
{
    auto library = openWithFez(90);
    writeFile(HeroicScan::playTimePath(heroicRoot()), "not json");
    rescan(*library);
    QVERIFY(readFile(heroicIndexPath()).contains("heroic_minutes = 90"));
    QVERIFY(fezFile().contains("play_minutes = 90"));
}

void LibraryHeroicTest::firstImportStartsFromHeroicsTotal()
{
    auto library = openWithFez(90);
    QVERIFY(fezFile().contains("play_minutes = 90"));
    QVERIFY(fezFile().contains("last_played = "));
    // It's the game's past, not a session.
    QVERIFY(fezHistory().isEmpty());
    QVERIFY(readFile(heroicIndexPath()).contains("heroic_minutes = 90"));
    // A game Heroic never ran gets no file.
    QVERIFY(readFile(heroicIndexPath()).contains("heroic_minutes = 0"));
    QVERIFY(!QFile::exists(m_data + u"/library/games/epic-a-short-hike.toml"_s));
}

void LibraryHeroicTest::firstCountKeepsAFileWithPlayTime()
{
    writeFile(m_data + u"/library/heroic.toml"_s,
              "[games]\nepic-fez = { title = \"Fez\", store = \"epic\", app_name = \"FezApp\" }\n");
    writeFile(m_data + u"/library/games/epic-fez.toml"_s, "play_minutes = 30\n");
    auto library = openWithFez(90);
    QVERIFY(fezFile().contains("play_minutes = 30"));
    QVERIFY(readFile(heroicIndexPath()).contains("heroic_minutes = 90"));
}

void LibraryHeroicTest::growthIsAddedToAnEdit()
{
    auto library = openWithFez(90);
    QCOMPARE(library->store().setGameValues(u"epic-fez"_s, {{u"play_minutes"_s, 500}}), QString());
    writePlayTime(100, u"2026-11-02T10:00:00.000Z"_s);
    rescan(*library);
    QVERIFY(fezFile().contains("play_minutes = 510"));
    QVERIFY(readFile(heroicIndexPath()).contains("heroic_minutes = 100"));
    const QList<PlayHistory::Session> sessions = fezHistory();
    QCOMPARE(sessions.size(), 1);
    QCOMPARE(sessions.first().minutes, 10);
    QVERIFY(sessions.first().merged);
    QCOMPARE(sessions.first().end, QDateTime::fromString(u"2026-11-02T10:00:00.000Z"_s, Qt::ISODateWithMs));

    // Nothing new: nothing added twice.
    rescan(*library);
    QVERIFY(fezFile().contains("play_minutes = 510"));
    QCOMPARE(fezHistory().size(), 1);
}

void LibraryHeroicTest::aDroppedTotalRestartsTheCount()
{
    auto library = openWithFez(90);
    writePlayTime(20);
    rescan(*library);
    QVERIFY(fezFile().contains("play_minutes = 90"));
    QVERIFY(readFile(heroicIndexPath()).contains("heroic_minutes = 20"));
    QVERIFY(fezHistory().isEmpty());

    writePlayTime(25);
    rescan(*library);
    QVERIFY(fezFile().contains("play_minutes = 95"));
    QCOMPARE(fezHistory().size(), 1);
}

void LibraryHeroicTest::aRestoredIndexAddsNothing()
{
    writeFile(m_data + u"/library/heroic.toml"_s,
              "[games]\nepic-fez = { title = \"Fez\", store = \"epic\", app_name = \"FezApp\", heroic_minutes = 500 }\n");
    writeFile(m_data + u"/library/games/epic-fez.toml"_s, "play_minutes = 500\n");
    auto library = openWithFez(90); // this computer's Heroic has played it less
    QVERIFY(fezFile().contains("play_minutes = 500"));
    QVERIFY(readFile(heroicIndexPath()).contains("heroic_minutes = 90"));
    QVERIFY(fezHistory().isEmpty());
}

void LibraryHeroicTest::aSessionUnderAMinuteChangesNothing()
{
    auto library = openWithFez(90);
    const QDateTime edited = QDateTime::fromString(u"2020-01-01T10:00:00Z"_s, Qt::ISODate);
    QCOMPARE(library->store().setGameValues(u"epic-fez"_s, {{u"last_played"_s, edited}}), QString());
    const QByteArray before = fezFile();
    writePlayTime(90, u"2026-11-05T10:00:00.000Z"_s);
    rescan(*library);
    QCOMPARE(fezFile(), before);
    QVERIFY(fezHistory().isEmpty());
}

void LibraryHeroicTest::aBrokenGameFileIsCountedOnceFixed()
{
    auto library = openWithFez(90);
    const QByteArray good = fezFile();
    writeFile(m_data + u"/library/games/epic-fez.toml"_s, "play_minutes = = 1\n");
    writePlayTime(100);
    rescan(*library);
    QVERIFY(readFile(heroicIndexPath()).contains("heroic_minutes = 90"));
    QVERIFY(fezHistory().isEmpty());

    writeFile(m_data + u"/library/games/epic-fez.toml"_s, good);
    rescan(*library);
    QVERIFY(fezFile().contains("play_minutes = 100"));
    QVERIFY(readFile(heroicIndexPath()).contains("heroic_minutes = 100"));
    QCOMPARE(fezHistory().size(), 1);
}

void LibraryHeroicTest::aNewTimestampFileIsNoticed()
{
    auto library = openWithFez(90);
    // Heroic replaces the file on save, which drops a watch on it.
    for (const int minutes : {100, 110}) {
        const QString path = HeroicScan::playTimePath(heroicRoot());
        writeFile(path + u".new"_s, QJsonDocument(QJsonObject{{u"FezApp"_s, QJsonObject{{u"lastPlayed"_s, u"2026-11-02T10:00:00.000Z"_s},
                                                                                        {u"totalPlayed"_s, minutes}}}})
                                        .toJson(QJsonDocument::Compact));
        QVERIFY(QFile::remove(path));
        QVERIFY(QFile::rename(path + u".new"_s, path));
        QTRY_VERIFY_WITH_TIMEOUT(fezFile().contains("play_minutes = " + QByteArray::number(minutes)), 5000);
    }
}

void LibraryHeroicTest::firstImportCopiesHeroicsCovers()
{
    writeLibrary(libraryWithArt());
    writeCached(u"FezApp"_s, "PNG");
    writeCached(u"FezAppLogo"_s, "PNG");
    auto library = openLibrary();

    QVERIFY(QFile::exists(m_data + u"/art/epic-fez/cover.png"_s));
    QVERIFY(QFile::exists(m_data + u"/art/epic-fez/logo.png"_s));
    QVERIFY(!library->game(u"epic-fez"_s)->coverUrl.isEmpty());
    QVERIFY(!library->game(u"epic-fez"_s)->logoUrl.isEmpty());
    // Nothing is downloaded for a game Heroic hasn't shown.
    QVERIFY(!QFile::exists(m_data + u"/art/epic-a-short-hike"_s));
    QVERIFY(library->game(u"epic-a-short-hike"_s)->coverUrl.isEmpty());
    QCOMPARE(QDir(m_data + u"/art/epic-fez"_s).entryList(QDir::Files | QDir::Hidden).size(), 2);
}

void LibraryHeroicTest::aRemovedCoverStaysRemoved()
{
    writeLibrary(libraryWithArt());
    writeCached(u"FezApp"_s, "JPEG");
    auto library = openLibrary();
    const QString cover = m_data + u"/art/epic-fez/cover.jpg"_s;
    QVERIFY(QFile::remove(cover));
    rescan(*library);
    QVERIFY(!QFile::exists(cover));
}

void LibraryHeroicTest::aBrokenCachedImageIsSkipped()
{
    writeLibrary(libraryWithArt());
    writeFile(cachedPath(u"FezApp"_s), "not an image");
    auto library = openLibrary();
    QVERIFY(library->game(u"epic-fez"_s));
    QVERIFY(!QFile::exists(m_data + u"/art/epic-fez"_s));
}

void LibraryHeroicTest::aGameSaysWhoRunsIt()
{
    writeLibrary(libraryCache(kLibrary));
    auto library = openLibrary();
    QCOMPARE(library->game(u"epic-fez"_s)->launcher(), u"heroic"_s);
    QCOMPARE(library->gameAt(library->rowOfGame(u"epic-fez"_s)).value(u"launcher"_s).toString(), u"heroic"_s);

    const QHash<QString, QString> launchers = {{kSteamPlatform, u"steam"_s}, {kProtonPlatform, u"steam"_s},
                                               {kLinuxPlatform, QString()}, {u"switch"_s, QString()}};
    for (auto it = launchers.cbegin(); it != launchers.cend(); ++it) {
        Game game;
        game.platform = it.key();
        QCOMPARE(game.launcher(), it.value());
    }
}

void LibraryHeroicTest::settingsShowWhatWasFound()
{
    writeLibrary(libraryCache(kLibrary));
    writeInstalled(installedFile({u"FezApp"_s}));
    auto library = openLibrary();
    QVariantMap heroic = library->heroic();
    QVERIFY(heroic.value(u"enabled"_s).toBool());
    QVERIFY(heroic.value(u"found"_s).toBool());
    QVERIFY(heroic.value(u"wholeLibrary"_s).toBool());
    QVERIFY(!heroic.value(u"problem"_s).toBool());
    QCOMPARE(heroic.value(u"games"_s).toInt(), 2);
    QCOMPARE(heroic.value(u"installed"_s).toInt(), 1);
    QCOMPARE(heroic.value(u"root"_s).toString(), QFileInfo(heroicRoot()).canonicalFilePath());

    QSignalSpy changed(library.get(), &Library::heroicChanged);
    QCOMPARE(library->setHeroicValue(u"path"_s, u"/nowhere"_s), QString());
    library->store().waitForIdle();
    QTRY_VERIFY(!library->heroic().value(u"found"_s).toBool());
    QVERIFY(changed.count() > 0);
    heroic = library->heroic();
    QVERIFY(heroic.value(u"problem"_s).toBool());
    QVERIFY(heroic.value(u"status"_s).toString().contains(u"/nowhere"_s));
}

void LibraryHeroicTest::settingsSaveTheirValues()
{
    auto library = openLibrary();
    const auto settings = [this] { return readFile(m_data + u"/settings.toml"_s); };
    QCOMPARE(library->setHeroicValue(u"whole_library"_s, false), QString());
    QVERIFY(settings().contains("[heroic]"));
    QVERIFY(settings().contains("whole_library = false"));
    QCOMPARE(library->setHeroicValue(u"enabled"_s, false), QString());
    QVERIFY(settings().contains("enabled = false"));
    QCOMPARE(library->setHeroicValue(u"path"_s, u"~/heroic"_s), QString());
    QVERIFY(settings().contains("path = \"~/heroic\""));

    // A default isn't written.
    QCOMPARE(library->setHeroicValue(u"whole_library"_s, true), QString());
    QVERIFY(!settings().contains("whole_library"));
    QCOMPARE(library->setHeroicValue(u"path"_s, QString()), QString());
    QVERIFY(!settings().contains("~/heroic"));

    QVERIFY(!library->setHeroicValue(u"colour"_s, true).isEmpty());
}

QTEST_GUILESS_MAIN(LibraryHeroicTest)
#include "library_heroic_test.moc"
