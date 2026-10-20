// Timings for a 1000-game library. Not part of the default ctest run:
//
//   build/tests/library_bench
//
// Set LIBRARY_BENCH_KEEP=<dir> to keep the generated libraries in <dir>/files, <dir>/steam
// and <dir>/roms, e.g. to time the app's first frame on them with tools/screenshot.sh.

#include "library.h"
#include "libraryversion.h"
#include "steamshortcuts.h"

#include <QDir>
#include <QElapsedTimer>
#include <QRandomGenerator>
#include <QTemporaryDir>
#include <QTest>
#include <QTimeZone>

#include <algorithm>

namespace {

const QStringList kWords = {
    "Shadow", "Legend", "Crystal", "Iron", "Star", "Ancient", "Last", "Dark", "Tales", "Kingdom", "Dragon",
    "Night", "Lost", "Eternal", "Fallen", "Hollow", "Rising", "Sky", "Storm", "Blade", "Knight", "Quest",
    "Chronicles", "Echoes", "Frontier", "Wild", "Neon", "Pixel", "Garden", "Ocean", "Void", "Ember",
};
const QStringList kTags = {"Adventure", "Racing", "Party", "Platformer", "Co-op", "MMO", "JRPG",
                           "RPG", "Roguelike", "Metroidvania", "Puzzle", "Strategy"};

QString title(QRandomGenerator &rng)
{
    QStringList words;
    const int count = 2 + rng.bounded(3);
    for (int i = 0; i < count; ++i)
        words.append(kWords.at(rng.bounded(kWords.size())));
    if (rng.bounded(3) == 0)
        words.append(QString::number(2 + rng.bounded(4)));
    return words.join(u' ');
}

// Steam and Proton games' play time is Steam's (in localconfig.vdf), so theirs is left out.
QByteArray userKeys(QRandomGenerator &rng, bool withPlayTime)
{
    QByteArray out;
    QStringList tags;
    for (int i = 0, n = 1 + rng.bounded(3); i < n; ++i)
        tags.append(u'"' + kTags.at(rng.bounded(kTags.size())) + u'"');
    tags.removeDuplicates();
    out += "tags = [" + tags.join(QStringLiteral(", ")).toUtf8() + "]\n";
    if (rng.bounded(5) == 0)
        out += "favorite = true\n";
    // Drawn either way, so the rest of the library comes out the same.
    const QByteArray playTime = "play_minutes = " + QByteArray::number(rng.bounded(20000)) + "\n"
        + QStringLiteral("last_played = 2026-%1-%2T21:%3:00-03:00\n")
              .arg(1 + rng.bounded(9), 2, 10, QChar(u'0'))
              .arg(1 + rng.bounded(28), 2, 10, QChar(u'0'))
              .arg(rng.bounded(60), 2, 10, QChar(u'0'))
              .toUtf8();
    if (withPlayTime)
        out += playTime;
    return out;
}

void write(const QString &path, const QByteArray &text)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path);
    if (file.open(QIODevice::WriteOnly))
        file.write(text);
}

void writeCommon(const QString &dir)
{
    write(dir + "/library/library.toml", "version = 1\n");
    write(dir + "/library/platforms.toml",
          "[[platform]]\nid = \"switch\"\nname = \"Switch\"\nicon = \"switch\"\n\n"
          "[[platform]]\nid = \"3ds\"\nname = \"3DS\"\nicon = \"3ds\"\n\n"
          "[[platform]]\nid = \"steam\"\nname = \"Steam\"\nicon = \"steam\"\n\n"
          "[[platform]]\nid = \"proton\"\nname = \"Proton\"\nicon = \"proton\"\n");
    QByteArray tags;
    for (int i = 0; i < kTags.size(); ++i)
        tags += "[[tag]]\nname = \"" + kTags.at(i).toUtf8() + "\"\ncolor = " + QByteArray::number(i % 8) + "\n\n";
    write(dir + "/library/tags.toml", tags);
}

// The worst case: 1000 complete game files, each with an art folder.
void generateFiles(const QString &dir, int count)
{
    QRandomGenerator rng(1);
    writeCommon(dir);
    const QStringList platforms = {"switch", "3ds", "steam", "proton"};
    for (int i = 0; i < count; ++i) {
        const QString platform = platforms.at(i % platforms.size());
        const QString id = QStringLiteral("%1-game-%2").arg(platform).arg(i);
        QByteArray text = "title = \"" + title(rng).toUtf8() + "\"\n";
        text += "platform = \"" + platform.toUtf8() + "\"\n";
        text += "year = " + QByteArray::number(1990 + rng.bounded(36)) + "\n";
        text += "description = \"\"\"\nA plausible paragraph about this game, long enough to look like a real "
                "store description. It goes on for a sentence or two more, the way they do.\"\"\"\n";
        text += "rating = " + QByteArray::number(rng.bounded(50) / 10.0, 'f', 1) + "\n";
        text += userKeys(rng, platform != u"steam" && platform != u"proton");
        text += "added = 2026-01-12\n";
        // A program that isn't there: a stat each, and no folder to add up.
        if (platform == u"proton")
            text += "exe = \"~/Games/" + id.toUtf8() + "/game.exe\"\ninstall_dir = \"~/Games/" + id.toUtf8()
                    + "\"\nshortcut = " + QByteArray::number(3000000000u + i) + "\n";
        write(QStringLiteral("%1/library/games/%2.toml").arg(dir, id), text);
        write(QStringLiteral("%1/art/%2/icon.png").arg(dir, id), {});
        if (i % 3 == 0)
            write(QStringLiteral("%1/art/%2/hero.jpg").arg(dir, id), {});
    }
}

// The expected shape of an imported Steam library: a 1000-entry index, and files only for
// the ~50 games the user changed.
void generateSteam(const QString &dir, int count, int withFiles)
{
    QRandomGenerator rng(2);
    writeCommon(dir);
    QByteArray index = "[games]\n";
    for (int i = 0; i < count; ++i) {
        const int appId = 100000 + i * 37;
        index += QByteArray::number(appId) + " = \"" + title(rng).toUtf8() + "\"\n";
        if (i % (count / withFiles) == 0)
            write(QStringLiteral("%1/library/games/steam-%2.toml").arg(dir).arg(appId), userKeys(rng, false));
    }
    write(dir + "/library/steam.toml", index);
}

// A Steam install in the fake `home`, with a manifest and a localconfig.vdf block for each
// of generateSteam()'s games, and `shortcuts` non-Steam games with play time.
void generateSteamInstall(const QString &home, int count, int shortcuts)
{
    const QString root = home + "/.local/share/Steam";
    write(root + "/steamapps/libraryfolders.vdf",
          R"("libraryfolders"
{
	"0"
	{
		"path"		")" + root.toUtf8() + R"("
	}
}
)");
    write(root + "/config/loginusers.vdf", R"("users"
{
	"76561198000000001"
	{
		"PersonaName"		"Bench"
		"MostRecent"		"1"
	}
}
)");
    write(root + "/config/config.vdf", "\"InstallConfigStore\"\n{\n}\n");
    QByteArray apps;
    for (int i = 0; i < count; ++i) {
        const QByteArray appId = QByteArray::number(100000 + i * 37);
        write(root + "/steamapps/appmanifest_" + appId + ".acf",
              R"("AppState"
{
	"appid"		")" + appId + R"("
	"Universe"		"1"
	"name"		"Game )" + appId + R"("
	"StateFlags"		"4"
	"installdir"		"Game )" + appId + R"("
	"SizeOnDisk"		"123456789"
	"buildid"		"1"
	"InstalledDepots"
	{
	}
}
)");
        apps += "\t\t\t\t\t\"" + appId + "\"\n\t\t\t\t\t{\n\t\t\t\t\t\t\"LastPlayed\"\t\t\"1780000000\"\n"
                "\t\t\t\t\t\t\"Playtime\"\t\t\"" + QByteArray::number(i) + "\"\n\t\t\t\t\t}\n";
    }
    BinaryVdf::Node vdf;
    for (int i = 0; i < shortcuts; ++i) {
        SteamShortcut s;
        s.name = QStringLiteral("Shortcut %1").arg(i);
        s.exe = QStringLiteral("/home/user/Games/Shortcut %1/Game.exe").arg(i);
        s.startDir = QStringLiteral("/home/user/Games/Shortcut %1/").arg(i);
        s.launchOptions = QStringLiteral("mangohud %command%");
        s.appId = SteamShortcuts::newAppId(SteamShortcuts::exeField(s.exe), s.name, {});
        SteamShortcuts::add(&vdf, s);
        apps += "\t\t\t\t\t\"" + QByteArray::number(SteamShortcuts::signedAppId(s.appId))
                + "\"\n\t\t\t\t\t{\n\t\t\t\t\t\t\"Playtime\"\t\t\"" + QByteArray::number(i) + "\"\n\t\t\t\t\t}\n";
    }
    write(root + "/userdata/39734273/config/shortcuts.vdf", BinaryVdf::write(vdf));
    write(root + "/userdata/39734273/config/localconfig.vdf",
          "\"UserLocalConfigStore\"\n{\n\t\"Software\"\n\t{\n\t\t\"Valve\"\n\t\t{\n\t\t\t\"Steam\"\n\t\t\t{\n"
          "\t\t\t\t\"apps\"\n\t\t\t\t{\n" + apps + "\t\t\t\t}\n\t\t\t}\n\t\t}\n\t}\n}\n");
    QDir().mkpath(home + "/.steam");
    QFile::link(root, home + "/.steam/steam");
}

// ROM folders with `count` empty files named like real dumps, half Switch and half 3DS,
// and no index yet: the first scan adds them all.
void generateRoms(const QString &dir, int count)
{
    QRandomGenerator rng(3);
    write(dir + "/library/library.toml", "version = 1\n");
    write(dir + "/library/platforms.toml", R"([[platform]]
id = "switch"
name = "Switch"
emulators = ["ryubing"]
rom_folders = ["roms/switch"]

[[platform]]
id = "3ds"
name = "3DS"
emulators = ["azahar"]
rom_folders = ["roms/3ds"]
)");
    for (int i = 0; i < count; ++i) {
        const QString name = title(rng) + QStringLiteral(" %1").arg(i);
        if (i % 2 == 0) {
            // A few per folder, like a library sorted by series.
            const QString titleId = QStringLiteral("0100%1000").arg(i, 9, 16, QChar(u'0')).toUpper();
            write(QStringLiteral("%1/roms/switch/%2/%3 [%4][v0].nsp").arg(dir).arg(i % 20).arg(name, titleId), {});
        } else {
            write(QStringLiteral("%1/roms/3ds/%2 (USA).3ds").arg(dir, name), {});
        }
    }
}

// Median of `runs` timings of `f`, in ms.
template<typename F>
double median(int runs, F f)
{
    QList<double> times;
    for (int i = 0; i < runs; ++i) {
        QElapsedTimer t;
        t.start();
        f();
        times.append(t.nsecsElapsed() / 1e6);
    }
    std::sort(times.begin(), times.end());
    return times.at(times.size() / 2);
}

} // namespace

class LibraryBench : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void measure_data();
    void measure();
    void playHistoryStats();

private:
    QTemporaryDir m_tmp;
    QString m_root;
};

void LibraryBench::initTestCase()
{
    m_root = qEnvironmentVariable("LIBRARY_BENCH_KEEP", m_tmp.path());
    QDir(m_root + "/files").removeRecursively();
    QDir(m_root + "/steam").removeRecursively();
    QDir(m_root + "/roms").removeRecursively();
    QDir(m_root + "/steam-home").removeRecursively();
    generateFiles(m_root + "/files", 1000);
    generateSteam(m_root + "/steam", 1000, 50);
    generateSteamInstall(m_root + "/steam-home", 1000, 200);
    generateRoms(m_root + "/roms", 500);
    // So the app started on these (LIBRARY_BENCH_KEEP) finds the fake Steam, not the real one.
    write(m_root + "/steam/settings.toml",
          "[steam]\npath = \"" + m_root.toUtf8() + "/steam-home/.local/share/Steam\"\n");
    write(m_root + "/files/settings.toml", "[steam]\nenabled = false\n");
    write(m_root + "/roms/settings.toml", "[steam]\nenabled = false\n");
}

void LibraryBench::measure_data()
{
    QTest::addColumn<QString>("shape");
    QTest::addColumn<int>("games");
    QTest::newRow("1000 game files") << "files" << 1000;
    QTest::newRow("1000-entry steam.toml + 50 files") << "steam" << 1000;
    QTest::newRow("500 ROMs in two folders, no index yet") << "roms" << 500;
}

void LibraryBench::measure()
{
    QFETCH(QString, shape);
    QFETCH(int, games);
    const QString dataDir = m_root + u'/' + shape;
    const QString cacheDir = m_root + u'/' + shape + "-cache";
    QDir(cacheDir).removeRecursively();
    const QString cachePath = LibraryCache::pathFor(cacheDir, dataDir);
    constexpr int runs = 7;
    ScanOptions options;
    options.presets = Emulators::presets();
    options.installs.home = QDir::homePath(); // no install folders: detection costs nothing here
    options.steam = {m_root + "/steam-home", {}};
    options.copySteamArt = true; // Steam has no art for these games: the cost of looking each time

    // As at startup, before anything reads the library: library.toml gets its id now, not
    // from the Library below after the cache is saved, which would make the warm check stale.
    QVERIFY(!LibraryVersion::prepare(dataDir).readOnly);

    ScanResult cold;
    const double coldMs = median(runs, [&] { cold = scanLibrary(dataDir, {}, options); });
    QVERIFY(cold.snapshot.warnings().isEmpty());
    QVERIFY(cold.romsScanned);

    const double saveMs = median(runs, [&] { QVERIFY(LibraryCache::save(cachePath, dataDir, cold.snapshot)); });
    const qint64 cacheBytes = QFileInfo(cachePath).size();

    std::optional<LibrarySnapshot> loaded;
    const double loadMs = median(runs, [&] { loaded = LibraryCache::load(cachePath, dataDir); });
    QVERIFY(loaded);

    BuiltLibrary built;
    const double buildMs = median(runs, [&] {
        built = buildLibrary(loaded->content, loaded->art, loaded->romFiles, options.presets);
    });
    QCOMPARE(built.games.size(), games);
    // The library without Steam has Proton games, which say they need it.
    QVERIFY2(std::all_of(built.warnings.cbegin(), built.warnings.cend(),
                         [](const QString &w) { return w.contains(QStringLiteral("Proton games need Steam")); }),
             qPrintable(built.warnings.join(u'\n')));

    // Everything the main thread does before the first frame: cache load, merge, rows.
    const double libraryMs = median(runs, [&] {
        Library library(dataDir, cacheDir);
        QCOMPARE(library.gameCount(), games);
    });

    // A keystroke in the search box: the rows rebuilt for a search one letter longer or shorter.
    double searchMs = 0;
    double sortMs = 0;
    {
        Library library(dataDir, cacheDir);
        int keystroke = 0;
        searchMs = median(runs * 3, [&] {
            library.setSearch(++keystroke % 2 ? QStringLiteral("dragon kni") : QStringLiteral("dragon knig"));
        });
        // A sort change: the order and the rows rebuilt.
        library.setSearch({});
        int change = 0;
        sortMs = median(runs * 3, [&] {
            library.setSort(++change % 2 ? QStringLiteral("last_played") : QStringLiteral("play_time"));
        });
    }

    ScanResult warm;
    // A warm start's first check: the ROM folders are walked again, but nothing is new.
    loaded->installs = cold.snapshot.installs; // never cached, so always "changed" on a real start
    const double warmMs = median(runs, [&] { warm = scanLibrary(dataDir, *loaded, options); });
    QVERIFY(!warm.changed);
    QCOMPARE(warm.parsed, 0);
    QVERIFY(warm.romsScanned);
    QVERIFY(warm.romWrites.isEmpty());
    QVERIFY(warm.steamWrites.isEmpty());
    QVERIFY(!warm.steamChanged);

    // The Steam import alone, part of the scans above: every file read, then only stats.
    SteamSettings steamSettings;
    steamSettings.enabled = shape == u"steam";
    SteamState steam;
    const double steamColdMs = median(runs, [&] { steam = SteamScan::scan(steamSettings, options.steam, {}); });
    const double steamWarmMs = median(runs, [&] { QCOMPARE(SteamScan::scan(steamSettings, options.steam, steam), steam); });
    if (shape == u"steam") {
        QCOMPARE(steam.games().size(), games);
        QCOMPARE(steam.apps.size(), games + 200);
        QCOMPARE(steam.shortcuts.size(), 200);
        QCOMPARE(steam.shortcutConfig(steam.shortcuts.last().appId).playMinutes, 199);
    }

    qInfo().noquote() << QStringLiteral(
                             "\n  %1:\n"
                             "    cold parse (background, no cache): %2 ms, %3 files\n"
                             "    warm check (background, nothing changed): %4 ms, of which the ROM folders %10 ms\n"
                             "    cache load: %5 ms\n"
                             "    merge + sort: %6 ms\n"
                             "    Library from cache (main thread before the first frame): %7 ms\n"
                             "    search keystroke (rows rebuilt): %11 ms\n"
                             "    sort change (order and rows rebuilt): %12 ms\n"
                             "    cache save (background): %8 ms\n"
                             "    cache size: %9 KiB\n"
                             "    Steam import (part of the scans): %13 ms reading everything, %14 ms checking")
                             .arg(QString::fromUtf8(QTest::currentDataTag()))
                             .arg(coldMs, 0, 'f', 1)
                             .arg(cold.parsed)
                             .arg(warmMs, 0, 'f', 1)
                             .arg(loadMs, 0, 'f', 1)
                             .arg(buildMs, 0, 'f', 1)
                             .arg(libraryMs, 0, 'f', 1)
                             .arg(saveMs, 0, 'f', 1)
                             .arg(cacheBytes / 1024.0, 0, 'f', 0)
                             .arg(warm.romsMs, 0, 'f', 1)
                             .arg(searchMs, 0, 'f', 2)
                             .arg(sortMs, 0, 'f', 2)
                             .arg(steamColdMs, 0, 'f', 1)
                             .arg(steamWarmMs, 0, 'f', 1);
}

void LibraryBench::playHistoryStats()
{
    const QString dataDir = m_root + "/history";
    QDir(dataDir).removeRecursively();
    const QTimeZone zone = QTimeZone::fromSecondsAheadOfUtc(2 * 3600);
    const QDateTime first(QDate(2025, 1, 1), QTime(20, 0), zone);
    const QStringList platforms = {"switch", "steam", "proton", "linux"};
    QList<PlayHistory::StatsGame> games;
    for (int g = 0; g < 1000; ++g) {
        PlayHistory::StatsGame game;
        game.id = QStringLiteral("game-%1").arg(g);
        game.title = game.id;
        game.platform = platforms.at(g % platforms.size());
        QByteArray text;
        for (int s = 0; s < 100; ++s) {
            const int minutes = 20 + (g * 31 + s * 17) % 180;
            const QDateTime end = first.addDays(s * 6 + g % 6).addSecs(((g + s) % 5) * 3600);
            text += "[[session]]\nend = " + end.toString(Qt::ISODate).toUtf8() + "\nminutes = " + QByteArray::number(minutes) + "\n\n";
        }
        write(PlayHistory::path(dataDir, game.id), text);
        games.append(game);
    }
    const QDate today = first.date().addDays(600);

    const double loadMs = median(3, [&] { PlayHistory::load(dataDir, &games); });
    QCOMPARE(games.first().sessions.size(), 100);
    PlayHistory::MonthStats stats;
    const double computeMs = median(5, [&] { stats = PlayHistory::monthStats(games, today.year(), today.month(), today, Qt::Monday, zone); });
    QVERIFY(stats.minutes > 0);
    const double mapMs = median(5, [&] { PlayHistory::toVariantMap(stats, {}, {}); });
    PlayHistory::YearStats year;
    const double yearMs = median(5, [&] { year = PlayHistory::yearStats(games, today.year(), today, Qt::Monday, zone); });
    QVERIFY(year.minutes > 0);
    const double yearMapMs = median(5, [&] { PlayHistory::toVariantMap(year, {}, {}); });
    qInfo().noquote() << QStringLiteral("\n  month stats, 1000 games x 100 sessions:\n    read and parse the 1000 history files (background): %1 ms\n    the month's numbers from them (background): %2 ms\n    as a QML map: %3 ms\n    the year's numbers from them (background): %4 ms\n    as a QML map: %5 ms")
                             .arg(loadMs, 0, 'f', 1).arg(computeMs, 0, 'f', 1).arg(mapMs, 0, 'f', 2)
                             .arg(yearMs, 0, 'f', 1).arg(yearMapMs, 0, 'f', 2);
}

QTEST_GUILESS_MAIN(LibraryBench)
#include "library_bench.moc"
