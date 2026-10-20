#include "binaryvdfbuilder.h"
#include "steamart.h"
#include "steamlocate.h"
#include "steamscan.h"
#include "steamsync.h"

#include <QColor>
#include <QDateTime>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QProcess>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>

using namespace Qt::StringLiterals;

class SteamTest : public QObject
{
    Q_OBJECT

private slots:
    void accountIds();
    void findNative();
    void findFlatpak();
    void nativeBeforeFlatpak();
    void findConfiguredPath();
    void configuredPathMissing();
    void noSteam();
    void configuredUser();
    void userByTimestamp();
    void userWithoutLoginUsers();
    void notRunning();
    void stalePid();
    void runningAndExit();
    void runningFlatpak();
    void scanFakeTree();
    void scanOff();
    void scanNoSteam();
    void notInstalled();
    void updateStates();
    void twoLibraryFolders();
    void blocklist();
    void valveToolNames();
    void borrowedGame();
    void otherUser();
    void rescanReadsOnlyChanges();
    void malformedFiles();
    void scanShortcuts();
    void rescanShortcuts();
    void missingShortcuts();
    void brokenShortcuts();
    void artFromLibraryCache();
    void gridArtWins();
    void newestCachedArt();
    void gamePid();
    void syncTable();
    void syncTable_data();
    void syncBothValues();
    void syncUnknownValues();
    void syncRecordFile();
    void defaultPresets();
    void applyEdits();
    void applyRefusedWhileSteamRuns();
    void applyStale();
    void syncShortcuts();
    void syncMissingShortcuts();
    void syncRemovedShortcuts();
    void applyShortcuts();
    void applyFirstShortcut();
    void gridArt();
    void gridArtFollowsTheArt();
    void syncShortcutIcon();
};

namespace {

const QString kFakeSteam = QStringLiteral(STEAM_DATA_DIR);

void copyTree(const QString &from, const QString &to)
{
    QDir().mkpath(to);
    QDirIterator it(from, QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden,
                    QDirIterator::Subdirectories);
    while (it.hasNext()) {
        it.next();
        const QString target = to + u'/' + QDir(from).relativeFilePath(it.filePath());
        if (it.fileInfo().isDir())
            QDir().mkpath(target);
        else
            QFile::copy(it.filePath(), target);
    }
}

// A home with a copy of the fake Steam tree at `relative`, and ~/.steam/steam linking to it
// when `link`.
QString makeHome(const QTemporaryDir &dir, const QString &relative, bool link)
{
    const QString home = dir.path() + u"/home"_s;
    copyTree(kFakeSteam, home + u'/' + relative);
    if (link) {
        QDir().mkpath(home + u"/.steam"_s);
        QFile::link(home + u'/' + relative, home + u"/.steam/steam"_s);
    }
    return home;
}

const QString kNative = u".local/share/Steam"_s;
const QString kFlatpak = u".var/app/com.valvesoftware.Steam/.local/share/Steam"_s;

// A copy of `sleep` named like Steam's binary, in `root`, so /proc/<pid>/exe points there.
QString fakeSteamBinary(const QString &root)
{
    const QString exe = root + u"/ubuntu12_32/steam"_s;
    QDir().mkpath(root + u"/ubuntu12_32"_s);
    QFile::copy(QStandardPaths::findExecutable(u"sleep"_s), exe);
    QFile::setPermissions(exe, QFile::permissions(exe) | QFile::ExeUser);
    return exe;
}

void writePid(const QString &home, qint64 pid)
{
    QDir().mkpath(home + u"/.steam"_s);
    QFile file(home + u"/.steam/steam.pid"_s);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write(QByteArray::number(pid));
}

} // namespace

void SteamTest::accountIds()
{
    QCOMPARE(SteamLocate::accountId(u"76561198000000001"_s), u"39734273"_s);
    QCOMPARE(SteamLocate::accountId(u"76561198112127509"_s), u"151861781"_s);
    QCOMPARE(SteamLocate::accountId(u"12"_s), QString());
    QCOMPARE(SteamLocate::accountId(u"abc"_s), QString());
}

void SteamTest::findNative()
{
    QTemporaryDir dir;
    const QString home = makeHome(dir, kNative, true);
    const SteamInstall install = SteamLocate::find({home});
    QCOMPARE(install.kind, SteamInstall::Kind::Native);
    QCOMPARE(install.root, QFileInfo(home + u'/' + kNative).canonicalFilePath());
    QCOMPARE(install.users.size(), 2);
    QCOMPARE(install.users[0].personaName, u"Player One"_s);
    QCOMPARE(install.users[0].accountName, u"player_one"_s);
    QCOMPARE(install.users[0].steamId64, u"76561198000000001"_s);
    QVERIFY(install.users[0].mostRecent);
    QCOMPARE(install.users[1].personaName, u"Player \"Two\""_s);
    QCOMPARE(install.userId, u"39734273"_s);
    QCOMPARE(install.userDataDir(), install.root + u"/userdata/39734273"_s);
    QVERIFY(!install.pathMissing);
    QVERIFY(!install.userMissing);
}

void SteamTest::findFlatpak()
{
    QTemporaryDir dir;
    const QString home = makeHome(dir, kFlatpak, false);
    const SteamInstall install = SteamLocate::find({home});
    QCOMPARE(install.kind, SteamInstall::Kind::Flatpak);
    QCOMPARE(install.root, QFileInfo(home + u'/' + kFlatpak).canonicalFilePath());
    QCOMPARE(install.userId, u"39734273"_s);

    // The same folder given as [steam] path is still the Flatpak.
    QCOMPARE(SteamLocate::find({home}, u"~/"_s + kFlatpak).kind, SteamInstall::Kind::Flatpak);
}

void SteamTest::nativeBeforeFlatpak()
{
    QTemporaryDir dir;
    makeHome(dir, kFlatpak, false);
    const QString home = makeHome(dir, kNative, true);
    QCOMPARE(SteamLocate::find({home}).kind, SteamInstall::Kind::Native);

    // A dangling ~/.steam/steam (Steam removed) falls through to the Flatpak.
    QDir(home + u'/' + kNative).removeRecursively();
    QCOMPARE(SteamLocate::find({home}).kind, SteamInstall::Kind::Flatpak);
}

void SteamTest::findConfiguredPath()
{
    QTemporaryDir dir;
    makeHome(dir, kNative, true);
    const QString home = makeHome(dir, u"Games/Steam"_s, false);
    const SteamInstall install = SteamLocate::find({home}, u"~/Games/Steam"_s);
    QCOMPARE(install.kind, SteamInstall::Kind::Native);
    QCOMPARE(install.root, QFileInfo(home + u"/Games/Steam"_s).canonicalFilePath());
    QCOMPARE(SteamLocate::find({home}, home + u"/Games/Steam"_s).root, install.root);
}

void SteamTest::configuredPathMissing()
{
    QTemporaryDir dir;
    const QString home = makeHome(dir, kNative, true);
    // Not a fallback to the detected one: the user asked for this folder.
    const SteamInstall install = SteamLocate::find({home}, u"~/Nowhere"_s);
    QVERIFY(!install.found());
    QVERIFY(install.pathMissing);
    QVERIFY(install.root.isEmpty());
}

void SteamTest::noSteam()
{
    QTemporaryDir dir;
    const SteamInstall install = SteamLocate::find({dir.path()});
    QVERIFY(!install.found());
    QVERIFY(!install.pathMissing);
    QVERIFY(install.users.isEmpty());
    QVERIFY(install.userDataDir().isEmpty());
    QCOMPARE(SteamLocate::runningPid(install, {dir.path()}), 0);
}

void SteamTest::configuredUser()
{
    QTemporaryDir dir;
    const QString home = makeHome(dir, kNative, true);
    QCOMPARE(SteamLocate::find({home}, {}, u"39734274"_s).userId, u"39734274"_s);
    QCOMPARE(SteamLocate::find({home}, {}, u"76561198000000002"_s).userId, u"39734274"_s);

    const SteamInstall unknown = SteamLocate::find({home}, {}, u"123"_s);
    QVERIFY(unknown.userMissing);
    QCOMPARE(unknown.userId, u"39734273"_s);
}

void SteamTest::userByTimestamp()
{
    // Steam doesn't always write MostRecent: the latest login wins then.
    QTemporaryDir dir;
    const QString home = makeHome(dir, kNative, true);
    const QString path = home + u'/' + kNative + u"/config/loginusers.vdf"_s;
    QFile file(path);
    QVERIFY(file.open(QIODevice::ReadOnly));
    QByteArray text = file.readAll();
    file.close();
    text.replace("\t\t\"MostRecent\"\t\t\"1\"\n", "");
    text.replace("\t\t\"MostRecent\"\t\t\"0\"\n", "");
    text.replace("\"1780000000\"", "\"1799999999\"");
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
    file.write(text);
    file.close();

    const SteamInstall install = SteamLocate::find({home});
    QVERIFY(!install.users[0].mostRecent);
    QCOMPARE(install.userId, u"39734274"_s);

    // A user without a userdata folder isn't picked.
    QDir(home + u'/' + kNative + u"/userdata/39734274"_s).removeRecursively();
    QCOMPARE(SteamLocate::find({home}).userId, u"39734273"_s);
}

void SteamTest::userWithoutLoginUsers()
{
    QTemporaryDir dir;
    const QString home = makeHome(dir, kNative, true);
    const QString root = home + u'/' + kNative;
    QFile::remove(root + u"/config/loginusers.vdf"_s);
    // Two accounts: no way to tell which.
    QVERIFY(SteamLocate::find({home}).userId.isEmpty());
    QDir(root + u"/userdata/39734274"_s).removeRecursively();
    QDir().mkpath(root + u"/userdata/0"_s);
    const SteamInstall install = SteamLocate::find({home});
    QVERIFY(install.users.isEmpty());
    QCOMPARE(install.userId, u"39734273"_s);
}

void SteamTest::notRunning()
{
    QTemporaryDir dir;
    const QString home = makeHome(dir, kNative, true);
    fakeSteamBinary(home + u'/' + kNative);
    const SteamInstall install = SteamLocate::find({home});
    QCOMPARE(SteamLocate::runningPid(install, {home}), 0);
}

void SteamTest::stalePid()
{
    QTemporaryDir dir;
    const QString home = makeHome(dir, kNative, true);
    fakeSteamBinary(home + u'/' + kNative);
    const SteamInstall install = SteamLocate::find({home});
    // After a crash the pid can belong to another program, or to nothing.
    writePid(home, QCoreApplication::applicationPid());
    QCOMPARE(SteamLocate::runningPid(install, {home}), 0);
    writePid(home, 999999999);
    QCOMPARE(SteamLocate::runningPid(install, {home}), 0);
}

void SteamTest::runningAndExit()
{
    QTemporaryDir dir;
    const QString home = makeHome(dir, kNative, true);
    const QString exe = fakeSteamBinary(home + u'/' + kNative);
    const SteamInstall install = SteamLocate::find({home});

    QProcess steam;
    steam.start(exe, {u"30"_s});
    QVERIFY(steam.waitForStarted());
    writePid(home, steam.processId());
    QCOMPARE(SteamLocate::runningPid(install, {home}), steam.processId());

    SteamExitWatch watch;
    QSignalSpy exited(&watch, &SteamExitWatch::exited);
    QVERIFY(watch.watch(steam.processId()));
    QVERIFY(watch.watching());
    QTest::qWait(50);
    QCOMPARE(exited.size(), 0);
    steam.terminate();
    QVERIFY(exited.wait(5000));
    QVERIFY(!watch.watching());
    steam.waitForFinished();
    QCOMPARE(SteamLocate::runningPid(install, {home}), 0);
}

void SteamTest::runningFlatpak()
{
    QTemporaryDir dir;
    const QString home = makeHome(dir, kFlatpak, false);
    const QString exe = fakeSteamBinary(home + u'/' + kFlatpak);
    const SteamInstall install = SteamLocate::find({home});
    QCOMPARE(SteamLocate::runningPid(install, {home}), 0);

    QProcess steam;
    steam.start(exe, {u"30"_s});
    QVERIFY(steam.waitForStarted());
    // No steam.pid to go by: found among the processes.
    QCOMPARE(SteamLocate::runningPid(install, {home}), steam.processId());
    steam.kill();
    steam.waitForFinished();
}

namespace {

void writeText(const QString &path, const QByteArray &text)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
    file.write(text);
}

QByteArray manifest(qint64 appId, const QString &name, int stateFlags = 4, qint64 size = 1000,
                    const QString &extra = {})
{
    return QStringLiteral("\"AppState\"\n{\n\t\"appid\"\t\t\"%1\"\n\t\"name\"\t\t\"%2\"\n\t\"StateFlags\"\t\t\"%3\"\n"
                          "\t\"installdir\"\t\t\"%2\"\n\t\"SizeOnDisk\"\t\t\"%4\"\n%5}\n")
        .arg(appId)
        .arg(name)
        .arg(stateFlags)
        .arg(size)
        .arg(extra)
        .toUtf8();
}

SteamState scanHome(const QString &home, const SteamState &previous = {}, const SteamSettings &settings = {})
{
    return SteamScan::scan(settings, {home, {}}, previous);
}

QList<qint64> appIds(const QList<SteamApp> &apps)
{
    QList<qint64> out;
    for (const SteamApp &a : apps)
        out.append(a.appId);
    return out;
}

} // namespace

void SteamTest::scanFakeTree()
{
    QTemporaryDir dir;
    const QString home = makeHome(dir, kNative, true);
    const SteamState s = scanHome(home);
    const QString root = QFileInfo(home + u'/' + kNative).canonicalFilePath();
    QVERIFY(s.enabled);
    QCOMPARE(s.install.root, root);
    QVERIFY2(s.warnings.isEmpty(), qPrintable(s.warnings.join(u'\n')));
    QCOMPARE(s.libraries, QStringList{root + u"/steamapps"_s});
    QCOMPARE(s.manifests.size(), 4);

    // No Proton, no runtime.
    const QList<SteamApp> games = s.games();
    QCOMPARE(appIds(games), (QList<qint64>{8000, 2231450}));
    const SteamApp &pizza = games.at(1);
    QCOMPARE(pizza.name, u"Pizza Tower"_s);
    QVERIFY(pizza.installed);
    QCOMPARE(pizza.sizeBytes, 404028818);
    QCOMPARE(pizza.installDir, root + u"/steamapps/common/Pizza Tower"_s);
    QVERIFY(QFileInfo(pizza.installDir).isDir());
    QCOMPARE(games.at(0).name, u"Tomb Raider: Anniversary"_s);

    // localconfig.vdf of the most recent user.
    QCOMPARE(s.localConfigPath, root + u"/userdata/39734273/config/localconfig.vdf"_s);
    QCOMPARE(s.apps.value(2231450).playMinutes, 4085);
    QCOMPARE(s.apps.value(2231450).lastPlayed, 1788735088);
    QVERIFY(s.apps.value(2231450).launchOptions.isEmpty());
    QCOMPARE(s.apps.value(8000).playMinutes, 70);
    QCOMPARE(s.apps.value(8000).launchOptions, u"PROTON_LOG=1 %command% -window \"C:\\Games\\save dir\""_s);
    QVERIFY(!s.apps.contains(7)); // only a cloud block

    QCOMPARE(s.compatTools.value(0), u"proton_experimental"_s);
    QCOMPARE(s.compatTools.value(8000), u"GE-Proton10-4"_s);
    QCOMPARE(s.tools(), (QList<CompatTool>{{u"proton_experimental"_s, u"Proton Experimental"_s},
                                           {u"GE-Proton10-4"_s, u"GE-Proton10-4"_s}}));

    const QStringList watched = s.watchPaths();
    QVERIFY(watched.contains(root + u"/steamapps/libraryfolders.vdf"_s));
    QVERIFY(watched.contains(root + u"/steamapps"_s));
    QVERIFY(watched.contains(s.localConfigPath));
    QVERIFY(watched.contains(root + u"/config/config.vdf"_s));
    QVERIFY(watched.contains(root + u"/compatibilitytools.d"_s));
}

void SteamTest::scanOff()
{
    QTemporaryDir dir;
    const QString home = makeHome(dir, kNative, true);
    SteamSettings off;
    off.enabled = false;
    const SteamState s = scanHome(home, {}, off);
    QVERIFY(!s.enabled);
    QVERIFY(!s.install.found());
    QVERIFY(s.games().isEmpty());
    QVERIFY(s.watchPaths().isEmpty());
}

void SteamTest::scanNoSteam()
{
    QTemporaryDir dir;
    const SteamState s = scanHome(dir.path());
    QVERIFY(s.enabled);
    QVERIFY(!s.install.found());
    QVERIFY(s.games().isEmpty());
    QVERIFY(s.warnings.isEmpty());

    SteamSettings elsewhere;
    elsewhere.path = u"~/Nowhere"_s;
    QCOMPARE(scanHome(dir.path(), {}, elsewhere).warnings,
             QStringList{u"settings.toml: [steam] path: no Steam at ~/Nowhere"_s});
}

void SteamTest::notInstalled()
{
    QTemporaryDir dir;
    const QString home = makeHome(dir, kNative, true);
    // Downloading for the first time: a manifest, but not fully installed yet.
    writeText(home + u'/' + kNative + u"/steamapps/appmanifest_620.acf"_s, manifest(620, u"Portal 2"_s, 1026, 0));
    const QList<SteamApp> games = scanHome(home).games();
    QCOMPARE(appIds(games), (QList<qint64>{620, 8000, 2231450}));
    QVERIFY(!games.at(0).installed);
    QVERIFY(games.at(1).installed);
    // An update pending on an installed game (4 + 2): still installed.
    writeText(home + u'/' + kNative + u"/steamapps/appmanifest_620.acf"_s, manifest(620, u"Portal 2"_s, 6, 0));
    QVERIFY(scanHome(home).games().at(0).installed);
}

void SteamTest::updateStates()
{
    QTemporaryDir dir;
    const QString home = makeHome(dir, kNative, true);
    const QString file = home + u'/' + kNative + u"/steamapps/appmanifest_620.acf"_s;
    const auto read = [&](int flags, const QString &extra) {
        writeText(file, manifest(620, u"Portal 2"_s, flags, 1000, extra));
        return scanHome(home).games().at(0);
    };
    const QString bytes = u"\t\"BytesToDownload\"\t\t\"1000\"\n\t\"BytesDownloaded\"\t\t\"450\"\n"_s;
    const QString built = u"\t\"buildid\"\t\t\"10\"\n"_s;

    SteamApp app = read(6, built + bytes);
    QCOMPARE(app.update, SteamUpdate::Waiting);
    QVERIFY(app.installed);
    QCOMPARE(app.updateBytes, 1000);
    QCOMPARE(app.updatePercent, 45); // no staging sizes: the download bytes
    // Steam's percentage follows the staging bytes, as a real manifest had it: 3% downloaded,
    // 10% staged, and Steam showed 10.
    app = read(1286, built + u"\t\"BytesToDownload\"\t\t\"20969404624\"\n\t\"BytesDownloaded\"\t\t\"690973408\"\n"
                              "\t\"BytesToStage\"\t\t\"29292040346\"\n\t\"BytesStaged\"\t\t\"2953297829\"\n"_s);
    QCOMPARE(app.update, SteamUpdate::Downloading);
    QCOMPARE(app.updatePercent, 10);
    // The same download suspended in Steam: "started" without "running".
    QCOMPARE(read(1030, built).update, SteamUpdate::Paused);
    // Waiting by build alone, with the installed bit set.
    QCOMPARE(read(4, built + u"\t\"TargetBuildID\"\t\t\"11\"\n"_s).update, SteamUpdate::Waiting);
    QCOMPARE(read(4, built + u"\t\"TargetBuildID\"\t\t\"10\"\n"_s).update, SteamUpdate::None);
    QCOMPARE(read(4, built + u"\t\"TargetBuildID\"\t\t\"0\"\n"_s).update, SteamUpdate::None);

    // Updating drops the installed bit; the build on disk keeps the game installed.
    app = read(1286, built + bytes);
    QCOMPARE(app.update, SteamUpdate::Downloading);
    QVERIFY(app.installed);
    QCOMPARE(read(258, built).update, SteamUpdate::Downloading);
    app = read(1538, built + bytes);
    QCOMPARE(app.update, SteamUpdate::Paused);
    QVERIFY(app.installed);

    // A first install has no build on disk yet.
    app = read(1282, u"\t\"buildid\"\t\t\"0\"\n"_s + bytes);
    QCOMPARE(app.update, SteamUpdate::Downloading);
    QVERIFY(!app.installed);

    // Steam keeps the last update's numbers after it finishes.
    app = read(4, built + bytes + u"\t\"ScheduledAutoUpdate\"\t\t\"1790000000\"\n"_s);
    QCOMPARE(app.update, SteamUpdate::None);
    QCOMPARE(app.updateBytes, 0);
    QCOMPARE(app.updatePercent, -1);
    QVERIFY(app.installed);

    // A manifest rewritten from Downloading to None is read on the next scan.
    writeText(file, manifest(620, u"Portal 2"_s, 1286, 1000, built + bytes));
    const SteamState downloading = scanHome(home);
    QCOMPARE(downloading.games().at(0).update, SteamUpdate::Downloading);
    writeText(file, manifest(620, u"Portal 2"_s, 4, 1001, built));
    QCOMPARE(scanHome(home, downloading).games().at(0).update, SteamUpdate::None);
}

void SteamTest::twoLibraryFolders()
{
    QTemporaryDir dir;
    const QString home = makeHome(dir, kNative, true);
    const QString second = dir.path() + u"/Games/SteamLibrary"_s;
    writeText(second + u"/steamapps/appmanifest_620.acf"_s, manifest(620, u"Portal 2"_s));
    const QString root = home + u'/' + kNative;
    QFile file(root + u"/steamapps/libraryfolders.vdf"_s);
    QVERIFY(file.open(QIODevice::ReadOnly));
    QByteArray text = file.readAll();
    file.close();
    const QByteArray extra = "\t\"1\"\n\t{\n\t\t\"path\"\t\t\"" + second.toUtf8() + "\"\n\t}\n"
                             "\t\"2\"\n\t{\n\t\t\"path\"\t\t\"/nonexistent/unmounted drive\"\n\t}\n";
    text.insert(text.lastIndexOf('}'), extra);
    writeText(root + u"/steamapps/libraryfolders.vdf"_s, text);

    const SteamState s = scanHome(home);
    QVERIFY2(s.warnings.isEmpty(), qPrintable(s.warnings.join(u'\n')));
    QCOMPARE(s.libraries.size(), 2);
    QCOMPARE(s.libraries.at(1), QFileInfo(second + u"/steamapps"_s).canonicalFilePath());
    const QList<SteamApp> games = s.games();
    QCOMPARE(appIds(games), (QList<qint64>{620, 8000, 2231450}));
    QCOMPARE(games.at(0).installDir, s.libraries.at(1) + u"/common/Portal 2"_s);
    QVERIFY(s.watchPaths().contains(s.libraries.at(1)));
}

void SteamTest::blocklist()
{
    QVERIFY(SteamScan::isBlocked(228980, u"Steamworks Common Redistributables"_s));
    QVERIFY(SteamScan::isBlocked(1493710, u"Proton Experimental"_s));
    QVERIFY(SteamScan::isBlocked(2805730, u"Proton 9.0"_s));
    QVERIFY(SteamScan::isBlocked(1826330, u"Proton EasyAntiCheat Runtime"_s));
    QVERIFY(SteamScan::isBlocked(4183110, u"Steam Linux Runtime 4.0"_s));
    QVERIFY(SteamScan::isBlocked(1391110, QString())); // known by its id
    QVERIFY(SteamScan::isBlocked(99999, u"Steam Linux Runtime 9.0 (future)"_s));
    QVERIFY(SteamScan::isBlocked(4690330, u"Legacy Steam Runtime"_s));
    QVERIFY(SteamScan::isBlocked(99998, u"Newer Steam Runtime"_s)); // by its name
    QVERIFY(!SteamScan::isBlocked(2231450, u"Pizza Tower"_s));
    QVERIFY(!SteamScan::isBlocked(1, u"Protonaut"_s));

    QTemporaryDir dir;
    const QString home = makeHome(dir, kNative, true);
    const QString apps = home + u'/' + kNative + u"/steamapps/"_s;
    writeText(apps + u"appmanifest_228980.acf"_s, manifest(228980, u"Steamworks Common Redistributables"_s));
    writeText(apps + u"appmanifest_1826330.acf"_s, manifest(1826330, u"Proton EasyAntiCheat Runtime"_s));
    writeText(apps + u"appmanifest_2805730.acf"_s, manifest(2805730, u"Proton 9.0"_s));
    writeText(apps + u"appmanifest_2348590.acf"_s, manifest(2348590, u"Proton 8.0"_s, 1026));
    const SteamState s = scanHome(home);
    QCOMPARE(appIds(s.games()), (QList<qint64>{8000, 2231450}));
    // The Protons are tools, newest first; a runtime isn't, nor one still downloading.
    QCOMPARE(s.tools(), (QList<CompatTool>{{u"proton_experimental"_s, u"Proton Experimental"_s},
                                           {u"proton_9"_s, u"Proton 9.0"_s},
                                           {u"GE-Proton10-4"_s, u"GE-Proton10-4"_s}}));
}

void SteamTest::valveToolNames()
{
    QCOMPARE(SteamScan::valveToolName(1493710, u"Proton Experimental"_s), u"proton_experimental"_s);
    QCOMPARE(SteamScan::valveToolName(2180100, u"Proton Hotfix"_s), u"proton_hotfix"_s);
    QCOMPARE(SteamScan::valveToolName(2805730, u"Proton 9.0"_s), u"proton_9"_s);
    QCOMPARE(SteamScan::valveToolName(3658110, u"Proton 10.0"_s), u"proton_10"_s);
    QCOMPARE(SteamScan::valveToolName(1580130, u"Proton 6.3"_s), u"proton_63"_s);
    QCOMPARE(SteamScan::valveToolName(1420170, u"Proton 5.13"_s), u"proton_513"_s);
    QCOMPARE(SteamScan::valveToolName(1826330, u"Proton EasyAntiCheat Runtime"_s), QString());
    QCOMPARE(SteamScan::valveToolName(1161040, u"Proton BattlEye Runtime"_s), QString());
    QCOMPARE(SteamScan::valveToolName(2231450, u"Pizza Tower"_s), QString());
}

void SteamTest::borrowedGame()
{
    // Never played on this account: a manifest, but nothing in localconfig.vdf; or LastPlayed without Playtime.
    QTemporaryDir dir;
    const QString home = makeHome(dir, kNative, true);
    const QString root = home + u'/' + kNative;
    writeText(root + u"/steamapps/appmanifest_620.acf"_s, manifest(620, u"Portal 2"_s));
    writeText(root + u"/steamapps/appmanifest_400.acf"_s, manifest(400, u"Portal"_s));
    const QString config = root + u"/userdata/39734273/config/localconfig.vdf"_s;
    QFile file(config);
    QVERIFY(file.open(QIODevice::ReadOnly));
    QByteArray text = file.readAll();
    file.close();
    text.replace("\t\t\t\t\t\"8000\"\n", "\t\t\t\t\t\"400\"\n\t\t\t\t\t{\n\t\t\t\t\t\t\"LastPlayed\"\t\t\"1700000000\"\n"
                                          "\t\t\t\t\t}\n\t\t\t\t\t\"8000\"\n");
    writeText(config, text);

    const SteamState s = scanHome(home);
    QVERIFY2(s.warnings.isEmpty(), qPrintable(s.warnings.join(u'\n')));
    QVERIFY(!s.apps.contains(620));
    QCOMPARE(s.apps.value(620).playMinutes, -1);
    QCOMPARE(s.apps.value(400).playMinutes, -1);
    QCOMPARE(s.apps.value(400).lastPlayed, 1700000000);
}

void SteamTest::otherUser()
{
    QTemporaryDir dir;
    const QString home = makeHome(dir, kNative, true);
    SteamSettings settings;
    settings.user = u"39734274"_s;
    const SteamState first = scanHome(home);
    const SteamState s = scanHome(home, first, settings);
    QCOMPARE(s.install.userId, u"39734274"_s);
    QCOMPARE(s.apps.value(2231450).playMinutes, 12);
    QVERIFY(!s.apps.contains(8000));
}

void SteamTest::rescanReadsOnlyChanges()
{
    QTemporaryDir dir;
    const QString home = makeHome(dir, kNative, true);
    const QString root = home + u'/' + kNative;
    const SteamState first = scanHome(home);
    QCOMPARE(scanHome(home, first), first);

    // A file whose stamp didn't change isn't read again: garbage with the same size and
    // time still gives the old values.
    const QString pizza = root + u"/steamapps/appmanifest_2231450.acf"_s;
    const QDateTime modified = QFileInfo(pizza).lastModified();
    const qint64 size = QFileInfo(pizza).size();
    writeText(pizza, QByteArray(size, '{'));
    {
        QFile f(pizza);
        QVERIFY(f.open(QIODevice::ReadWrite));
        QVERIFY(f.setFileTime(modified, QFileDevice::FileModificationTime));
    }
    QCOMPARE(scanHome(home, first), first);

    // A changed one is.
    writeText(pizza, manifest(2231450, u"Pizza Tower"_s, 4, 5));
    const SteamState second = scanHome(home, first);
    QCOMPARE(second.games().at(1).sizeBytes, 5);
    QCOMPARE(second.games().at(0), first.games().at(0));
    // A removed manifest (uninstalled) is gone from the games.
    QFile::remove(pizza);
    QCOMPARE(appIds(scanHome(home, second).games()), QList<qint64>{8000});

    // Play time written by Steam when a game closes.
    const QString config = root + u"/userdata/39734273/config/localconfig.vdf"_s;
    QFile file(config);
    QVERIFY(file.open(QIODevice::ReadOnly));
    QByteArray text = file.readAll();
    file.close();
    // A new length: a rewrite of the same size within one timestamp tick looks unchanged.
    text.replace("\"Playtime\"\t\t\"70\"", "\"Playtime\"\t\t\"195\"");
    writeText(config, text);
    QCOMPARE(scanHome(home, second).apps.value(8000).playMinutes, 195);
}

void SteamTest::malformedFiles()
{
    QTemporaryDir dir;
    const QString home = makeHome(dir, kNative, true);
    const QString root = QFileInfo(home + u'/' + kNative).canonicalFilePath();
    const QString bad = root + u"/steamapps/appmanifest_620.acf"_s;
    writeText(bad, "\"AppState\"\n{\n\t\"appid\"\t\t\"620\n");
    writeText(root + u"/config/config.vdf"_s, "}");
    const SteamState s = scanHome(home);
    QCOMPARE(s.warnings, (QStringList{root + u"/config/config.vdf: line 1: a '}' without its '{'"_s,
                                      bad + u": line 3: a string isn't closed"_s}));
    QCOMPARE(appIds(s.games()), (QList<qint64>{8000, 2231450}));
    QVERIFY(s.compatTools.isEmpty());
    // Still reported on the next scan, though the files aren't read again.
    QCOMPARE(scanHome(home, s).warnings, s.warnings);
    // Fixed: gone.
    writeText(bad, manifest(620, u"Portal 2"_s));
    QCOMPARE(scanHome(home, s).warnings.size(), 1);
}

void SteamTest::scanShortcuts()
{
    QTemporaryDir dir;
    const QString home = makeHome(dir, kNative, true);
    const SteamState s = scanHome(home);
    const QString root = QFileInfo(home + u'/' + kNative).canonicalFilePath();
    QVERIFY2(s.warnings.isEmpty(), qPrintable(s.warnings.join(u'\n')));
    QCOMPARE(s.shortcutsPath, root + u"/userdata/39734273/config/shortcuts.vdf"_s);
    QVERIFY(s.shortcutsOk);
    QCOMPARE(s.shortcuts.size(), 3);
    const SteamShortcut &cave = s.shortcuts.at(0);
    QCOMPARE(cave.appId, 2900000001u);
    QCOMPARE(cave.name, u"Cave Story"_s);
    QCOMPARE(cave.exe, u"/home/user/Games/Cave Story/Doukutsu.exe"_s);
    QCOMPARE(s.shortcuts.at(2).exeArgs, u"--fullscreen \"/home/user/Games/switch/Smash.nsp\""_s);

    // Play time under the signed app ID; the unsigned block (BadgeData only) is nothing.
    QCOMPARE(s.shortcutConfig(2900000001u).playMinutes, 575);
    QVERIFY(!s.apps.contains(2900000001));
    QCOMPARE(s.shortcutConfig(3100000002u).playMinutes, -1);
    QCOMPARE(s.compatTools.value(2900000001), u"GE-Proton10-4"_s);
    // The Steam games are as before.
    QCOMPARE(appIds(s.games()), (QList<qint64>{8000, 2231450}));
    QCOMPARE(s.apps.value(2231450).playMinutes, 4085);

    QVERIFY(s.watchPaths().contains(s.shortcutsPath));
    // The other user has no shortcuts.vdf.
    SteamSettings other;
    other.user = u"39734274"_s;
    const SteamState o = scanHome(home, s, other);
    QVERIFY(o.shortcuts.isEmpty());
    QVERIFY(o.shortcutsOk);
    QVERIFY(!o.watchPaths().contains(o.shortcutsPath));
}

void SteamTest::rescanShortcuts()
{
    QTemporaryDir dir;
    const QString home = makeHome(dir, kNative, true);
    const SteamState first = scanHome(home);
    const QString path = first.shortcutsPath;

    // Same stamp: not read again, even with other bytes.
    const QDateTime modified = QFileInfo(path).lastModified();
    const qint64 size = QFileInfo(path).size();
    writeText(path, QByteArray(size, '\x08'));
    {
        QFile f(path);
        QVERIFY(f.open(QIODevice::ReadWrite));
        QVERIFY(f.setFileTime(modified, QFileDevice::FileModificationTime));
    }
    QCOMPARE(scanHome(home, first), first);

    // Changed in Steam: read again.
    BinaryVdf::Document doc = BinaryVdf::parse(fakeTreeShortcuts());
    QVERIFY(SteamShortcuts::remove(&doc.root, 3100000002u));
    writeText(path, BinaryVdf::write(doc.root));
    const SteamState second = scanHome(home, first);
    QCOMPARE(second.shortcuts.size(), 2);
    QCOMPARE(second.shortcuts.at(1).name, u"Super Smash Bros. Ultimate"_s);
    QCOMPARE(second.manifests, first.manifests);
}

void SteamTest::missingShortcuts()
{
    // No shortcut added yet: no file, no warning, nothing else missing.
    QTemporaryDir dir;
    const QString home = makeHome(dir, kNative, true);
    const SteamState first = scanHome(home);
    QVERIFY(QFile::remove(first.shortcutsPath));
    const SteamState s = scanHome(home, first);
    QVERIFY2(s.warnings.isEmpty(), qPrintable(s.warnings.join(u'\n')));
    QVERIFY(s.shortcutsOk);
    QVERIFY(s.shortcuts.isEmpty());
    QCOMPARE(s.shortcutsStamp.size, -1);
    QCOMPARE(appIds(s.games()), (QList<qint64>{8000, 2231450}));
    QCOMPARE(s.shortcutConfig(2900000001u).playMinutes, 575);
    QCOMPARE(scanHome(home, s), s);
}

void SteamTest::brokenShortcuts()
{
    // Cut short while Steam wrote it: a warning, and the rest of the import as usual.
    QTemporaryDir dir;
    const QString home = makeHome(dir, kNative, true);
    const SteamState first = scanHome(home);
    const QString path = first.shortcutsPath;
    writeText(path, fakeTreeShortcuts().left(100));
    const SteamState s = scanHome(home, first);
    QCOMPARE(s.warnings, QStringList{path + u": byte 94: a string isn't ended"_s});
    QVERIFY(!s.shortcutsOk);
    QVERIFY(s.shortcuts.isEmpty());
    QCOMPARE(appIds(s.games()), (QList<qint64>{8000, 2231450}));
    QCOMPARE(s.apps, first.apps);
    QCOMPARE(s.compatTools, first.compatTools);
    // Still reported on the next scan; gone once fixed.
    QCOMPARE(scanHome(home, s).warnings, s.warnings);
    writeText(path, fakeTreeShortcuts());
    const SteamState fixed = scanHome(home, s);
    QVERIFY(fixed.warnings.isEmpty());
    QVERIFY(fixed.shortcutsOk);
    QCOMPARE(fixed.shortcuts, first.shortcuts);
}

void SteamTest::artFromLibraryCache()
{
    QTemporaryDir dir;
    const QString home = makeHome(dir, kNative, true);
    const SteamInstall install = SteamLocate::find({home});
    const QString cache = install.root + u"/appcache/librarycache/2231450/"_s;
    const QStringList all = {u"hero"_s, u"logo"_s, u"icon"_s, u"cover"_s};
    const QHash<QString, QString> art = SteamArt::find(install, 2231450, all);
    QCOMPARE(art.size(), 4);
    // In the folder, or in one of its hashed subfolders.
    QCOMPARE(art.value(u"hero"_s), cache + u"5f1c0e2a9b7d4c3e8a6f1b2d3c4e5f6a7b8c9d0e/library_hero.jpg"_s);
    QCOMPARE(art.value(u"cover"_s), cache + u"library_600x900.jpg"_s);
    QCOMPARE(art.value(u"logo"_s), cache + u"logo.png"_s);
    QCOMPARE(art.value(u"icon"_s), cache + u"8b7e86d9fa3634015a44c6af85618a21758edc34.jpg"_s);

    // Only the kinds asked for.
    QCOMPARE(SteamArt::find(install, 2231450, {u"logo"_s}).keys(), QStringList{u"logo"_s});
    // An app without cached art, and no Steam at all.
    QVERIFY(SteamArt::find(install, 620, all).isEmpty());
    QVERIFY(SteamArt::find(SteamInstall(), 2231450, all).isEmpty());
}

void SteamTest::gridArtWins()
{
    QTemporaryDir dir;
    const QString home = makeHome(dir, kNative, true);
    const SteamInstall install = SteamLocate::find({home});
    const QHash<QString, QString> art =
        SteamArt::find(install, 8000, {u"hero"_s, u"logo"_s, u"icon"_s, u"cover"_s});
    // The user's custom art in Steam wins over the library cache's hero.
    QCOMPARE(art.value(u"hero"_s), install.userDataDir() + u"/config/grid/8000_hero.png"_s);
    // Newer Steam's name for the cover.
    QVERIFY(art.value(u"cover"_s).endsWith(u"/library_capsule.jpg"_s));
    QCOMPARE(art.size(), 2);

    // Another user's grid isn't used.
    QTemporaryDir other;
    const QString otherHome = makeHome(other, kNative, true);
    const SteamInstall two = SteamLocate::find({otherHome}, {}, u"39734274"_s);
    QVERIFY(SteamArt::find(two, 8000, {u"hero"_s}).value(u"hero"_s).endsWith(u"/librarycache/8000/library_hero.jpg"_s));
}

void SteamTest::newestCachedArt()
{
    QTemporaryDir dir;
    const QString home = makeHome(dir, kNative, true);
    const SteamInstall install = SteamLocate::find({home});
    const QString cache = install.root + u"/appcache/librarycache/2231450/"_s;
    const QString old = cache + u"library_hero.jpg"_s;
    const QString newer = cache + u"5f1c0e2a9b7d4c3e8a6f1b2d3c4e5f6a7b8c9d0e/library_hero.jpg"_s;
    QVERIFY(QFile::copy(newer, old));
    const auto age = [](const QString &path, qint64 secs) {
        QFile file(path);
        QVERIFY(file.open(QIODevice::ReadWrite));
        QVERIFY(file.setFileTime(QDateTime::currentDateTime().addSecs(-secs), QFileDevice::FileModificationTime));
    };
    // Steam keeps older versions next to the current one: the newest wins, wherever it is.
    age(old, 100);
    age(newer, 1000);
    QCOMPARE(SteamArt::find(install, 2231450, {u"hero"_s}).value(u"hero"_s), old);
    age(newer, 10);
    QCOMPARE(SteamArt::find(install, 2231450, {u"hero"_s}).value(u"hero"_s), newer);

    // library_600x900 beats library_capsule, however old.
    QVERIFY(QFile::copy(cache + u"library_600x900.jpg"_s, cache + u"library_capsule.jpg"_s));
    age(cache + u"library_600x900.jpg"_s, 5000);
    QVERIFY(SteamArt::find(install, 2231450, {u"cover"_s}).value(u"cover"_s).endsWith(u"/library_600x900.jpg"_s));
}

void SteamTest::gamePid()
{
    // A stand-in for Steam's reaper: `reaper SteamLaunch AppId=4242 -- <the game's command>`.
    QProcess reaper;
    reaper.start(u"/bin/sh"_s, {u"-c"_s, u"sleep 10; true"_s, u"reaper"_s, u"SteamLaunch"_s, u"AppId=4242"_s,
                                 u"--"_s, u"game"_s, u"AppId=777"_s});
    QVERIFY(reaper.waitForStarted());
    QCOMPARE(SteamLocate::gamePid(4242), reaper.processId());
    // After "--" it's the game's own command line, not Steam's.
    QCOMPARE(SteamLocate::gamePid(777), 0);
    QCOMPARE(SteamLocate::gamePid(424), 0);
    QCOMPARE(SteamLocate::gamePid(0), 0);
    reaper.kill();
    reaper.waitForFinished();
    QCOMPARE(SteamLocate::gamePid(4242), 0);

    // A shortcut's 64-bit game ID, past qint64.
    QProcess shortcut;
    shortcut.start(u"/bin/sh"_s, {u"-c"_s, u"sleep 10; true"_s, u"reaper"_s, u"SteamLaunch"_s,
                                   u"AppId=10557610576647815168"_s, u"--"_s, u"game"_s});
    QVERIFY(shortcut.waitForStarted());
    QCOMPARE(SteamLocate::gamePid(10557610576647815168ull), shortcut.processId());
    shortcut.kill();
    shortcut.waitForFinished();
}

void SteamTest::syncTable_data()
{
    QTest::addColumn<QString>("gameFile");
    QTest::addColumn<QString>("steam");
    QTest::addColumn<bool>("hasRecord");
    QTest::addColumn<QString>("record");
    QTest::addColumn<int>("side");

    const int inSync = int(SteamSync::Side::InSync);
    const int queued = int(SteamSync::Side::Queued);
    const int fromSteam = int(SteamSync::Side::FromSteam);
    // Game file = Steam: in sync, whatever the record says.
    QTest::newRow("same") << "a" << "a" << true << "a" << inSync;
    QTest::newRow("same, old record") << "b" << "b" << true << "a" << inSync;
    QTest::newRow("same, no record") << "a" << "a" << false << "" << inSync;
    QTest::newRow("both empty") << "" << "" << false << "" << inSync;
    // Steam = record: changed here.
    QTest::newRow("changed here") << "b" << "a" << true << "a" << queued;
    QTest::newRow("cleared here") << "" << "a" << true << "a" << queued;
    QTest::newRow("set here") << "b" << "" << true << "" << queued;
    // Steam ≠ record: changed in Steam, which also wins when both changed.
    QTest::newRow("changed in Steam") << "a" << "b" << true << "a" << fromSteam;
    QTest::newRow("cleared in Steam") << "a" << "" << true << "a" << fromSteam;
    QTest::newRow("both changed") << "c" << "b" << true << "a" << fromSteam;
    // No record: the game file's value is queued; without one, Steam's is copied.
    QTest::newRow("no record, game file set") << "b" << "a" << false << "" << queued;
    QTest::newRow("no record, Steam empty") << "b" << "" << false << "" << queued;
    QTest::newRow("no record, game file empty") << "" << "a" << false << "" << fromSteam;
}

void SteamTest::syncTable()
{
    QFETCH(QString, gameFile);
    QFETCH(QString, steam);
    QFETCH(bool, hasRecord);
    QFETCH(QString, record);
    QFETCH(int, side);
    QCOMPARE(int(SteamSync::compare(gameFile, steam, hasRecord ? std::optional(record) : std::nullopt)), side);
}

namespace {

// Steam with launch options and Proton versions known.
SteamState syncState()
{
    SteamState s;
    s.localConfigOk = true;
    s.configOk = true;
    s.apps.insert(10, {60, 0, QStringLiteral("mangohud %command%")});
    s.apps.insert(20, {-1, 0, QStringLiteral("-steam")});
    s.compatTools.insert(0, QStringLiteral("proton_experimental")); // the default for all games, not game 30's
    s.compatTools.insert(20, QStringLiteral("proton_9"));
    return s;
}

} // namespace

void SteamTest::syncBothValues()
{
    SteamSync::Record record;
    record.launchOptions.insert(10, QStringLiteral("mangohud %command%"));
    record.proton.insert(10, QString());
    record.launchOptions.insert(20, QStringLiteral("-old"));
    record.proton.insert(20, QStringLiteral("proton_8"));

    const QList<SteamSync::Game> games = {
        // Launch options in sync; Proton set here.
        {QStringLiteral("steam-10"), 10, QStringLiteral("mangohud %command%"), QStringLiteral("GE-Proton10-4")},
        // Both changed in Steam; the launch options here too (Steam wins).
        {QStringLiteral("steam-20"), 20, QStringLiteral("-mine"), QStringLiteral("proton_8")},
        // Nothing anywhere: in sync, recorded empty. Game 0's tool is no game's own.
        {QStringLiteral("steam-30"), 30, {}, {}},
        // Not a Steam app: skipped.
        {QStringLiteral("steam-hades"), 0, QStringLiteral("x"), {}},
    };
    const SteamSync::Result r = SteamSync::sync(games, syncState(), record);

    QCOMPARE(r.queued.keys(), QList<QString>{"steam-10"});
    QCOMPARE(r.queued.value("steam-10"), QStringList{"proton"});
    QCOMPARE(r.copies.keys(), QList<QString>{"steam-20"});
    const QList<std::pair<QString, QVariant>> copies = {{"launch_options", QStringLiteral("-steam")},
                                                        {"proton", QStringLiteral("proton_9")}};
    QCOMPARE(r.copies.value("steam-20"), copies);
    QCOMPARE(r.log.size(), 1);
    QVERIFY(r.log.first().contains("steam-20: launch_options"));
    QVERIFY(r.log.first().contains("\"-steam\" over \"-mine\""));

    // The record follows Steam where the two agree now; a queued value keeps its old record.
    QCOMPARE(r.record.launchOptions.value(10), QStringLiteral("mangohud %command%"));
    QCOMPARE(r.record.proton.value(10), QString());
    QCOMPARE(r.record.launchOptions.value(20), QStringLiteral("-steam"));
    QCOMPARE(r.record.proton.value(20), QStringLiteral("proton_9"));
    QVERIFY(r.record.launchOptions.contains(30));
    QVERIFY(r.record.proton.contains(30));
    QVERIFY(!r.record.launchOptions.contains(0));
}

void SteamTest::syncUnknownValues()
{
    // localconfig.vdf or config.vdf missing or unreadable: that value is left alone, so a
    // broken file can't wipe the game files' copies.
    SteamState steam = syncState();
    steam.localConfigOk = false;
    steam.apps.clear();
    SteamSync::Record record;
    record.launchOptions.insert(10, QStringLiteral("mangohud %command%"));
    const QList<SteamSync::Game> games = {
        {QStringLiteral("steam-10"), 10, QStringLiteral("mangohud %command%"), QStringLiteral("proton_9")},
    };
    SteamSync::Result r = SteamSync::sync(games, steam, record);
    QVERIFY(r.copies.isEmpty());
    QCOMPARE(r.queued.value("steam-10"), QStringList{"proton"}); // no record: the game file's is queued
    QCOMPARE(r.record.launchOptions, record.launchOptions);

    steam.configOk = false;
    r = SteamSync::sync(games, steam, record);
    QVERIFY(r.copies.isEmpty());
    QVERIFY(r.queued.isEmpty());
    QCOMPARE(r.record, record);
}

void SteamTest::syncRecordFile()
{
    QTemporaryDir dir;
    // The state dir doesn't exist yet.
    const QString path = SteamSync::recordPath(dir.filePath("state/lattis"), QStringLiteral("0123456789ab"));
    QCOMPARE(path, dir.filePath("state/lattis/steam-sync-0123456789ab.toml"));
    const QString library = QStringLiteral("lib-1");
    const SteamSync::Record none{library};
    QStringList warnings;
    QCOMPARE(SteamSync::readRecord(path, library, &warnings), none);
    QVERIFY(warnings.isEmpty());

    SteamSync::Record record;
    record.library = library;
    record.launchOptions.insert(8000, QStringLiteral(R"(PROTON_LOG=1 %command% -window "C:\Games\save dir")"));
    record.proton.insert(8000, QStringLiteral("GE-Proton10-4"));
    record.launchOptions.insert(2231450, QString());
    record.proton.insert(620, QStringLiteral("proton_9")); // only one value recorded
    record.shortcuts.insert(2900000001, {QStringLiteral("Cave Story"), QStringLiteral("/home/user/Games/Cave Story/Doukutsu.exe"),
                                         QStringLiteral("/home/user/Games/Cave Story/"),
                                         QStringLiteral(" mangohud %command%"), QStringLiteral("GE-Proton10-4")});
    record.shortcuts.insert(3100000002, {QStringLiteral("MH4U"), QStringLiteral("/a.appimage"), {}, {}, {}});
    record.grid[2900000001].insert(QStringLiteral("hero"), {QStringLiteral("2900000001_hero.png"), 5, 4,
                                                           QStringLiteral("hero.png"), 3, 2});
    record.grid[2900000001].insert(QStringLiteral("icon"), {QStringLiteral("2900000001_icon.png"), 1, 1,
                                                           QStringLiteral("icon.webp"), 1, 1});
    record.removed.insert(3200000003, QStringLiteral("Hollow \"Knight\""));
    QString error;
    QVERIFY2(SteamSync::writeRecord(path, record, QStringLiteral("/data"), &error), qPrintable(error));
    QFile file(path);
    QVERIFY(file.open(QIODevice::ReadOnly));
    const QByteArray text = file.readAll();
    QVERIFY(text.startsWith("# What Steam had"));
    QVERIFY(text.contains("/data"));
    QVERIFY2(text.contains("library = \"lib-1\"\n\n"
                           "[games]\n"
                           "620 = { proton = \"proton_9\" }\n"
                           "8000 = { launch_options = \"PROTON_LOG=1 %command% -window \\\"C:\\\\Games\\\\save dir\\\"\", "
                           "proton = \"GE-Proton10-4\" }\n"
                           "2231450 = { launch_options = \"\" }\n"
                           "\n[shortcuts]\n"
                           "2900000001 = { exe = \"/home/user/Games/Cave Story/Doukutsu.exe\", "
                           "launch_options = \" mangohud %command%\", proton = \"GE-Proton10-4\", "
                           "start_dir = \"/home/user/Games/Cave Story/\", title = \"Cave Story\" }\n"
                           "3100000002 = { exe = \"/a.appimage\", launch_options = \"\", proton = \"\", start_dir = \"\", "
                           "title = \"MH4U\" }\n"
                           "\n[removed]\n"
                           "3200000003 = \"Hollow \\\"Knight\\\"\"\n"
                           "\n[grid.2900000001]\n"
                           "hero = { art = \"hero.png\", art_modified = 2, art_size = 3, file = \"2900000001_hero.png\", "
                           "modified = 4, size = 5 }\n"
                           "icon = { art = \"icon.webp\", art_modified = 1, art_size = 1, file = \"2900000001_icon.png\", "
                           "modified = 1, size = 1 }\n"),
             text.constData());
    QCOMPARE(SteamSync::readRecord(path, library, &warnings), record);
    QVERIFY(warnings.isEmpty());

    // Another library's record (a new data dir at the same path) counts as none, as does one
    // without an id, or any record when the library has no id.
    QCOMPARE(SteamSync::readRecord(path, QStringLiteral("lib-2"), &warnings), SteamSync::Record{QStringLiteral("lib-2")});
    QCOMPARE(warnings.size(), 1);
    QVERIFY(warnings.first().startsWith(path + QStringLiteral(" isn't this library's")));
    QCOMPARE(SteamSync::readRecord(path, QString(), &warnings), SteamSync::Record());
    file.close();
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
    file.write(text.mid(text.indexOf("[games]")));
    file.close();
    QCOMPARE(SteamSync::readRecord(path, library, &warnings), none);
    QCOMPARE(warnings.size(), 3);

    // A broken file counts as no record, with a warning.
    warnings.clear();
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
    file.write("[games\n");
    file.close();
    QCOMPARE(SteamSync::readRecord(path, library, &warnings), none);
    QCOMPARE(warnings.size(), 1);
    QVERIFY(warnings.first().startsWith(path + ":1:"));
}

void SteamTest::defaultPresets()
{
    SteamSettings settings;
    settings.presets = builtInSteamPresets();
    QCOMPARE(SteamSync::withDefaults(QStringLiteral("-novid"), settings), QStringLiteral("-novid"));
    settings.defaultPresets = {QStringLiteral("mangohud"), QStringLiteral("gamemode"), QStringLiteral("nonexistent")};
    QCOMPARE(SteamSync::withDefaults(QString(), settings), QStringLiteral("mangohud gamemoderun %command%"));
    QCOMPARE(SteamSync::withDefaults(QStringLiteral("mangohud %command% -novid"), settings),
             QStringLiteral("mangohud gamemoderun %command% -novid"));
}

namespace {

QByteArray readAll(const QString &path)
{
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}

bool steamClosed()
{
    return false;
}

const QString kLocalConfig = u"/userdata/39734273/config/localconfig.vdf"_s;
const QString kConfig = u"/config/config.vdf"_s;

} // namespace

void SteamTest::applyEdits()
{
    QTemporaryDir dir;
    const QString home = makeHome(dir, kNative, true);
    const QString root = QFileInfo(home + u'/' + kNative).canonicalFilePath();
    const QByteArray localBefore = readAll(root + kLocalConfig);
    const QByteArray configBefore = readAll(root + kConfig);
    const QString backups = dir.filePath("state/steam-backup");
    const SteamState s = scanHome(home);

    const QList<SteamSync::Edit> edits = {
        {2231450, u"launch_options"_s, u"mangohud %command%"_s, {}},       // added to the app's block
        {8000, u"launch_options"_s, u"-fullscreen"_s, s.apps.value(8000).launchOptions}, // changed
        {2231450, u"proton"_s, u"proton_9"_s, {}},                        // a new CompatToolMapping entry
        {8000, u"proton"_s, {}, u"GE-Proton10-4"_s},                      // Steam's default again
    };
    const SteamSync::ApplyResult r = SteamSync::apply(s, edits, backups, steamClosed);
    QVERIFY2(r.error.isEmpty(), qPrintable(r.error));
    QCOMPARE(r.applied, edits);
    QVERIFY(r.stale.isEmpty());

    // Only those values changed, laid out as Steam writes them.
    QByteArray local = localBefore;
    local.replace("\"LaunchOptions\"\t\t\"PROTON_LOG=1 %command% -window \\\"C:\\\\Games\\\\save dir\\\"\"",
                  "\"LaunchOptions\"\t\t\"-fullscreen\"");
    local.replace("\"BadgeData\"\t\t\"0a000000082d1206080310011800\"",
                  "\"BadgeData\"\t\t\"0a000000082d1206080310011800\"\n\t\t\t\t\t\t\"LaunchOptions\"\t\t\"mangohud %command%\"");
    QCOMPARE(readAll(root + kLocalConfig), local);
    const QByteArray block = "\t\t\t\t\t\"%1\"\n\t\t\t\t\t{\n\t\t\t\t\t\t\"name\"\t\t\"%2\"\n\t\t\t\t\t\t\"config\"\t\t\"\"\n"
                             "\t\t\t\t\t\t\"priority\"\t\t\"250\"\n\t\t\t\t\t}\n";
    QByteArray config = configBefore;
    config.replace(QByteArray(block).replace("%1", "8000").replace("%2", "GE-Proton10-4"),
                   QByteArray(block).replace("%1", "2231450").replace("%2", "proton_9"));
    QCOMPARE(readAll(root + kConfig), config);

    const SteamState after = scanHome(home, s);
    QCOMPARE(after.apps.value(2231450).launchOptions, u"mangohud %command%"_s);
    QCOMPARE(after.apps.value(2231450).playMinutes, 4085);
    QCOMPARE(after.compatTools, (QHash<qint64, QString>{{0, u"proton_experimental"_s}, {2231450, u"proton_9"_s},
                                                        {2900000001, u"GE-Proton10-4"_s}}));

    // Backed up once, as they were before the first write, readable only by the user.
    const QString localBackup = backups + kLocalConfig;
    QCOMPARE(readAll(localBackup), localBefore);
    QCOMPARE(readAll(backups + kConfig), configBefore);
    QCOMPARE(QFile::permissions(backups + kConfig) & 0x0777, QFile::ReadUser | QFile::WriteUser);
    QCOMPARE(r.log.size(), 4);

    // Emptied: the key goes, and the file is back to what it was.
    const SteamSync::ApplyResult cleared = SteamSync::apply(
        after, {{2231450, u"launch_options"_s, {}, u"mangohud %command%"_s}, {8000, u"launch_options"_s, u"PROTON_LOG=1 %command% -window \"C:\\Games\\save dir\""_s, u"-fullscreen"_s}},
        backups, steamClosed);
    QVERIFY2(cleared.error.isEmpty(), qPrintable(cleared.error));
    QCOMPARE(readAll(root + kLocalConfig), localBefore);
    QCOMPARE(readAll(localBackup), localBefore);
    QCOMPARE(cleared.log.size(), 1); // no second backup

    // A value Steam already has: nothing written.
    const QDateTime modified = QFileInfo(root + kConfig).lastModified();
    const SteamSync::ApplyResult same =
        SteamSync::apply(scanHome(home, after), {{2231450, u"proton"_s, u"proton_9"_s, u"proton_9"_s}}, backups, steamClosed);
    QCOMPARE(same.applied.size(), 1);
    QVERIFY(same.log.isEmpty());
    QCOMPARE(QFileInfo(root + kConfig).lastModified(), modified);
}

void SteamTest::applyRefusedWhileSteamRuns()
{
    QTemporaryDir dir;
    const QString home = makeHome(dir, kNative, true);
    const QString root = QFileInfo(home + u'/' + kNative).canonicalFilePath();
    const QByteArray localBefore = readAll(root + kLocalConfig);
    const QString backups = dir.filePath("state/steam-backup");
    const SteamState s = scanHome(home);
    const QList<SteamSync::Edit> edits = {{2231450, u"launch_options"_s, u"mangohud %command%"_s, {}}};

    // Running from the start: nothing read, nothing backed up.
    SteamSync::ApplyResult r = SteamSync::apply(s, edits, backups, [] { return true; });
    QCOMPARE(r.error, u"Steam is running"_s);
    QVERIFY(r.applied.isEmpty());
    QVERIFY(!QFileInfo::exists(backups));

    // Started while the file was being written: checked again before the rename.
    int asked = 0;
    r = SteamSync::apply(s, edits, backups, [&] { return ++asked > 1; });
    QCOMPARE(asked, 2);
    QCOMPARE(r.error, u"Steam started; %1 wasn't written"_s.arg(root + kLocalConfig));
    QVERIFY(r.applied.isEmpty());
    QCOMPARE(readAll(root + kLocalConfig), localBefore);
    QCOMPARE(QDir(QFileInfo(root + kLocalConfig).absolutePath()).entryList(QDir::Files),
             (QStringList{u"localconfig.vdf"_s, u"shortcuts.vdf"_s})); // no temporary file left

    // A Steam process that exits: the `sleep` stand-in, checked the way the app checks.
    const QString exe = fakeSteamBinary(root);
    QProcess steam;
    steam.start(exe, {u"30"_s});
    QVERIFY(steam.waitForStarted());
    writePid(home, steam.processId());
    const auto running = [&] { return SteamLocate::runningPid(s.install, {home}) > 0; };
    QCOMPARE(SteamSync::apply(s, edits, backups, running).error, u"Steam is running"_s);
    steam.kill();
    steam.waitForFinished();
    r = SteamSync::apply(s, edits, backups, running);
    QVERIFY2(r.error.isEmpty(), qPrintable(r.error));
    QCOMPARE(r.applied, edits);
    QVERIFY(readAll(root + kLocalConfig).contains("\"LaunchOptions\"\t\t\"mangohud %command%\""));
}

void SteamTest::applyStale()
{
    QTemporaryDir dir;
    const QString home = makeHome(dir, kNative, true);
    const QString root = QFileInfo(home + u'/' + kNative).canonicalFilePath();
    const SteamState s = scanHome(home);

    // Changed in Steam since the scan (the user picked a version before closing it): left
    // alone, for the next scan to sort out.
    const QList<SteamSync::Edit> edits = {{8000, u"proton"_s, u"proton_9"_s, u"proton_hotfix"_s},
                                          {2231450, u"proton"_s, u"proton_9"_s, {}}};
    const SteamSync::ApplyResult r = SteamSync::apply(s, edits, dir.filePath("backup"), steamClosed);
    QVERIFY(r.error.isEmpty());
    QCOMPARE(r.stale, edits.mid(0, 1));
    QCOMPARE(r.applied, edits.mid(1));
    const SteamState after = scanHome(home, s);
    QCOMPARE(after.compatTools.value(8000), u"GE-Proton10-4"_s);
    QCOMPARE(after.compatTools.value(2231450), u"proton_9"_s);

    // A file that doesn't parse isn't touched.
    QFile file(root + kConfig);
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
    file.write("\"InstallConfigStore\"\n{\n");
    file.close();
    const SteamSync::ApplyResult broken =
        SteamSync::apply(after, {{8000, u"proton"_s, u"proton_9"_s, u"GE-Proton10-4"_s}}, dir.filePath("backup"), steamClosed);
    QVERIFY(broken.error.startsWith(root + kConfig + u": "));
    QCOMPARE(readAll(root + kConfig), QByteArray("\"InstallConfigStore\"\n{\n"));
}

QTEST_MAIN(SteamTest)
namespace {

const QString kShortcutsVdf = u"/userdata/39734273/config/shortcuts.vdf"_s;

// Cave Story as the fake tree's Steam has it: a shortcut run with GE-Proton.
SteamState shortcutState()
{
    SteamState s;
    s.shortcutsOk = true;
    s.configOk = true;
    SteamShortcut cave;
    cave.appId = 2900000001u;
    cave.name = u"Cave Story"_s;
    cave.exe = u"/home/user/Games/Cave Story/Doukutsu.exe"_s;
    cave.startDir = u"/home/user/Games/Cave Story/"_s;
    cave.launchOptions = u" mangohud %command%"_s;
    s.shortcuts = {cave};
    s.compatTools.insert(2900000001, u"GE-Proton10-4"_s);
    return s;
}

// Its game file, in step with it (start_dir left out: the program's folder).
SteamSync::ShortcutGame caveGame()
{
    return {u"proton-cave-story"_s, 2900000001u, u"Cave Story"_s, u"/home/user/Games/Cave Story/Doukutsu.exe"_s, {},
            u" mangohud %command%"_s, u"GE-Proton10-4"_s, true};
}

SteamSync::ShortcutRecord caveRecord()
{
    return {u"Cave Story"_s, u"/home/user/Games/Cave Story/Doukutsu.exe"_s, u"/home/user/Games/Cave Story"_s,
            u" mangohud %command%"_s, u"GE-Proton10-4"_s};
}

using Copies = QList<std::pair<QString, QVariant>>;

} // namespace

void SteamTest::syncShortcuts()
{
    const SteamState steam = shortcutState();
    const QString cave = u"proton-cave-story"_s;
    // In sync, the program's folder being Steam's StartDir: the record takes Steam's values.
    SteamSync::Result r = SteamSync::sync({}, steam, {}, {caveGame()});
    QVERIFY(r.queued.isEmpty());
    QVERIFY(r.copies.isEmpty());
    QCOMPARE(r.record.shortcuts.value(2900000001), caveRecord());

    SteamSync::Record record;
    record.shortcuts.insert(2900000001, caveRecord());
    // Changed here: queued field by field; the record keeps what Steam has.
    SteamSync::ShortcutGame game = caveGame();
    game.title = u"Doukutsu Monogatari"_s;
    game.startDir = u"/home/user/Games/"_s;
    game.proton.clear(); // runs natively
    r = SteamSync::sync({}, steam, record, {game});
    QCOMPARE(r.queued.value(cave), (QStringList{u"title"_s, u"start_dir"_s, u"proton"_s}));
    QVERIFY(r.copies.isEmpty());
    QCOMPARE(r.record.shortcuts.value(2900000001), caveRecord());
    // With no record, the game file's values are queued too; an empty one takes Steam's.
    r = SteamSync::sync({}, steam, {}, {game});
    QCOMPARE(r.queued.value(cave), (QStringList{u"title"_s, u"start_dir"_s}));
    QCOMPARE(r.copies.value(cave), (Copies{{u"proton"_s, u"GE-Proton10-4"_s}}));

    // Changed in Steam: copied into the game file. A start folder that's the program's own
    // is no start_dir.
    SteamState moved = steam;
    moved.shortcuts[0].exe = u"/home/user/Games/Cave Story+/CaveStory+.exe"_s;
    moved.shortcuts[0].startDir = u"/home/user/Games/Cave Story+/"_s;
    moved.shortcuts[0].launchOptions.clear();
    r = SteamSync::sync({}, moved, record, {caveGame()});
    QVERIFY(r.queued.isEmpty());
    QCOMPARE(r.copies.value(cave), (Copies{{u"exe"_s, u"/home/user/Games/Cave Story+/CaveStory+.exe"_s},
                                           {u"start_dir"_s, QString()},
                                           {u"launch_options"_s, QString()}}));
    QCOMPARE(r.record.shortcuts.value(2900000001).exe, u"/home/user/Games/Cave Story+/CaveStory+.exe"_s);
    QCOMPARE(r.record.shortcuts.value(2900000001).startDir, u"/home/user/Games/Cave Story+"_s);
    QVERIFY(r.log.isEmpty());
    // Changed on both sides: Steam wins, logged.
    game = caveGame();
    game.launchOptions = u"gamemoderun %command%"_s;
    r = SteamSync::sync({}, moved, record, {game});
    QVERIFY(r.copies.value(cave).contains(std::pair<QString, QVariant>(u"launch_options"_s, QString())));
    QCOMPARE(r.log.size(), 1);

    // A Proton version Steam's config.vdf doesn't tell: neither queued nor copied.
    SteamState noConfig = steam;
    noConfig.configOk = false;
    noConfig.compatTools.clear();
    game = caveGame();
    game.proton = u"proton_9"_s;
    r = SteamSync::sync({}, noConfig, record, {game});
    QVERIFY(r.queued.isEmpty());
    QVERIFY(r.copies.isEmpty());
    QCOMPARE(r.record.shortcuts.value(2900000001).proton, u"GE-Proton10-4"_s);

    // shortcuts.vdf not read: nothing about shortcuts.
    SteamState unread = steam;
    unread.shortcutsOk = false;
    unread.shortcuts.clear();
    r = SteamSync::sync({}, unread, {}, {caveGame()});
    QVERIFY(r.queued.isEmpty());
    QVERIFY(r.copies.isEmpty());
    QVERIFY(r.record.shortcuts.isEmpty());
}

void SteamTest::syncRemovedShortcuts()
{
    const SteamState steam = shortcutState();
    const QString key = SteamSync::removedKey(2900000001u);
    QCOMPARE(SteamSync::removedAppId(key), 2900000001u);
    QCOMPARE(SteamSync::removedAppId(u"proton-cave-story"_s), 0u);
    SteamSync::Record record;
    record.shortcuts.insert(2900000001, caveRecord());
    record.removed.insert(2900000001, u"Cave Story"_s);

    // Its game gone, the shortcut and its Proton version in Steam: queued to go.
    SteamSync::Result r = SteamSync::sync({}, steam, record, {});
    QCOMPARE(r.queued, (QHash<QString, QStringList>{{key, {SteamSync::kRemoveShortcut}}}));
    QCOMPARE(r.record, record);
    // Only its CompatToolMapping entry left: still queued.
    SteamState mapped = steam;
    mapped.shortcuts.clear();
    QCOMPARE(SteamSync::sync({}, mapped, record, {}).queued.value(key), QStringList{SteamSync::kRemoveShortcut});
    // Neither left: forgotten, record and all.
    SteamState gone = mapped;
    gone.compatTools.clear();
    r = SteamSync::sync({}, gone, record, {});
    QVERIFY(r.queued.isEmpty());
    QVERIFY(r.record.removed.isEmpty());
    QVERIFY(!r.record.shortcuts.contains(2900000001));
    // A game has the app ID again (added back from Steam): it stays, and isn't removed.
    r = SteamSync::sync({}, steam, record, {caveGame()});
    QVERIFY(r.queued.isEmpty());
    QVERIFY(r.record.removed.isEmpty());
    QCOMPARE(r.record.shortcuts.value(2900000001), caveRecord());
    // shortcuts.vdf not read: left alone.
    SteamState unread = steam;
    unread.shortcutsOk = false;
    r = SteamSync::sync({}, unread, record, {});
    QVERIFY(r.queued.isEmpty());
    QCOMPARE(r.record, record);
}

void SteamTest::syncMissingShortcuts()
{
    SteamState none = shortcutState();
    none.shortcuts.clear();
    const QString cave = u"proton-cave-story"_s;
    // Not in Steam, and no record of it (a new game, a restore): added.
    SteamSync::Result r = SteamSync::sync({}, none, {}, {caveGame()});
    QCOMPARE(r.queued.value(cave), QStringList{SteamSync::kAddShortcut});
    QVERIFY(r.record.shortcuts.isEmpty());

    // With a record: deleted in Steam, so the game is marked not installed.
    SteamSync::Record record;
    record.shortcuts.insert(2900000001, caveRecord());
    r = SteamSync::sync({}, none, record, {caveGame()});
    QVERIFY(r.queued.isEmpty());
    QCOMPARE(r.copies.value(cave), (Copies{{u"installed"_s, false}}));
    QVERIFY(r.record.shortcuts.isEmpty());
    QCOMPARE(r.log.size(), 1);

    // Uninstalled and kept: its shortcut goes; once it's gone, so does its record.
    SteamSync::ShortcutGame kept = caveGame();
    kept.installed = false;
    r = SteamSync::sync({}, shortcutState(), record, {kept});
    QCOMPARE(r.queued.value(cave), QStringList{SteamSync::kRemoveShortcut});
    QVERIFY(r.copies.isEmpty());
    r = SteamSync::sync({}, none, record, {kept});
    QVERIFY(r.queued.isEmpty());
    QVERIFY(r.copies.isEmpty());
    QVERIFY(r.record.shortcuts.isEmpty());
}

void SteamTest::applyShortcuts()
{
    QTemporaryDir dir;
    const QString home = makeHome(dir, kNative, true);
    const QString root = QFileInfo(home + u'/' + kNative).canonicalFilePath();
    const QByteArray before = readAll(root + kShortcutsVdf);
    const QString backups = dir.filePath("state/steam-backup");
    const SteamState s = scanHome(home);
    QCOMPARE(s.shortcuts.size(), 3);
    const SteamShortcut cave = s.shortcuts.at(0);
    const SteamShortcut smash = s.shortcuts.at(2);

    SteamShortcut renamed = cave;
    renamed.name = u"Doukutsu Monogatari"_s;
    renamed.launchOptions.clear();
    SteamShortcut hades;
    hades.appId = 3400000004u;
    hades.name = u"Hades"_s;
    hades.exe = u"/home/user/Games/Hades/Hades.exe"_s;
    hades.startDir = u"/home/user/Games/Hades/"_s;
    const QList<SteamSync::ShortcutEdit> edits = {
        {SteamSync::ShortcutEdit::Kind::Update, renamed, cave},
        {SteamSync::ShortcutEdit::Kind::Remove, smash, smash},
        {SteamSync::ShortcutEdit::Kind::Add, hades, std::nullopt},
    };
    // The new shortcut's Proton version goes into config.vdf after it.
    const QList<SteamSync::Edit> proton = {{3400000004, u"proton"_s, u"proton_9"_s, {}}};
    const SteamSync::ApplyResult r = SteamSync::apply(s, proton, backups, steamClosed, edits);
    QVERIFY2(r.error.isEmpty(), qPrintable(r.error));
    QCOMPARE(r.shortcutsApplied, edits);
    QCOMPARE(r.applied, proton);
    QVERIFY(r.shortcutsStale.isEmpty());

    // Monster Hunter's bytes are untouched; Cave Story's only in the two fields that changed.
    const BinaryVdf::Document old = BinaryVdf::parse(before);
    const BinaryVdf::Document now = BinaryVdf::parse(readAll(root + kShortcutsVdf));
    QVERIFY(now.ok);
    const BinaryVdf::Node *oldList = old.root.child("shortcuts");
    const BinaryVdf::Node *newList = now.root.child("shortcuts");
    QCOMPARE(newList->children.size(), 3);
    QCOMPARE(newList->children.at(1), oldList->children.at(1));
    const BinaryVdf::Node &oldCave = oldList->children.at(0);
    const BinaryVdf::Node &newCave = newList->children.at(0);
    QCOMPARE(newCave.children.size(), oldCave.children.size());
    for (qsizetype i = 0; i < oldCave.children.size(); ++i) {
        const QByteArray key = oldCave.children.at(i).key;
        QCOMPARE(newCave.children.at(i).key, key);
        if (key != "appname" && key != "LaunchOptions")
            QCOMPARE(newCave.children.at(i), oldCave.children.at(i));
    }
    // Hades takes Smash's place at the end, numbered after the rest.
    QCOMPARE(newList->children.at(2).key, QByteArray("2"));

    const SteamState after = scanHome(home, s);
    QCOMPARE(after.shortcuts.size(), 3);
    QVERIFY(SteamSync::sameShortcut(after.shortcuts.at(0), renamed));
    QVERIFY(SteamSync::sameShortcut(after.shortcuts.at(2), hades));
    QCOMPARE(after.compatTools.value(3400000004), u"proton_9"_s);
    // Backed up once, as it was.
    QCOMPARE(readAll(backups + kShortcutsVdf), before);

    // Changed in Steam since the scan: left alone.
    const QByteArray edited = readAll(root + kShortcutsVdf);
    SteamShortcut again = cave;
    again.name = u"Cave Story+"_s;
    const SteamSync::ApplyResult stale =
        SteamSync::apply(after, {}, backups, steamClosed, {{SteamSync::ShortcutEdit::Kind::Update, again, cave}});
    QVERIFY(stale.error.isEmpty());
    QCOMPARE(stale.shortcutsStale.size(), 1);
    QVERIFY(stale.shortcutsApplied.isEmpty());
    QCOMPARE(readAll(root + kShortcutsVdf), edited);

    // Refused while Steam runs.
    const SteamSync::ApplyResult refused = SteamSync::apply(
        after, {}, backups, [] { return true; }, {{SteamSync::ShortcutEdit::Kind::Update, again, after.shortcuts.at(0)}});
    QCOMPARE(refused.error, u"Steam is running"_s);
    QCOMPARE(readAll(root + kShortcutsVdf), edited);
}

void SteamTest::applyFirstShortcut()
{
    QTemporaryDir dir;
    const QString home = makeHome(dir, kNative, true);
    const QString root = QFileInfo(home + u'/' + kNative).canonicalFilePath();
    QVERIFY(QFile::remove(root + kShortcutsVdf));
    const SteamState s = scanHome(home);
    QVERIFY(s.shortcutsOk);
    QVERIFY(s.shortcuts.isEmpty());

    // No shortcuts.vdf yet: the first shortcut makes it, with nothing to back up.
    SteamShortcut hades;
    hades.appId = 3400000004u;
    hades.name = u"Hades"_s;
    hades.exe = u"/home/user/Games/Hades/Hades.exe"_s;
    hades.startDir = u"/home/user/Games/Hades/"_s;
    const QString backups = dir.filePath("state/steam-backup");
    const SteamSync::ApplyResult r =
        SteamSync::apply(s, {}, backups, steamClosed, {{SteamSync::ShortcutEdit::Kind::Add, hades, std::nullopt}});
    QVERIFY2(r.error.isEmpty(), qPrintable(r.error));
    QCOMPARE(r.shortcutsApplied.size(), 1);
    const SteamState after = scanHome(home, s);
    QCOMPARE(after.shortcuts.size(), 1);
    QVERIFY(SteamSync::sameShortcut(after.shortcuts.at(0), hades));
    QVERIFY(!QFileInfo::exists(backups + kShortcutsVdf));
}

namespace {

// An image of one color, in the format its extension says.
void writeImage(const QString &path, const QColor &color, int width = 64, int height = 32)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QImage image(width, height, QImage::Format_RGB32);
    image.fill(color);
    QVERIFY(image.save(path));
}

// Moves a file's modification time `secs` from now, so a change never shares its stamp.
void touch(const QString &path, int secs)
{
    QFile file(path);
    QVERIFY(file.open(QIODevice::ReadWrite));
    QVERIFY(file.setFileTime(QDateTime::currentDateTimeUtc().addSecs(secs), QFileDevice::FileModificationTime));
}

using GameArt = QHash<qint64, QHash<QString, QString>>;

} // namespace

void SteamTest::gridArt()
{
    QTemporaryDir dir;
    const QString grid = dir.filePath("grid");
    const QString art = dir.filePath("art/proton-cave-story");
    writeImage(art + u"/hero.png"_s, Qt::red);
    writeImage(art + u"/cover.jpg"_s, Qt::green);
    writeImage(art + u"/icon.jpg"_s, Qt::blue);
    writeImage(art + u"/logo.png"_s, Qt::white);
    // A logo the user gave the shortcut in Steam.
    writeImage(grid + u"/2900000001_logo.jpg"_s, Qt::black);
    const QByteArray usersLogo = readAll(grid + u"/2900000001_logo.jpg"_s);

    const GameArt games = {{2900000001, {{u"hero"_s, art + u"/hero.png"_s}, {u"cover"_s, art + u"/cover.jpg"_s},
                                         {u"icon"_s, art + u"/icon.jpg"_s}, {u"logo"_s, art + u"/logo.png"_s}}}};
    SteamArt::GridFiles written;
    const QStringList log = SteamArt::writeGrid(grid, games, &written);
    QCOMPARE(log.size(), 3);

    // The missing kinds: PNG and JPEG copied as they are, the icon made a PNG.
    QCOMPARE(readAll(grid + u"/2900000001_hero.png"_s), readAll(art + u"/hero.png"_s));
    QCOMPARE(readAll(grid + u"/2900000001p.jpg"_s), readAll(art + u"/cover.jpg"_s));
    QVERIFY(readAll(grid + u"/2900000001_icon.png"_s).startsWith("\x89PNG"));
    QCOMPARE(QImage(grid + u"/2900000001_icon.png"_s).size(), QSize(64, 32));
    // The user's own is untouched, and nothing half-written is left behind.
    QCOMPARE(readAll(grid + u"/2900000001_logo.jpg"_s), usersLogo);
    QCOMPARE(QDir(grid).entryList(QDir::Files | QDir::Hidden).size(), 4);

    QStringList kinds = written.value(2900000001).keys();
    kinds.sort();
    QCOMPARE(kinds, (QStringList{u"cover"_s, u"hero"_s, u"icon"_s}));
    const SteamArt::GridFile hero = written.value(2900000001).value(u"hero"_s);
    QCOMPARE(hero.name, u"2900000001_hero.png"_s);
    QCOMPARE(hero.art, u"hero.png"_s);
    QCOMPARE(hero.size, QFileInfo(grid + u"/2900000001_hero.png"_s).size());

    // Nothing changed: nothing written.
    const SteamArt::GridFiles before = written;
    QVERIFY(SteamArt::writeGrid(grid, games, &written).isEmpty());
    QCOMPARE(written, before);

    // No grid folder: made for the first file. No grid at all (no Steam user): nothing.
    const QString fresh = dir.filePath("other/grid");
    SteamArt::GridFiles more;
    QCOMPARE(SteamArt::writeGrid(fresh, games, &more).size(), 4);
    SteamArt::GridFiles none;
    QVERIFY(SteamArt::writeGrid(QString(), games, &none).isEmpty());
    QVERIFY(none.isEmpty());
}

void SteamTest::gridArtFollowsTheArt()
{
    QTemporaryDir dir;
    const QString grid = dir.filePath("grid");
    const QString art = dir.filePath("art/proton-cave-story");
    writeImage(art + u"/hero.png"_s, Qt::red);
    writeImage(art + u"/cover.jpg"_s, Qt::green);
    writeImage(art + u"/icon.png"_s, Qt::blue);
    writeImage(art + u"/logo.png"_s, Qt::white);
    GameArt games = {{2900000001, {{u"hero"_s, art + u"/hero.png"_s}, {u"cover"_s, art + u"/cover.jpg"_s},
                                   {u"icon"_s, art + u"/icon.png"_s}, {u"logo"_s, art + u"/logo.png"_s}}}};
    SteamArt::GridFiles written;
    QCOMPARE(SteamArt::writeGrid(grid, games, &written).size(), 4);

    // The art changed: written again, as a JPEG now, the PNG gone.
    QVERIFY(QFile::remove(art + u"/hero.png"_s));
    writeImage(art + u"/hero.jpg"_s, Qt::yellow);
    games[2900000001][u"hero"_s] = art + u"/hero.jpg"_s;
    SteamArt::writeGrid(grid, games, &written);
    QVERIFY(!QFileInfo::exists(grid + u"/2900000001_hero.png"_s));
    QCOMPARE(readAll(grid + u"/2900000001_hero.jpg"_s), readAll(art + u"/hero.jpg"_s));
    QCOMPARE(written.value(2900000001).value(u"hero"_s).name, u"2900000001_hero.jpg"_s);

    // Replaced in Steam: the user's from then on, whether the art changes or goes.
    writeImage(grid + u"/2900000001p.jpg"_s, Qt::black, 10, 10);
    const QByteArray usersCover = readAll(grid + u"/2900000001p.jpg"_s);
    writeImage(art + u"/cover.jpg"_s, Qt::cyan);
    touch(art + u"/cover.jpg"_s, 10);
    SteamArt::writeGrid(grid, games, &written);
    QCOMPARE(readAll(grid + u"/2900000001p.jpg"_s), usersCover);
    QVERIFY(!written.value(2900000001).contains(u"cover"_s));
    games[2900000001].remove(u"cover"_s);
    SteamArt::writeGrid(grid, games, &written);
    QCOMPARE(readAll(grid + u"/2900000001p.jpg"_s), usersCover);

    // Deleted in Steam: not written again while the art stays the same.
    QVERIFY(QFile::remove(grid + u"/2900000001_icon.png"_s));
    QVERIFY(SteamArt::writeGrid(grid, games, &written).isEmpty());
    QVERIFY(!QFileInfo::exists(grid + u"/2900000001_icon.png"_s));
    // Until it changes.
    touch(art + u"/icon.png"_s, 20);
    SteamArt::writeGrid(grid, games, &written);
    QVERIFY(QFileInfo::exists(grid + u"/2900000001_icon.png"_s));

    // The art removed: its file goes.
    games[2900000001].remove(u"hero"_s);
    SteamArt::writeGrid(grid, games, &written);
    QVERIFY(!QFileInfo::exists(grid + u"/2900000001_hero.jpg"_s));
    QVERIFY(!written.value(2900000001).contains(u"hero"_s));

    // The game gone: the rest of the launcher's files go, the user's stay.
    SteamArt::writeGrid(grid, {}, &written);
    QVERIFY(written.isEmpty());
    QCOMPARE(QDir(grid).entryList(QDir::Files | QDir::Hidden), QStringList{u"2900000001p.jpg"_s});
}

void SteamTest::syncShortcutIcon()
{
    QTemporaryDir dir;
    SteamState steam = shortcutState();
    steam.install.kind = SteamInstall::Kind::Native;
    steam.install.root = dir.path();
    steam.install.userId = u"39734273"_s;
    const QString icon = SteamArt::gridIconPath(steam.install, 2900000001);
    QCOMPARE(icon, dir.path() + u"/userdata/39734273/config/grid/2900000001_icon.png"_s);
    const QString cave = u"proton-cave-story"_s;
    SteamSync::Record record;
    record.shortcuts.insert(2900000001, caveRecord());
    SteamSync::ShortcutGame game = caveGame();
    game.icon = icon;

    // Steam's shortcut has no icon: the launcher's is queued.
    SteamSync::Result r = SteamSync::sync({}, steam, record, {game});
    QCOMPARE(r.queued.value(cave), QStringList{SteamSync::kShortcutIcon});
    // It has that one: in sync. It has another (the user's): left alone.
    steam.shortcuts[0].icon = icon;
    QVERIFY(SteamSync::sync({}, steam, record, {game}).queued.isEmpty());
    SteamState own = steam;
    own.shortcuts[0].icon = u"/home/user/icons/cave.png"_s;
    QVERIFY(SteamSync::sync({}, own, record, {game}).queued.isEmpty());
    QVERIFY(SteamSync::sync({}, own, record, {caveGame()}).queued.isEmpty());

    // The launcher's icon gone: cleared while the shortcut points at the missing file.
    r = SteamSync::sync({}, steam, record, {caveGame()});
    QCOMPARE(r.queued.value(cave), QStringList{SteamSync::kShortcutIcon});
    // A file there again (the user's now): left alone.
    writeImage(icon, Qt::red);
    QVERIFY(SteamSync::sync({}, steam, record, {caveGame()}).queued.isEmpty());

    // A new shortcut takes the icon with it, not as a change of its own.
    SteamState none = steam;
    none.shortcuts.clear();
    QCOMPARE(SteamSync::sync({}, none, {}, {game}).queued.value(cave), QStringList{SteamSync::kAddShortcut});
}

#include "steam_test.moc"
