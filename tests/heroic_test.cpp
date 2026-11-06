#include "heroiclocate.h"
#include "heroicscan.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QTest>
#include <QTimeZone>

using namespace Qt::StringLiterals;

namespace {
QByteArray cache(const QList<QByteArray> &entries)
{
    QByteArray list;
    for (const QByteArray &entry : entries)
        list += (list.isEmpty() ? "" : ",") + entry;
    return R"({"library": [)" + list + R"(], "__timestamp": {"library": "2026-11-05T22:00:00.000Z"}})";
}

void writeFile(const QString &path, const QByteArray &content)
{
    QVERIFY(QDir().mkpath(QFileInfo(path).absolutePath()));
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
    QCOMPARE(file.write(content), content.size());
}

void writeProgram(const QString &dir, const QString &name)
{
    const QString path = dir + u'/' + name;
    writeFile(path, "#!/bin/sh\n");
    QVERIFY(QFile::setPermissions(path, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner));
}

const HeroicGame *findGame(const QList<HeroicGame> &games, const QString &appName)
{
    const auto it = std::find_if(games.cbegin(), games.cend(),
                                 [&](const HeroicGame &game) { return game.appName == appName; });
    return it == games.cend() ? nullptr : &*it;
}
}

class HeroicTest : public QObject
{
    Q_OBJECT

private slots:
    void init();

    void findNative();
    void findFlatpak();
    void nativeBeforeFlatpak();
    void nativeWithoutFilesIsSkipped();
    void findsOnInstalledFileAlone();
    void configHomeIsFollowed();
    void findConfiguredPath();
    void configuredFlatpakPath();
    void configuredPathMissing();
    void noHeroic();
    void programOnPath();
    void programFlatpak();
    void flatpakRootPrefersFlatpak();
    void flatpakNeedsTheApp();
    void programXdgOpen();
    void noProgram();

    void wholeLibrary();
    void installedDecides();
    void missingFiles();
    void emptyCache();
    void emptyCacheKeepsInstalled();
    void corruptCache();
    void corruptInstalled();
    void corruptPlayTime();
    void dlc();
    void allDlcIsStillKnown();
    void noTitle();
    void installedMissingFromCache();
    void repeatedEntries();
    void badEntries();
    void playTime();
    void watchPaths();
    void noRoot();
    void rescanReusesUnchangedCache();
    void rescanReadsChangedCache();
    void rescanKeepsCacheWarning();
    void lastPlayed();
    void cachedImage();
    void launchAndOpenArgs();

private:
    QString nativeRoot() const { return m_where.nativeRoot(); }
    QString flatpakRoot() const { return m_where.flatpakRoot(); }
    void writeLibrary(const QString &root, const QByteArray &content) { writeFile(HeroicScan::libraryPath(root), content); }
    void writeInstalled(const QString &root, const QByteArray &content) { writeFile(HeroicScan::installedPath(root), content); }

    std::unique_ptr<QTemporaryDir> m_dir;
    HeroicLocations m_where;
    QString m_root;
};

// Below the class: moc loses it after a multi-line raw string.
namespace {
// Shaped like Heroic 2.22.4's files; install_size is text in the cache and a number in installed.json.
const QByteArray kHike = R"({
    "app_name": "d6407c9e6fd54cb492b8c6635480d792",
    "art_cover": "https://cdn.test/hike-cover.jpg",
    "art_square": "https://cdn.test/hike-square.jpg",
    "install": {"executable": "AShortHike.exe", "install_path": "/old/path", "install_size": "308.78 MiB",
                "is_dlc": false, "platform": "Windows", "version": "1"},
    "is_installed": true,
    "runner": "legendary",
    "title": "A Short Hike"
})";
const QByteArray kFortnite = R"({
    "app_name": "Fortnite",
    "art_cover": "https://cdn.test/fn-cover.jpg",
    "art_logo": "https://cdn.test/fn-logo.png",
    "art_square": "https://cdn.test/fn-square.jpg",
    "install": {"install_size": "0", "is_dlc": false},
    "is_installed": false,
    "title": "Fortnite"
})";
const QByteArray kDlc = R"({
    "app_name": "dlc1",
    "install": {"install_size": "0", "is_dlc": true},
    "is_installed": false,
    "title": "A Season Pass"
})";
const QByteArray kInstalledHike = R"({
    "d6407c9e6fd54cb492b8c6635480d792": {
        "app_name": "d6407c9e6fd54cb492b8c6635480d792",
        "install_path": "/games/AShortHike",
        "install_size": 323783140,
        "is_dlc": false,
        "title": "A Short Hike"
    }
})";
}

void HeroicTest::init()
{
    m_dir = std::make_unique<QTemporaryDir>();
    QVERIFY(m_dir->isValid());
    const QString base = QFileInfo(m_dir->path()).canonicalFilePath();
    m_where.home = base + u"/home"_s;
    m_where.configHome = m_where.home + u"/.config"_s;
    m_where.pathDirs = {base + u"/bin"_s};
    m_where.flatpakDirs = {base + u"/flatpak/app"_s};
    QVERIFY(QDir().mkpath(m_where.home));
    QVERIFY(QDir().mkpath(m_where.pathDirs.first()));
    m_root = base + u"/root"_s;
}

void HeroicTest::findNative()
{
    writeLibrary(nativeRoot(), cache({}));
    const HeroicInstall install = HeroicLocate::find(m_where);
    QCOMPARE(install.kind, HeroicInstall::Kind::Native);
    QCOMPARE(install.root, nativeRoot());
    QVERIFY(install.found());
}

void HeroicTest::findFlatpak()
{
    writeLibrary(flatpakRoot(), cache({}));
    QVERIFY(flatpakRoot().endsWith(u"/.var/app/com.heroicgameslauncher.hgl/config/heroic"_s));
    const HeroicInstall install = HeroicLocate::find(m_where);
    QCOMPARE(install.kind, HeroicInstall::Kind::Flatpak);
    QCOMPARE(install.root, flatpakRoot());
}

void HeroicTest::nativeBeforeFlatpak()
{
    writeLibrary(nativeRoot(), cache({}));
    writeLibrary(flatpakRoot(), cache({}));
    QCOMPARE(HeroicLocate::find(m_where).kind, HeroicInstall::Kind::Native);
}

void HeroicTest::nativeWithoutFilesIsSkipped()
{
    QVERIFY(QDir().mkpath(nativeRoot()));
    writeLibrary(flatpakRoot(), cache({}));
    QCOMPARE(HeroicLocate::find(m_where).kind, HeroicInstall::Kind::Flatpak);
}

void HeroicTest::findsOnInstalledFileAlone()
{
    writeInstalled(nativeRoot(), "{}");
    QCOMPARE(HeroicLocate::find(m_where).kind, HeroicInstall::Kind::Native);
}

void HeroicTest::configHomeIsFollowed()
{
    m_where.configHome = m_where.home + u"/elsewhere"_s;
    writeLibrary(m_where.home + u"/elsewhere/heroic"_s, cache({}));
    QCOMPARE(HeroicLocate::find(m_where).root, m_where.home + u"/elsewhere/heroic"_s);
}

void HeroicTest::findConfiguredPath()
{
    writeLibrary(nativeRoot(), cache({}));
    writeLibrary(m_where.home + u"/my-heroic"_s, cache({}));
    const HeroicInstall install = HeroicLocate::find(m_where, u"~/my-heroic"_s);
    QCOMPARE(install.kind, HeroicInstall::Kind::Native);
    QCOMPARE(install.root, m_where.home + u"/my-heroic"_s);
}

void HeroicTest::configuredFlatpakPath()
{
    writeLibrary(nativeRoot(), cache({}));
    writeLibrary(flatpakRoot(), cache({}));
    QCOMPARE(HeroicLocate::find(m_where, flatpakRoot()).kind, HeroicInstall::Kind::Flatpak);
}

void HeroicTest::configuredPathMissing()
{
    writeLibrary(nativeRoot(), cache({}));
    const HeroicInstall install = HeroicLocate::find(m_where, u"~/nowhere"_s);
    QVERIFY(!install.found());
    QVERIFY(install.pathMissing);
}

void HeroicTest::noHeroic()
{
    const HeroicInstall install = HeroicLocate::find(m_where);
    QVERIFY(!install.found());
    QVERIFY(!install.pathMissing);
    QVERIFY(install.root.isEmpty());
}

void HeroicTest::programOnPath()
{
    const QString bin = m_where.pathDirs.first();
    writeProgram(bin, u"heroic"_s);
    writeProgram(bin, u"flatpak"_s);
    QVERIFY(QDir().mkpath(m_where.flatpakDirs.first() + u"/com.heroicgameslauncher.hgl/current"_s));
    writeLibrary(nativeRoot(), cache({}));
    const HeroicProgram program = HeroicLocate::find(m_where).program;
    QCOMPARE(program.program, bin + u"/heroic"_s);
    QCOMPARE(program.args, QStringList{u"--no-gui"_s});
    QVERIFY(program.noGui);
}

void HeroicTest::programFlatpak()
{
    const QString bin = m_where.pathDirs.first();
    writeProgram(bin, u"flatpak"_s);
    writeProgram(bin, u"xdg-open"_s);
    QVERIFY(QDir().mkpath(m_where.flatpakDirs.first() + u"/com.heroicgameslauncher.hgl/current"_s));
    writeLibrary(flatpakRoot(), cache({}));
    const HeroicProgram program = HeroicLocate::find(m_where).program;
    QCOMPARE(program.program, bin + u"/flatpak"_s);
    QCOMPARE(program.args, (QStringList{u"run"_s, u"com.heroicgameslauncher.hgl"_s, u"--no-gui"_s}));
    QVERIFY(program.noGui);
}

void HeroicTest::flatpakRootPrefersFlatpak()
{
    const QString bin = m_where.pathDirs.first();
    writeProgram(bin, u"heroic"_s);
    writeProgram(bin, u"flatpak"_s);
    QVERIFY(QDir().mkpath(m_where.flatpakDirs.first() + u"/com.heroicgameslauncher.hgl/current"_s));
    writeLibrary(flatpakRoot(), cache({}));
    QCOMPARE(HeroicLocate::find(m_where).program.program, bin + u"/flatpak"_s);
}

void HeroicTest::flatpakNeedsTheApp()
{
    const QString bin = m_where.pathDirs.first();
    writeProgram(bin, u"flatpak"_s);
    writeProgram(bin, u"heroic"_s);
    writeLibrary(flatpakRoot(), cache({}));
    QCOMPARE(HeroicLocate::find(m_where).program.program, bin + u"/heroic"_s);
}

void HeroicTest::programXdgOpen()
{
    const QString bin = m_where.pathDirs.first();
    writeProgram(bin, u"xdg-open"_s);
    writeLibrary(nativeRoot(), cache({}));
    const HeroicProgram program = HeroicLocate::find(m_where).program;
    QCOMPARE(program.program, bin + u"/xdg-open"_s);
    QVERIFY(program.args.isEmpty());
    QVERIFY(!program.noGui);
}

void HeroicTest::noProgram()
{
    writeLibrary(nativeRoot(), cache({}));
    const HeroicInstall install = HeroicLocate::find(m_where);
    QVERIFY(install.found());
    QVERIFY(!install.program.found());
}

void HeroicTest::wholeLibrary()
{
    writeLibrary(m_root, cache({kHike, kFortnite}));
    writeInstalled(m_root, kInstalledHike);
    const HeroicState state = HeroicScan::scan(m_root);
    QVERIFY(state.warnings.isEmpty());
    QVERIFY(state.libraryKnown);
    QVERIFY(state.installedOk);

    const QList<HeroicGame> games = state.games();
    QCOMPARE(games.size(), 2);
    const HeroicGame &hike = games.at(0);
    QCOMPARE(hike.appName, u"d6407c9e6fd54cb492b8c6635480d792"_s);
    QCOMPARE(hike.title, u"A Short Hike"_s);
    QVERIFY(hike.installed);
    QCOMPARE(hike.installPath, u"/games/AShortHike"_s);
    QCOMPARE(hike.sizeBytes, 323783140);
    QCOMPARE(hike.artCover, u"https://cdn.test/hike-cover.jpg"_s);
    QCOMPARE(hike.artSquare, u"https://cdn.test/hike-square.jpg"_s);
    QVERIFY(hike.artLogo.isEmpty());

    const HeroicGame &fortnite = games.at(1);
    QCOMPARE(fortnite.appName, u"Fortnite"_s);
    QVERIFY(!fortnite.installed);
    QVERIFY(fortnite.installPath.isEmpty());
    QCOMPARE(fortnite.sizeBytes, 0);
    QCOMPARE(fortnite.artLogo, u"https://cdn.test/fn-logo.png"_s);
}

void HeroicTest::installedDecides()
{
    writeLibrary(m_root, cache({kHike}));
    writeInstalled(m_root, "{}");
    const QList<HeroicGame> games = HeroicScan::scan(m_root).games();
    QCOMPARE(games.size(), 1);
    QVERIFY(!games.first().installed);
    QVERIFY(games.first().installPath.isEmpty());
}

void HeroicTest::missingFiles()
{
    QVERIFY(QDir().mkpath(m_root));
    const HeroicState state = HeroicScan::scan(m_root);
    QVERIFY(state.warnings.isEmpty());
    QVERIFY(!state.libraryKnown);
    QVERIFY(state.installedOk);
    QVERIFY(state.playTimeOk);
    QVERIFY(state.games().isEmpty());
}

void HeroicTest::emptyCache()
{
    for (const QByteArray &content : {cache({}), QByteArray("{}")}) {
        writeLibrary(m_root, content);
        const HeroicState state = HeroicScan::scan(m_root);
        QVERIFY(state.warnings.isEmpty());
        QVERIFY(!state.libraryKnown);
        QVERIFY(state.games().isEmpty());
    }
}

void HeroicTest::emptyCacheKeepsInstalled()
{
    writeLibrary(m_root, cache({}));
    writeInstalled(m_root, kInstalledHike);
    const HeroicState state = HeroicScan::scan(m_root);
    QVERIFY(!state.libraryKnown);
    QCOMPARE(state.games().size(), 1);
    QVERIFY(state.games().first().installed);
}

void HeroicTest::corruptCache()
{
    for (const QByteArray &content : {QByteArray(R"({"library": [{"app_na)"), QByteArray(), QByteArray("[1]")}) {
        writeLibrary(m_root, content);
        writeInstalled(m_root, kInstalledHike);
        const HeroicState state = HeroicScan::scan(m_root);
        QVERIFY(!state.libraryKnown);
        QCOMPARE(state.warnings.size(), 1);
        QVERIFY(state.warnings.first().contains(u"legendary_library.json"_s));
        QCOMPARE(state.games().size(), 1);
    }
}

void HeroicTest::corruptInstalled()
{
    writeLibrary(m_root, cache({kHike, kFortnite}));
    writeInstalled(m_root, "{ not json");
    const HeroicState state = HeroicScan::scan(m_root);
    QVERIFY(!state.installedOk);
    QCOMPARE(state.warnings.size(), 1);
    QVERIFY(state.warnings.first().contains(u"installed.json"_s));

    const QList<HeroicGame> games = state.games();
    QCOMPARE(games.size(), 2);
    QVERIFY(games.at(0).installed);
    QCOMPARE(games.at(0).installPath, u"/old/path"_s);
    QVERIFY(!games.at(1).installed);
}

void HeroicTest::corruptPlayTime()
{
    writeFile(HeroicScan::playTimePath(m_root), "nope");
    const HeroicState state = HeroicScan::scan(m_root);
    QVERIFY(!state.playTimeOk);
    QCOMPARE(state.warnings.size(), 1);
    QVERIFY(state.warnings.first().contains(u"timestamp.json"_s));
}

void HeroicTest::dlc()
{
    writeLibrary(m_root, cache({kHike, kDlc}));
    writeInstalled(m_root, R"({"dlc1": {"title": "A Season Pass", "install_path": "/games/x", "is_dlc": true}})");
    const QList<HeroicGame> games = HeroicScan::scan(m_root).games();
    QCOMPARE(games.size(), 1);
    QCOMPARE(games.first().title, u"A Short Hike"_s);
}

void HeroicTest::allDlcIsStillKnown()
{
    writeLibrary(m_root, cache({kDlc}));
    const HeroicState state = HeroicScan::scan(m_root);
    QVERIFY(state.libraryKnown);
    QVERIFY(state.games().isEmpty());
}

void HeroicTest::noTitle()
{
    writeLibrary(m_root, cache({R"({"app_name": "bare", "install": {}})",
                                R"({"app_name": "named", "title": "  ", "install": {}})"}));
    writeInstalled(m_root, R"({"named": {"title": "Named on Disk", "install_path": "/games/n"},
                               "lost": {"install_path": "/games/l"}})");
    const QList<HeroicGame> games = HeroicScan::scan(m_root).games();
    QCOMPARE(games.size(), 3);
    QCOMPARE(findGame(games, u"bare"_s)->title, u"bare"_s);
    QCOMPARE(findGame(games, u"named"_s)->title, u"Named on Disk"_s);
    QCOMPARE(findGame(games, u"lost"_s)->title, u"lost"_s);
}

void HeroicTest::installedMissingFromCache()
{
    writeLibrary(m_root, cache({kFortnite}));
    writeInstalled(m_root, kInstalledHike);
    const HeroicState state = HeroicScan::scan(m_root);
    QVERIFY(state.libraryKnown);
    const QList<HeroicGame> games = state.games();
    QCOMPARE(games.size(), 2);
    const HeroicGame *hike = findGame(games, u"d6407c9e6fd54cb492b8c6635480d792"_s);
    QVERIFY(hike);
    QCOMPARE(hike->title, u"A Short Hike"_s);
    QVERIFY(hike->installed);
    QCOMPARE(hike->installPath, u"/games/AShortHike"_s);
    QCOMPARE(hike->sizeBytes, 323783140);
}

void HeroicTest::repeatedEntries()
{
    writeLibrary(m_root, cache({kHike, kHike}));
    QCOMPARE(HeroicScan::scan(m_root).games().size(), 1);
}

void HeroicTest::badEntries()
{
    writeLibrary(m_root, cache({"1", "null", R"({"title": "No App Name"})", kFortnite}));
    writeInstalled(m_root, R"({"broken": 5})");
    const HeroicState state = HeroicScan::scan(m_root);
    QVERIFY(state.warnings.isEmpty());
    QCOMPARE(state.games().size(), 1);
    QCOMPARE(state.games().first().appName, u"Fortnite"_s);
}

void HeroicTest::playTime()
{
    writeFile(HeroicScan::playTimePath(m_root), R"({
        "hike": {"firstPlayed": "2026-11-05T22:11:53.958Z", "lastPlayed": "2026-11-05T22:19:44.817Z", "totalPlayed": 2},
        "started": {"firstPlayed": "2026-11-05T22:11:53.958Z"},
        "broken": 7
    })");
    const HeroicState state = HeroicScan::scan(m_root);
    QVERIFY(state.playTimeOk);
    QCOMPARE(state.playTime.size(), 2);
    const QDateTime last(QDate(2026, 11, 5), QTime(22, 19, 44), QTimeZone::UTC);
    QCOMPARE(state.playTime.value(u"hike"_s), (HeroicPlayTime{2, last.toSecsSinceEpoch()}));
    QCOMPARE(state.playTime.value(u"started"_s), HeroicPlayTime{});
}

void HeroicTest::watchPaths()
{
    QVERIFY(QDir().mkpath(m_root));
    QCOMPARE(HeroicScan::scan(m_root).watchPaths(),
             (QStringList{m_root + u"/store_cache/legendary_library.json"_s,
                          m_root + u"/legendaryConfig/legendary/installed.json"_s, m_root + u"/store/timestamp.json"_s}));
}

void HeroicTest::noRoot()
{
    const HeroicState state = HeroicScan::scan({});
    QCOMPARE(state, HeroicState{});
    QVERIFY(state.watchPaths().isEmpty());
}

void HeroicTest::rescanReusesUnchangedCache()
{
    const QString path = HeroicScan::libraryPath(m_root);
    writeLibrary(m_root, cache({kFortnite}));
    const QDateTime modified = QFileInfo(path).lastModified();
    const HeroicState first = HeroicScan::scan(m_root);

    QByteArray same = cache({kFortnite});
    same.replace("Fortnite", "Fortnott");
    writeLibrary(m_root, same);
    QFile file(path);
    QVERIFY(file.open(QIODevice::ReadWrite));
    QVERIFY(file.setFileTime(modified, QFileDevice::FileModificationTime));
    file.close();

    QCOMPARE(HeroicScan::scan(m_root, first).games().first().title, u"Fortnite"_s);
    QCOMPARE(HeroicScan::scan(m_root).games().first().title, u"Fortnott"_s);
}

void HeroicTest::rescanReadsChangedCache()
{
    writeLibrary(m_root, cache({kFortnite}));
    const HeroicState first = HeroicScan::scan(m_root);
    writeLibrary(m_root, cache({kFortnite, kHike}));
    QCOMPARE(HeroicScan::scan(m_root, first).games().size(), 2);

    QVERIFY(QFile::remove(HeroicScan::libraryPath(m_root)));
    const HeroicState gone = HeroicScan::scan(m_root, first);
    QVERIFY(!gone.libraryKnown);
    QVERIFY(gone.games().isEmpty());
}

void HeroicTest::rescanKeepsCacheWarning()
{
    writeLibrary(m_root, "{ not json");
    const HeroicState first = HeroicScan::scan(m_root);
    QCOMPARE(first.warnings.size(), 1);
    QCOMPARE(HeroicScan::scan(m_root, first).warnings, first.warnings);
}

void HeroicTest::cachedImage()
{
    const QString url = u"https://cdn.test/fez-square.jpg"_s;
    QCOMPARE(HeroicScan::cachedImage(m_root, url), QString());
    QCOMPARE(HeroicScan::cachedImage(m_root, QString()), QString());

    // The name of a real copy: sha256 of the URL and the size Heroic asks for.
    const QByteArray name = QCryptographicHash::hash(url.toUtf8() + "?h=400&resize=1&w=300", QCryptographicHash::Sha256).toHex();
    const QString path = m_root + u"/images-cache/"_s + QString::fromLatin1(name);
    writeFile(path, "image");
    QCOMPARE(HeroicScan::cachedImage(m_root, url), path);
    QCOMPARE(HeroicScan::cachedImage(m_root, u"https://cdn.test/other.jpg"_s), QString());
}

void HeroicTest::lastPlayed()
{
    QCOMPARE(HeroicScan::lastPlayed(m_root, u"Fez"_s), std::optional<qint64>(0)); // no file yet
    writeFile(HeroicScan::playTimePath(m_root),
              R"({"Fez":{"lastPlayed":"2026-11-05T10:00:00.000Z","totalPlayed":3},"New":{"firstPlayed":"2026-11-05T10:00:00.000Z"}})");
    QCOMPARE(HeroicScan::lastPlayed(m_root, u"Fez"_s),
             std::optional<qint64>(QDateTime(QDate(2026, 11, 5), QTime(10, 0), QTimeZone::UTC).toSecsSinceEpoch()));
    QCOMPARE(HeroicScan::lastPlayed(m_root, u"New"_s), std::optional<qint64>(0));
    QCOMPARE(HeroicScan::lastPlayed(m_root, u"Other"_s), std::optional<qint64>(0));
    writeFile(HeroicScan::playTimePath(m_root), "{\"Fez\":{");
    QCOMPARE(HeroicScan::lastPlayed(m_root, u"Fez"_s), std::nullopt);
}

void HeroicTest::launchAndOpenArgs()
{
    const HeroicProgram native{u"/usr/bin/heroic"_s, {u"--no-gui"_s}, true};
    QCOMPARE(HeroicLocate::launchArgs(native, u"Fez&App"_s),
             (QStringList{u"--no-gui"_s, u"heroic://launch?appName=Fez%26App&runner=legendary&gui=false"_s}));
    QCOMPARE(HeroicLocate::openArgs(native), QStringList());

    const HeroicProgram flatpak{u"/usr/bin/flatpak"_s, {u"run"_s, u"com.heroicgameslauncher.hgl"_s, u"--no-gui"_s}, true};
    QCOMPARE(HeroicLocate::openArgs(flatpak), (QStringList{u"run"_s, u"com.heroicgameslauncher.hgl"_s}));

    const HeroicProgram xdgOpen{u"/usr/bin/xdg-open"_s, {}, false};
    QCOMPARE(HeroicLocate::launchArgs(xdgOpen, u"App"_s),
             QStringList{u"heroic://launch?appName=App&runner=legendary&gui=false"_s});
    QCOMPARE(HeroicLocate::openArgs(xdgOpen), QStringList{u"heroic://ping"_s});
}

QTEST_GUILESS_MAIN(HeroicTest)
#include "heroic_test.moc"
