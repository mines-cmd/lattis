#include "gamefile.h"
#include "libraryfiles.h"

#include <QDir>
#include <QTemporaryDir>
#include <QTest>
#include <QTimeZone>

namespace {

const QString kFile = QStringLiteral("library/games/hades.toml");

bool anyContains(const QStringList &list, const QString &needle)
{
    for (const QString &s : list) {
        if (s.contains(needle))
            return true;
    }
    return false;
}

const Game *findGame(const BuiltLibrary &lib, const QString &id)
{
    for (const Game &g : lib.games) {
        if (g.id == id)
            return &g;
    }
    return nullptr;
}

} // namespace

class LibraryFormatTest : public QObject
{
    Q_OBJECT

private slots:
    void gameEveryField();
    void gameDefaults();
    void gameScrimFalseIsNone();
    void gameWrongTypesFallBack();
    void gameOutOfRange();
    void gameUnknownKeySuggests();
    void gameSectionWarns();
    void gameSyntaxError();
    void gameTimeWithoutOffsetIsLocal();
    void gamePlainDateIsStartOfDay();
    void gameRatingAcceptsInteger();

    void validateAndFormat();
    void formatDateTimeKeepsOffset();
    void formatEscapesStrings();

    void libraryInfo();
    void libraryInfoWithoutVersion();
    void platformsKeepFileOrder();
    void platformsInvalidAndDuplicateIds();
    void platformsIgdbIds();
    void tags();
    void tagsIgdb();
    void steamIndex();
    void steamIndexInvalidEntries();

    void mergeFileWinsOverIndex();
    void mergeSkipsBlockedIndexEntries();
    void romIndex();
    void romIndexInvalidEntries();
    void mergeRomIndex();
    void mergeRomIdClash();
    void mergeRomKey();
    void findRomKeyFiles();
    void mergeEmulatorProblems();
    void mergeIndexEntryAlone();
    void mergeFileAlone();
    void mergeHiddenViaFile();
    void mergeUnknownPlatform();
    void mergeMissingTitleAndPlatform();
    void mergeTagColorsAndArt();
    void mergeSortsByTitle();

    void demoData();
    void docsListEveryField();
};

void LibraryFormatTest::docsListEveryField()
{
    QFile file(QStringLiteral(DOCS_DIR "/dev/library.md"));
    if (!file.open(QIODevice::ReadOnly))
        QSKIP("the library format docs aren't in the public repo");
    // Rows of the game file table: | `key` | type | default | description |
    QStringList keys;
    bool inSection = false;
    for (const QString &line : QString::fromUtf8(file.readAll()).split(u'\n')) {
        if (line.startsWith(QLatin1String("## ")) || line.startsWith(QLatin1String("### ")))
            inSection = line == QLatin1String("### `games/<id>.toml`");
        if (!inSection || !line.startsWith(QStringLiteral("| `")))
            continue;
        const QStringList cells = line.split(u'|');
        QVERIFY(cells.size() >= 5);
        const QString key = cells.at(1).trimmed().remove(u'`');
        const GameFields::Field *field = GameFields::find(key);
        QVERIFY2(field, qPrintable(QStringLiteral("library.md lists unknown key %1").arg(key)));
        QVERIFY2(cells.at(2).trimmed().startsWith(GameFields::typeName(field->type)), qPrintable(key));
        QCOMPARE(cells.at(3).trimmed().remove(u'`'), QString::fromLatin1(field->defaultText));
        keys.append(key);
    }
    QCOMPARE(keys, GameFields::keys());
}

void LibraryFormatTest::demoData()
{
    const QDir dir(QStringLiteral(DEMO_DATA_DIR "/library"));
    QVERIFY(dir.exists());
    const auto read = [&](const QString &name) {
        QFile f(dir.filePath(name));
        return f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray();
    };
    QStringList warnings;
    const auto check = [&](const auto &parsed) {
        QVERIFY(parsed.ok);
        warnings += parsed.warnings;
    };

    const Parsed<LibraryInfo> version = parseLibraryInfo(read("library.toml"), "library.toml");
    check(version);
    QCOMPARE(version.value.version, 1);

    LibraryContent c;
    const auto platforms = parsePlatforms(read("platforms.toml"), "platforms.toml");
    check(platforms);
    c.platforms = platforms.value;
    const auto tags = parseTags(read("tags.toml"), "tags.toml");
    check(tags);
    c.tags = tags.value;
    const auto steam = parseSteamIndex(read("steam.toml"), "steam.toml");
    check(steam);
    c.steam = steam.value;
    for (const QFileInfo &file : QDir(dir.filePath("games")).entryInfoList({"*.toml"}, QDir::Files)) {
        const ParsedGameFile g = parseGameFile(read("games/" + file.fileName()), file.fileName());
        check(g);
        c.games.insert(file.completeBaseName(), g.game);
    }

    for (const QString &platform : {QStringLiteral("switch"), QStringLiteral("3ds")}) {
        const auto roms = parseRomIndex(read("roms/" + platform + ".toml"), platform);
        check(roms);
        c.roms.insert(platform, roms.value);
    }

    // The demo's own fake Steam, which its settings.toml points at.
    SteamSettings steamSettings;
    steamSettings.path = QStringLiteral(DEMO_DATA_DIR "/steam");
    const SteamState steamState = SteamScan::scan(steamSettings, {QDir::tempPath(), {}}, {});
    QVERIFY(steamState.install.found());
    warnings += steamState.warnings;
    const QHash<QString, ProgramFiles> proton = findProgramFiles(c.games, QStringLiteral(DEMO_DATA_DIR), QDir::tempPath());

    const BuiltLibrary lib = buildLibrary(c, {}, {}, Emulators::presets(), {}, steamState, {}, proton);
    warnings += lib.warnings;
    QVERIFY2(warnings.isEmpty(), qPrintable(warnings.join(u'\n')));

    QCOMPARE(lib.games.size(), 25);
    QCOMPARE(lib.platforms.size(), 5);
    QStringList ids;
    for (const Platform &p : lib.platforms)
        ids.append(p.id);
    QCOMPARE(ids, (QStringList{"switch", "3ds", "steam", "proton", "linux"}));

    const Game *celeste = findGame(lib, QStringLiteral("steam-504230"));
    QVERIFY(celeste);
    QCOMPARE(celeste->title, QStringLiteral("Celeste"));
    QCOMPARE(celeste->platformName, QStringLiteral("Steam"));

    const Game *hades = findGame(lib, QStringLiteral("steam-1145360"));
    QVERIFY(hades);
    QCOMPARE(hades->title, QStringLiteral("Hades"));
    QVERIFY(hades->favorite);
    QCOMPARE(hades->tags.size(), 1);
    QCOMPARE(hades->tags.first().color, 6); // Roguelike: purple

    const Game *pokemon = findGame(lib, QStringLiteral("3ds-pokemon-ultra-sun"));
    QVERIFY(pokemon);
    QCOMPARE(pokemon->title, QStringLiteral("Pokémon Ultra Sun"));
    QCOMPARE(pokemon->tags.first().color, 5); // JRPG: blue

    const Game *kart = findGame(lib, QStringLiteral("switch-mario-kart-8-deluxe"));
    QVERIFY(kart);
    QCOMPARE(kart->tags.size(), 2);
    QCOMPARE(kart->tags.at(0).color, 1); // Racing: orange
    QCOMPARE(kart->tags.at(1).color, 2); // Party: yellow

    // Proton games: one installed, with Steam's play time; one whose program isn't there.
    const Game *ffxiv = findGame(lib, QStringLiteral("proton-final-fantasy-xiv-online"));
    QVERIFY(ffxiv);
    QVERIFY(ffxiv->installed);
    QCOMPARE(ffxiv->playMinutes, 16000);
    QCOMPARE(ffxiv->lastPlayed, QDateTime::fromString(QStringLiteral("2026-09-20T21:34:00-03:00"), Qt::ISODate));
    QCOMPARE(ffxiv->shortcut, 2458135266u);
    QCOMPARE(ffxiv->installDir, QStringLiteral(DEMO_DATA_DIR "/pc-games/Final Fantasy XIV Online"));
    const Game *diablo = findGame(lib, QStringLiteral("proton-diablo-iv"));
    QVERIFY(diablo);
    QVERIFY(!diablo->installed);
    QVERIFY(!diablo->uninstalled);
    QCOMPARE(diablo->playMinutes, 1500);
    QCOMPARE(diablo->launchOptions, QStringLiteral("mangohud %command%"));
}

void LibraryFormatTest::gameEveryField()
{
    const QByteArray toml = R"(title = "Hades"
platform = "steam"
year = 2020
description = """
Defy the god of the dead."""
rating = 4.5
tags = ["Roguelike", "Action"]
favorite = true
status = "finished"
igdb = 1942
steamgriddb = 0
play_minutes = 4100
last_played = 2026-09-19T21:04:00-03:00
added = 2026-01-12
hidden = true
emulator = "ryubing"
rom = "~/Downloads/Hades.nsp"
args = ["--fullscreen", "--no-gui"]
logo_scale = 1.5
logo_position = "center_right"
logo_offset_x = -0.25
logo_offset_y = 1
logo_scrim = "gradient"
logo_scrim_side = "full"
logo_scrim_strength = 0.5
)";
    const ParsedGameFile r = parseGameFile(toml, kFile);
    QVERIFY(r.ok);
    QVERIFY2(r.warnings.isEmpty(), qPrintable(r.warnings.join(u'\n')));
    const GameFile &g = r.game;
    QCOMPARE(g.title, QStringLiteral("Hades"));
    QCOMPARE(g.platform, QStringLiteral("steam"));
    QCOMPARE(g.year, 2020);
    QCOMPARE(g.description, QStringLiteral("Defy the god of the dead."));
    QCOMPARE(g.rating, 4.5);
    QCOMPARE(g.tags, (QStringList{"Roguelike", "Action"}));
    QVERIFY(g.favorite);
    QCOMPARE(g.status, QStringLiteral("finished"));
    QCOMPARE(g.igdb, 1942);
    QCOMPARE(g.steamgriddb, 0);
    QCOMPARE(g.playMinutes, 4100);
    QCOMPARE(g.lastPlayed.offsetFromUtc(), -3 * 3600);
    QCOMPARE(g.lastPlayed.toUTC(), QDateTime(QDate(2026, 9, 20), QTime(0, 4), QTimeZone::UTC));
    QCOMPARE(g.added, QDate(2026, 1, 12));
    QVERIFY(g.hidden);
    QCOMPARE(g.emulator, QStringLiteral("ryubing"));
    QCOMPARE(g.rom, QStringLiteral("~/Downloads/Hades.nsp"));
    QCOMPARE(g.romLine, 17);
    QCOMPARE(g.args, (QStringList{"--fullscreen", "--no-gui"}));
    QCOMPARE(g.logoScale, 1.5);
    QCOMPARE(g.logoPosition, QStringLiteral("center_right"));
    QCOMPARE(g.logoOffsetX, -0.25);
    QCOMPARE(g.logoOffsetY, 1.0);
    QCOMPARE(g.logoScrim, QStringLiteral("gradient"));
    QCOMPARE(g.logoScrimSide, QStringLiteral("full"));
    QCOMPARE(g.logoScrimStrength, 0.5);
}

void LibraryFormatTest::gameDefaults()
{
    const ParsedGameFile r = parseGameFile("favorite = true\n", kFile);
    QVERIFY(r.ok);
    QVERIFY(r.warnings.isEmpty());
    QVERIFY(r.game.title.isEmpty());
    QVERIFY(r.game.platform.isEmpty());
    QCOMPARE(r.game.year, 0);
    QCOMPARE(r.game.rating, -1.0);
    QCOMPARE(r.game.playMinutes, 0);
    QCOMPARE(r.game.logoScale, 1.0);
    QVERIFY(r.game.logoPosition.isEmpty());
    QCOMPARE(r.game.logoOffsetX, 0.0);
    QCOMPARE(r.game.logoOffsetY, 0.0);
    QVERIFY(r.game.logoScrim.isEmpty());
    QVERIFY(r.game.logoScrimSide.isEmpty());
    QCOMPARE(r.game.logoScrimStrength, 1.0);
    QVERIFY(!r.game.lastPlayed.isValid());
    QVERIFY(!r.game.added.isValid());
    QVERIFY(!r.game.hidden);
    QVERIFY(r.game.favorite);
}

void LibraryFormatTest::gameScrimFalseIsNone()
{
    const ParsedGameFile off = parseGameFile("logo_scrim = false\n", kFile);
    QVERIFY(off.ok);
    QVERIFY(off.warnings.isEmpty());
    QCOMPARE(off.game.logoScrim, QStringLiteral("none"));
    const ParsedGameFile on = parseGameFile("logo_scrim = true\n", kFile);
    QVERIFY(on.warnings.isEmpty());
    QVERIFY(on.game.logoScrim.isEmpty());
}

void LibraryFormatTest::gameWrongTypesFallBack()
{
    const QByteArray toml = R"(title = "Hades"
favorite = "yes"
year = "2020"
tags = ["ok", 3]
added = 2026-01-12T10:00:00
)";
    const ParsedGameFile r = parseGameFile(toml, kFile);
    QVERIFY(r.ok);
    QCOMPARE(r.game.title, QStringLiteral("Hades"));
    QVERIFY(!r.game.favorite);
    QCOMPARE(r.game.year, 0);
    QVERIFY(r.game.tags.isEmpty());
    QVERIFY(!r.game.added.isValid());
    QCOMPARE(r.warnings.size(), 4);
    QVERIFY(anyContains(r.warnings, kFile + QStringLiteral(":2: favorite must be true or false")));
    QVERIFY(anyContains(r.warnings, kFile + QStringLiteral(":3: year must be")));
    QVERIFY(anyContains(r.warnings, kFile + QStringLiteral(":4: tags must be")));
    QVERIFY(anyContains(r.warnings, kFile + QStringLiteral(":5: added must be a date")));
}

void LibraryFormatTest::gameOutOfRange()
{
    const ParsedGameFile r =
        parseGameFile("rating = 7\nplay_minutes = -5\nstatus = \"beaten\"\nigdb = -1\nsteamgriddb = \"5268\"\n"
                      "logo_scale = 9\nlogo_position = \"left\"\nlogo_offset_x = 11\nlogo_offset_y = -10.5\n"
                      "logo_scrim = \"glow\"\nlogo_scrim_side = \"middle\"\nlogo_scrim_strength = 2.5\n", kFile);
    QVERIFY(r.ok);
    QCOMPARE(r.game.rating, -1.0);
    QCOMPARE(r.game.playMinutes, 0);
    QCOMPARE(r.game.status, QString());
    QCOMPARE(r.game.igdb, -1);
    QCOMPARE(r.game.steamgriddb, -1);
    QCOMPARE(r.game.logoScale, 1.0);
    QVERIFY(r.game.logoPosition.isEmpty());
    QCOMPARE(r.game.logoOffsetX, 0.0);
    QCOMPARE(r.game.logoOffsetY, 0.0);
    QVERIFY(r.game.logoScrim.isEmpty());
    QVERIFY(r.game.logoScrimSide.isEmpty());
    QCOMPARE(r.game.logoScrimStrength, 1.0);
    QCOMPARE(r.warnings.size(), 12);
    QVERIFY(anyContains(r.warnings, QStringLiteral(":10: logo_scrim must be one of \"gradient\", \"shadow\", \"none\"")));
    QVERIFY(anyContains(r.warnings, QStringLiteral(":11: logo_scrim_side must be one of \"auto\"")));
    QVERIFY(anyContains(r.warnings, QStringLiteral(":12: logo_scrim_strength must be a number from 0 to 2")));
    QVERIFY(anyContains(r.warnings, QStringLiteral(":8: logo_offset_x must be a number from -10 to 10")));
    QVERIFY(anyContains(r.warnings, QStringLiteral(":9: logo_offset_y must be a number from -10 to 10")));
    QVERIFY(anyContains(r.warnings, QStringLiteral(":6: logo_scale must be a number from 0.2 to 4")));
    QVERIFY(anyContains(r.warnings, QStringLiteral(":7: logo_position must be one of \"bottom_left\"")));
    QVERIFY(anyContains(r.warnings, QStringLiteral(":4: igdb must be a whole number, 0 or more")));
    QVERIFY(anyContains(r.warnings, QStringLiteral(":5: steamgriddb must be a whole number, 0 or more")));
    QVERIFY(anyContains(r.warnings, QStringLiteral(":1: rating must be a number from 0 to 5")));
    QVERIFY(anyContains(r.warnings, QStringLiteral(":2: play_minutes must be a whole number, 0 or more")));
    QVERIFY(anyContains(r.warnings, QStringLiteral(":3: status must be one of \"not_started\", \"playing\", \"finished\", \"completed\", \"dropped\"")));
}

void LibraryFormatTest::gameUnknownKeySuggests()
{
    const ParsedGameFile r = parseGameFile("title = \"Hades\"\nfavourite = true\n", kFile);
    QVERIFY(r.ok);
    QCOMPARE(r.warnings.size(), 1);
    QVERIFY2(r.warnings.first().startsWith(kFile + QStringLiteral(":2: unknown key favourite")),
             qPrintable(r.warnings.first()));
    QVERIFY(r.warnings.first().contains(QStringLiteral("\"favorite\"")));
}

void LibraryFormatTest::gameSectionWarns()
{
    const ParsedGameFile r = parseGameFile("title = \"Hades\"\n\n[game]\nyear = 2020\n", kFile);
    QVERIFY(r.ok);
    QCOMPARE(r.warnings.size(), 1);
    QVERIFY(r.warnings.first().contains(QStringLiteral("unknown section [game]")));
}

void LibraryFormatTest::gameSyntaxError()
{
    const ParsedGameFile r = parseGameFile("title = \"Hades\"\nfavorite = \n", kFile);
    QVERIFY(!r.ok);
    QCOMPARE(r.warnings.size(), 1);
    QVERIFY2(r.warnings.first().startsWith(kFile + QStringLiteral(":2: ")), qPrintable(r.warnings.first()));
}

void LibraryFormatTest::gameTimeWithoutOffsetIsLocal()
{
    const ParsedGameFile r = parseGameFile("last_played = 2026-09-19T21:04:00\n", kFile);
    QVERIFY(r.ok);
    QCOMPARE(r.game.lastPlayed.timeSpec(), Qt::LocalTime);
    QCOMPARE(r.game.lastPlayed, QDateTime(QDate(2026, 9, 19), QTime(21, 4)));
}

void LibraryFormatTest::gamePlainDateIsStartOfDay()
{
    const ParsedGameFile r = parseGameFile("last_played = 2026-09-19\n", kFile);
    QVERIFY(r.ok);
    QVERIFY(r.warnings.isEmpty());
    QCOMPARE(r.game.lastPlayed, QDate(2026, 9, 19).startOfDay());
}

void LibraryFormatTest::gameRatingAcceptsInteger()
{
    const ParsedGameFile r = parseGameFile("rating = 4\n", kFile);
    QVERIFY(r.warnings.isEmpty());
    QCOMPARE(r.game.rating, 4.0);
}

void LibraryFormatTest::validateAndFormat()
{
    QString error;
    QCOMPARE(GameFields::validate("favorite", true, &error), QVariant(true));
    QVERIFY(!GameFields::validate("favorite", 1, &error).isValid());
    QVERIFY(error.contains(QStringLiteral("true or false")));
    QVERIFY(!GameFields::validate("nope", 1, &error).isValid());
    QVERIFY(!GameFields::validate("rating", 5.5, &error).isValid());
    QCOMPARE(GameFields::validate("play_minutes", 12.0, &error), QVariant(12));
    QCOMPARE(GameFields::validate("tags", QVariantList{"a", "b"}, &error), QVariant(QStringList{"a", "b"}));

    QCOMPARE(GameFields::validate("status", QStringLiteral("dropped"), &error), QVariant(QStringLiteral("dropped")));
    QVERIFY(!GameFields::validate("status", QStringLiteral("Dropped"), &error).isValid());
    QVERIFY(GameFields::isCleared("status", QString()));

    QCOMPARE(GameFields::format("favorite", true), QStringLiteral("true"));
    QCOMPARE(GameFields::format("status", QStringLiteral("playing")), QStringLiteral("\"playing\""));
    QCOMPARE(GameFields::format("play_minutes", 4100), QStringLiteral("4100"));
    QCOMPARE(GameFields::format("rating", 4.0), QStringLiteral("4.0"));
    QCOMPARE(GameFields::format("rating", 3.5), QStringLiteral("3.5"));
    QCOMPARE(GameFields::format("tags", QStringList{"RPG", "Co-op"}), QStringLiteral("[\"RPG\", \"Co-op\"]"));
    QCOMPARE(GameFields::format("added", QDate(2026, 1, 12)), QStringLiteral("2026-01-12"));

    // Whatever format writes, the parser reads back.
    const QString line = QStringLiteral("rating = %1\n").arg(GameFields::format("rating", 4.0));
    QCOMPARE(parseGameFile(line.toUtf8(), kFile).game.rating, 4.0);
}

void LibraryFormatTest::formatDateTimeKeepsOffset()
{
    const QDateTime withOffset(QDate(2026, 9, 19), QTime(21, 4), QTimeZone::fromSecondsAheadOfUtc(-3 * 3600));
    QCOMPARE(GameFields::format("last_played", withOffset), QStringLiteral("2026-09-19T21:04:00-03:00"));

    // Local times get this machine's current offset written out.
    const QDateTime local(QDate(2026, 9, 19), QTime(21, 4));
    const QString text = GameFields::format("last_played", local);
    QVERIFY2(text.size() == 25 || text.endsWith(u'Z'), qPrintable(text));
    const ParsedGameFile r = parseGameFile(QStringLiteral("last_played = %1\n").arg(text).toUtf8(), kFile);
    QCOMPARE(r.game.lastPlayed, local);
}

void LibraryFormatTest::formatEscapesStrings()
{
    const QString tricky = QStringLiteral("Say \"hi\"\\\nnext\tline");
    const QString text = GameFields::format("description", tricky);
    const ParsedGameFile r = parseGameFile(QStringLiteral("description = %1\n").arg(text).toUtf8(), kFile);
    QVERIFY(r.ok);
    QCOMPARE(r.game.description, tricky);
}

void LibraryFormatTest::libraryInfo()
{
    const Parsed<LibraryInfo> r = parseLibraryInfo("# comment\nversion = 3\nid = \"abc\"\n", "library/library.toml");
    QVERIFY(r.ok);
    QCOMPARE(r.value.version, 3);
    QCOMPARE(r.value.id, QStringLiteral("abc"));
    QVERIFY(r.warnings.isEmpty());

    // An id that isn't text counts as none: the app makes a new one.
    const Parsed<LibraryInfo> number = parseLibraryInfo("version = 1\nid = 5\n", "library/library.toml");
    QVERIFY(number.ok);
    QVERIFY(number.value.id.isEmpty());
    QCOMPARE(number.warnings.size(), 1);
}

void LibraryFormatTest::libraryInfoWithoutVersion()
{
    const Parsed<LibraryInfo> r = parseLibraryInfo("# empty\n", "library/library.toml");
    QVERIFY(r.ok);
    QCOMPARE(r.value.version, 1);
    QVERIFY(r.value.id.isEmpty());
    QCOMPARE(r.warnings.size(), 1);
}

void LibraryFormatTest::platformsKeepFileOrder()
{
    const QByteArray toml = R"(# Platforms
[[platform]]
id = "switch"
name = "Switch"
icon = "switch"
emulators = ["ryubing", "eden"]
rom_folders = ["/mnt/games/switch", "~/roms/switch"]

[[platform]]
id = "3ds"
name = "3DS"

[[platform]]
id = "amiga"
)";
    const auto r = parsePlatforms(toml, "library/platforms.toml");
    QVERIFY(r.ok);
    QVERIFY(r.warnings.isEmpty());
    QCOMPARE(r.value.size(), 3);
    QCOMPARE(r.value.at(0), (Platform{"switch", "Switch", "switch", {"ryubing", "eden"},
                                      {"/mnt/games/switch", "~/roms/switch"}, 2}));
    QCOMPARE(r.value.at(1), (Platform{"3ds", "3DS", "", {}, {}, 9}));
    QCOMPARE(r.value.at(2), (Platform{"amiga", "amiga", "", {}, {}, 13})); // name defaults to the id
}

void LibraryFormatTest::platformsIgdbIds()
{
    const QByteArray toml = R"([[platform]]
id = "switch"
icon = "switch"

[[platform]]
id = "handhelds"
icon = "3ds"
igdb_platforms = [37, 20]

[[platform]]
id = "gamecube"

[[platform]]
id = "amiga"

[[platform]]
id = "wrong"
igdb_platforms = [0, "130"]
)";
    const auto r = parsePlatforms(toml, "library/platforms.toml");
    QVERIFY(r.ok);
    QCOMPARE(r.value.size(), 5);
    QCOMPARE(igdbPlatformIds(r.value.at(0)), (QList<int>{130})); // built in, by icon
    QCOMPARE(igdbPlatformIds(r.value.at(1)), (QList<int>{37, 20})); // written ones win
    QCOMPARE(igdbPlatformIds(r.value.at(2)), (QList<int>{21})); // built in, by id
    QCOMPARE(igdbPlatformIds(r.value.at(3)), QList<int>()); // every platform
    QCOMPARE(r.value.at(4).igdbPlatforms, QList<int>());
    QCOMPARE(igdbPlatformIds(Platform{.id = QStringLiteral("linux")}), (QList<int>{3, 6})); // Linux, and PC
    QCOMPARE(r.warnings.size(), 1);
    QVERIFY(anyContains(r.warnings, QStringLiteral(":18: igdb_platforms must be a list of IGDB platform ids")));
}

void LibraryFormatTest::platformsInvalidAndDuplicateIds()
{
    const QByteArray toml = R"([[platform]]
id = "Switch"

[[platform]]
id = "3ds"

[[platform]]
id = "3ds"
nmae = "Dup"

[[platform]]
name = "No id"

[switch]
name = "Wrong shape"
)";
    const auto r = parsePlatforms(toml, "library/platforms.toml");
    QVERIFY(r.ok);
    QCOMPARE(r.value.size(), 1);
    QCOMPARE(r.value.first().id, QStringLiteral("3ds"));
    QCOMPARE(r.warnings.size(), 5);
    QVERIFY(anyContains(r.warnings, QStringLiteral(":1: platform id \"Switch\" may only use")));
    QVERIFY(anyContains(r.warnings, QStringLiteral("used twice")));
    QVERIFY(anyContains(r.warnings, QStringLiteral(":9: unknown key nmae")));
    QVERIFY(anyContains(r.warnings, QStringLiteral("without an id")));
    QVERIFY(anyContains(r.warnings, QStringLiteral("unknown key switch; entries go in [[platform]] blocks")));
}

void LibraryFormatTest::tags()
{
    const QByteArray toml = R"([[tag]]
name = "Adventure"
color = 0

[[tag]]
name = "JRPG"
color = 5

[[tag]]
name = "Bad"
color = -1

[[tag]]
color = 2
)";
    const auto r = parseTags(toml, "library/tags.toml");
    QVERIFY(r.ok);
    QCOMPARE(r.value, (QList<TagDef>{{"Adventure", 0}, {"JRPG", 5}, {"Bad", 0}}));
    QCOMPARE(r.warnings.size(), 2);
}

void LibraryFormatTest::tagsIgdb()
{
    const QByteArray toml = R"([[tag]]
name = "Roguelike"
igdb = ["keyword:roguelike", "genre:8", "theme:19", "mode:3", "keyword:deck-building--1", "genre:8"]

[[tag]]
name = "Off"
igdb = []

[[tag]]
name = "Bad"
igdb = ["genre:rpg", "platform:6", "keyword:Rogue Like", 5, "genre:0", "theme:21"]

[[tag]]
name = "NotAList"
igdb = "genre:8"
)";
    const auto r = parseTags(toml, "library/tags.toml");
    QVERIFY(r.ok);
    QCOMPARE(r.value.size(), 4);
    // Every kind; a repeat is dropped.
    QCOMPARE(r.value[0].igdb,
             (QStringList{"keyword:roguelike", "genre:8", "theme:19", "mode:3", "keyword:deck-building--1"}));
    QVERIFY(r.value[0].igdbGiven);
    // [] is given, and empty.
    QVERIFY(r.value[1].igdb.isEmpty());
    QVERIFY(r.value[1].igdbGiven);
    // Bad entries are skipped with a warning each, with the line.
    QCOMPARE(r.value[2].igdb, QStringList{"theme:21"});
    QVERIFY(r.value[2].igdbGiven);
    // Not a list: a warning, and as if it weren't there.
    QVERIFY(!r.value[3].igdbGiven);
    QCOMPARE(r.warnings.size(), 6);
    QVERIFY(r.warnings.first().startsWith("library/tags.toml:11:"));
    QVERIFY(r.warnings.last().contains("igdb must be a list"));
}

void LibraryFormatTest::steamIndex()
{
    const QByteArray toml = R"(# Steam games
[games]
1145360 = "Hades"
504230 = "Celeste"
)";
    const auto r = parseSteamIndex(toml, "library/steam.toml");
    QVERIFY(r.ok);
    QVERIFY(r.warnings.isEmpty());
    QCOMPARE(r.value.size(), 2);
    QVERIFY(r.value.contains(IndexEntry{1145360, "Hades"}));
    QVERIFY(r.value.contains(IndexEntry{504230, "Celeste"}));
}

void LibraryFormatTest::steamIndexInvalidEntries()
{
    const QByteArray toml = R"(version = 1
[games]
hades = "Hades"
-5 = "Negative"
0 = "Zero"
504230 = 12
367520 = "Hollow Knight"
)";
    const auto r = parseSteamIndex(toml, "library/steam.toml");
    QVERIFY(r.ok);
    QCOMPARE(r.value, (QList<IndexEntry>{{367520, "Hollow Knight"}}));
    QCOMPARE(r.warnings.size(), 5);
    QVERIFY(anyContains(r.warnings, QStringLiteral(":3: \"hades\" is not a Steam app ID")));
    QVERIFY(anyContains(r.warnings, QStringLiteral(":6: the title of 504230 must be text")));
}

void LibraryFormatTest::mergeFileWinsOverIndex()
{
    LibraryContent c;
    c.platforms = {{"steam", "Steam", "steam"}};
    c.steam = {{1145360, "Hades"}};
    GameFile f;
    f.title = QStringLiteral("Hades (renamed)");
    f.favorite = true;
    c.games.insert(QStringLiteral("steam-1145360"), f);

    const BuiltLibrary lib = buildLibrary(c, {});
    QVERIFY(lib.warnings.isEmpty());
    QCOMPARE(lib.games.size(), 1);
    const Game &g = lib.games.first();
    QCOMPARE(g.id, QStringLiteral("steam-1145360"));
    QCOMPARE(g.title, QStringLiteral("Hades (renamed)"));
    QCOMPARE(g.platform, QStringLiteral("steam"));
    QCOMPARE(g.platformName, QStringLiteral("Steam"));
    QVERIFY(g.favorite);
}

void LibraryFormatTest::mergeSkipsBlockedIndexEntries()
{
    // steam.toml from before the blocklist knew an app: it isn't a game.
    LibraryContent c;
    c.platforms = {{"steam", "Steam", "steam"}};
    c.steam = {{1145360, "Hades"}, {4690330, "Legacy Steam Runtime"}};

    const BuiltLibrary lib = buildLibrary(c, {});
    QCOMPARE(lib.games.size(), 1);
    QCOMPARE(lib.games.first().id, QStringLiteral("steam-1145360"));
}

void LibraryFormatTest::romIndex()
{
    const QByteArray toml = R"(# Switch ROMs
[games]
"Mario Kart 8 Deluxe [0100152000022000][v0].xci" = { id = "switch-mario-kart-8-deluxe", title = "Mario Kart 8 Deluxe", title_id = "0100152000022000", size = 7340032000 }
"Homebrew/Tetris.nro" = { id = "switch-tetris" }
)";
    const auto r = parseRomIndex(toml, "library/roms/switch.toml");
    QVERIFY(r.ok);
    QVERIFY2(r.warnings.isEmpty(), qPrintable(r.warnings.join(u'\n')));
    // Sorted by path, whatever the file's order.
    QCOMPARE(r.value,
             (QList<RomEntry>{{"Homebrew/Tetris.nro", "switch-tetris", "", ""},
                              {"Mario Kart 8 Deluxe [0100152000022000][v0].xci", "switch-mario-kart-8-deluxe",
                               "Mario Kart 8 Deluxe", "0100152000022000", 7340032000}}));

    // What the scan writes reads back the same.
    const auto again = parseRomIndex(formatRomIndex("Switch", r.value).toUtf8(), "again");
    QVERIFY(again.ok && again.warnings.isEmpty());
    QCOMPARE(again.value, r.value);
    QVERIFY(formatRomIndex("Switch", {}).startsWith("# Switch ROMs found by the folder scan"));
}

void LibraryFormatTest::romIndexInvalidEntries()
{
    const QByteArray toml = R"(roms = 1
[games]
"a.nsp" = "switch-a"
"b.nsp" = { title = "No id" }
"c.nsp" = { id = "Bad Id" }
"d.nsp" = { id = "switch-d", tilte = "D" }
"e.nsp" = { id = "switch-d" }
"f.nsp" = { id = "switch-f", size = "big" }
)";
    const auto r = parseRomIndex(toml, "library/roms/switch.toml");
    QVERIFY(r.ok);
    QCOMPARE(r.value.size(), 2);
    QCOMPARE(r.value.first().id, QStringLiteral("switch-d"));
    QCOMPARE(r.value.last().size, 0);
    QVERIFY(anyContains(r.warnings, ":8: size must be a number of bytes"));
    QVERIFY(anyContains(r.warnings, ":3: \"a.nsp\" must be { id = \"…\", title = \"…\" }; skipped"));
    QVERIFY(anyContains(r.warnings, ":4: \"b.nsp\" has no id; skipped"));
    QVERIFY(anyContains(r.warnings, ":5: id \"Bad Id\" may only use lowercase a-z, 0-9 and -; skipped"));
    QVERIFY(anyContains(r.warnings, ":6: unknown key tilte — did you mean \"title\""));
    QVERIFY(anyContains(r.warnings, ":7: id \"switch-d\" is used twice; skipped"));
    QVERIFY(anyContains(r.warnings, ":1: unknown key roms; games go in the [games] section"));
}

void LibraryFormatTest::mergeRomIndex()
{
    LibraryContent c;
    c.platforms = {{"switch", "Switch", "switch", {"ryubing"}, {"roms"}, 1}};
    c.roms.insert("switch", {{"mk8.xci", "switch-mario-kart-8-deluxe", "Mario Kart 8 Deluxe", "0100152000022000"},
                             {"Sub/Tetris [x].nro", "switch-tetris", "", ""},
                             {"gone.nsp", "switch-gone", "Gone", ""}});
    GameFile f;
    f.title = QStringLiteral("MK8 Deluxe");
    f.favorite = true;
    f.emulator = QStringLiteral("ryubing");
    c.games.insert(QStringLiteral("switch-mario-kart-8-deluxe"), f);
    const RomFiles found = {{"switch", {{"mk8.xci", "/roms"}, {"Sub/Tetris [x].nro", "/roms"}}}};

    const BuiltLibrary lib = buildLibrary(c, {}, found, Emulators::presets());
    QVERIFY2(lib.warnings.isEmpty(), qPrintable(lib.warnings.join(u'\n')));
    QCOMPARE(lib.games.size(), 3);
    QCOMPARE(lib.emulators.size(), 12);

    const Game *mk8 = findGame(lib, "switch-mario-kart-8-deluxe");
    QVERIFY(mk8);
    QCOMPARE(mk8->title, QStringLiteral("MK8 Deluxe")); // the file wins
    QCOMPARE(mk8->platform, QStringLiteral("switch"));  // from the index
    QVERIFY(mk8->favorite);
    QVERIFY(mk8->rom);
    QVERIFY(mk8->installed);
    QCOMPARE(mk8->romPath, QStringLiteral("/roms/mk8.xci"));
    QCOMPARE(mk8->emulator, QStringLiteral("ryubing"));

    const Game *tetris = findGame(lib, "switch-tetris");
    QCOMPARE(tetris->title, QStringLiteral("Tetris")); // no title: from the file name
    QCOMPARE(tetris->romPath, QStringLiteral("/roms/Sub/Tetris [x].nro"));

    const Game *gone = findGame(lib, "switch-gone");
    QVERIFY(gone->rom);
    QVERIFY(!gone->installed);
    QVERIFY(gone->romPath.isEmpty());
}

void LibraryFormatTest::mergeRomKey()
{
    LibraryContent c;
    c.platforms = {{"switch", "Switch", "switch", {"ryubing"}, {}, 1}};
    const auto romGame = [](const QString &title, const QString &rom) {
        GameFile f;
        f.title = title;
        f.platform = QStringLiteral("switch");
        f.rom = rom;
        f.romLine = 3;
        return f;
    };
    GameFile found = romGame("Celeste", "~/Downloads/Celeste.nsp");
    found.args = {QStringLiteral("--fullscreen")};
    c.games.insert("switch-celeste", found);
    c.games.insert("switch-gone", romGame("Gone", "/nowhere/Gone.xci"));
    c.games.insert("switch-readme", romGame("Readme", "/games/readme.txt"));
    c.games.insert("switch-plain", romGame("Plain", {}));
    const QHash<QString, RomFile> romGameFiles = {{"switch-celeste", {"/home/me/Downloads/Celeste.nsp", 4096}},
                                                  {"switch-readme", {"/games/readme.txt", 0}}};

    const BuiltLibrary lib = buildLibrary(c, {}, {}, Emulators::presets(), romGameFiles);
    QCOMPARE(lib.warnings, QStringList{"library/games/switch-readme.toml:3: readme.txt isn't a file Switch's emulators "
                                       "run; they take .nsp, .xci, .nro, .nso, .nca"});

    const Game *celeste = findGame(lib, "switch-celeste");
    QVERIFY(celeste->rom);
    QVERIFY(celeste->romKey);
    QVERIFY(celeste->installed);
    QCOMPARE(celeste->romPath, QStringLiteral("/home/me/Downloads/Celeste.nsp"));
    QCOMPARE(celeste->sizeBytes, 4096);
    QCOMPARE(celeste->args, QStringList{"--fullscreen"});

    const Game *gone = findGame(lib, "switch-gone");
    QVERIFY(gone->rom);
    QVERIFY(gone->romKey);
    QVERIFY(!gone->installed);
    QVERIFY(gone->romPath.isEmpty());

    QVERIFY(findGame(lib, "switch-readme")->installed); // listed, with the problem
    const Game *plain = findGame(lib, "switch-plain"); // no rom key, no index entry
    QVERIFY(!plain->rom);
    QVERIFY(plain->installed);
}

void LibraryFormatTest::findRomKeyFiles()
{
    QTemporaryDir dir;
    const QString data = dir.filePath("data");
    const QString home = dir.filePath("home");
    QVERIFY(QDir().mkpath(data + "/roms"));
    QVERIFY(QDir().mkpath(home + "/Downloads/Folder.nsp"));
    for (const QString &path : {data + "/roms/A.nsp", home + "/Downloads/B.nsp"}) {
        QFile f(path);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("rom");
    }
    QMap<QString, GameFile> games;
    games["a"].rom = QStringLiteral("roms/A.nsp");          // relative to the data dir
    games["b"].rom = QStringLiteral("~/Downloads/B.nsp");   // in the home folder
    games["c"].rom = home + QStringLiteral("/Downloads/C.nsp"); // missing
    games["d"].rom = QStringLiteral("~/Downloads/Folder.nsp"); // a folder, not a file
    games["e"].title = QStringLiteral("No rom");
    QCOMPARE(findRomGameFiles(games, data, home),
             (QHash<QString, RomFile>{{"a", {data + "/roms/A.nsp", 3}}, {"b", {home + "/Downloads/B.nsp", 3}}}));
}

void LibraryFormatTest::mergeRomIdClash()
{
    LibraryContent c;
    c.platforms = {{"switch", "Switch", "switch"}, {"3ds", "3DS", "3ds"}};
    c.roms.insert("3ds", {{"a.3ds", "shared-id", "A", ""}});
    c.roms.insert("switch", {{"b.nsp", "shared-id", "B", ""}});
    const BuiltLibrary lib = buildLibrary(c, {});
    QCOMPARE(lib.games.size(), 1);
    QCOMPARE(lib.warnings,
             QStringList{"library/roms/switch.toml: id \"shared-id\" is already used by another game; skipped \"b.nsp\""});
}

void LibraryFormatTest::mergeEmulatorProblems()
{
    LibraryContent c;
    c.platforms = {{"switch", "Switch", "switch", {"ryubng", "azahar", "ryubing"}, {}, 4}};
    GameFile f;
    f.title = QStringLiteral("Game");
    f.platform = QStringLiteral("switch");
    f.emulator = QStringLiteral("azahr");
    c.games.insert(QStringLiteral("switch-game"), f);

    const BuiltLibrary lib = buildLibrary(c, {}, {}, Emulators::presets());
    QCOMPARE(lib.platforms.first().emulators, (QStringList{"azahar", "ryubing"})); // unknown ones dropped
    QCOMPARE(lib.games.first().emulator, QString());
    QVERIFY(anyContains(lib.warnings, "library/platforms.toml:4: unknown emulator \"ryubng\" for Switch — did you "
                                      "mean \"ryubing\""));
    QVERIFY(anyContains(lib.warnings, "library/platforms.toml:4: Azahar is for 3ds, not switch"));
    QVERIFY(anyContains(lib.warnings, "library/games/switch-game.toml: unknown emulator \"azahr\" — did you mean "
                                      "\"azahar\""));
    QCOMPARE(lib.warnings.size(), 3);
}

void LibraryFormatTest::mergeIndexEntryAlone()
{
    LibraryContent c;
    c.platforms = {{"steam", "Steam", "steam"}};
    c.steam = {{504230, "Celeste"}};
    const BuiltLibrary lib = buildLibrary(c, {});
    QCOMPARE(lib.games.size(), 1);
    QCOMPARE(lib.games.first().id, QStringLiteral("steam-504230"));
    QCOMPARE(lib.games.first().title, QStringLiteral("Celeste"));
    QVERIFY(!lib.games.first().favorite);
}

void LibraryFormatTest::mergeFileAlone()
{
    LibraryContent c;
    c.platforms = {{"steam", "Steam", "steam"}};
    GameFile f;
    f.title = QStringLiteral("Portal");
    f.platform = QStringLiteral("steam");
    c.games.insert(QStringLiteral("steam-400"), f);
    const BuiltLibrary lib = buildLibrary(c, {});
    QVERIFY(lib.warnings.isEmpty());
    QCOMPARE(lib.games.size(), 1);
    QCOMPARE(lib.games.first().title, QStringLiteral("Portal"));
}

void LibraryFormatTest::mergeHiddenViaFile()
{
    LibraryContent c;
    c.platforms = {{"steam", "Steam", "steam"}};
    c.steam = {{504230, "Celeste"}, {1145360, "Hades"}};
    GameFile f;
    f.hidden = true;
    c.games.insert(QStringLiteral("steam-504230"), f);
    const BuiltLibrary lib = buildLibrary(c, {});
    QCOMPARE(lib.games.size(), 1);
    QCOMPARE(lib.games.first().title, QStringLiteral("Hades"));
}

void LibraryFormatTest::mergeUnknownPlatform()
{
    LibraryContent c;
    c.platforms = {{"switch", "Switch", "switch"}};
    for (const char *id : {"swich-a", "swich-b"}) {
        GameFile f;
        f.title = QLatin1String(id);
        f.platform = QStringLiteral("swich");
        c.games.insert(QLatin1String(id), f);
    }
    const BuiltLibrary lib = buildLibrary(c, {});
    QCOMPARE(lib.games.size(), 2);
    QCOMPARE(lib.games.first().platformName, QStringLiteral("swich"));
    QCOMPARE(lib.platforms.size(), 2);
    QCOMPARE(lib.platforms.at(1), (Platform{"swich", "swich", ""}));
    QCOMPARE(lib.warnings.size(), 1); // once per platform, not per game
    QVERIFY(lib.warnings.first().contains(QStringLiteral("platform \"swich\" isn't in library/platforms.toml")));
    QVERIFY(lib.warnings.first().contains(QStringLiteral("\"switch\"")));
    QVERIFY(lib.warnings.first().contains(QStringLiteral("1 more game")));
}

void LibraryFormatTest::mergeMissingTitleAndPlatform()
{
    LibraryContent c;
    c.games.insert(QStringLiteral("mystery"), GameFile{});
    const BuiltLibrary lib = buildLibrary(c, {});
    QCOMPARE(lib.games.size(), 1);
    QCOMPARE(lib.games.first().title, QStringLiteral("mystery"));
    QVERIFY(anyContains(lib.warnings, QStringLiteral("library/games/mystery.toml: no title")));
    QVERIFY(anyContains(lib.warnings, QStringLiteral("library/games/mystery.toml: no platform")));
}

void LibraryFormatTest::mergeTagColorsAndArt()
{
    LibraryContent c;
    c.platforms = {{"switch", "Switch", "switch"}};
    c.tags = {{"RPG", 5}};
    GameFile f;
    f.title = QStringLiteral("Game");
    f.platform = QStringLiteral("switch");
    f.tags = {QStringLiteral("RPG"), QStringLiteral("Unlisted")};
    c.games.insert(QStringLiteral("switch-game"), f);
    const QHash<QString, ArtSet> art{{QStringLiteral("switch-game"), {"/art/hero.png", {}, "/art/icon.webp", {}}}};

    const BuiltLibrary lib = buildLibrary(c, art);
    QVERIFY(lib.warnings.isEmpty()); // an unlisted tag is fine
    const Game &g = lib.games.first();
    QCOMPARE(g.tags.size(), 2);
    QCOMPARE(g.tags.at(0).color, 5);
    QCOMPARE(g.tags.at(1).color, 0);
    QCOMPARE(g.heroUrl, QUrl::fromLocalFile("/art/hero.png"));
    QCOMPARE(g.iconUrl, QUrl::fromLocalFile("/art/icon.webp"));
    QVERIFY(g.logoUrl.isEmpty());
}

void LibraryFormatTest::mergeSortsByTitle()
{
    LibraryContent c;
    c.platforms = {{"steam", "Steam", "steam"}};
    c.steam = {{1, "zork"}, {2, "Celeste"}, {3, "Game 10"}, {4, "Game 9"}};
    const BuiltLibrary lib = buildLibrary(c, {});
    QStringList titles;
    for (const Game &g : lib.games)
        titles.append(g.title);
    QCOMPARE(titles, (QStringList{"Celeste", "Game 9", "Game 10", "zork"}));
    QVERIFY(findGame(lib, QStringLiteral("steam-1")));
}

QTEST_MAIN(LibraryFormatTest)
#include "libraryformat_test.moc"
