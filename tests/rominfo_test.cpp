#include "rombytes.h"
#include "rominfo.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>

using RomInfoReader::read;
using RomInfoReader::readFolder;
using namespace RomBytes;

class RomInfoTest : public QObject
{
    Q_OBJECT

    QTemporaryDir m_dir;

    QString write(const QString &name, const QByteArray &bytes)
    {
        const QString path = m_dir.filePath(name);
        QDir().mkpath(QFileInfo(path).path());
        QFile file(path);
        [&] { QVERIFY(file.open(QIODevice::WriteOnly)); }();
        file.write(bytes);
        return path;
    }

private slots:
    void reads();
    void threeDsNamesAreRead();
    void encryptedThreeDsKeepsItsTitleId();
    void smdhLanguages();
    void ciaCxiAnd3dsx();
    void dsBanners();
    void pspImages();
    void folderGames();
    void badFilesGiveNothing();
    void keysFromHeaders();
    void playStationSerials();
    void gameCubeAndWiiDiscs();
    void switchTitleIdFromTicket();
    void cartridgeChecksums();
};

void RomInfoTest::reads()
{
    QVERIFY(RomInfoReader::reads(QStringLiteral("3ds"), QStringLiteral("3DS")));
    QVERIFY(RomInfoReader::reads(QStringLiteral("nds"), QStringLiteral("nds")));
    QVERIFY(RomInfoReader::reads(QStringLiteral("psp"), QStringLiteral("iso")));
    QVERIFY(RomInfoReader::reads(QStringLiteral("ps2"), QStringLiteral("iso")));
    QVERIFY(RomInfoReader::reads(QStringLiteral("gba"), QStringLiteral("gba")));
    QVERIFY(RomInfoReader::reads(QStringLiteral("switch"), QStringLiteral("nsp")));
    QVERIFY(!RomInfoReader::reads(QStringLiteral("ps2"), QStringLiteral("chd")));
    QVERIFY(!RomInfoReader::reads(QStringLiteral("switch"), QStringLiteral("xci")));
    QVERIFY(!RomInfoReader::reads(QStringLiteral("wiiu"), QStringLiteral("wua")));
}

void RomInfoTest::threeDsNamesAreRead()
{
    const QByteArray layton =
        ncsd(ncch(smdh(QStringLiteral("Professor Layton vs. Phoenix Wright\nAce Attorney")), 0x0004000000123400));
    const RomInfo info = read(write(QStringLiteral("0699 - Professor Layton.3ds"), layton), QStringLiteral("3ds"));
    QCOMPARE(info.title, QStringLiteral("Professor Layton vs. Phoenix Wright: Ace Attorney"));
    QCOMPARE(info.titleId, QStringLiteral("0004000000123400"));

    // A line that already ends in a colon needs no second one.
    const QByteArray zelda = ncsd(ncch(smdh(QStringLiteral("The Legend of Zelda:\nTri Force Heroes")), 0x0004000000164800));
    QCOMPARE(read(write(QStringLiteral("zelda.cci"), zelda), QStringLiteral("3ds")).title,
             QStringLiteral("The Legend of Zelda: Tri Force Heroes"));
}

void RomInfoTest::encryptedThreeDsKeepsItsTitleId()
{
    QByteArray encrypted = ncsd(ncch(smdh(QStringLiteral("Hidden")), 0x0004000000ABCDEF));
    // The ExeFS header of an encrypted dump is noise.
    for (int i = 0x200 + 0x400; i < 0x200 + 0x600; ++i)
        encrypted[i] = char(0x5A);
    const RomInfo info = read(write(QStringLiteral("encrypted.3ds"), encrypted), QStringLiteral("3ds"));
    QVERIFY(info.title.isEmpty());
    QCOMPARE(info.titleId, QStringLiteral("0004000000ABCDEF"));
}

void RomInfoTest::smdhLanguages()
{
    const auto titleOf = [&](const QByteArray &icon) {
        return read(write(QStringLiteral("l.cxi"), ncch(icon, 1)), QStringLiteral("3ds")).title;
    };
    QCOMPARE(titleOf(smdh(QStringLiteral("English"), {}, QStringLiteral("Japanese"))), QStringLiteral("English"));
    QCOMPARE(titleOf(smdh({}, QStringLiteral("Short"), QStringLiteral("Japanese"))), QStringLiteral("Short"));
    QCOMPARE(titleOf(smdh({}, {}, QStringLiteral("Japanese"))), QStringLiteral("Japanese"));
}

void RomInfoTest::ciaCxiAnd3dsx()
{
    const QByteArray icon = smdh(QStringLiteral("Homebrew\nTool"));
    QCOMPARE(read(write(QStringLiteral("a.cia"), cia(icon)), QStringLiteral("3ds")).title, QStringLiteral("Homebrew: Tool"));
    QCOMPARE(read(write(QStringLiteral("a.cxi"), ncch(icon, 5)), QStringLiteral("3ds")).title,
             QStringLiteral("Homebrew: Tool"));
    QCOMPARE(read(write(QStringLiteral("a.3dsx"), threeDsx(icon)), QStringLiteral("3ds")).title,
             QStringLiteral("Homebrew: Tool"));
}

void RomInfoTest::dsBanners()
{
    const auto titleOf = [&](const QByteArray &rom) {
        return read(write(QStringLiteral("a.nds"), rom), QStringLiteral("nds")).title;
    };
    QCOMPARE(titleOf(nds(QStringLiteral("Mario Kart DS\nNintendo"))), QStringLiteral("Mario Kart DS"));
    QCOMPARE(titleOf(nds(QStringLiteral("Professor Layton\nand the Curious Village\nLEVEL-5"))),
             QStringLiteral("Professor Layton: and the Curious Village"));
    QCOMPARE(titleOf(nds(QStringLiteral("Only Japanese"), 0x240)), QStringLiteral("Only Japanese"));
    QCOMPARE(titleOf(nds({})), QString());
}

void RomInfoTest::pspImages()
{
    const QByteArray param = sfo("Crisis Core -Final Fantasy VII-", "ULUS10336");
    const RomInfo iso = read(write(QStringLiteral("a.iso"), pspIso(param)), QStringLiteral("psp"));
    QCOMPARE(iso.title, QStringLiteral("Crisis Core -Final Fantasy VII-"));
    QCOMPARE(iso.titleId, QStringLiteral("ULUS10336"));

    const RomInfo eboot = read(write(QStringLiteral("a.pbp"), pbp(param)), QStringLiteral("psp"));
    QCOMPARE(eboot, iso);

    // The same bytes are not read as a PSP image on another platform.
    QVERIFY(read(write(QStringLiteral("b.iso"), pspIso(param)), QStringLiteral("ps2")).title.isEmpty());
}

void RomInfoTest::folderGames()
{
    write(QStringLiteral("ps3/Game/PS3_GAME/PARAM.SFO"), sfo("Demon's Souls", "BLUS30443"));
    const RomInfo ps3 = readFolder(m_dir.filePath(QStringLiteral("ps3/Game")), QStringLiteral("ps3"));
    QCOMPARE(ps3.title, QStringLiteral("Demon's Souls"));
    QCOMPARE(ps3.titleId, QStringLiteral("BLUS30443"));

    write(QStringLiteral("ps3/Flat/param.sfo"), sfo("Flat Game", "BLES00001"));
    QCOMPARE(readFolder(m_dir.filePath(QStringLiteral("ps3/Flat")), QStringLiteral("ps3")).title, QStringLiteral("Flat Game"));

    write(QStringLiteral("ps4/Sfo/sce_sys/param.sfo"), sfo("Bloodborne", "CUSA00900"));
    QCOMPARE(readFolder(m_dir.filePath(QStringLiteral("ps4/Sfo")), QStringLiteral("ps4")).title, QStringLiteral("Bloodborne"));

    write(QStringLiteral("ps4/Json/sce_sys/param.json"),
          R"({"titleId":"CUSA12345","localizedParameters":{"defaultLanguage":"en-US","en-US":{"titleName":"Astro Bot"}}})");
    const RomInfo json = readFolder(m_dir.filePath(QStringLiteral("ps4/Json")), QStringLiteral("ps4"));
    QCOMPARE(json.title, QStringLiteral("Astro Bot"));
    QCOMPARE(json.titleId, QStringLiteral("CUSA12345"));

    QVERIFY(readFolder(m_dir.filePath(QStringLiteral("ps4/Missing")), QStringLiteral("ps4")).title.isEmpty());
    QVERIFY(readFolder(m_dir.filePath(QStringLiteral("ps4/Json")), QStringLiteral("switch")).title.isEmpty());
}

void RomInfoTest::badFilesGiveNothing()
{
    const QByteArray good = ncsd(ncch(smdh(QStringLiteral("Name")), 1));
    // Every truncation of a good file, and a hostile length or offset, gives nothing and doesn't crash.
    for (const qsizetype size : {qsizetype(0), qsizetype(1), qsizetype(0x100), qsizetype(0x1FF), qsizetype(0x400),
                                 qsizetype(0x700), good.size() - 1}) {
        QVERIFY2(read(write(QStringLiteral("cut.3ds"), good.left(size)), QStringLiteral("3ds")).title.isEmpty(),
                 qPrintable(QString::number(size)));
    }
    QByteArray wild = good;
    put(wild, 0x200 + 0x1A0, 0xFFFFFFFF, 4);
    put(wild, 0x200 + 0x1A4, 0xFFFFFFFF, 4);
    QVERIFY(read(write(QStringLiteral("wild.3ds"), wild), QStringLiteral("3ds")).title.isEmpty());
    put(wild, 0x120, 0xFFFFFFFF, 4);
    QVERIFY(read(write(QStringLiteral("wild.3ds"), wild), QStringLiteral("3ds")).title.isEmpty());

    QByteArray badSfo = sfo("Name", "ID");
    put(badSfo, 16, 0xFFFFFFFF, 4);
    QVERIFY(read(write(QStringLiteral("count.pbp"), pbp(badSfo)), QStringLiteral("psp")).title.isEmpty());
    // A value longer than the file is cut at the file's end, not read past it.
    badSfo = sfo("Name", "ID");
    put(badSfo, 20 + 4, 0xFFFFFFF0, 4);
    QCOMPARE(read(write(QStringLiteral("length.pbp"), pbp(badSfo)), QStringLiteral("psp")).title, QStringLiteral("Name"));

    QByteArray badIso = pspIso(sfo("Name", "ID"));
    put(badIso, 16 * 2048 + 156 + 2, 0x7FFFFFFF, 4);
    QVERIFY(read(write(QStringLiteral("bad.iso"), badIso), QStringLiteral("psp")).title.isEmpty());

    QByteArray bannerAway = nds(QStringLiteral("Name"));
    put(bannerAway, 0x68, 0xFFFFFFF0, 4);
    QVERIFY(read(write(QStringLiteral("away.nds"), bannerAway), QStringLiteral("nds")).title.isEmpty());

    QVERIFY(read(m_dir.filePath(QStringLiteral("nothing.3ds")), QStringLiteral("3ds")).title.isEmpty());
    QVERIFY(read(write(QStringLiteral("junk.cia"), QByteArray(5000, 'x')), QStringLiteral("3ds")).title.isEmpty());
}

void RomInfoTest::keysFromHeaders()
{
    QByteArray ncchBytes = ncch(smdh(QStringLiteral("Game")), 0x0004000000123400);
    putText(ncchBytes, 0x150, "CTR-P-AVSE");
    QCOMPARE(read(write(QStringLiteral("a.3ds"), ncsd(ncchBytes)), QStringLiteral("3ds")).serial, QStringLiteral("AVSE"));

    QByteArray game = nds(QStringLiteral("Game"));
    putText(game, 0x0C, "ABCE");
    const RomInfo ds = read(write(QStringLiteral("a.nds"), game), QStringLiteral("nds"));
    QCOMPARE(ds.serial, QStringLiteral("ABCE"));
    QCOMPARE(ds.title, QStringLiteral("Game"));

    // A banner-less game still gives its code.
    QByteArray bare(0x200, '\0');
    putText(bare, 0x0C, "ABCJ");
    QCOMPARE(read(write(QStringLiteral("b.nds"), bare), QStringLiteral("nds")).serial, QStringLiteral("ABCJ"));
}

void RomInfoTest::playStationSerials()
{
    const QByteArray ps2 = "BOOT2 = cdrom0:\\SLUS_203.12;1\r\nVER = 1.00\r\nVMODE = NTSC\r\n";
    QCOMPARE(read(write(QStringLiteral("a.iso"), isoWithFile("SYSTEM.CNF;1", ps2)), QStringLiteral("ps2")).serial,
             QStringLiteral("SLUS-20312"));
    const QByteArray ps1 = "BOOT = cdrom:\\SCUS_944.76;1\r\nTCB = 4\r\n";
    QCOMPARE(read(write(QStringLiteral("b.iso"), isoWithFile("SYSTEM.CNF;1", ps1)), QStringLiteral("ps1")).serial,
             QStringLiteral("SCUS-94476"));
    // A raw .bin has 16 bytes of header in front of each 2048 of data, and 2352 per sector.
    QCOMPARE(read(write(QStringLiteral("c.bin"), isoWithFile("SYSTEM.CNF;1", ps1, 2352, 16)), QStringLiteral("ps1")).serial,
             QStringLiteral("SCUS-94476"));
    QCOMPARE(read(write(QStringLiteral("d.bin"), isoWithFile("SYSTEM.CNF;1", ps1, 2352, 24)), QStringLiteral("ps1")).serial,
             QStringLiteral("SCUS-94476"));
    // No SYSTEM.CNF, or one without a BOOT line.
    QVERIFY(read(write(QStringLiteral("e.iso"), isoWithFile("README.TXT;1", ps1)), QStringLiteral("ps1")).serial.isEmpty());
    QVERIFY(read(write(QStringLiteral("f.iso"), isoWithFile("SYSTEM.CNF;1", "VMODE = PAL\r\n")), QStringLiteral("ps2")).serial.isEmpty());
}

void RomInfoTest::gameCubeAndWiiDiscs()
{
    const RomInfo cube = read(write(QStringLiteral("a.iso"), discHeader("GZLE01", "THE LEGEND OF ZELDA", false)),
                              QStringLiteral("gamecube"));
    QCOMPARE(cube.serial, QStringLiteral("GZLE01"));
    QCOMPARE(cube.fallbackTitle, QStringLiteral("THE LEGEND OF ZELDA"));
    QVERIFY(cube.title.isEmpty());

    const QByteArray wii = discHeader("RMGE01", "Mario Kart Wii", true);
    QCOMPARE(read(write(QStringLiteral("b.iso"), wii), QStringLiteral("wii")).serial, QStringLiteral("RMGE01"));

    // RVZ and WIA keep a copy of the header after their own; WBFS in its first disc sector.
    QByteArray rvz(0x58, '\0');
    putText(rvz, 0, QByteArray("RVZ\x01", 4));
    QCOMPARE(read(write(QStringLiteral("c.rvz"), rvz + wii), QStringLiteral("wii")).serial, QStringLiteral("RMGE01"));
    QByteArray wbfs(0x200, '\0');
    putText(wbfs, 0, "WBFS");
    wbfs[8] = 9;
    QCOMPARE(read(write(QStringLiteral("d.wbfs"), wbfs + wii.left(0x100)), QStringLiteral("wii")).serial, QStringLiteral("RMGE01"));

    // Not a disc: no magic, or a bad ID.
    QVERIFY(read(write(QStringLiteral("e.iso"), QByteArray(0x200, 'x')), QStringLiteral("wii")).serial.isEmpty());
    QVERIFY(read(write(QStringLiteral("f.iso"), discHeader("gz!e01", "x", false)), QStringLiteral("gamecube")).serial.isEmpty());
}

void RomInfoTest::switchTitleIdFromTicket()
{
    const QByteArray ticket = "0100152000022000" "0000000000000001.tik";
    QCOMPARE(read(write(QStringLiteral("a.nsp"), nsp({"abc.cnmt.nca", ticket})), QStringLiteral("switch")).titleId,
             QStringLiteral("0100152000022000"));
    QVERIFY(read(write(QStringLiteral("b.nsp"), nsp({"abc.cnmt.nca"})), QStringLiteral("switch")).titleId.isEmpty());
    // A hostile header: a huge file count, a string offset outside the table.
    QByteArray bad = nsp({ticket});
    put(bad, 4, 0xFFFFFFFF, 4);
    QVERIFY(read(write(QStringLiteral("c.nsp"), bad), QStringLiteral("switch")).titleId.isEmpty());
    QByteArray outside = nsp({ticket});
    put(outside, 16 + 16, 0xFFFFFF, 4);
    QVERIFY(read(write(QStringLiteral("d.nsp"), outside), QStringLiteral("switch")).titleId.isEmpty());
}

namespace {
QString crcOf(const QByteArray &data)
{
    quint32 crc = 0xFFFFFFFFu;
    for (const char byte : data) {
        crc ^= quint8(byte);
        for (int k = 0; k < 8; ++k)
            crc = (crc & 1) ? 0xEDB88320u ^ (crc >> 1) : crc >> 1;
    }
    return QStringLiteral("%1").arg(~crc, 8, 16, QLatin1Char('0')).toUpper();
}
}

void RomInfoTest::cartridgeChecksums()
{
    QByteArray gba(1000, '\0');
    for (int i = 0; i < gba.size(); ++i)
        gba[i] = char(i * 7);
    QCOMPARE(read(write(QStringLiteral("a.gba"), gba), QStringLiteral("gba")).crc32, crcOf(gba));
    QCOMPARE(crcOf("123456789"), QStringLiteral("CBF43926"));

    // All three byte orders of a Nintendo 64 ROM give the big-endian one's checksum.
    QByteArray z64 = QByteArray("\x80\x37\x12\x40", 4);
    for (int i = 0; i < 100000; ++i)
        z64.append(char(i * 13 + 5));
    z64.resize(z64.size() & ~3);
    QByteArray v64 = z64;
    for (int i = 0; i + 1 < v64.size(); i += 2)
        std::swap(v64[i], v64[i + 1]);
    QByteArray n64 = z64;
    for (int i = 0; i + 3 < n64.size(); i += 4) {
        std::swap(n64[i], n64[i + 3]);
        std::swap(n64[i + 1], n64[i + 2]);
    }
    const QString expected = crcOf(z64);
    QCOMPARE(read(write(QStringLiteral("a.z64"), z64), QStringLiteral("n64")).crc32, expected);
    QCOMPARE(read(write(QStringLiteral("a.v64"), v64), QStringLiteral("n64")).crc32, expected);
    QCOMPARE(read(write(QStringLiteral("a.n64"), n64), QStringLiteral("n64")).crc32, expected);
}

QTEST_APPLESS_MAIN(RomInfoTest)
#include "rominfo_test.moc"
