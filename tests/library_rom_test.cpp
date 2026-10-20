#include "librarytestbase.h"

// The library tests on the ROM folder scan.
class LibraryRomTest : public LibraryTestBase
{
    Q_OBJECT

private slots:
    void romScanAddsNewRoms();
    void romScanSecondStartWritesNothing();
    void romScanReportsNewGames();
    void romRenamedFileKeepsItsGame();
    void romRenamedFileMatchedBySize();
    void locateRomMergesTheDuplicate();
    void locateRomOutsideRomFolders();
    void favoriteOnRomOnlyGameCreatesFile();
};

void LibraryRomTest::romScanAddsNewRoms()
{
    const QByteArray before = readFile(romIndexPath("switch"));
    auto library = openLibrary();
    QVERIFY2(library->problems().isEmpty(), qPrintable(library->problems().join(u'\n')));

    // The unindexed ROM is added; the update and the DLC aren't; everything else stays.
    const QByteArray after = readFile(romIndexPath("switch"));
    QVERIFY(after.contains("\n\"Homebrew/Tetris.nro\" = { id = \"switch-tetris\", title = \"Tetris\" }\n"));
    QVERIFY(!after.contains("22800"));
    QVERIFY(!after.contains("Booster"));
    for (const QByteArray &line : before.split('\n')) {
        if (line.startsWith('"'))
            QVERIFY2(after.contains(line), line.constData());
    }
    QCOMPARE(readFile(romIndexPath("3ds")), readFile(QStringLiteral(DEMO_DATA_DIR "/library/roms/3ds.toml")));

    const QVariantMap tetris = library->gameAt(library->rowOfGame(QStringLiteral("switch-tetris")));
    QCOMPARE(tetris.value("title").toString(), QStringLiteral("Tetris"));
    QCOMPARE(tetris.value("platform").toString(), QStringLiteral("switch"));
    QVERIFY(tetris.value("installed").toBool());
    QCOMPARE(tetris.value("romPath").toString(), m_data + QStringLiteral("/roms/switch/Homebrew/Tetris.nro"));

    // Indexed, with a game file, but no ROM: listed only without "Installed only".
    QCOMPARE(library->rowOfGame(QStringLiteral("switch-mario-tennis-aces")), -1);
    library->setInstalledOnly(false);
    const QVariantMap tennis = library->gameAt(library->rowOfGame(QStringLiteral("switch-mario-tennis-aces")));
    QVERIFY(tennis.value("rom").toBool());
    QVERIFY(!tennis.value("installed").toBool());

    const QVariantMap mk8 = library->gameAt(library->rowOfGame(QStringLiteral("switch-mario-kart-8-deluxe")));
    QVERIFY(mk8.value("installed").toBool());
    QCOMPARE(mk8.value("year").toString(), QStringLiteral("2017")); // from its game file

    // Steam games aren't ROMs.
    QVERIFY(!library->gameAt(library->rowOfGame(QStringLiteral("steam-1145360"))).value("rom").toBool());

    const LibrarySnapshot &s = library->store().snapshot();
    QCOMPARE(s.installs.value("ryubing").kind, EmulatorInstall::Kind::Flatpak);
    QCOMPARE(s.installs.value("azahar").kind, EmulatorInstall::Kind::Flatpak);
    const QByteArray reference = readFile(m_data + QStringLiteral("/library/emulators.reference.toml"));
    QVERIFY(reference.contains("id = \"ryubing\""));
}

void LibraryRomTest::romScanSecondStartWritesNothing()
{
    openLibrary().reset(); // adds Tetris, saves the cache
    const QString index = romIndexPath("switch");
    const QString reference = m_data + QStringLiteral("/library/emulators.reference.toml");
    const QDateTime indexTime = QFileInfo(index).lastModified();
    const QDateTime referenceTime = QFileInfo(reference).lastModified();

    auto library = openLibrary();
    QCOMPARE(library->store().filesParsed(), 0);
    QCOMPARE(library->gameCount(), 26);
    QCOMPARE(QFileInfo(index).lastModified(), indexTime);
    QCOMPARE(QFileInfo(reference).lastModified(), referenceTime);
    QVERIFY(library->problems().isEmpty());
}

void LibraryRomTest::romScanReportsNewGames()
{
    auto library = openLibrary();
    QSignalSpy added(&library->store(), &LibraryStore::gamesAdded);
    const QString folder = m_data + QStringLiteral("/roms/switch/");
    QVERIFY(QFile::rename(folder + "Metroid Dread [010093801237C000][v0].nsp", folder + "metroid_dread_[010093801237C000].xci"));
    writeFile(folder + "Celeste.nsp", {});

    library->store().scanRoms();
    library->store().waitForIdle();
    QCoreApplication::processEvents();

    // The new file, not the renamed one; the library has it by then.
    QCOMPARE(added.size(), 1);
    QCOMPARE(added.first().first().toStringList(), QStringList{"switch-celeste"});
    QVERIFY(library->rowOfGame(QStringLiteral("switch-celeste")) >= 0);

    // Nothing new: no signal.
    library->store().scanRoms();
    library->store().waitForIdle();
    QCoreApplication::processEvents();
    QCOMPARE(added.size(), 1);
}

void LibraryRomTest::romRenamedFileKeepsItsGame()
{
    auto library = openLibrary();
    const QString folder = m_data + QStringLiteral("/roms/switch/");
    QVERIFY(QFile::rename(folder + "Metroid Dread [010093801237C000][v0].nsp", folder + "metroid_dread_[010093801237C000].xci"));

    library->store().scanRoms();
    library->store().waitForIdle();
    QCoreApplication::processEvents();

    const QVariantMap dread = library->gameAt(library->rowOfGame(QStringLiteral("switch-metroid-dread")));
    QVERIFY(dread.value("installed").toBool());
    QCOMPARE(dread.value("romPath").toString(), folder + "metroid_dread_[010093801237C000].xci");
    QCOMPARE(dread.value("title").toString(), QStringLiteral("Metroid Dread"));
    const QByteArray index = readFile(romIndexPath("switch"));
    QVERIFY(index.contains("\"metroid_dread_[010093801237C000].xci\" = { id = \"switch-metroid-dread\""));
    QVERIFY(!index.contains("Metroid Dread [010093801237C000][v0].nsp"));
    QCOMPARE(library->gameCount(), 26);
}

void LibraryRomTest::romRenamedFileMatchedBySize()
{
    // No title ID and a new title: the size tells it's the same file.
    const QString folder = m_data + QStringLiteral("/roms/3ds/");
    writeFile(folder + "Fire Emblem - Awakening (USA).3ds", "rom bytes");
    auto library = openLibrary();
    QVERIFY(readFile(romIndexPath("3ds")).contains("title = \"Fire Emblem Awakening\", size = 9 }"));
    const QVariantMap before = library->gameAt(library->rowOfGame(QStringLiteral("3ds-fire-emblem-awakening")));
    QCOMPARE(before.value("sizeText").toString(), QLocale().formattedDataSize(9, 1, QLocale::DataSizeTraditionalFormat));

    QVERIFY(QFile::rename(folder + "Fire Emblem - Awakening (USA).3ds", folder + "FE Awakening Decrypted.3ds"));
    library->store().scanRoms();
    library->store().waitForIdle();
    QCoreApplication::processEvents();

    const QVariantMap fe = library->gameAt(library->rowOfGame(QStringLiteral("3ds-fire-emblem-awakening")));
    QVERIFY(fe.value("installed").toBool());
    QCOMPARE(fe.value("romPath").toString(), folder + "FE Awakening Decrypted.3ds");
    QCOMPARE(library->gameCount(), 26);
}

void LibraryRomTest::locateRomMergesTheDuplicate()
{
    // Renamed while the index had no size: the scan makes a new game, and the old one's ROM
    // isn't found.
    const QString folder = m_data + QStringLiteral("/roms/3ds/");
    auto library = openLibrary();
    QVERIFY(QFile::rename(folder + "Fire Emblem - Awakening (USA).3ds", folder + "FE Awakening.3ds"));
    library->store().scanRoms();
    library->store().waitForIdle();
    QCoreApplication::processEvents();
    const QString id = QStringLiteral("3ds-fire-emblem-awakening");
    const QString dup = QStringLiteral("3ds-fe-awakening");
    QCOMPARE(library->gameCount(), 27);
    QCOMPARE(library->gameForEditing(id).value("romPath").toString(), QString());

    // The duplicate was played and given a tag and art before the user noticed.
    QCOMPARE(library->saveGame(dup, {{QStringLiteral("play_minutes"), 60},
                                     {QStringLiteral("last_played"), QDateTime::fromString("2026-09-01T20:00:00-03:00", Qt::ISODate)},
                                     {QStringLiteral("tags"), QVariantList{"JRPG", "Portable"}}}),
             QString());
    QImage picture(4, 4, QImage::Format_RGB32);
    picture.fill(Qt::red);
    const QString png = m_dir->filePath(QStringLiteral("hero.png"));
    QVERIFY(picture.save(png));
    QCOMPARE(library->setArt(dup, QStringLiteral("hero"), png), QString());

    // Locate… picks the renamed file.
    QCOMPARE(library->inspectRom(folder + "FE Awakening.3ds").value("existing").toString(), dup);
    QCOMPARE(library->saveGame(id, {{QStringLiteral("rom"), QUrl::fromLocalFile(folder + "FE Awakening.3ds").toString()}}),
             QString());

    QVERIFY(library->gameForEditing(dup).isEmpty());
    QVERIFY(!QFile::exists(gamePath(dup)));
    QCOMPARE(library->gameCount(), 26);
    const QVariantMap fe = library->gameForEditing(id);
    QCOMPARE(fe.value("romPath").toString(), folder + "FE Awakening.3ds");
    QCOMPARE(fe.value("playMinutes").toInt(), 2960);
    QVERIFY(QFile::exists(m_data + QStringLiteral("/art/") + id + QStringLiteral("/hero.png")));
    QVERIFY(!QFile::exists(m_data + QStringLiteral("/art/") + dup));
    const QByteArray text = readFile(gamePath(id));
    QVERIFY(text.contains("tags = [\"JRPG\", \"Portable\"]\n"));
    QVERIFY(text.contains("last_played = 2026-09-01T20:00:00-03:00\n"));
    QVERIFY(!text.contains("rom ="));
    const QByteArray index = readFile(romIndexPath("3ds"));
    QVERIFY(index.contains("\"FE Awakening.3ds\" = { id = \"3ds-fire-emblem-awakening\""));
    QVERIFY(!index.contains(dup.toUtf8()));

    // The next scan agrees.
    library->store().scanRoms();
    library->store().waitForIdle();
    QCoreApplication::processEvents();
    QCOMPARE(library->gameCount(), 26);
    QCOMPARE(readFile(romIndexPath("3ds")), index);
}

void LibraryRomTest::locateRomOutsideRomFolders()
{
    // Moved out of the ROM folders: the game file says where.
    const QString moved = m_installs.home + QStringLiteral("/Elsewhere/Kid Icarus.3ds");
    writeFile(moved, "12345");
    QVERIFY(QFile::remove(m_data + QStringLiteral("/roms/3ds/Kid Icarus - Uprising (USA) (En,Fr,Es).3ds")));
    auto library = openLibrary();
    const QString id = QStringLiteral("3ds-kid-icarus-uprising");
    QCOMPARE(library->saveGame(id, {{QStringLiteral("rom"), moved}}), QString());
    QVERIFY(readFile(gamePath(id)).contains("rom = \"~/Elsewhere/Kid Icarus.3ds\"\n"));
    const QVariantMap kid = library->gameAt(library->rowOfGame(id));
    QVERIFY(kid.value("installed").toBool());
    QCOMPARE(kid.value("romPath").toString(), moved);
    QCOMPARE(kid.value("sizeText").toString(), QLocale().formattedDataSize(5, 1, QLocale::DataSizeTraditionalFormat));
}

void LibraryRomTest::favoriteOnRomOnlyGameCreatesFile()
{
    auto library = openLibrary();
    QCOMPARE(library->setGameValue(QStringLiteral("switch-tetris"), QStringLiteral("favorite"), true), QString());
    QCOMPARE(readFile(gamePath("switch-tetris")), QByteArray("favorite = true\n"));
    const QVariantMap tetris = library->gameAt(library->rowOfGame(QStringLiteral("switch-tetris")));
    QVERIFY(tetris.value("favorite").toBool());
    QCOMPARE(tetris.value("title").toString(), QStringLiteral("Tetris"));
}

QTEST_GUILESS_MAIN(LibraryRomTest)

#include "library_rom_test.moc"
