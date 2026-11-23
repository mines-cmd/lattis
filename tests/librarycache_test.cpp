#include "apppaths.h"
#include "librarycache.h"

#include <QCryptographicHash>
#include <QDataStream>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>
#include <QTimeZone>

// The cache streams each struct's fields by hand. A struct that gains a field changes its size, and
// these fail until `everyFieldRoundTrips` sets the new field and the stream carries it (S7.11).
static_assert(sizeof(GameFile) == 744, "GameFile changed: stream the new field and set it in fullGame()");
static_assert(sizeof(Collection) == 384, "Collection changed: stream the new field and set it in the test");
static_assert(sizeof(ModManager) == 200, "ModManager changed: stream the new field and set it in the test");
static_assert(sizeof(RomEntry) == 128, "RomEntry changed: stream the new field and set it in the test");
static_assert(sizeof(RomFile) == 32, "RomFile changed: stream the new field and set it in the test");
static_assert(sizeof(EmulatorEntry) == 56, "EmulatorEntry changed: stream the new field and set it in the test");

static GameFile fullGame()
{
    GameFile g;
    g.title = QStringLiteral("T");
    g.platform = QStringLiteral("linux");
    g.rom = QStringLiteral("/r/x.rom");
    g.romLine = 7;
    g.description = QStringLiteral("D");
    g.year = 2001;
    g.rating = 4.5;
    g.ratingSource = QStringLiteral("igdb");
    g.tags = {QStringLiteral("a"), QStringLiteral("b")};
    g.favorite = true;
    g.status = QStringLiteral("playing");
    g.logoScale = 2.5;
    g.logoPosition = QStringLiteral("bottom_left");
    g.logoOffsetX = 0.25;
    g.logoOffsetY = -0.75;
    g.logoScrim = QStringLiteral("gradient");
    g.logoScrimSide = QStringLiteral("right");
    g.logoScrimStrength = 0.5;
    g.igdb = 11;
    g.steamgriddb = 12;
    g.steamApp = 13;
    g.unavailable = {QStringLiteral("u")};
    g.playMinutes = 14;
    g.lastPlayed = QDateTime(QDate(2025, 5, 5), QTime(5, 5), QTimeZone::UTC);
    g.added = QDate(2024, 4, 4);
    g.hidden = true;
    g.emulator = QStringLiteral("e");
    g.args = {QStringLiteral("--x")};
    g.launchOptions = QStringLiteral("lo");
    g.proton = QStringLiteral("p");
    g.skipSteamArt = {QStringLiteral("hero")};
    g.exe = QStringLiteral("/e.exe");
    g.startDir = QStringLiteral("/s");
    g.installDir = QStringLiteral("/i");
    g.mediaFolders = {QStringLiteral("/m")};
    g.updateSource = QStringLiteral("github:a/b");
    g.updateAsset = QStringLiteral("*.AppImage");
    g.updatePrereleases = true;
    g.savePaths = {QStringLiteral("/sv")};
    g.savesGame = QStringLiteral("sg");
    g.autoBackup = 1;
    g.noSaves = true;
    g.shortcut = 15;
    g.installed = false;
    g.sameAs = QStringLiteral("other");
    return g;
}

class LibraryCacheTest : public QObject
{
    Q_OBJECT

private slots:
    void init();

    void roundTrip();
    void missingFileIsNoCache();
    void versionMismatchIsNoCache();
    void otherDataDirIsNoCache();
    void garbageIsNoCache();
    void truncatedIsNoCache();
    void pathPerDataDir();
    void diffDetectsChanges();
    void everyFieldRoundTrips();
    void flippedByteIsRefusedAndRemoved();
    void hugeCountIsNoCache();

private:
    LibrarySnapshot sample() const;

    QTemporaryDir m_dir;
    QString m_cachePath;
    QString m_dataDir;
};

void LibraryCacheTest::init()
{
    QVERIFY(m_dir.isValid());
    m_cachePath = m_dir.filePath(QStringLiteral("cache/library.bin"));
    m_dataDir = m_dir.filePath(QStringLiteral("data"));
    QFile::remove(m_cachePath);
}

LibrarySnapshot LibraryCacheTest::sample() const
{
    LibrarySnapshot s;
    s.version = 1;
    s.files.insert(QStringLiteral("library/platforms.toml"), {{1000, 120}, {}});
    s.files.insert(QStringLiteral("library/games/steam-1145360.toml"),
                   {{2000, 300}, {QStringLiteral("library/games/steam-1145360.toml:3: unknown key x")}});
    s.content.platforms = {{"steam", "Steam", "steam"}, {"3ds", "3DS", "", {}, {}, 7, {37, 137}}};
    s.content.tags = {{"RPG", 5, {"genre:12", "keyword:jrpg"}, true}, {"Co-op", 4}, {"Solo", 1, {}, true}};
    s.content.steam = {{1145360, "Hades"}, {504230, "Celeste"}};
    Collection filtered;
    filtered.id = QStringLiteral("old-favorites");
    filtered.name = QStringLiteral("Old favorites");
    filtered.added = {"a"};
    filtered.rules = CollectionRules{};
    filtered.rules->tags = {"RPG"};
    filtered.rules->yearFrom = 1990;
    filtered.rules->yearTo = 1999;
    filtered.rules->minRating = 3.5;
    filtered.rules->playTimes = {"never", "10_50h"};
    filtered.rules->lastPlayed = QStringLiteral("older");
    filtered.rules->achievements = {"completed"};
    Collection handPicked;
    handPicked.id = QStringLiteral("picked");
    handPicked.name = QStringLiteral("Picked");
    s.content.collections = {filtered, handPicked};
    s.content.heroic = {{"epic-fez", "Fez", "epic", "FezApp", 12}, {"epic-hike", "A Short Hike", "epic", "HikeApp"}};
    s.heroic.install = {HeroicInstall::Kind::Flatpak, "/h/heroic", false, {"/bin/flatpak", {"run", "--no-gui"}, true}};
    s.heroic.installed.insert("epic-fez", {"/games/Fez", 1000, GameUpdate::Waiting});
    s.heroic.gone = {"epic-hike"};
    s.heroic.warnings = {"installed.json: not a JSON object"};
    GameFile g;
    g.title = QStringLiteral("Pokémon Ultra Sun");
    g.platform = QStringLiteral("3ds");
    g.description = QStringLiteral("Line one\nLine two");
    g.year = 2017;
    g.rating = 3.5;
    g.tags = {QStringLiteral("JRPG")};
    g.favorite = true;
    g.igdb = 0;
    g.steamgriddb = 5268;
    g.playMinutes = 4100;
    g.lastPlayed = QDateTime(QDate(2026, 9, 19), QTime(21, 4), QTimeZone::fromSecondsAheadOfUtc(-3 * 3600));
    g.added = QDate(2026, 1, 12);
    g.hidden = true;
    g.logoScale = 1.5;
    g.logoPosition = QStringLiteral("center_left");
    g.logoOffsetX = 0.125;
    g.logoOffsetY = -0.5;
    g.logoScrim = QStringLiteral("shadow");
    g.logoScrimSide = QStringLiteral("left");
    g.logoScrimStrength = 1.5;
    s.content.games.insert(QStringLiteral("3ds-pokemon-ultra-sun"), g);
    GameFile proton;
    proton.title = QStringLiteral("Mortal Shell 2");
    proton.platform = QStringLiteral("proton");
    proton.exe = QStringLiteral("~/Games/Mortal Shell II/MortalShell2.exe");
    proton.startDir = QStringLiteral("~/Games/Mortal Shell II/Binaries");
    proton.installDir = QStringLiteral("~/Games/Mortal Shell II");
    proton.shortcut = 2320877088;
    proton.installed = false;
    proton.launchOptions = QStringLiteral("mangohud %command%");
    proton.proton = QStringLiteral("proton_experimental");
    s.content.games.insert(QStringLiteral("proton-mortal-shell-2"), proton);
    s.programFiles.insert(QStringLiteral("proton-mortal-shell-2"),
                         {"/home/x/Games/Mortal Shell II/MortalShell2.exe", true, "/home/x/Games/Mortal Shell II",
                          50'000'000'000});
    GameFile appImage;
    appImage.title = QStringLiteral("Helium");
    appImage.platform = QStringLiteral("linux");
    appImage.exe = QStringLiteral("~/AppImages/Helium.AppImage");
    appImage.updateSource = QStringLiteral("github:imputnet/helium-linux");
    appImage.updateAsset = QStringLiteral("helium-*-x86_64.AppImage");
    appImage.updatePrereleases = true;
    s.content.games.insert(QStringLiteral("linux-helium"), appImage);
    s.content.games.insert(QStringLiteral("steam-1145360"), GameFile{});
    s.art.insert(QStringLiteral("steam-1145360"), {"/data/art/steam-1145360/hero.png", {}, "/x/icon.webp", {}});

    SteamState &steam = s.steam;
    steam.enabled = true;
    steam.install.kind = SteamInstall::Kind::Flatpak;
    steam.install.root = QStringLiteral("/home/x/.var/app/com.valvesoftware.Steam/.local/share/Steam");
    steam.install.users = {{"39734273", "76561198000000001", "Player One", "player_one", true, 1790445095}};
    steam.install.userId = QStringLiteral("39734273");
    steam.libraryFoldersStamp = {3000, 377};
    steam.libraries = {steam.install.root + "/steamapps"};
    steam.manifests.insert(steam.install.root + "/steamapps/appmanifest_1145360.acf",
                           {{4000, 600}, {1145360, "Hades", "/games/Hades", true, 15'000'000'000, GameUpdate::Paused, 1000, 45}, true});
    steam.localConfigPath = steam.install.root + "/userdata/39734273/config/localconfig.vdf";
    steam.localConfigStamp = {5000, 95809};
    steam.apps.insert(1145360, {4100, 1788735088, "mangohud %command%"});
    steam.apps.insert(620, {-1, 1700000000, {}});
    steam.apps.insert(-1974090208, {575, 0, {}});
    steam.shortcutsPath = steam.install.root + "/userdata/39734273/config/shortcuts.vdf";
    steam.shortcutsStamp = {5500, 3013};
    steam.shortcuts = {{2320877088u, "Mortal Shell 2", "/home/x/Games/Mortal Shell II/MortalShell2.exe", {},
                        "/home/x/Games/Mortal Shell II/", {}, " mangohud %command%", 1790446608, false},
                       {4253812108u, "Smash", "/home/x/ryujinx.appimage", "--fullscreen \"/g/x.nsp\"", "/home/x",
                        "/home/x/icon.png", {}, 0, true}};
    steam.shortcutsOk = true;
    steam.configStamp = {6000, 26676};
    steam.compatTools = {{0, "proton_experimental"}, {1145360, "GE-Proton10-4"}};
    steam.toolDirs = {steam.install.root + "/compatibilitytools.d"};
    steam.toolFiles.insert(steam.install.root + "/compatibilitytools.d/GE-Proton10-4/compatibilitytool.vdf", {7000, 500});
    steam.customTools = {{"GE-Proton10-4", "GE-Proton10-4"}};
    steam.warnings = {QStringLiteral("/x/appmanifest_1.acf: line 1: a string isn't closed")};
    return s;
}

void LibraryCacheTest::roundTrip()
{
    const LibrarySnapshot s = sample();
    QVERIFY(LibraryCache::save(m_cachePath, m_dataDir, s));
    const auto loaded = LibraryCache::load(m_cachePath, m_dataDir);
    QVERIFY(loaded);
    QCOMPARE(loaded->version, 1);
    QCOMPARE(loaded->files.size(), 2);
    QCOMPARE(loaded->files.value("library/platforms.toml").stamp, (FileStamp{1000, 120}));
    QCOMPARE(loaded->files.value("library/games/steam-1145360.toml").warnings,
             s.files.value("library/games/steam-1145360.toml").warnings);
    QCOMPARE(loaded->content.platforms, s.content.platforms);
    QCOMPARE(loaded->content.collections, s.content.collections);
    QCOMPARE(loaded->content.tags, s.content.tags);
    QCOMPARE(loaded->content.steam, s.content.steam);
    QCOMPARE(loaded->content.heroic, s.content.heroic);
    QCOMPARE(loaded->heroic, s.heroic);
    QCOMPARE(loaded->content.games, s.content.games);
    QCOMPARE(loaded->content.games.value("3ds-pokemon-ultra-sun").lastPlayed.offsetFromUtc(), -3 * 3600);
    QCOMPARE(loaded->art, s.art);
    QCOMPARE(loaded->programFiles, s.programFiles);
    QCOMPARE(loaded->steam, s.steam);
    QCOMPARE(loaded->warnings().size(), 3);
}

void LibraryCacheTest::missingFileIsNoCache()
{
    QVERIFY(!LibraryCache::load(m_cachePath, m_dataDir));
}

void LibraryCacheTest::versionMismatchIsNoCache()
{
    QVERIFY(LibraryCache::save(m_cachePath, m_dataDir, sample()));
    QFile file(m_cachePath);
    QVERIFY(file.open(QIODevice::ReadWrite));
    // The format version follows the 4-byte magic.
    file.seek(4);
    QDataStream out(&file);
    out << quint32(LibraryCache::kFormatVersion + 1);
    file.close();
    QVERIFY(!LibraryCache::load(m_cachePath, m_dataDir));
}

void LibraryCacheTest::otherDataDirIsNoCache()
{
    QVERIFY(LibraryCache::save(m_cachePath, m_dataDir, sample()));
    QVERIFY(!LibraryCache::load(m_cachePath, m_dataDir + QStringLiteral("-other")));
}

void LibraryCacheTest::garbageIsNoCache()
{
    QDir().mkpath(QFileInfo(m_cachePath).absolutePath());
    QFile file(m_cachePath);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("this is not a cache file at all");
    file.close();
    QVERIFY(!LibraryCache::load(m_cachePath, m_dataDir));
}

void LibraryCacheTest::truncatedIsNoCache()
{
    QVERIFY(LibraryCache::save(m_cachePath, m_dataDir, sample()));
    QFile file(m_cachePath);
    QVERIFY(file.open(QIODevice::ReadWrite));
    QVERIFY(file.resize(file.size() - 10));
    file.close();
    QVERIFY(!LibraryCache::load(m_cachePath, m_dataDir));
}

void LibraryCacheTest::pathPerDataDir()
{
    const QString a = LibraryCache::pathFor(QStringLiteral("/cache"), QStringLiteral("/home/me/data"));
    const QString b = LibraryCache::pathFor(QStringLiteral("/cache"), QStringLiteral("/tmp/scratch"));
    QVERIFY(a != b);
    QVERIFY(a.startsWith(QStringLiteral("/cache/library-")));
    QVERIFY(a.endsWith(QStringLiteral(".bin")));
    QCOMPARE(AppPaths::dataDirKey(QStringLiteral("/home/me/data")).size(), 12);
    QCOMPARE(AppPaths::dataDirKey(QStringLiteral("/home/me/data")), AppPaths::dataDirKey(QStringLiteral("/home/me/data/")));
}

void LibraryCacheTest::diffDetectsChanges()
{
    QHash<QString, CachedFile> cached;
    cached.insert(QStringLiteral("same"), {{10, 1}, {}});
    cached.insert(QStringLiteral("touched"), {{10, 1}, {}});
    cached.insert(QStringLiteral("resized"), {{10, 1}, {}});
    cached.insert(QStringLiteral("gone"), {{10, 1}, {}});
    const QHash<QString, FileStamp> onDisk{
        {QStringLiteral("same"), {10, 1}},
        {QStringLiteral("touched"), {11, 1}},
        {QStringLiteral("resized"), {10, 2}},
        {QStringLiteral("new"), {10, 1}},
    };
    const LibraryCache::Changes c = LibraryCache::diff(onDisk, cached);
    QCOMPARE(c.added, QStringList{"new"});
    QCOMPARE(c.changed, (QStringList{"resized", "touched"}));
    QCOMPARE(c.removed, QStringList{"gone"});
    QVERIFY(!c.isEmpty());
    QVERIFY(LibraryCache::diff({{"same", {10, 1}}}, {{"same", {{10, 1}, {}}}}).isEmpty());
}

void LibraryCacheTest::everyFieldRoundTrips()
{
    LibrarySnapshot s;
    s.content.games.insert(QStringLiteral("full"), fullGame());

    Collection c;
    c.id = QStringLiteral("c");
    c.name = QStringLiteral("C");
    c.rules = CollectionRules{};
    c.rules->platform = QStringLiteral("3ds");
    c.added = {QStringLiteral("a")};
    c.removed = {QStringLiteral("r")};
    c.sort = QStringLiteral("title");
    c.sortDescending = true;
    c.groupBy = QStringLiteral("platform");
    c.steamId = QStringLiteral("uc-1");
    s.content.collections = {c};

    ModManager m;
    m.id = QStringLiteral("m");
    m.name = QStringLiteral("M");
    m.exe = QStringLiteral("/m.exe");
    m.startDir = QStringLiteral("/m");
    m.launchOptions = QStringLiteral("lo");
    m.games = {QStringLiteral("g")};
    m.prefix = QStringLiteral("own");
    m.proton = QStringLiteral("p");
    m.shortcut = 99;
    m.inSteam = false;
    s.content.modManagers.insert(m.id, m);

    s.content.roms.insert(QStringLiteral("3ds"), {RomEntry{QStringLiteral("/r.3ds"), QStringLiteral("id"), QStringLiteral("Title"),
                                                           QStringLiteral("tid"), 123, QStringLiteral("file")}});
    s.romGameFiles.insert(QStringLiteral("g"), RomFile{QStringLiteral("/r.3ds"), 456});
    s.content.emulators = {EmulatorEntry{QStringLiteral("e"), {{QStringLiteral("k"), QStringLiteral("v")}}, QStringLiteral("/at")}};

    QVERIFY(LibraryCache::save(m_cachePath, m_dataDir, s));
    const auto loaded = LibraryCache::load(m_cachePath, m_dataDir);
    QVERIFY(loaded);
    QCOMPARE(loaded->content.games, s.content.games);
    QCOMPARE(loaded->content.collections, s.content.collections);
    QCOMPARE(loaded->content.modManagers, s.content.modManagers);
    QCOMPARE(loaded->content.roms, s.content.roms);
    QCOMPARE(loaded->romGameFiles, s.romGameFiles);
    QCOMPARE(loaded->content.emulators, s.content.emulators);
}

void LibraryCacheTest::flippedByteIsRefusedAndRemoved()
{
    QVERIFY(LibraryCache::save(m_cachePath, m_dataDir, sample()));
    QFile file(m_cachePath);
    QVERIFY(file.open(QIODevice::ReadWrite));
    QVERIFY(file.seek(file.size() - 5));
    char byte = 0;
    QVERIFY(file.read(&byte, 1) == 1);
    QVERIFY(file.seek(file.size() - 5));
    byte = char(byte ^ 0x01);
    QVERIFY(file.write(&byte, 1) == 1);
    file.close();
    QVERIFY(!LibraryCache::load(m_cachePath, m_dataDir));
    QVERIFY(!QFile::exists(m_cachePath));
}

void LibraryCacheTest::hugeCountIsNoCache()
{
    QByteArray body;
    {
        QDataStream out(&body, QIODevice::WriteOnly);
        out.setVersion(QDataStream::Qt_6_8);
        out << QDir(m_dataDir).absolutePath() << qint32(1) << quint32(0x7fffffffu);
    }
    QVERIFY(QDir().mkpath(QFileInfo(m_cachePath).absolutePath()));
    QFile file(m_cachePath);
    QVERIFY(file.open(QIODevice::WriteOnly));
    QDataStream out(&file);
    out << quint32(0x474c4c43) << LibraryCache::kFormatVersion << qint32(QDataStream::Qt_6_8)
        << QCryptographicHash::hash(body, QCryptographicHash::Sha256);
    file.write(body);
    file.close();
    std::optional<LibrarySnapshot> loaded;
    QVERIFY_THROWS_NO_EXCEPTION(loaded = LibraryCache::load(m_cachePath, m_dataDir));
    QVERIFY(!loaded);
}

QTEST_APPLESS_MAIN(LibraryCacheTest)
#include "librarycache_test.moc"
