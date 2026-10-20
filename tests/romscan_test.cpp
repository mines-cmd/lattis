#include "romscan.h"

#include <QDir>
#include <QTemporaryDir>
#include <QTest>

Q_DECLARE_METATYPE(RomName::Kind)

using RomName::Kind;

class RomScanTest : public QObject
{
    Q_OBJECT

private slots:
    void parse_data();
    void parse();
    void fixCase_data();
    void fixCase();
    void kindOf_data();
    void kindOf();

    void indexAddsNewFiles();
    void indexSkipsUpdatesAndDlc();
    void indexSkipsFolderUpdates();
    void indexKeepsExistingEntries();
    void indexFixesCaseOfNewEntries();
    void indexMatchesRenamesByTitleId();
    void indexMatchesRenamesByTitle();
    void indexMatchesRenamesBySize();
    void indexUpdatesSizes();
    void scanFolders();
    void scanFolderGames();
    void scanRecordsSizes();
    void scanSkipsClaimedFiles();
    void inspectPick_data();
    void inspectPick();
    void scanWithoutEmulators();
    void resolveFolders();
};

void RomScanTest::parse_data()
{
    QTest::addColumn<QString>("fileName");
    QTest::addColumn<QString>("title");
    QTest::addColumn<QString>("titleId");
    QTest::addColumn<Kind>("kind");

    // Switch dumps
    QTest::newRow("base") << "Mario Kart 8 Deluxe [0100152000022000][v0].nsp"
                          << "Mario Kart 8 Deluxe" << "0100152000022000" << Kind::Game;
    QTest::newRow("update") << "Mario Kart 8 Deluxe [0100152000022800][v196608].nsp"
                            << "Mario Kart 8 Deluxe" << "0100152000022800" << Kind::Update;
    QTest::newRow("dlc") << "Mario Kart 8 Deluxe - Booster Course Pass [0100152000023001][v0].nsp"
                         << "Mario Kart 8 Deluxe - Booster Course Pass" << "0100152000023001" << Kind::Dlc;
    QTest::newRow("colon and size") << "The Legend of Zelda: Tears of the Kingdom [0100F2C0115B6000][v0] (15.96 GB).nsp"
                                    << "The Legend of Zelda: Tears of the Kingdom" << "0100F2C0115B6000" << Kind::Game;
    QTest::newRow("dash for colon") << "The Legend of Zelda - Tears of the Kingdom [0100F2C0115B6000][USA].xci"
                                    << "The Legend of Zelda - Tears of the Kingdom" << "0100F2C0115B6000" << Kind::Game;
    QTest::newRow("lowercase hex") << "Kirby and the Forgotten Land [01004d300c5ae000][v0].nsp"
                                   << "Kirby and the Forgotten Land" << "01004D300C5AE000" << Kind::Game;
    QTest::newRow("underscores") << "Animal_Crossing_New_Horizons_[01006F8002326000][v0].nsp"
                                 << "Animal Crossing New Horizons" << "01006F8002326000" << Kind::Game;
    QTest::newRow("no space before group") << "Metroid Dread[010093801237C000][v0].xci"
                                           << "Metroid Dread" << "010093801237C000" << Kind::Game;
    QTest::newRow("dots in title") << "Super Smash Bros. Ultimate [01006A800016E000][v0] (NSP).nsp"
                                   << "Super Smash Bros. Ultimate" << "01006A800016E000" << Kind::Game;
    QTest::newRow("apostrophe") << "Luigi's Mansion 3 [0100DCA0064A6000][v0] (1.2 GB).nsp"
                                << "Luigi's Mansion 3" << "0100DCA0064A6000" << Kind::Game;
    QTest::newRow("dangling dash") << "Hollow Knight - [0100633007D48000][v0].nsp"
                                   << "Hollow Knight" << "0100633007D48000" << Kind::Game;
    QTest::newRow("tag before id") << "Xenoblade Chronicles 3 [Base] [010074F013262000] [v0].nsp"
                                   << "Xenoblade Chronicles 3" << "010074F013262000" << Kind::Game;
    QTest::newRow("curly braces") << "Fire Emblem  Engage {v0} [0100A6301214E000].xci"
                                  << "Fire Emblem Engage" << "0100A6301214E000" << Kind::Game;
    QTest::newRow("homebrew") << "Tetris.nro" << "Tetris" << "" << Kind::Game;
    QTest::newRow("only an id") << "[0100152000022000].nsp" << "[0100152000022000]" << "0100152000022000"
                                << Kind::Game;
    QTest::newRow("not an id") << "Game [0100152000022000X].nsp" << "Game" << "" << Kind::Game;
    // Scene-style names: dots for spaces, the title ID and version without brackets.
    QTest::newRow("scene update") << "MONSTER.HUNTER.GENERATIONS.ULTIMATE.0100770008DD8800.v262144.nsp"
                                  << "Monster Hunter Generations Ultimate" << "0100770008DD8800" << Kind::Update;
    QTest::newRow("scene base") << "Super.Mario.Odyssey.0100000000010000.v0.nsp" << "Super Mario Odyssey"
                                << "0100000000010000" << Kind::Game;
    QTest::newRow("scene without id") << "Celeste.nsp" << "Celeste" << "" << Kind::Game;
    QTest::newRow("bare id with spaces") << "Hades 0100535012974000 v0.nsp" << "Hades" << "0100535012974000"
                                         << Kind::Game;
    QTest::newRow("bare hex that isn't an id") << "Game deadbeefdeadbeef.nsp" << "Game deadbeefdeadbeef" << ""
                                               << Kind::Game;
    QTest::newRow("version word kept without an id") << "Portal v2.nsp" << "Portal v2" << "" << Kind::Game;

    // 3DS dumps (No-Intro style)
    QTest::newRow("regions and languages") << "Pokémon Ultra Sun (USA) (En,Ja,Fr,De,Es,It) (Rev 1).3ds"
                                           << "Pokémon Ultra Sun" << "" << Kind::Game;
    QTest::newRow("no-intro dash") << "Kirby - Planet Robobot (Europe) (En,Fr,De,Es,It,Nl).3ds"
                                   << "Kirby - Planet Robobot" << "" << Kind::Game;
    QTest::newRow("3ds title id is a game") << "Pokemon Y [0004000000055E00].cci"
                                            << "Pokemon Y" << "0004000000055E00" << Kind::Game;
    QTest::newRow("decomposed accent") << QString::fromUtf8("Pokémon X (USA).3ds")
                                       << "Pokémon X" << "" << Kind::Game;
    QTest::newRow("unbalanced group") << "Some Game (USA.3ds" << "Some Game (USA" << "" << Kind::Game;
    QTest::newRow("dots in name") << "Ys VIII. Lacrimosa of Dana.xci" << "Ys VIII. Lacrimosa of Dana" << ""
                                  << Kind::Game;
}

void RomScanTest::parse()
{
    QFETCH(QString, fileName);
    QFETCH(QString, title);
    QFETCH(QString, titleId);
    QFETCH(Kind, kind);

    const RomName::Info info = RomName::parse(fileName);
    QCOMPARE(info.title, title);
    QCOMPARE(info.titleId, titleId);
    QCOMPARE(info.kind, kind);
}

void RomScanTest::fixCase_data()
{
    QTest::addColumn<QString>("title");
    QTest::addColumn<QString>("fixed");

    QTest::newRow("all caps") << "MONSTER HUNTER RISE" << "Monster Hunter Rise";
    QTest::newRow("all caps scene dump") << RomName::cleanTitle("MONSTER.HUNTER.GENERATIONS.ULTIMATE.nsp")
                                         << "Monster Hunter Generations Ultimate";
    QTest::newRow("lowercase") << "the legend of zelda" << "The Legend Of Zelda";
    QTest::newRow("mixed left alone") << "The Legend of Zelda: Tears of the Kingdom"
                                      << "The Legend of Zelda: Tears of the Kingdom";
    QTest::newRow("mixed with caps word") << "FIFA 23 Legacy edition" << "FIFA 23 Legacy edition";
    QTest::newRow("roman numerals") << "FINAL FANTASY VII REMAKE" << "Final Fantasy VII Remake";
    QTest::newRow("lowercase roman numerals") << "final fantasy xiv: a realm reborn"
                                              << "Final Fantasy XIV: A Realm Reborn";
    QTest::newRow("roman i") << "I AM SETSUNA" << "I Am Setsuna";
    QTest::newRow("not numerals") << "MIX DID CIVIL" << "Mix Did Civil";
    QTest::newRow("hyphen") << "MARVEL'S SPIDER-MAN" << "Marvel's Spider-Man";
    QTest::newRow("colon without space") << "zelda:tears" << "Zelda:Tears";
    QTest::newRow("apostrophe") << "luigi's mansion 3" << "Luigi's Mansion 3";
    QTest::newRow("typographic apostrophe") << QString::fromUtf8("LUIGI’S MANSION") << QString::fromUtf8("Luigi’s Mansion");
    QTest::newRow("accents") << QString::fromUtf8("POKÉMON ÉCLAT") << QString::fromUtf8("Pokémon Éclat");
    QTest::newRow("lowercase accents") << QString::fromUtf8("él pokémon") << QString::fromUtf8("Él Pokémon");
    QTest::newRow("digit words kept") << "SUPER MARIO 3D WORLD" << "Super Mario 3D World";
    QTest::newRow("no letters") << "1942" << "1942";
    QTest::newRow("empty") << "" << "";
}

void RomScanTest::fixCase()
{
    QFETCH(QString, title);
    QFETCH(QString, fixed);
    QCOMPARE(RomName::fixCase(title), fixed);
}

void RomScanTest::kindOf_data()
{
    QTest::addColumn<QString>("titleId");
    QTest::addColumn<Kind>("kind");

    QTest::newRow("base") << "0100152000022000" << Kind::Game;
    QTest::newRow("update") << "0100152000022800" << Kind::Update;
    QTest::newRow("dlc") << "0100152000023001" << Kind::Dlc;
    QTest::newRow("dlc high index") << "010015200002307F" << Kind::Dlc;
    QTest::newRow("3ds base") << "0004000000055D00" << Kind::Game;
    QTest::newRow("3ds update") << "0004000E00055D00" << Kind::Game; // only Switch ids are classified
    QTest::newRow("none") << "" << Kind::Game;
}

void RomScanTest::kindOf()
{
    QFETCH(QString, titleId);
    QFETCH(Kind, kind);
    QCOMPARE(RomName::kindOf(titleId), kind);
}

namespace {

RomEntry entry(const QString &path, const QString &id, const QString &title, const QString &titleId = {},
               qint64 size = 0)
{
    return {path, id, title, titleId, size};
}

// A file of `size` bytes.
bool touch(const QString &path, qint64 size = 0)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write(QByteArray(size, 'x')) == size;
}

bool anyContains(const QStringList &list, const QString &needle)
{
    return std::any_of(list.cbegin(), list.cend(), [&](const QString &s) { return s.contains(needle); });
}

QList<EmulatorDef> presets()
{
    QStringList warnings;
    return Emulators::merge(Emulators::presets(), {}, &warnings);
}

Platform switchPlatform(const QStringList &folders)
{
    return {"switch", "Switch", "switch", {"ryubing"}, folders, 7};
}

} // namespace

void RomScanTest::indexAddsNewFiles()
{
    QSet<QString> taken = {"switch-tetris"};
    const RomIndexUpdate u = updateRomIndex(
        "switch", {}, {"Tetris.nro", "Homebrew/Tetris.nro", "Metroid Dread [010093801237C000][v0].nsp"}, &taken);
    QVERIFY(u.changed);
    QCOMPARE(u.added, 3);
    QCOMPARE(u.addedIds, (QStringList{"switch-tetris-2", "switch-metroid-dread", "switch-tetris-3"}));
    // Sorted by path, so ids are handed out the same way every time.
    QCOMPARE(u.entries,
             (QList<RomEntry>{entry("Homebrew/Tetris.nro", "switch-tetris-2", "Tetris"),
                              entry("Metroid Dread [010093801237C000][v0].nsp", "switch-metroid-dread", "Metroid Dread",
                                    "010093801237C000"),
                              entry("Tetris.nro", "switch-tetris-3", "Tetris")}));
    QVERIFY(taken.contains("switch-tetris-3"));
}

void RomScanTest::indexSkipsUpdatesAndDlc()
{
    QSet<QString> taken;
    const RomIndexUpdate u = updateRomIndex("switch", {},
                                            {"Mario Kart 8 Deluxe [0100152000022800][v196608].nsp",
                                             "Mario Kart 8 Deluxe - Booster Course Pass [0100152000023001][v0].nsp",
                                             "Mario Kart 8 Deluxe - Booster Course Pass [0100152000023002][v0].nsp"},
                                            &taken);
    QVERIFY(!u.changed);
    QVERIFY(u.entries.isEmpty());
    QCOMPARE(u.updates, 1);
    QCOMPARE(u.dlc, 2);
}

void RomScanTest::indexSkipsFolderUpdates()
{
    // shadPS4 finds a game's update next to it, in <game>-UPDATE or <game>-patch.
    const QSet<QString> files = {"eboot.bin"};
    const QList<RomEntry> index = {entry("Bloodborne/CUSA00900-patch/eboot.bin", "ps4-bloodborne", "Bloodborne")};
    QSet<QString> taken = {"ps4-bloodborne"};
    const RomIndexUpdate u = updateRomIndex("ps4", index,
                                            {"Bloodborne/CUSA00900/eboot.bin", "Bloodborne/CUSA00900-patch/eboot.bin",
                                             "Bloodborne/CUSA00900-UPDATE/EBOOT.BIN"},
                                            &taken, files);
    QVERIFY(u.changed);
    QCOMPARE(u.dropped, 1);
    QCOMPARE(u.updates, 1); // the -patch one was already indexed
    QCOMPARE(u.entries.size(), 1);
    QCOMPARE(u.entries.first().path, QStringLiteral("Bloodborne/CUSA00900/eboot.bin"));

    // Only folder games: a ROM file in such a folder is still a game.
    const RomIndexUpdate roms = updateRomIndex("ps4", {}, {"Tetris-patch/Tetris.nro"}, &taken, files);
    QCOMPARE(roms.added, 1);
}

void RomScanTest::indexFixesCaseOfNewEntries()
{
    // Only new entries: an existing all-caps title is left as the index has it.
    const QList<RomEntry> index = {entry("METROID DREAD.nsp", "switch-metroid-dread", "METROID DREAD")};
    QSet<QString> taken = {"switch-metroid-dread"};
    const RomIndexUpdate u =
        updateRomIndex("switch", index, {"METROID DREAD.nsp", "SUPER.MARIO.ODYSSEY.nsp"}, &taken);
    QCOMPARE(u.entries, (QList<RomEntry>{entry("METROID DREAD.nsp", "switch-metroid-dread", "METROID DREAD"),
                                         entry("SUPER.MARIO.ODYSSEY.nsp", "switch-super-mario-odyssey",
                                               "Super Mario Odyssey")}));

    // Folder games too.
    const RomIndexUpdate folders = updateRomIndex("ps4", {}, {"BLOODBORNE/eboot.bin"}, &taken, {"eboot.bin"});
    QCOMPARE(folders.entries.size(), 1);
    QCOMPARE(folders.entries.first().title, QStringLiteral("Bloodborne"));
}

void RomScanTest::indexKeepsExistingEntries()
{
    // A hand-fixed title and a missing file stay as they are; nothing to write.
    const QList<RomEntry> index = {entry("mk8.xci", "switch-mario-kart-8-deluxe", "Mario Kart 8 Deluxe (my copy)"),
                                   entry("gone.nsp", "switch-gone", "Gone")};
    QSet<QString> taken = {"switch-mario-kart-8-deluxe", "switch-gone"};
    const RomIndexUpdate u = updateRomIndex("switch", index, {"mk8.xci"}, &taken);
    QVERIFY(!u.changed);
    QCOMPARE(u.entries, index);
}

void RomScanTest::indexMatchesRenamesByTitleId()
{
    const QList<RomEntry> index = {entry("Metroid Dread [010093801237C000][v0].nsp", "switch-metroid-dread",
                                         "Metroid Dread", "010093801237C000")};
    QSet<QString> taken = {"switch-metroid-dread"};
    const RomIndexUpdate u =
        updateRomIndex("switch", index, {"Nintendo/Metroid_Dread_[010093801237c000].xci"}, &taken);
    QVERIFY(u.changed);
    QCOMPARE(u.renamed, 1);
    QCOMPARE(u.added, 0);
    QCOMPARE(u.entries, (QList<RomEntry>{entry("Nintendo/Metroid_Dread_[010093801237c000].xci",
                                               "switch-metroid-dread", "Metroid Dread", "010093801237C000")}));

    // While the old file is still there, a second copy is a new game.
    const RomIndexUpdate both = updateRomIndex(
        "switch", index, {"Metroid Dread [010093801237C000][v0].nsp", "Copy [010093801237C000].nsp"}, &taken);
    QCOMPARE(both.renamed, 0);
    QCOMPARE(both.added, 1);
    QCOMPARE(both.entries.last().id, QStringLiteral("switch-copy"));
}

void RomScanTest::indexMatchesRenamesByTitle()
{
    // Only the tags changed, and the file moved into a subfolder.
    const QList<RomEntry> index = {entry("Zelda (USA).z64", "n64-zelda", "Zelda", {}, 100)};
    QSet<QString> taken = {"n64-zelda"};
    const RomIndexUpdate u = updateRomIndex("n64", index, {"Nintendo/zelda (USA) [!].z64"}, &taken, {},
                                            {{"Nintendo/zelda (USA) [!].z64", 200}});
    QVERIFY(u.changed);
    QCOMPARE(u.renamed, 1);
    QCOMPARE(u.added, 0);
    QCOMPARE(u.entries, (QList<RomEntry>{entry("Nintendo/zelda (USA) [!].z64", "n64-zelda", "Zelda", {}, 200)}));

    // Two new files with that title: no guess.
    const RomIndexUpdate two = updateRomIndex("n64", index, {"Zelda (Europe).z64", "Zelda (Japan).z64"}, &taken);
    QCOMPARE(two.renamed, 0);
    QCOMPARE(two.added, 2);
    QCOMPARE(two.entries.first(), index.first());

    // Folder games by their folder.
    const QSet<QString> files = {"eboot.bin"};
    const QList<RomEntry> ps4 = {entry("Bloodborne/eboot.bin", "ps4-bloodborne", "Bloodborne")};
    const RomIndexUpdate folder = updateRomIndex("ps4", ps4, {"Bloodborne [CUSA00900]/eboot.bin"}, &taken, files);
    QCOMPARE(folder.renamed, 1);
    QCOMPARE(folder.entries.first().path, QStringLiteral("Bloodborne [CUSA00900]/eboot.bin"));
}

void RomScanTest::indexMatchesRenamesBySize()
{
    const QList<RomEntry> index = {
        entry("Monster Hunter 4 Ultimate Decrypted.3ds", "3ds-monster-hunter-4-ultimate-decrypted",
              "Monster Hunter 4 Ultimate Decrypted", {}, 4096),
        entry("Other.3ds", "3ds-other", "Other", {}, 999)};
    QSet<QString> taken = {"3ds-monster-hunter-4-ultimate-decrypted", "3ds-other"};
    const RomIndexUpdate u = updateRomIndex("3ds", index, {"Monster Hunter 4 Ultimate.3ds", "Other.3ds"}, &taken, {},
                                            {{"Monster Hunter 4 Ultimate.3ds", 4096}, {"Other.3ds", 999}});
    QVERIFY(u.changed);
    QCOMPARE(u.renamed, 1);
    QCOMPARE(u.added, 0);
    QCOMPARE(u.entries.first(), entry("Monster Hunter 4 Ultimate.3ds", "3ds-monster-hunter-4-ultimate-decrypted",
                                      "Monster Hunter 4 Ultimate Decrypted", {}, 4096));

    // Another extension, two new files of that size, or an entry without a size: no guess.
    const auto renamed = [&](const QStringList &found, const QHash<QString, qint64> &sizes,
                             const QList<RomEntry> &from = {}) {
        return updateRomIndex("3ds", from.isEmpty() ? index : from, found, &taken, {}, sizes).renamed;
    };
    QCOMPARE(renamed({"MH4U.cia"}, {{"MH4U.cia", 4096}}), 0);
    QCOMPARE(renamed({"A.3ds", "B.3ds"}, {{"A.3ds", 4096}, {"B.3ds", 4096}}), 0);
    QCOMPARE(renamed({"MH4U.3ds"}, {{"MH4U.3ds", 4096}}, {entry("MH.3ds", "3ds-mh", "MH")}), 0);
    // Two gone entries of that size.
    QCOMPARE(renamed({"MH4U.3ds"}, {{"MH4U.3ds", 4096}},
                     {entry("A.3ds", "3ds-a", "A", {}, 4096), entry("B.3ds", "3ds-b", "B", {}, 4096)}),
             0);

    // Different title IDs are different games, whatever their size.
    const QList<RomEntry> switchIndex = {entry("A [0100000000010000].nsp", "switch-a", "A", "0100000000010000", 50)};
    QCOMPARE(updateRomIndex("switch", switchIndex, {"B [0100000000020000].nsp"}, &taken, {},
                            {{"B [0100000000020000].nsp", 50}})
                 .renamed,
             0);
}

void RomScanTest::indexUpdatesSizes()
{
    const QList<RomEntry> index = {entry("a.nsp", "switch-a", "A", {}, 10), entry("gone.nsp", "switch-gone", "Gone", {}, 20)};
    QSet<QString> taken = {"switch-a", "switch-gone"};
    const RomIndexUpdate same = updateRomIndex("switch", index, {"a.nsp"}, &taken, {}, {{"a.nsp", 10}});
    QVERIFY(!same.changed);
    const RomIndexUpdate grown = updateRomIndex("switch", index, {"a.nsp"}, &taken, {}, {{"a.nsp", 11}});
    QVERIFY(grown.changed);
    QCOMPARE(grown.entries, (QList<RomEntry>{entry("a.nsp", "switch-a", "A", {}, 11), index.last()}));
}

void RomScanTest::scanFolders()
{
    QTemporaryDir dir;
    const QString data = dir.filePath("data");
    QVERIFY(touch(data + "/roms/switch/Metroid Dread [010093801237C000][v0].nsp"));
    QVERIFY(touch(data + "/roms/switch/Sub/Pikmin 4 [0100B7C00933A000][v0].NSP")); // extension case
    QVERIFY(touch(data + "/roms/switch/readme.txt"));
    QVERIFY(touch(data + "/roms/switch/.hidden.nsp"));
    QVERIFY(touch(data + "/roms/switch/.trash/Old [0100000000010000].nsp"));
    QVERIFY(touch(dir.filePath("home/more/Metroid Dread [010093801237C000][v0].nsp"))); // same path, 2nd folder
    QVERIFY(touch(dir.filePath("home/more/Tetris.nro")));

    const Platform platform = switchPlatform({"roms/switch", "~/more", "/nonexistent/games", "roms/switch/"});
    const QList<RomEntry> index = {entry("Mario Tennis Aces.nsp", "switch-mario-tennis-aces", "Mario Tennis Aces")};
    const RomScanResult r = scanRomFolders({platform}, presets(), {{"switch", index}}, {"switch-mario-tennis-aces"},
                                           data, dir.filePath("home"));

    const QHash<QString, QString> files = r.files.value("switch");
    QCOMPARE(files.size(), 3);
    QCOMPARE(files.value("Metroid Dread [010093801237C000][v0].nsp"), data + "/roms/switch"); // first folder wins
    QCOMPARE(files.value("Sub/Pikmin 4 [0100B7C00933A000][v0].NSP"), data + "/roms/switch");
    QCOMPARE(files.value("Tetris.nro"), dir.filePath("home/more"));

    const QList<RomEntry> written = r.indexes.value("switch");
    QCOMPARE(written.size(), 4);
    QCOMPARE(written.first(), index.first()); // missing, but kept

    QVERIFY(anyContains(r.warnings, "library/platforms.toml:7: ROM folder /nonexistent/games not found; its games "
                                    "show as \"ROM not found\""));
    QVERIFY(anyContains(r.warnings, "library/platforms.toml:7: ROM folder roms/switch/ is listed twice"));
    QVERIFY(anyContains(r.warnings, "Metroid Dread [010093801237C000][v0].nsp is in two ROM folders of Switch"));
    QCOMPARE(r.warnings.size(), 3);
    QCOMPARE(r.missingFolders.value("switch"), QStringList{"/nonexistent/games"});
    QCOMPARE(r.log, QStringList{"switch: 3 files in 2 folders, 3 new, 0 renamed, 1 not found"});
}

void RomScanTest::scanSkipsClaimedFiles()
{
    // A file a game file's `rom` already has isn't indexed again.
    QTemporaryDir dir;
    const QString data = dir.filePath("data");
    QVERIFY(touch(data + "/roms/switch/Celeste.nsp"));
    QVERIFY(touch(data + "/roms/switch/Tetris.nro"));
    const RomScanResult r = scanRomFolders({switchPlatform({"roms/switch"})}, presets(), {}, {}, data,
                                           dir.filePath("home"), {data + "/roms/switch/Celeste.nsp"});
    QCOMPARE(r.files.value("switch").keys(), QStringList{"Tetris.nro"});
    QCOMPARE(r.indexes.value("switch"), (QList<RomEntry>{entry("Tetris.nro", "switch-tetris", "Tetris")}));
}

void RomScanTest::inspectPick_data()
{
    QTest::addColumn<QString>("path");
    QTest::addColumn<QStringList>("platforms");
    QTest::addColumn<QString>("title");
    QTest::addColumn<QString>("problem");

    QTest::newRow("one platform") << "/dl/SUPER MARIO ODYSSEY [0100000000010000][v0].nsp" << QStringList{"switch"}
                                  << "Super Mario Odyssey" << "";
    QTest::newRow("extension case") << "/dl/Pokemon Y.3DS" << QStringList{"3ds"} << "Pokemon Y" << "";
    // In rail order, as the platforms are given.
    QTest::newRow("several") << "/dl/Ico (USA).iso" << QStringList{"psp", "ps2", "gamecube"} << "Ico" << "";
    QTest::newRow("none") << "/dl/readme.txt" << QStringList{} << "Readme" << "";
    QTest::newRow("no extension") << "/dl/Makefile" << QStringList{} << "Makefile" << "";
    QTest::newRow("update") << "/dl/Mario Kart 8 Deluxe [0100152000022800][v196608].nsp" << QStringList{"switch"}
                            << "Mario Kart 8 Deluxe"
                            << "This is an update for Mario Kart 8 Deluxe; install it in the emulator";
    QTest::newRow("dlc") << "/dl/Mario Kart 8 Deluxe - Booster Course Pass [0100152000023001][v0].nsp"
                         << QStringList{"switch"} << "Mario Kart 8 Deluxe - Booster Course Pass"
                         << "This is DLC for Mario Kart 8 Deluxe - Booster Course Pass; install it in the emulator";
    // Folder games: named after the folder above the dump's layout and serials.
    QTest::newRow("ps4") << "/games/BLOODBORNE/CUSA00900/eboot.bin" << QStringList{"ps3", "ps4"} << "Bloodborne"
                         << "";
    QTest::newRow("ps4 update") << "/games/Bloodborne/CUSA00900-UPDATE/eboot.bin" << QStringList{"ps3", "ps4"}
                                << "Bloodborne" << "This is an update for Bloodborne; install it in the emulator";
    QTest::newRow("ps3") << "/games/Demon's Souls [BLUS30443]/PS3_GAME/USRDIR/EBOOT.BIN" << QStringList{"ps3", "ps4"}
                         << "Demon's Souls" << "";
}

void RomScanTest::inspectPick()
{
    QFETCH(QString, path);
    QFETCH(QStringList, platforms);
    QFETCH(QString, title);
    QFETCH(QString, problem);

    const QList<Platform> all = {{"switch", "Switch", "", {"ryubing"}, {}, 1}, {"3ds", "3DS", "", {"azahar"}, {}, 2},
                                 {"psp", "PSP", "", {"ppsspp"}, {}, 3},     {"ps2", "PS2", "", {"pcsx2"}, {}, 4},
                                 {"gamecube", "GameCube", "", {"dolphin"}, {}, 5},
                                 {"ps3", "PS3", "", {"rpcs3"}, {}, 6},      {"ps4", "PS4", "", {"shadps4"}, {}, 7},
                                 {"steam", "Steam", "", {}, {}, 8},         {"odd", "Odd", "", {"unknown"}, {}, 9}};
    const RomPick pick = inspectRomPick(path, all, presets());
    QCOMPARE(pick.platforms, platforms);
    QCOMPARE(pick.info.title, title);
    QCOMPARE(pick.problem, problem);
}

void RomScanTest::scanFolderGames()
{
    QTemporaryDir dir;
    QVERIFY(touch(dir.filePath("ps3/Demon's Souls [BLUS30443]/PS3_GAME/USRDIR/EBOOT.BIN")));
    QVERIFY(touch(dir.filePath("ps3/Demon's Souls [BLUS30443]/PS3_GAME/USRDIR/data.bin")));
    QVERIFY(touch(dir.filePath("ps3/God of War III/PS3_GAME/USRDIR/EBOOT.BIN")));

    EmulatorDef rpcs3;
    rpcs3.id = QStringLiteral("rpcs3");
    rpcs3.files = {QStringLiteral("eboot.bin")};
    Platform ps3;
    ps3.id = QStringLiteral("ps3");
    ps3.name = QStringLiteral("PS3");
    ps3.emulators = {QStringLiteral("rpcs3")};
    ps3.romFolders = {dir.filePath("ps3")};

    const RomScanResult r = scanRomFolders({ps3}, {rpcs3}, {}, {}, dir.path(), dir.path());
    QCOMPARE(r.files.value("ps3").size(), 2); // not data.bin
    const QList<RomEntry> written = r.indexes.value("ps3");
    QCOMPARE(written.size(), 2);
    QCOMPARE(written.at(0).path, QStringLiteral("Demon's Souls [BLUS30443]/PS3_GAME/USRDIR/EBOOT.BIN"));
    QCOMPARE(written.at(0).title, QStringLiteral("Demon's Souls"));
    QCOMPARE(written.at(0).id, QStringLiteral("ps3-demon-s-souls"));
    QCOMPARE(written.at(1).title, QStringLiteral("God of War III"));
}

void RomScanTest::scanRecordsSizes()
{
    QTemporaryDir dir;
    QVERIFY(touch(dir.filePath("switch/Tetris.nro"), 3));
    QVERIFY(touch(dir.filePath("ps3/Demon's Souls/PS3_GAME/USRDIR/EBOOT.BIN"), 5));
    QVERIFY(touch(dir.filePath("ps3/Demon's Souls/PS3_GAME/USRDIR/data.bin"), 7));
    QVERIFY(touch(dir.filePath("ps3/Demon's Souls/.hidden"), 1));

    EmulatorDef rpcs3;
    rpcs3.id = QStringLiteral("rpcs3");
    rpcs3.files = {QStringLiteral("eboot.bin")};
    Platform ps3 = {"ps3", "PS3", {}, {"rpcs3"}, {dir.filePath("ps3")}};
    QList<EmulatorDef> emulators = presets();
    emulators.append(rpcs3);
    const Platform switchP = switchPlatform({dir.filePath("switch")});

    RomScanResult r = scanRomFolders({switchP, ps3}, emulators, {}, {}, dir.path(), dir.path());
    QCOMPARE(r.indexes.value("switch").first().size, 3);
    QCOMPARE(r.indexes.value("ps3").first().size, 13); // the whole folder

    // A folder game's size is added up once; later scans keep it.
    QVERIFY(touch(dir.filePath("ps3/Demon's Souls/more.bin"), 100));
    r = scanRomFolders({switchP, ps3}, emulators, r.indexes, {}, dir.path(), dir.path());
    QVERIFY(!r.indexes.contains("ps3"));
}

void RomScanTest::scanWithoutEmulators()
{
    QTemporaryDir dir;
    Platform platform = switchPlatform({dir.path()});
    platform.emulators = {"nope"};
    const RomScanResult r = scanRomFolders({platform}, presets(), {}, {}, dir.path(), dir.path());
    QVERIFY(r.files.isEmpty());
    QCOMPARE(r.warnings, QStringList{"library/platforms.toml:7: Switch has ROM folders but no emulators, so they "
                                     "aren't scanned; add emulators = [\"ryubing\"]"});
}

void RomScanTest::resolveFolders()
{
    QCOMPARE(resolveRomFolder("roms/switch", "/data", "/home/u"), QStringLiteral("/data/roms/switch"));
    QCOMPARE(resolveRomFolder("~/roms/", "/data", "/home/u"), QStringLiteral("/home/u/roms"));
    QCOMPARE(resolveRomFolder("/mnt/games/../games/3ds", "/data", "/home/u"), QStringLiteral("/mnt/games/3ds"));
}

QTEST_APPLESS_MAIN(RomScanTest)
#include "romscan_test.moc"
