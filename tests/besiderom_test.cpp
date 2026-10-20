#include "besiderom.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>

#include <memory>

class BesideRomTest : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void foundPerGame();
    void otherFilesLeftOut();
    void romInSubfolder();
    void missingRomFolder();
    void onlyTheEmulatorsGames();
    void noExtensions();
    void restoreNextToMovedRom();
    void restoreUnderRenamedRom();
    void restoreWithoutRom();

private:
    QString path(const QString &relative) const { return m_dir->filePath(relative); }
    void touch(const QString &relative, qint64 size = 0);
    Game romGame(const QString &id, const QString &platform, const QString &rom) const;
    QList<BesideRom::GameSaves> find(const QList<Game> &games) const;

    std::unique_ptr<QTemporaryDir> m_dir;
    EmulatorDef m_mgba;
    QList<Platform> m_platforms;
};

void BesideRomTest::init()
{
    m_dir = std::make_unique<QTemporaryDir>();
    QVERIFY(m_dir->isValid());
    m_mgba = {};
    m_mgba.id = QStringLiteral("mgba");
    m_mgba.savesBesideRom = {QStringLiteral("sav"), QStringLiteral("ss1")};
    Platform gba;
    gba.id = QStringLiteral("gba");
    gba.emulators = {QStringLiteral("other"), QStringLiteral("mgba")};
    Platform nds;
    nds.id = QStringLiteral("nds");
    nds.emulators = {QStringLiteral("melonds")};
    m_platforms = {gba, nds};
}

void BesideRomTest::touch(const QString &relative, qint64 size)
{
    const QString file = path(relative);
    QVERIFY(QDir().mkpath(QFileInfo(file).absolutePath()));
    QFile f(file);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write(QByteArray(size, 'x'));
}

Game BesideRomTest::romGame(const QString &id, const QString &platform, const QString &rom) const
{
    Game game;
    game.id = id;
    game.platform = platform;
    game.rom = true;
    game.romPath = rom.isEmpty() ? QString() : path(rom);
    game.installed = !rom.isEmpty();
    return game;
}

QList<BesideRom::GameSaves> BesideRomTest::find(const QList<Game> &games) const
{
    return BesideRom::find(m_mgba, m_platforms, games);
}

void BesideRomTest::foundPerGame()
{
    touch(QStringLiteral("roms/Metroid Fusion.gba"));
    touch(QStringLiteral("roms/Metroid Fusion.sav"), 3);
    touch(QStringLiteral("roms/Metroid Fusion.ss1"), 5);
    touch(QStringLiteral("roms/Pokemon Emerald (v1.1).gba"));
    touch(QStringLiteral("roms/Pokemon Emerald (v1.1).SAV"), 7); // any case
    touch(QStringLiteral("roms/Golden Sun.gba")); // no saves

    const QList<Game> games = {romGame(QStringLiteral("gba-metroid-fusion"), QStringLiteral("gba"),
                                       QStringLiteral("roms/Metroid Fusion.gba")),
                               romGame(QStringLiteral("gba-golden-sun"), QStringLiteral("gba"),
                                       QStringLiteral("roms/Golden Sun.gba")),
                               romGame(QStringLiteral("gba-pokemon-emerald"), QStringLiteral("gba"),
                                       QStringLiteral("roms/Pokemon Emerald (v1.1).gba"))};
    const QList<BesideRom::GameSaves> expected = {
        {QStringLiteral("gba-metroid-fusion"),
         {{path(QStringLiteral("roms/Metroid Fusion.sav")), 3}, {path(QStringLiteral("roms/Metroid Fusion.ss1")), 5}}},
        {QStringLiteral("gba-pokemon-emerald"), {{path(QStringLiteral("roms/Pokemon Emerald (v1.1).SAV")), 7}}},
    };
    QCOMPARE(find(games), expected);
}

void BesideRomTest::otherFilesLeftOut()
{
    touch(QStringLiteral("roms/Advance Wars.gba"));
    touch(QStringLiteral("roms/Advance Wars.sav"));
    for (const char *name : {"Advance Wars.png", "Advance Wars.cheats", "Advance Wars.ss1.bak",
                             "Advance Wars (Beta).sav", "Advance Wars 2.sav", "advance wars.sav",
                             "Advance Wars.gba.sav", "Other.sav", "Advance Wars.ss1/state"})
        touch(QStringLiteral("roms/") + QLatin1String(name));

    const QList<BesideRom::GameSaves> saves = find(
        {romGame(QStringLiteral("gba-advance-wars"), QStringLiteral("gba"), QStringLiteral("roms/Advance Wars.gba"))});
    QCOMPARE(saves.size(), 1);
    QCOMPARE(saves.first().files, (QList<BesideRom::SaveFile>{{path(QStringLiteral("roms/Advance Wars.sav")), 0}}));
}

void BesideRomTest::romInSubfolder()
{
    touch(QStringLiteral("roms/Hacks/Zelda.gba"));
    touch(QStringLiteral("roms/Hacks/Zelda.sav"));
    touch(QStringLiteral("roms/Zelda.sav")); // beside another ROM of that name, not this one

    const QList<BesideRom::GameSaves> saves =
        find({romGame(QStringLiteral("gba-zelda"), QStringLiteral("gba"), QStringLiteral("roms/Hacks/Zelda.gba"))});
    QCOMPARE(saves.size(), 1);
    QCOMPARE(saves.first().files, (QList<BesideRom::SaveFile>{{path(QStringLiteral("roms/Hacks/Zelda.sav")), 0}}));
}

void BesideRomTest::missingRomFolder()
{
    // A ROM the library didn't find, and one whose folder is gone since.
    QVERIFY(find({romGame(QStringLiteral("gba-lost"), QStringLiteral("gba"), QString()),
                  romGame(QStringLiteral("gba-gone"), QStringLiteral("gba"), QStringLiteral("gone/Gone.gba"))})
                .isEmpty());
}

void BesideRomTest::onlyTheEmulatorsGames()
{
    touch(QStringLiteral("roms/A.gba"));
    touch(QStringLiteral("roms/A.sav"));
    touch(QStringLiteral("roms/B.gba"));
    touch(QStringLiteral("roms/B.sav"));
    touch(QStringLiteral("roms/C.nds"));
    touch(QStringLiteral("roms/C.sav"));
    touch(QStringLiteral("roms/D.nds"));
    touch(QStringLiteral("roms/D.sav"));

    Game pinnedAway = romGame(QStringLiteral("gba-b"), QStringLiteral("gba"), QStringLiteral("roms/B.gba"));
    pinnedAway.emulator = QStringLiteral("other");
    Game pinnedHere = romGame(QStringLiteral("nds-d"), QStringLiteral("nds"), QStringLiteral("roms/D.nds"));
    pinnedHere.emulator = QStringLiteral("mgba");
    Game steam;
    steam.id = QStringLiteral("steam-1145360");
    steam.platform = QStringLiteral("pc");

    const QList<BesideRom::GameSaves> saves =
        find({romGame(QStringLiteral("gba-a"), QStringLiteral("gba"), QStringLiteral("roms/A.gba")), pinnedAway,
              romGame(QStringLiteral("nds-c"), QStringLiteral("nds"), QStringLiteral("roms/C.nds")), pinnedHere,
              steam});
    QCOMPARE(saves.size(), 2);
    QCOMPARE(saves.at(0).gameId, QStringLiteral("gba-a"));
    QCOMPARE(saves.at(1).gameId, QStringLiteral("nds-d"));
}

void BesideRomTest::noExtensions()
{
    touch(QStringLiteral("roms/A.gba"));
    touch(QStringLiteral("roms/A.sav"));
    m_mgba.savesBesideRom.clear();
    QVERIFY(find({romGame(QStringLiteral("gba-a"), QStringLiteral("gba"), QStringLiteral("roms/A.gba"))}).isEmpty());
}

void BesideRomTest::restoreNextToMovedRom()
{
    QCOMPARE(BesideRom::restorePath(QStringLiteral("/games/new/Metroid Fusion.gba"), QStringLiteral("Metroid Fusion.ss1")),
             QStringLiteral("/games/new/Metroid Fusion.ss1"));
}

void BesideRomTest::restoreUnderRenamedRom()
{
    QCOMPARE(BesideRom::restorePath(QStringLiteral("/games/Metroid Fusion (USA, v1.1).gba"),
                                    QStringLiteral("Metroid Fusion.sav")),
             QStringLiteral("/games/Metroid Fusion (USA, v1.1).sav"));
    // The save's extension keeps its case.
    QCOMPARE(BesideRom::restorePath(QStringLiteral("/games/B.gba"), QStringLiteral("A.b.SAV")),
             QStringLiteral("/games/B.SAV"));
}

void BesideRomTest::restoreWithoutRom()
{
    QCOMPARE(BesideRom::restorePath(QString(), QStringLiteral("A.sav")), QString());
    QCOMPARE(BesideRom::restorePath(QStringLiteral("/games/A.gba"), QStringLiteral("A")), QString());
    QCOMPARE(BesideRom::restorePath(QStringLiteral("/games/A.gba"), QStringLiteral("A.")), QString());
}

QTEST_GUILESS_MAIN(BesideRomTest)
#include "besiderom_test.moc"
