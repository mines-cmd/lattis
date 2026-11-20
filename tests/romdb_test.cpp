#include "romdb.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>

class RomDbTest : public QObject
{
    Q_OBJECT

    static void write(const QString &path, const QByteArray &bytes)
    {
        QDir().mkpath(QFileInfo(path).path());
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(bytes);
    }

private slots:
    void parsesLines();
    void filesPerPlatform();
    void namesFromTheDatabase();
    void retroPlatformsAreKeyedByChecksum();
    void arcadeListsMachinesAndNonGames();
    void switchOnlyNamesBaseGames();
    void keepsTheNameTheFileGave();
    void fallsBackToTheDiscsOwnName();
    void downloadsOncePerPlatformAndOnlyWhenNeeded();
    void aFailedDownloadIsNotRetried();
};

void RomDbTest::parsesLines()
{
    const QHash<QString, QString> db = RomDb::parse("# source: x, 2026-10-23\n\nslus-20312\tGran Turismo 4\nno tab\n\tno key\nAB\t \n"
                                                    "CD\tName\twith tab\r\n");
    QCOMPARE(db.size(), 2);
    QCOMPARE(db.value("SLUS-20312"), QStringLiteral("Gran Turismo 4"));
    QCOMPARE(db.value("CD"), QStringLiteral("Name\twith tab"));
}

void RomDbTest::filesPerPlatform()
{
    QCOMPARE(RomDb::fileOf("wii"), QStringLiteral("wii"));
    QCOMPARE(RomDb::fileOf("gamecube"), QStringLiteral("wii"));
    QCOMPARE(RomDb::fileOf("ps2"), QStringLiteral("ps2"));
    QVERIFY(RomDb::fileOf("psp").isEmpty());
    QVERIFY(!RomDb::covers("wiiu"));
    QVERIFY(RomDb::covers("gba"));
    for (const char *id : {"nes", "snes", "genesis", "gb", "gbc", "sms", "gamegear", "arcade"})
        QCOMPARE(RomDb::fileOf(id), QString::fromLatin1(id));
    QVERIFY(!RomDb::covers("dreamcast"));
}

void RomDbTest::retroPlatformsAreKeyedByChecksum()
{
    QTemporaryDir dir;
    write(dir.filePath("nes.tsv"), "00000001\tFoo (USA)\n00000002\tBar (Europe)\n");
    write(dir.filePath("gb.tsv"), "00000003\tBaz (World)\n");
    RomDb db({dir.path(), {}});

    // The checksum without the header first, then the whole file's.
    RomInfo plain;
    plain.crc32 = "00000001";
    plain.crc32WithHeader = "00000002";
    db.name("nes", {}, &plain);
    QCOMPARE(plain.title, QStringLiteral("Foo"));
    RomInfo headered;
    headered.crc32 = "0000FFFF";
    headered.crc32WithHeader = "00000002";
    db.name("nes", {}, &headered);
    QCOMPARE(headered.title, QStringLiteral("Bar"));
    QVERIFY(headered.fromDatabase);
    RomInfo neither;
    neither.crc32 = "0000FFFF";
    neither.crc32WithHeader = "0000EEEE";
    db.name("nes", {}, &neither);
    QVERIFY(neither.title.isEmpty());

    // Each platform searches its own list: a Game Boy's checksum isn't a NES's.
    RomInfo gb;
    gb.crc32 = "00000003";
    db.name("nes", {}, &gb);
    QVERIFY(gb.title.isEmpty());
    db.name("gb", {}, &gb);
    QCOMPARE(gb.title, QStringLiteral("Baz"));
}

void RomDbTest::arcadeListsMachinesAndNonGames()
{
    QTemporaryDir dir;
    write(dir.filePath("arcade.tsv"), "SF2\tStreet Fighter II: The World Warrior (World 910522)\nNEOGEO\t-\n");
    RomDb db({dir.path(), {}});

    RomInfo sf2;
    sf2.serial = "sf2";
    db.name("arcade", {}, &sf2);
    QCOMPARE(sf2.title, QStringLiteral("Street Fighter II: The World Warrior"));
    // The marker is never a name.
    RomInfo bios;
    bios.serial = "neogeo";
    db.name("arcade", {}, &bios);
    QVERIFY(bios.title.isEmpty());

    QVERIFY(db.isNotAGame("arcade", "neogeo.zip"));
    QVERIFY(db.isNotAGame("arcade", "Sub folder/NeoGeo.ZIP"));
    QVERIFY(!db.isNotAGame("arcade", "sf2.zip"));
    QVERIFY(!db.isNotAGame("arcade", "unlisted.zip"));
    // Only arcade has them, and without a database nothing is left out.
    QVERIFY(!db.isNotAGame("nes", "neogeo.zip"));
    RomDb without;
    QVERIFY(!without.isNotAGame("arcade", "neogeo.zip"));
}

void RomDbTest::namesFromTheDatabase()
{
    QTemporaryDir dir;
    write(dir.filePath("ps2.tsv"), "SLUS-20312\tGran Turismo 4 (USA)\n");
    write(dir.filePath("3ds.tsv"), "AREE\tLegend of Zelda, The - Tri Force Heroes (USA) (En,Fr,Es)\n");
    write(dir.filePath("gba.tsv"), "1A2B3C4D\tPokemon - Emerald Version (USA, Europe)\n");
    RomDb db({dir.path(), {}});

    RomInfo ps2;
    ps2.serial = "SLUS-20312";
    db.name("ps2", {}, &ps2);
    QCOMPARE(ps2.title, QStringLiteral("Gran Turismo 4"));
    QVERIFY(ps2.fromDatabase);

    RomInfo zelda;
    zelda.serial = "AREE";
    db.name("3ds", {}, &zelda);
    QCOMPARE(zelda.title, QStringLiteral("The Legend of Zelda: Tri Force Heroes"));

    RomInfo gba;
    gba.crc32 = "1a2b3c4d";
    db.name("gba", {}, &gba);
    QCOMPARE(gba.title, QStringLiteral("Pokemon: Emerald Version"));

    RomInfo unknown;
    unknown.serial = "SLUS-99999";
    db.name("ps2", {}, &unknown);
    QVERIFY(unknown.title.isEmpty());
    QVERIFY(!unknown.fromDatabase);

    // No database for the platform, no key, no folder: nothing happens.
    RomInfo psp;
    psp.serial = "ULUS10336";
    db.name("psp", {}, &psp);
    QVERIFY(psp.title.isEmpty());
    RomInfo nothing;
    db.name("gba", {}, &nothing);
    QVERIFY(nothing.title.isEmpty());
    RomDb without;
    RomInfo offline;
    offline.serial = "SLUS-20312";
    without.name("ps2", {}, &offline);
    QVERIFY(offline.title.isEmpty());
}

void RomDbTest::switchOnlyNamesBaseGames()
{
    QTemporaryDir dir;
    write(dir.filePath("switch.tsv"), "0100152000022000\tMario Kart 8 Deluxe\n0100152000022800\tMario Kart 8 Deluxe Update\n");
    RomDb db({dir.path(), {}});

    RomInfo byName;
    db.name("switch", "0100152000022000", &byName);
    QCOMPARE(byName.title, QStringLiteral("Mario Kart 8 Deluxe"));

    // The ID from inside the file, when the file name has none.
    RomInfo byTicket;
    byTicket.titleId = "0100152000022000";
    db.name("switch", {}, &byTicket);
    QCOMPARE(byTicket.title, QStringLiteral("Mario Kart 8 Deluxe"));

    RomInfo update;
    db.name("switch", "0100152000022800", &update);
    QVERIFY(update.title.isEmpty());
}

void RomDbTest::keepsTheNameTheFileGave()
{
    QTemporaryDir dir;
    write(dir.filePath("ps2.tsv"), "SLUS-20312\tGran Turismo 4\n");
    RomDb db({dir.path(), {}});
    RomInfo info;
    info.title = "Own Name";
    info.serial = "SLUS-20312";
    db.name("ps2", {}, &info);
    QCOMPARE(info.title, QStringLiteral("Own Name"));
    QVERIFY(!info.fromDatabase);
}

void RomDbTest::fallsBackToTheDiscsOwnName()
{
    QTemporaryDir dir;
    write(dir.filePath("wii.tsv"), "RMGE01\tMario Kart Wii\n");
    RomDb db({dir.path(), {}});

    RomInfo known;
    known.serial = "RMGE01";
    known.fallbackTitle = "MARIO KART";
    db.name("wii", {}, &known);
    QCOMPARE(known.title, QStringLiteral("Mario Kart Wii"));

    RomInfo homebrew;
    homebrew.serial = "DHOM01";
    homebrew.fallbackTitle = "My Homebrew";
    db.name("gamecube", {}, &homebrew);
    QCOMPARE(homebrew.title, QStringLiteral("My Homebrew"));
    QVERIFY(!homebrew.fromDatabase);
}

void RomDbTest::downloadsOncePerPlatformAndOnlyWhenNeeded()
{
    QTemporaryDir dir;
    QStringList asked;
    RomDbOptions options;
    options.dir = dir.filePath("romdb");
    options.download = [&](const QString &platform, const QString &path) {
        asked.append(platform);
        write(path, "SLUS-20312\tGran Turismo 4\n");
        return true;
    };
    RomDb db(options);

    RomInfo noKey;
    db.name("ps2", {}, &noKey);
    QVERIFY(asked.isEmpty());

    for (int i = 0; i < 3; ++i) {
        RomInfo info;
        info.serial = "SLUS-20312";
        db.name("ps2", {}, &info);
        QCOMPARE(info.title, QStringLiteral("Gran Turismo 4"));
    }
    QCOMPARE(asked, QStringList{"ps2"});

    // A later scan finds the file in the folder.
    RomDb later(options);
    RomInfo info;
    info.serial = "SLUS-20312";
    later.name("ps2", {}, &info);
    QCOMPARE(asked.size(), 1);
    QCOMPARE(info.title, QStringLiteral("Gran Turismo 4"));
}

void RomDbTest::aFailedDownloadIsNotRetried()
{
    QTemporaryDir dir;
    int asked = 0;
    RomDbOptions options;
    options.dir = dir.path();
    options.download = [&](const QString &, const QString &) {
        ++asked;
        return false;
    };
    RomDb db(options);
    for (int i = 0; i < 3; ++i) {
        RomInfo info;
        info.serial = "SLUS-20312";
        db.name("ps2", {}, &info);
        QVERIFY(info.title.isEmpty());
    }
    QCOMPARE(asked, 1);
}

QTEST_APPLESS_MAIN(RomDbTest)
#include "romdb_test.moc"
