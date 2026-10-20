#include "protonsaves.h"
#include "libraryfiles.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>

using ProtonSaves::Entry;

namespace {

const QString kCompatdata = QStringLiteral(STEAM_DATA_DIR "/steamapps/compatdata");
const quint32 kCaveStory = 2900000001u; // the fake Steam's prefix

Game protonGame(const QString &id, quint32 shortcut)
{
    Game game;
    game.id = id;
    game.platform = kProtonPlatform;
    game.shortcut = shortcut;
    return game;
}

void writeFile(const QString &path, const QByteArray &data = {})
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write(data);
}

} // namespace

class ProtonSavesTest : public QObject
{
    Q_OBJECT

private slots:
    void fakePrefix();
    void classify_data();
    void classify();
    void carryOver();
    void skipReasons();
    void onlyProtonGames();
    void archiveHoldsTheSaves();
};

void ProtonSavesTest::fakePrefix()
{
    const QList<ProtonSaves::GameSaves> found =
        ProtonSaves::find({protonGame(QStringLiteral("proton-cave-story"), kCaveStory)}, kCompatdata);
    QCOMPARE(found.size(), 1);
    const ProtonSaves::GameSaves &saves = found.first();
    QCOMPARE(saves.gameId, QStringLiteral("proton-cave-story"));
    QCOMPARE(saves.shortcut, kCaveStory);
    QCOMPARE(saves.prefix, kCompatdata + QStringLiteral("/2900000001"));
    QCOMPARE(saves.skipped, ProtonSaves::Skipped::No);

    QStringList paths;
    qint64 bytes = 0;
    for (const ProtonSaves::SaveFile &file : saves.files) {
        paths.append(file.path);
        bytes += file.size;
    }
    const QStringList expected = {
        QStringLiteral("config_info"),
        QStringLiteral("pfx/drive_c/users/steamuser/AppData/Roaming/CaveStory/Profile.dat"),
        QStringLiteral("pfx/drive_c/users/steamuser/Documents/CaveStory/Config.dat"),
        QStringLiteral("pfx/drive_c/users/steamuser/Local Settings/Application Data"), // a link inside
        QStringLiteral("pfx/drive_c/users/steamuser/My Documents"),                    // likewise
        QStringLiteral("pfx/drive_c/windows/Fonts/extra.ttf"), // not Proton's, though in its folder
        QStringLiteral("pfx/system.reg"),
        QStringLiteral("pfx/user.reg"),
        QStringLiteral("pfx/userdef.reg"),
        QStringLiteral("version"),
    };
    QCOMPARE(paths, expected);
    QCOMPARE(saves.bytes, bytes);
    QCOMPARE(saves.files.at(1).size, qint64(15)); // "Quote, level 12"
    QCOMPARE(saves.files.at(3).size, qint64(0));  // links count as nothing
}

void ProtonSavesTest::classify_data()
{
    QTest::addColumn<QString>("relative");
    QTest::addColumn<bool>("folder");
    QTest::addColumn<QString>("link");
    QTest::addColumn<Entry>("expected");

    const QString none;
    QTest::newRow("the prefix") << "pfx" << true << none << Entry::Save;
    QTest::newRow("version") << "version" << false << none << Entry::Save;
    QTest::newRow("config_info") << "config_info" << false << none << Entry::Save;
    QTest::newRow("tracked_files") << "tracked_files" << false << none << Entry::Proton;
    QTest::newRow("upscaler_files") << "upscaler_files" << false << none << Entry::Proton;
    QTest::newRow("other bookkeeping") << "gstreamer-1.0" << true << none << Entry::Proton;
    QTest::newRow("lock") << "pfx.lock" << false << none << Entry::Drop;
    QTest::newRow("tracked file") << "pfx/drive_c/windows/system32/kernel32.dll" << false << none << Entry::Proton;
    QTest::newRow("tracked folder") << "pfx/drive_c/users/steamuser/AppData/Roaming" << true << none << Entry::Save;
    QTest::newRow("upscaler file") << "pfx/drive_c/windows/system32/amdxcffx64.dll" << false << none << Entry::Proton;
    QTest::newRow("untracked file") << "pfx/drive_c/windows/Fonts/extra.ttf" << false << none << Entry::Save;
    QTest::newRow("registry, tracked") << "pfx/user.reg" << false << none << Entry::Save;
    QTest::newRow("registry copy") << "pfx/user.reg.old" << false << none << Entry::Drop;
    QTest::newRow("reg deeper") << "pfx/drive_c/x.reg.old" << false << none << Entry::Save;
    QTest::newRow("drive links") << "pfx/dosdevices" << true << none << Entry::Proton;
    QTest::newRow("drive link") << "pfx/dosdevices/z:" << false << "/" << Entry::Proton;
    QTest::newRow("guard") << "pfx/creation_sync_guard" << false << none << Entry::Proton;
    QTest::newRow("temp") << "pfx/drive_c/users/steamuser/AppData/Local/Temp" << true << none << Entry::Drop;
    QTest::newRow("in temp") << "pfx/drive_c/users/steamuser/AppData/Local/Temp/a.log" << false << none << Entry::Drop;
    QTest::newRow("temp-like") << "pfx/drive_c/users/steamuser/AppData/Local/Temporary" << true << none << Entry::Save;
    QTest::newRow("absolute link") << "pfx/drive_c/windows/Fonts/arial.ttf" << false
                                   << "/home/u/.steam/root/compatibilitytools.d/GE/files/share/fonts/arial.ttf"
                                   << Entry::Proton;
    QTest::newRow("link out") << "pfx/drive_c/a" << false << "../../../x" << Entry::Proton;
    QTest::newRow("link inside") << "pfx/drive_c/users/steamuser/My Documents" << false << "./Documents" << Entry::Save;
    QTest::newRow("link up, inside") << "pfx/drive_c/users/steamuser/Local Settings/Application Data" << false
                                     << "../AppData/Local" << Entry::Save;
}

void ProtonSavesTest::classify()
{
    QFETCH(QString, relative);
    QFETCH(bool, folder);
    QFETCH(QString, link);
    QFETCH(Entry, expected);

    const std::optional<ProtonSaves::Prefix> prefix =
        ProtonSaves::Prefix::read(kCompatdata + QStringLiteral("/2900000001"));
    QVERIFY(prefix);
    QCOMPARE(ProtonSaves::classify(*prefix, relative, folder, link), expected);
}

void ProtonSavesTest::carryOver()
{
    const std::optional<ProtonSaves::Prefix> prefix =
        ProtonSaves::Prefix::read(kCompatdata + QStringLiteral("/2900000001"));
    QVERIFY(prefix);
    const QStringList expected = {
        QStringLiteral("gstreamer-1.0"),
        QStringLiteral("pfx/.update-timestamp"),
        QStringLiteral("pfx/creation_sync_guard"),
        QStringLiteral("pfx/dosdevices"),
        QStringLiteral("pfx/drive_c/windows/Fonts/arial.ttf"),
        QStringLiteral("pfx/drive_c/windows/system32/amdxcffx64.dll"),
        QStringLiteral("pfx/drive_c/windows/system32/kernel32.dll"),
        QStringLiteral("tracked_files"),
        QStringLiteral("upscaler_files"),
    };
    QCOMPARE(ProtonSaves::carryOver(*prefix), expected);
}

void ProtonSavesTest::skipReasons()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString compatdata = dir.filePath(QStringLiteral("compatdata"));
    // A prefix whose Proton doesn't list its files.
    writeFile(compatdata + QStringLiteral("/300/pfx/user.reg"), "settings");

    Game neverLaunched = protonGame(QStringLiteral("never"), 100);
    Game noShortcut = protonGame(QStringLiteral("no-shortcut"), 0);
    Game notFound = protonGame(QStringLiteral("not-found"), 200);
    notFound.installed = false;
    Game uninstalled = protonGame(QStringLiteral("uninstalled"), 201);
    uninstalled.uninstalled = true;
    const Game unlisted = protonGame(QStringLiteral("unlisted"), 300);

    using ProtonSaves::Skipped;
    const QList<ProtonSaves::GameSaves> found =
        ProtonSaves::find({neverLaunched, noShortcut, notFound, uninstalled, unlisted}, compatdata);
    QCOMPARE(found.size(), 5);
    QCOMPARE(found.at(0).skipped, Skipped::NeverLaunched);
    QCOMPARE(found.at(1).skipped, Skipped::NeverLaunched);
    QCOMPARE(found.at(2).skipped, Skipped::Uninstalled);
    QCOMPARE(found.at(3).skipped, Skipped::Uninstalled);
    QCOMPARE(found.at(4).skipped, Skipped::Unlisted);
    QCOMPARE(found.at(4).prefix, compatdata + QStringLiteral("/300"));
    for (const ProtonSaves::GameSaves &saves : found) {
        QVERIFY(saves.files.isEmpty());
        QCOMPARE(saves.prefix.isEmpty(), saves.skipped != Skipped::Unlisted);
    }

    // No Steam at all.
    QCOMPARE(ProtonSaves::find({neverLaunched}, QString()).first().skipped, Skipped::NeverLaunched);
    QVERIFY(!ProtonSaves::Prefix::read(compatdata + QStringLiteral("/100")));
    QVERIFY(!ProtonSaves::Prefix::read(QString()));
}

void ProtonSavesTest::onlyProtonGames()
{
    Game steam;
    steam.id = QStringLiteral("steam-1145360");
    steam.platform = QStringLiteral("steam");
    steam.shortcut = kCaveStory; // never set on a Steam game, but it's the platform that counts
    Game rom;
    rom.id = QStringLiteral("gba-metroid-fusion");
    rom.platform = QStringLiteral("gba");
    QVERIFY(ProtonSaves::find({steam, rom}, kCompatdata).isEmpty());
}

void ProtonSavesTest::archiveHoldsTheSaves()
{
    const std::optional<ProtonSaves::Prefix> prefix =
        ProtonSaves::Prefix::read(kCompatdata + QStringLiteral("/2900000001"));
    QVERIFY(prefix);
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString archive = dir.filePath(QStringLiteral("saves.tar.zst"));
    const QString part = QStringLiteral("proton/proton-cave-story");

    Archive::Writer writer;
    QVERIFY(writer.open(archive));
    QVERIFY2(writer.addTree(prefix->path, part, ProtonSaves::skip(*prefix)), qPrintable(writer.error()));
    QVERIFY(writer.finish());

    // Its links stay inside the part, so it extracts; and it holds just the saves.
    QString error;
    const QString out = dir.filePath(QStringLiteral("out"));
    QVERIFY2(Archive::extract(archive, {{part, out}}, {}, &error), qPrintable(error));
    QStringList extracted;
    QDirIterator it(out, QDir::AllEntries | QDir::NoDotAndDotDot | QDir::System | QDir::Hidden,
                    QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const QFileInfo info = it.nextFileInfo();
        if (info.isSymLink() || info.isFile())
            extracted.append(info.filePath().mid(out.size() + 1));
    }
    extracted.sort();

    QStringList expected;
    const QList<ProtonSaves::GameSaves> found =
        ProtonSaves::find({protonGame(QStringLiteral("proton-cave-story"), kCaveStory)}, kCompatdata);
    for (const ProtonSaves::SaveFile &file : found.first().files)
        expected.append(file.path);
    QCOMPARE(extracted, expected);
    QVERIFY(QFileInfo(out + QStringLiteral("/pfx/drive_c/users/steamuser/My Documents")).isSymLink());
}

QTEST_GUILESS_MAIN(ProtonSavesTest)
#include "protonsaves_test.moc"
