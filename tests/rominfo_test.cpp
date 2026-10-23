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
};

void RomInfoTest::reads()
{
    QVERIFY(RomInfoReader::reads(QStringLiteral("3ds"), QStringLiteral("3DS")));
    QVERIFY(RomInfoReader::reads(QStringLiteral("nds"), QStringLiteral("nds")));
    QVERIFY(RomInfoReader::reads(QStringLiteral("psp"), QStringLiteral("iso")));
    QVERIFY(!RomInfoReader::reads(QStringLiteral("ps2"), QStringLiteral("iso")));
    QVERIFY(!RomInfoReader::reads(QStringLiteral("gba"), QStringLiteral("gba")));
    QVERIFY(!RomInfoReader::reads(QStringLiteral("switch"), QStringLiteral("nsp")));
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

QTEST_APPLESS_MAIN(RomInfoTest)
#include "rominfo_test.moc"
