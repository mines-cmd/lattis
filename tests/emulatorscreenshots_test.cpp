#include "emulatorscreenshots.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>

#include <memory>

class EmulatorScreenshotsTest : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void presetsParse();
    void perGameFolder();
    void idPrefixJpgAndPng();
    void titlePrefixWithStrippedCharacters();
    void titlePrefixDoesNotTakeALongerTitle();
    void titleIgnoresPunctuation();
    void romTitleThenLibraryTitle();
    void duckStationLayouts();
    void pcsx2Layouts();
    void besideTheRom();
    void cemuBySessionTime();
    void portableInstall();
    void noMatches();
    void noScreenshotsKey();
    void newestFirst();
    void ownFolders();
    void ownFoldersWithoutPatterns();
    void builtInFolders();
    void timeInName_data();
    void timeInName();
    void idOfPrefersSerial();

private:
    QString path(const QString &relative) const { return m_dir->filePath(relative); }
    void touch(const QString &relative);
    EmulatorDef emulator(const QString &id) const;
    EmulatorInstall install(const QString &kind = QStringLiteral("native")) const;
    Game game(const QString &id, const QString &title, const QString &rom = {}) const;
    QList<EmulatorScreenshots::Shot> list(const QString &emulatorId, const Game &game, const RomInfo &info,
                                          const EmulatorInstall &install = {},
                                          const QList<PlayHistory::Session> &sessions = {}) const;
    QStringList names(const QList<EmulatorScreenshots::Shot> &shots) const;

    std::unique_ptr<QTemporaryDir> m_dir;
    QList<EmulatorDef> m_emulators;
};

void EmulatorScreenshotsTest::init()
{
    m_dir = std::make_unique<QTemporaryDir>();
    QVERIFY(m_dir->isValid());
    QStringList warnings;
    m_emulators = Emulators::merge(Emulators::presets(), {}, &warnings);
}

void EmulatorScreenshotsTest::touch(const QString &relative)
{
    const QString file = path(relative);
    QVERIFY(QDir().mkpath(QFileInfo(file).absolutePath()));
    QFile f(file);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write("x");
}

EmulatorDef EmulatorScreenshotsTest::emulator(const QString &id) const
{
    const EmulatorDef *def = Emulators::find(m_emulators, id);
    return def ? *def : EmulatorDef();
}

EmulatorInstall EmulatorScreenshotsTest::install(const QString &kind) const
{
    EmulatorInstall install;
    install.kind = kind == QLatin1String("flatpak") ? EmulatorInstall::Kind::Flatpak : EmulatorInstall::Kind::Path;
    install.portable = kind == QLatin1String("portable");
    install.program = kind == QLatin1String("portable") ? path(QStringLiteral("portable-app/emu")) : QStringLiteral("emu");
    return install;
}

Game EmulatorScreenshotsTest::game(const QString &id, const QString &title, const QString &rom) const
{
    Game g;
    g.id = id;
    g.title = title;
    g.rom = true;
    g.romPath = rom.isEmpty() ? QString() : path(rom);
    return g;
}

QList<EmulatorScreenshots::Shot> EmulatorScreenshotsTest::list(const QString &emulatorId, const Game &game,
                                                               const RomInfo &info, const EmulatorInstall &install,
                                                               const QList<PlayHistory::Session> &sessions) const
{
    const EmulatorInstall chosen = install.found() ? install : this->install();
    return EmulatorScreenshots::list(emulator(emulatorId), chosen, game, info, sessions, m_dir->path());
}

QStringList EmulatorScreenshotsTest::names(const QList<EmulatorScreenshots::Shot> &shots) const
{
    QStringList out;
    for (const EmulatorScreenshots::Shot &s : shots)
        out.append(s.id);
    return out;
}

void EmulatorScreenshotsTest::presetsParse()
{
    QStringList withShots;
    for (const EmulatorDef &e : m_emulators) {
        if (!e.screenshots.isEmpty())
            withShots.append(e.id);
    }
    withShots.sort();
    QCOMPARE(withShots, (QStringList{"azahar", "cemu", "dolphin", "duckstation", "mgba", "pcsx2", "ppsspp", "rmg",
                                     "rpcs3", "ryubing", "shadps4"}));
    QVERIFY(emulator(QStringLiteral("melonds")).screenshots.isEmpty());
}

void EmulatorScreenshotsTest::perGameFolder()
{
    touch(".local/share/dolphin-emu/ScreenShots/GALE01/GALE01_2024-05-01_10-00-00.png");
    touch(".local/share/dolphin-emu/ScreenShots/GALE01/GALE01_2024-05-02_10-00-00.png");
    touch(".local/share/dolphin-emu/ScreenShots/GM4E01/GM4E01_2024-05-03_10-00-00.png");
    RomInfo info;
    info.serial = QStringLiteral("GALE01");
    const auto shots = list(QStringLiteral("dolphin"), game(QStringLiteral("g"), QStringLiteral("Melee")), info);
    QCOMPARE(names(shots), (QStringList{"GALE01_2024-05-02_10-00-00.png", "GALE01_2024-05-01_10-00-00.png"}));
    QCOMPARE(shots.first().time, QDateTime(QDate(2024, 5, 2), QTime(10, 0, 0)));
}

void EmulatorScreenshotsTest::idPrefixJpgAndPng()
{
    touch(".config/ppsspp/PSP/SCREENSHOT/ULUS10041_00000.jpg");
    touch(".config/ppsspp/PSP/SCREENSHOT/ULUS10041_00001.png");
    touch(".config/ppsspp/PSP/SCREENSHOT/ULUS10042_00000.jpg");
    RomInfo info;
    info.discId = QStringLiteral("ULUS10041");
    const auto shots = list(QStringLiteral("ppsspp"), game(QStringLiteral("g"), QStringLiteral("Game")), info);
    QStringList got = names(shots);
    got.sort();
    QCOMPARE(got, (QStringList{"ULUS10041_00000.jpg", "ULUS10041_00001.png"}));
}

void EmulatorScreenshotsTest::titlePrefixWithStrippedCharacters()
{
    touch(".local/share/azahar-emu/screenshots/Zelda Link's Awakening_20240501_100000.png");
    touch(".local/share/azahar-emu/screenshots/Other_20240501_100000.png");
    RomInfo info;
    info.title = QStringLiteral("Zelda: Link's Awakening");
    // The emulator dropped the colon; the library's own title has other spelling.
    const auto shots = list(QStringLiteral("azahar"), game(QStringLiteral("g"), QStringLiteral("Zelda LA")), info);
    QCOMPARE(names(shots), (QStringList{"Zelda Link's Awakening_20240501_100000.png"}));
}

void EmulatorScreenshotsTest::titlePrefixDoesNotTakeALongerTitle()
{
    touch(".local/share/azahar-emu/screenshots/Mario Kart_20240501_100000.png");
    touch(".local/share/azahar-emu/screenshots/Mario_20240501_100000.png");
    RomInfo info;
    info.title = QStringLiteral("mario");
    const auto shots = list(QStringLiteral("azahar"), game(QStringLiteral("g"), QStringLiteral("Mario")), info);
    QCOMPARE(names(shots), (QStringList{"Mario_20240501_100000.png"}));
}

void EmulatorScreenshotsTest::titleIgnoresPunctuation()
{
    touch(".local/share/azahar-emu/screenshots/Super Smash Bros. Ultimate_20240501_100000.png");
    touch(".local/share/azahar-emu/screenshots/Super Smash Bros Melee_20240501_100000.png");
    const auto shots = list(QStringLiteral("azahar"), game(QStringLiteral("g"), QStringLiteral("Super Smash Bros Ultimate")), {});
    QCOMPARE(names(shots), (QStringList{"Super Smash Bros. Ultimate_20240501_100000.png"}));
}

void EmulatorScreenshotsTest::romTitleThenLibraryTitle()
{
    touch(".local/share/azahar-emu/screenshots/Rom Title_20240501_100000.png");
    touch(".local/share/azahar-emu/screenshots/Renamed_20240502_100000.png");
    RomInfo info;
    info.title = QStringLiteral("Rom Title");
    const auto shots = list(QStringLiteral("azahar"), game(QStringLiteral("g"), QStringLiteral("Renamed")), info);
    QCOMPARE(names(shots), (QStringList{"Renamed_20240502_100000.png", "Rom Title_20240501_100000.png"}));
}

void EmulatorScreenshotsTest::duckStationLayouts()
{
    touch(".local/share/duckstation/screenshots/Crash 2024-05-01-10-00-00.png");
    touch(".local/share/duckstation/screenshots/Crash Bandicoot 2024-05-01-10-00-00.png");
    touch(".local/share/duckstation/screenshots/Crash/2024-05-02-10-00-00.png");
    touch(".local/share/duckstation/screenshots/Crash/Crash 2024-05-03-10-00-00.png");
    touch(".local/share/duckstation/screenshots/Spyro/Spyro 2024-05-03-10-00-00.png");
    const auto shots = list(QStringLiteral("duckstation"), game(QStringLiteral("g"), QStringLiteral("Crash")), {});
    QCOMPARE(names(shots), (QStringList{"Crash 2024-05-03-10-00-00.png", "2024-05-02-10-00-00.png",
                                        "Crash 2024-05-01-10-00-00.png"}));
}

void EmulatorScreenshotsTest::pcsx2Layouts()
{
    touch(".config/PCSX2/snaps/Jak_SCUS-97124_20240501100000.png");
    touch(".config/PCSX2/snaps/Daxter_SCUS-97129_20240501100000.png");
    touch(".config/PCSX2/snaps/Jak/Jak_SCUS-97124_20240502100000.png");
    RomInfo info;
    info.serial = QStringLiteral("SCUS-97124");
    info.title = QStringLiteral("Jak");
    const auto shots = list(QStringLiteral("pcsx2"), game(QStringLiteral("g"), QStringLiteral("Jak")), info);
    QCOMPARE(names(shots), (QStringList{"Jak_SCUS-97124_20240502100000.png", "Jak_SCUS-97124_20240501100000.png"}));
}

void EmulatorScreenshotsTest::besideTheRom()
{
    touch("roms/Pokemon.gba");
    touch("roms/Pokemon-0.png");
    touch("roms/Pokemon-12.png");
    touch("roms/Pokemon Fire-0.png");
    touch("roms/Pokemon.sav");
    const auto shots = list(QStringLiteral("mgba"),
                            game(QStringLiteral("g"), QStringLiteral("Pokemon"), QStringLiteral("roms/Pokemon.gba")), {});
    QStringList got = names(shots);
    got.sort();
    QCOMPARE(got, (QStringList{"Pokemon-0.png", "Pokemon-12.png"}));
    QCOMPARE(shots.first().path, path(QStringLiteral("roms/")) + shots.first().id);
}

void EmulatorScreenshotsTest::cemuBySessionTime()
{
    touch(".local/share/Cemu/screenshots/Screenshot_2024-05-01_10-30-00.png");
    touch(".local/share/Cemu/screenshots/Screenshot_2024-05-01_12-30-00.png");
    PlayHistory::Session session;
    session.minutes = 60;
    session.end = QDateTime(QDate(2024, 5, 1), QTime(11, 0, 0)).toUTC();
    const Game g = game(QStringLiteral("g"), QStringLiteral("Mario"));
    QCOMPARE(names(list(QStringLiteral("cemu"), g, {}, {}, {session})),
             (QStringList{"Screenshot_2024-05-01_10-30-00.png"}));
    QVERIFY(list(QStringLiteral("cemu"), g, {}, {}, {}).isEmpty());
}

void EmulatorScreenshotsTest::portableInstall()
{
    touch("portable-app/User/ScreenShots/GALE01/GALE01_2024-05-01_10-00-00.png");
    touch(".local/share/dolphin-emu/ScreenShots/GALE01/GALE01_2024-05-02_10-00-00.png");
    RomInfo info;
    info.serial = QStringLiteral("GALE01");
    const auto shots = list(QStringLiteral("dolphin"), game(QStringLiteral("g"), QStringLiteral("Melee")), info,
                            install(QStringLiteral("portable")));
    QCOMPARE(names(shots), (QStringList{"GALE01_2024-05-01_10-00-00.png"}));
}

void EmulatorScreenshotsTest::noMatches()
{
    touch(".local/share/dolphin-emu/ScreenShots/GM4E01/GM4E01_2024-05-03_10-00-00.png");
    RomInfo info;
    info.serial = QStringLiteral("GALE01");
    QVERIFY(list(QStringLiteral("dolphin"), game(QStringLiteral("g"), QStringLiteral("Melee")), info).isEmpty());
    // Without an ID the pattern can't name a folder: nothing, and not every game's.
    QVERIFY(list(QStringLiteral("dolphin"), game(QStringLiteral("g"), QStringLiteral("Melee")), {}).isEmpty());
}

void EmulatorScreenshotsTest::noScreenshotsKey()
{
    QVERIFY(list(QStringLiteral("melonds"), game(QStringLiteral("g"), QStringLiteral("Game")), {}).isEmpty());
}

void EmulatorScreenshotsTest::newestFirst()
{
    touch(".local/share/dolphin-emu/ScreenShots/GALE01/a.png");
    touch(".local/share/dolphin-emu/ScreenShots/GALE01/b.png");
    QFile a(path(".local/share/dolphin-emu/ScreenShots/GALE01/a.png"));
    QVERIFY(a.open(QIODevice::ReadWrite));
    QVERIFY(a.setFileTime(QDateTime(QDate(2020, 1, 1), QTime(0, 0)), QFileDevice::FileModificationTime));
    a.close();
    RomInfo info;
    info.serial = QStringLiteral("GALE01");
    const auto shots = list(QStringLiteral("dolphin"), game(QStringLiteral("g"), QStringLiteral("Melee")), info);
    QCOMPARE(names(shots), (QStringList{"b.png", "a.png"}));
}

void EmulatorScreenshotsTest::ownFolders()
{
    touch("elsewhere/GALE01/GALE01_2024-05-01_10-00-00.png");
    touch(".local/share/dolphin-emu/ScreenShots/GALE01/GALE01_2024-05-02_10-00-00.png");
    EmulatorDef dolphin = emulator(QStringLiteral("dolphin"));
    dolphin.screenshotFolders = {QStringLiteral("~/elsewhere")};
    dolphin.screenshotFoldersSet = true;
    RomInfo info;
    info.serial = QStringLiteral("GALE01");
    const Game g = game(QStringLiteral("g"), QStringLiteral("Melee"));
    QCOMPARE(names(EmulatorScreenshots::list(dolphin, install(), g, info, {}, m_dir->path())),
             (QStringList{"GALE01_2024-05-01_10-00-00.png"}));
    dolphin.screenshotFolders.clear();
    QVERIFY(EmulatorScreenshots::list(dolphin, install(), g, info, {}, m_dir->path()).isEmpty());
}

void EmulatorScreenshotsTest::ownFoldersWithoutPatterns()
{
    touch("shots/a_2024-05-01_10-30-00.png");
    touch("shots/b_2024-05-01_12-30-00.png");
    EmulatorDef melonds = emulator(QStringLiteral("melonds"));
    melonds.screenshotFolders = {path(QStringLiteral("shots"))};
    melonds.screenshotFoldersSet = true;
    PlayHistory::Session session;
    session.minutes = 60;
    session.end = QDateTime(QDate(2024, 5, 1), QTime(11, 0, 0)).toUTC();
    QCOMPARE(names(EmulatorScreenshots::list(melonds, install(), game(QStringLiteral("g"), QStringLiteral("Game")),
                                             {}, {session}, m_dir->path())),
             (QStringList{"a_2024-05-01_10-30-00.png"}));
}

void EmulatorScreenshotsTest::builtInFolders()
{
    QCOMPARE(EmulatorScreenshots::folders(emulator(QStringLiteral("dolphin")), install(), m_dir->path()),
             (QStringList{path(QStringLiteral(".local/share/dolphin-emu/ScreenShots"))}));
    QCOMPARE(EmulatorScreenshots::folders(emulator(QStringLiteral("mgba")), install(), m_dir->path()),
             (QStringList{"{romdir}"}));
    QVERIFY(EmulatorScreenshots::folders(emulator(QStringLiteral("melonds")), install(), m_dir->path()).isEmpty());
}

void EmulatorScreenshotsTest::timeInName_data()
{
    QTest::addColumn<QString>("name");
    QTest::addColumn<QDateTime>("expected");
    const QDateTime t(QDate(2024, 5, 1), QTime(10, 20, 30));
    QTest::newRow("dolphin") << "GALE01_2024-05-01_10-20-30.png" << t;
    QTest::newRow("duckstation") << "Crash 2024-05-01-10-20-30.png" << t;
    QTest::newRow("pcsx2") << "Jak_SCUS-97124_20240501102030.png" << t;
    QTest::newRow("shadps4") << "CUSA00001_20240501_102030_123_game_1.png" << t;
    QTest::newRow("ppsspp") << "ULUS10041_00001.jpg" << QDateTime();
    QTest::newRow("invalid") << "x_2024-13-45_99-99-99.png" << QDateTime();
}

void EmulatorScreenshotsTest::timeInName()
{
    QFETCH(QString, name);
    QFETCH(QDateTime, expected);
    QCOMPARE(EmulatorScreenshots::timeInName(name), expected);
}

void EmulatorScreenshotsTest::idOfPrefersSerial()
{
    RomInfo info;
    QVERIFY(EmulatorScreenshots::idOf(info).isEmpty());
    info.headerName = QStringLiteral("SUPER MARIO 64");
    QCOMPARE(EmulatorScreenshots::idOf(info), QStringLiteral("SUPER MARIO 64"));
    info.discId = QStringLiteral("ULUS10041");
    QCOMPARE(EmulatorScreenshots::idOf(info), QStringLiteral("ULUS10041"));
    info.titleId = QStringLiteral("BLUS30443");
    QCOMPARE(EmulatorScreenshots::idOf(info), QStringLiteral("BLUS30443"));
    info.serial = QStringLiteral("GALE01");
    QCOMPARE(EmulatorScreenshots::idOf(info), QStringLiteral("GALE01"));
}

QTEST_APPLESS_MAIN(EmulatorScreenshotsTest)
#include "emulatorscreenshots_test.moc"
