#include "romfoldermatch.h"

#include <QTest>

class RomFolderMatchTest : public QObject
{
    Q_OBJECT

private slots:
    void byAlias_data();
    void byAlias();
    void normalize();
};

void RomFolderMatchTest::byAlias_data()
{
    QTest::addColumn<QString>("folder");
    QTest::addColumn<QString>("expected");

    QTest::newRow("switch id") << "switch" << "switch";
    QTest::newRow("switch name") << "Nintendo Switch" << "switch";
    QTest::newRow("switch abbreviation") << "NSW" << "switch";
    QTest::newRow("switch dashes") << "nintendo-switch" << "switch";
    QTest::newRow("switch underscores") << "nintendo_switch" << "switch";
    QTest::newRow("switch 2 name") << "Switch 2" << "switch-2";
    QTest::newRow("switch 2 folder") << "switch2" << "switch-2";
    QTest::newRow("switch 2 abbreviation") << "NS2" << "switch-2";
    QTest::newRow("3ds") << "3DS" << "3ds";
    QTest::newRow("3ds name") << "Nintendo 3DS" << "3ds";
    QTest::newRow("wiiu") << "Wii U" << "wiiu";
    QTest::newRow("wii, not wiiu") << "Wii" << "wii";
    QTest::newRow("gamecube abbreviation") << "GC" << "gamecube";
    QTest::newRow("nds") << "Nintendo DS" << "nds";
    QTest::newRow("ds, not 3ds") << "DS" << "nds";
    QTest::newRow("gba") << "Game Boy Advance" << "gba";
    QTest::newRow("gba still gba") << "GBA" << "gba";
    QTest::newRow("game boy") << "Game Boy" << "gb";
    QTest::newRow("gb, not gba") << "GB" << "gb";
    QTest::newRow("game boy color") << "Game Boy Color" << "gbc";
    QTest::newRow("gbc") << "gbc" << "gbc";
    QTest::newRow("snes") << "SNES" << "snes";
    QTest::newRow("super famicom") << "Super Famicom" << "snes";
    QTest::newRow("sfc") << "sfc" << "snes";
    QTest::newRow("nes") << "NES" << "nes";
    QTest::newRow("famicom") << "Famicom" << "nes";
    QTest::newRow("fds") << "FDS" << "nes";
    QTest::newRow("mega drive") << "Mega Drive" << "genesis";
    QTest::newRow("sega genesis") << "Sega Genesis" << "genesis";
    QTest::newRow("md") << "MD" << "genesis";
    QTest::newRow("master system") << "Master System" << "sms";
    QTest::newRow("sms") << "SMS" << "sms";
    QTest::newRow("game gear") << "Game Gear" << "gamegear";
    QTest::newRow("gg") << "GG" << "gamegear";
    QTest::newRow("dreamcast") << "Dreamcast" << "dreamcast";
    QTest::newRow("dc, not ds") << "DC" << "dreamcast";
    QTest::newRow("ds, not dc") << "ds" << "nds";
    QTest::newRow("arcade") << "arcade" << "arcade";
    QTest::newRow("mame") << "MAME" << "arcade";
    QTest::newRow("ps2") << "PlayStation 2" << "ps2";
    QTest::newRow("ps1, not ps2") << "PSX" << "ps1";
    QTest::newRow("psp") << "PlayStation Portable" << "psp";
    QTest::newRow("no match") << "bios" << "";
    QTest::newRow("empty") << "" << "";
}

void RomFolderMatchTest::byAlias()
{
    QFETCH(QString, folder);
    QFETCH(QString, expected);
    QCOMPARE(RomFolderMatch::byAlias(folder), expected);
}

void RomFolderMatchTest::normalize()
{
    QCOMPARE(RomFolderMatch::normalize("Nintendo Switch"), QStringLiteral("nintendoswitch"));
    QCOMPARE(RomFolderMatch::normalize("nintendo-switch"), QStringLiteral("nintendoswitch"));
    QCOMPARE(RomFolderMatch::normalize("nintendo_switch"), QStringLiteral("nintendoswitch"));
    QCOMPARE(RomFolderMatch::normalize(""), QString());
}

QTEST_APPLESS_MAIN(RomFolderMatchTest)
#include "romfoldermatch_test.moc"
