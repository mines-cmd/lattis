#include "applocation.h"
#include "appupdate.h"
#include "appimageinstall.h"
#include "desktopentry.h"
#include "fakehttpserver.h"
#include "ffmpegtool.h"
#include "flatpakinstall.h"
#include "installer.h"
#include "installrecord.h"
#include "releasesource.h"
#include "testfiles.h"
#include "versions.h"

#include <QCryptographicHash>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>

#include <algorithm>
#include <memory>

#include <unistd.h>

#include <archive.h>
#include <archive_entry.h>

namespace {
QString sha(const QByteArray &bytes)
{
    return QString::fromLatin1(QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex());
}

bool write(const QString &path, const QByteArray &bytes)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size();
}

struct ZipEntry
{
    QString name;
    QByteArray data;
    unsigned type = AE_IFREG;
    QString link;
};

void makeZip(const QString &path, const QList<ZipEntry> &entries)
{
    struct archive *a = archive_write_new();
    archive_write_set_format_zip(a);
    archive_write_open_filename(a, QFile::encodeName(path).constData());
    for (const ZipEntry &e : entries) {
        struct archive_entry *entry = archive_entry_new();
        archive_entry_set_pathname(entry, e.name.toUtf8().constData());
        archive_entry_set_filetype(entry, e.type);
        archive_entry_set_perm(entry, 0644);
        if (!e.link.isEmpty())
            archive_entry_set_symlink(entry, e.link.toUtf8().constData());
        else
            archive_entry_set_size(entry, e.data.size());
        archive_write_header(a, entry);
        if (e.link.isEmpty())
            archive_write_data(a, e.data.constData(), size_t(e.data.size()));
        archive_entry_free(entry);
    }
    archive_write_close(a);
    archive_write_free(a);
}

QStringList names(const QString &folder)
{
    return QDir(folder).entryList(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden, QDir::Name);
}

QByteArray json(const char *text)
{
    return QByteArray(text).replace('\'', '"');
}

const QByteArray kGitHub = json("{\n"
        "  'tag_name': '0.10.5', 'published_at': '2026-09-01T10:00:00Z',\n"
        "  'assets': [\n"
        "    {'id': 11, 'name': 'mGBA-0.10.5-win64.7z', 'browser_download_url': 'https://github.com/o/r/releases/download/0.10.5/win.7z',\n"
        "     'size': 10, 'digest': null, 'updated_at': '2026-09-01T10:01:00Z'},\n"
        "    {'id': 12, 'name': 'mGBA-0.10.5-appimage-x64.appimage', 'browser_download_url': 'https://github.com/o/r/releases/download/0.10.5/a.appimage',\n"
        "     'size': 25208000, 'digest': 'sha256:ABCDEF', 'updated_at': '2026-09-01T10:02:00Z'}\n"
        "  ]}");
}

class InstallTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        qRegisterMetaType<AppImageDownload::Result>();
        qRegisterMetaType<FlatpakJob::Result>();
    }

    void sourceParses_data();
    void sourceParses();
    void sourceUrls();
    void pastedUrls();
    void githubRelease();
    void gitlabRelease();
    void releaseErrors();
    void prereleaseList();
    void assetPatterns_data();
    void assetPatterns();
    void assetPreference();
    void targetFolder();
    void archiveNames();

    void extractsOneFile();
    void extractRefuses_data();
    void extractRefuses();
    void extractNoMatch();

    void desktopRewrite();
    void ownEntry();
    void moveAppImage();
    void downloadInstalls();
    void downloadChecksum();
    void downloadCutShort();
    void downloadRefusesPlainHttp();
    void downloadRefusesNoChecksum();
    void downloadRefusesUnsafeNames();
    void installerAsksWithoutChecksum();
    void releaseDropsUnsafeNames();
    void downloadNotFound();
    void downloadCancelled();
    void downloadFromZip();
    void downloadKeepsExisting();
    void downloadPaced();

    void recordRoundTrip();
    void recordRules();
    void recordBroken();

    void installerInstalls();
    void installerFailsClearly();
    void installerCancels();

    void flatpakHelpers();
    void flatpakJob();
    void flatpakJobCancels();
    void installerFlatpak();
    void installerFlatpakFails();
    void installerNoFlatpak();

    void replacingKeepsTheOldUntilTheNewIsIn();
    void twoUpdatesKeepBothOldBuilds();
    void aTrashThatRefusesIsTold();
    void installerChecksUpdates();
    void installerChecksByFileForARollingTag();
    void installerAskedCheckSaysWhenDone();
    void installerUsesTheReleaseCache();
    void installerUpdates();
    void installerUpdatesInPlace();
    void installerChecksGames();
    void installerUpdatesGames();
    void installerRefusesGameUpdates();
    void gameUpdateRecords();
    void installerUninstalls();
    void installerRefusesWhileRunning();

    void ffmpegAssetNames();
    void ffmpegInstalls();
    void ffmpegRefusesWrongChecksum();
    void ffmpegRefusesBadReleases();
    void ffmpegRefusesBadArchives();
    void ffmpegKeepsTheOldCopyWhenTheNewOneWontStart();
    void ffmpegCancelLeavesNothing();
    void ffmpegRemoves();
    void ffmpegPrefersTheOwnCopy();

    void versionCompare_data();
    void versionCompare();
    void releaseNotes();
    void downloadTargetName();
    void appUpdateFindsANewerRelease();
    void appUpdateIgnoresOlderAndEqual();
    void appUpdateUnavailableDoesNothing();
    void appUpdateRefusesWithoutChecksum();
    void appUpdateWrongChecksumKeepsTheOldFile();
    void appUpdateReplacesKeepingThePath();
    void appUpdateCancels();
    void appUpdateCantReplace();
    void appUpdateSkipsAVersion();
    void appUpdateRestart();
    void appUpdateWhatsNew();
    void appUpdateDailyCheck();

    void menuEntryRefusals();
    void menuEntryFromAnAppImage();
    void menuEntryFollowsLinks();
    void installerAsksTheReleaseApi();
    void installerRefusesARedirectToHttp();
    void installerTimesOut();
    void appUpdateAsksTheReleaseApi();
    void appUpdateRefusesARedirectToHttp();
    void appUpdateTimesOut();
    void appUpdateUsesTheCachedAnswer();
    void appUpdateTrashRefuses();
    void installerUpdateTrashRefuses();
    void appLocationMoves();
    void flatpakJobEnvironment();
    void flatpakJobKillsWhatIgnoresTerm();
    void ffmpegCancelWhileVerifying();
};

void InstallTest::sourceParses_data()
{
    QTest::addColumn<QString>("text");
    QTest::addColumn<bool>("ok");
    QTest::addColumn<QString>("server");
    QTest::addColumn<QString>("project");
    QTest::newRow("github") << "github:mgba-emu/mgba" << true << "github.com" << "mgba-emu/mgba";
    QTest::newRow("gitlab") << "gitlab:git.ryujinx.app/ryubing/ryujinx" << true << "git.ryujinx.app" << "ryubing/ryujinx";
    QTest::newRow("gitlab subgroup") << "gitlab:gitlab.com/a/b/c" << true << "gitlab.com" << "a/b/c";
    QTest::newRow("forgejo") << "forgejo:git.eden-emu.dev/eden-emu/eden" << true << "git.eden-emu.dev" << "eden-emu/eden";
    QTest::newRow("forgejo no owner") << "forgejo:git.eden-emu.dev/eden" << false << "" << "";
    QTest::newRow("forgejo subgroup") << "forgejo:codeberg.org/a/b/c" << false << "" << "";
    QTest::newRow("forgejo bad host") << "forgejo:localhost/a/b" << false << "" << "";
    QTest::newRow("no scheme") << "mgba-emu/mgba" << false << "" << "";
    QTest::newRow("github one part") << "github:mgba" << false << "" << "";
    QTest::newRow("github three parts") << "github:a/b/c" << false << "" << "";
    QTest::newRow("github dots") << "github:../etc" << false << "" << "";
    QTest::newRow("gitlab no group") << "gitlab:git.example.org/project" << false << "" << "";
    QTest::newRow("gitlab bad host") << "gitlab:localhost/a/b" << false << "" << "";
    QTest::newRow("gitlab path trick") << "gitlab:evil.com@x.org/a/b" << false << "" << "";
    QTest::newRow("empty") << "" << false << "" << "";
}

void InstallTest::sourceParses()
{
    QFETCH(QString, text);
    QFETCH(bool, ok);
    QFETCH(QString, server);
    QFETCH(QString, project);
    QString error;
    const auto source = ReleaseSources::parse(text, &error);
    QCOMPARE(source.has_value(), ok);
    if (ok) {
        QCOMPARE(source->server, server);
        QCOMPARE(source->project, project);
    } else {
        QVERIFY(!error.isEmpty());
    }
}

void InstallTest::sourceUrls()
{
    QCOMPARE(ReleaseSources::parse("github:mgba-emu/mgba")->apiUrl().toString(),
             QStringLiteral("https://api.github.com/repos/mgba-emu/mgba/releases/latest"));
    const QUrl gitlab = ReleaseSources::parse("gitlab:git.ryujinx.app/ryubing/ryujinx")->apiUrl();
    QCOMPARE(gitlab.host(), QStringLiteral("git.ryujinx.app"));
    QCOMPARE(gitlab.scheme(), QStringLiteral("https"));
    QCOMPARE(gitlab.toEncoded(), QByteArray("https://git.ryujinx.app/api/v4/projects/ryubing%2Fryujinx/releases?per_page=1"));
    QCOMPARE(ReleaseSources::parse("github:o/r")->label(), QStringLiteral("GitHub"));
    QCOMPARE(ReleaseSources::parse("gitlab:git.ryujinx.app/a/b")->label(), QStringLiteral("git.ryujinx.app"));
    QCOMPARE(ReleaseSources::parse("forgejo:git.eden-emu.dev/eden-emu/eden")->apiUrl().toString(),
             QStringLiteral("https://git.eden-emu.dev/api/v1/repos/eden-emu/eden/releases/latest"));
    QCOMPARE(ReleaseSources::parse("forgejo:git.eden-emu.dev/a/b")->label(), QStringLiteral("git.eden-emu.dev"));
}

void InstallTest::pastedUrls()
{
    QCOMPARE(ReleaseSources::fromUrl("https://github.com/imputnet/helium-linux"), QString("github:imputnet/helium-linux"));
    QCOMPARE(ReleaseSources::fromUrl(" https://github.com/imputnet/helium-linux/releases/latest "),
             QString("github:imputnet/helium-linux"));
    QCOMPARE(ReleaseSources::fromUrl("github.com/o/r.git?tab=readme#x"), QString("github:o/r"));
    QCOMPARE(ReleaseSources::fromUrl("https://gitlab.com/group/sub/project/-/releases"),
             QString("gitlab:gitlab.com/group/sub/project"));
    QCOMPARE(ReleaseSources::fromUrl("https://git.ryujinx.app/ryubing/ryujinx"),
             QString());
    QCOMPARE(ReleaseSources::fromUrl("https://gitlab.example.org/a/b"), QString("gitlab:gitlab.example.org/a/b"));
    QCOMPARE(ReleaseSources::fromUrl("github:o/r"), QString());
    QCOMPARE(ReleaseSources::fromUrl("https://github.com/onlyowner"), QString());
    QCOMPARE(ReleaseSources::fromUrl("https://example.com/a/b"), QString());
}

void InstallTest::githubRelease()
{
    QString error;
    const auto release = ReleaseSources::parseRelease(*ReleaseSources::parse("github:o/r"), kGitHub, &error);
    QVERIFY2(release.has_value(), qPrintable(error));
    QCOMPARE(release->tag, QStringLiteral("0.10.5"));
    QCOMPARE(release->date, QStringLiteral("2026-09-01T10:00:00Z"));
    QCOMPARE(release->assets.size(), 2);
    const ReleaseAsset &mgba = release->assets.at(1);
    QCOMPARE(mgba.id, QStringLiteral("12"));
    QCOMPARE(mgba.size, qint64(25208000));
    QCOMPARE(mgba.sha256, QStringLiteral("abcdef"));
    QCOMPARE(mgba.date, QStringLiteral("2026-09-01T10:02:00Z"));
    QCOMPARE(mgba.url.toString(), QStringLiteral("https://github.com/o/r/releases/download/0.10.5/a.appimage"));
    QCOMPARE(release->assets.at(0).sha256, QString());
}

void InstallTest::gitlabRelease()
{
    const QByteArray body = json("[{\n"
        "      'tag_name': '1.3.3', 'released_at': '2026-10-01T00:00:00Z',\n"
        "      'assets': {'count': 2, 'sources': [{'format': 'zip', 'url': 'https://git.example.org/x.zip'}],\n"
        "        'links': [\n"
        "          {'id': 5, 'name': 'ryujinx-1.3.3-x64.AppImage', 'url': 'https://git.example.org/a', 'direct_asset_url': 'https://git.example.org/direct/a'},\n"
        "          {'id': 6, 'name': 'ryujinx-1.3.3-win.zip', 'url': 'https://git.example.org/w'}\n"
        "        ]}}]");
    QString error;
    const auto release = ReleaseSources::parseRelease(*ReleaseSources::parse("gitlab:git.example.org/a/b"), body, &error);
    QVERIFY2(release.has_value(), qPrintable(error));
    QCOMPARE(release->tag, QStringLiteral("1.3.3"));
    QCOMPARE(release->assets.size(), 2);
    QCOMPARE(release->assets.at(0).url.toString(), QStringLiteral("https://git.example.org/direct/a"));
    QCOMPARE(release->assets.at(1).url.toString(), QStringLiteral("https://git.example.org/w"));
    QCOMPARE(release->assets.at(0).sha256, QString());
    QCOMPARE(release->assets.at(0).date, QStringLiteral("2026-10-01T00:00:00Z"));
}

void InstallTest::releaseErrors()
{
    const auto github = *ReleaseSources::parse("github:o/r");
    const auto gitlab = *ReleaseSources::parse("gitlab:git.example.org/a/b");
    QString error;

    QVERIFY(!ReleaseSources::parseRelease(github, json("{'message': 'API rate limit exceeded', 'status': '403'}"), &error));
    QCOMPARE(error, QStringLiteral("API rate limit exceeded"));

    error.clear();
    QVERIFY(!ReleaseSources::parseRelease(gitlab, "[]", &error));
    QVERIFY(error.contains(QStringLiteral("no release")));

    error.clear();
    QVERIFY(!ReleaseSources::parseRelease(github, "<html>", &error));
    QVERIFY(error.contains(QStringLiteral("GitHub")));

    error.clear();
    QVERIFY(!ReleaseSources::parseRelease(gitlab, json("{'message': '404 Not Found'}"), &error));
    QCOMPARE(error, QStringLiteral("404 Not Found"));

    // A release without assets is a release; picking from it finds nothing.
    const auto empty = ReleaseSources::parseRelease(github, json("{'tag_name': '1'}"), &error);
    QVERIFY(empty.has_value());
    QVERIFY(empty->assets.isEmpty());
}

void InstallTest::prereleaseList()
{
    auto github = *ReleaseSources::parse("github:o/r");
    QCOMPARE(github.apiUrl().toString(), QStringLiteral("https://api.github.com/repos/o/r/releases/latest"));
    github.prereleases = true;
    QCOMPARE(github.apiUrl().toString(), QStringLiteral("https://api.github.com/repos/o/r/releases?per_page=20"));

    const QByteArray list = json("[\n"
        "  {'tag_name': 'b4', 'draft': true, 'assets': [{'id': 1, 'name': 'x-pgo.AppImage', 'browser_download_url': 'https://github.com/o/r/d/1'}]},\n"
        "  {'tag_name': 'b3', 'prerelease': true, 'assets': [{'id': 2, 'name': 'x.tar.gz', 'browser_download_url': 'https://github.com/o/r/d/2'}]},\n"
        "  {'tag_name': 'b2', 'prerelease': true, 'assets': [{'id': 3, 'name': 'x.AppImage', 'browser_download_url': 'https://github.com/o/r/d/3'}]},\n"
        "  {'tag_name': 'b1', 'assets': [{'id': 4, 'name': 'x-pgo.AppImage', 'browser_download_url': 'https://github.com/o/r/d/4'}]}\n"
        "]");
    QString error;
    auto release = ReleaseSources::parseRelease(github, list, &error, QStringLiteral("x-pgo.AppImage;x.AppImage"));
    QVERIFY2(release.has_value(), qPrintable(error));
    QCOMPARE(release->tag, QStringLiteral("b2")); // the draft and the release without a download are skipped
    QVERIFY(release->prerelease);

    release = ReleaseSources::parseRelease(github, list, &error, QStringLiteral("x-pgo.AppImage"));
    QVERIFY(release.has_value());
    QCOMPARE(release->tag, QStringLiteral("b1"));
    QVERIFY(!release->prerelease);

    error.clear();
    QVERIFY(!ReleaseSources::parseRelease(github, list, &error, QStringLiteral("*.zip")));
    QVERIFY2(error.contains(QStringLiteral("None of the latest releases has a download named *.zip")), qPrintable(error));

    // An answer cached before the key was set is a single release.
    release = ReleaseSources::parseRelease(github, kGitHub, &error, QStringLiteral("*.appimage"));
    QVERIFY(release.has_value());
    QCOMPARE(release->tag, QStringLiteral("0.10.5"));
}

void InstallTest::assetPatterns_data()
{
    QTest::addColumn<QString>("name");
    QTest::addColumn<QString>("patterns");
    QTest::addColumn<bool>("matches");
    QTest::newRow("exact") << "azahar.AppImage" << "azahar.AppImage" << true;
    QTest::newRow("case") << "AZAHAR.appimage" << "azahar.AppImage" << true;
    QTest::newRow("star") << "Cemu-2.6-x86_64.AppImage" << "Cemu-*-x86_64.AppImage" << true;
    QTest::newRow("lowercase extension") << "mGBA-0.10.5-appimage-x64.appimage" << "mGBA-*-appimage-x64.appimage" << true;
    QTest::newRow("whole name") << "azahar.AppImage.zsync" << "azahar.AppImage" << false;
    QTest::newRow("other build") << "azahar-wayland.AppImage" << "azahar.AppImage" << false;
    QTest::newRow("second pattern") << "rosalie.appimage" << "RMG*.AppImage;rosalie*.AppImage" << true;
    QTest::newRow("dot is literal") << "azaharXAppImage" << "azahar.AppImage" << false;
    QTest::newRow("empty patterns") << "azahar.AppImage" << "" << false;
}

void InstallTest::assetPatterns()
{
    QFETCH(QString, name);
    QFETCH(QString, patterns);
    QFETCH(bool, matches);
    QCOMPARE(ReleaseSources::matches(name, patterns), matches);
}

void InstallTest::assetPreference()
{
    Release release;
    release.assets = {{"1", "app-wayland.AppImage", {}, 0, {}, {}}, {"2", "app.AppImage", {}, 0, {}, {}},
                      {"3", "app-x11.AppImage", {}, 0, {}, {}}};
    // The first pattern wins, whatever the order of the release's files.
    QCOMPARE(ReleaseSources::pickAsset(release, "app-x11.AppImage;app.AppImage")->id, QStringLiteral("3"));
    QCOMPARE(ReleaseSources::pickAsset(release, "app*.AppImage")->id, QStringLiteral("1"));
    QVERIFY(!ReleaseSources::pickAsset(release, "nothing*"));
}

void InstallTest::targetFolder()
{
    QTemporaryDir home;
    const QString apps = home.filePath("Applications");
    const QString images = home.filePath("AppImages");
    const QStringList both{apps, images};

    QCOMPARE(AppImageInstall::targetFolder(both, home.path()), apps);
    QVERIFY(QDir().mkpath(images));
    QCOMPARE(AppImageInstall::targetFolder(both, home.path()), images);
    QVERIFY(QDir().mkpath(apps));
    QCOMPARE(AppImageInstall::targetFolder(both, home.path()), apps);
    // A file called Applications isn't a folder.
    QTemporaryDir other;
    QVERIFY(write(other.filePath("Applications"), "x"));
    QCOMPARE(AppImageInstall::targetFolder({other.filePath("Applications")}, other.path()), other.filePath("Applications"));
}

void InstallTest::archiveNames()
{
    for (const char *name : {"a.zip", "A.ZIP", "a.tar", "a.tar.gz", "a.tgz", "a.tar.xz", "a.tar.zst", "a.tar.bz2"})
        QVERIFY2(AppImageInstall::isArchive(QString::fromLatin1(name)), name);
    for (const char *name : {"a.AppImage", "a.appimage", "zip", "a.zip.sig", "a.7z"})
        QVERIFY2(!AppImageInstall::isArchive(QString::fromLatin1(name)), name);
}

void InstallTest::extractsOneFile()
{
    QTemporaryDir dir;
    makeZip(dir.filePath("a.zip"), {{"readme.txt", "hi"}, {"bin/Shadps4-sdl.AppImage", "payload"}, {"other.AppImage", "no"}});
    const auto result = AppImageInstall::extractOne(dir.filePath("a.zip"), "*.AppImage", dir.path(), nullptr);
    QVERIFY2(result.error.isEmpty(), qPrintable(result.error));
    QCOMPARE(result.name, QStringLiteral("Shadps4-sdl.AppImage"));
    QFile file(dir.filePath(".Shadps4-sdl.AppImage.partial"));
    QVERIFY(file.open(QIODevice::ReadOnly));
    QCOMPARE(file.readAll(), QByteArray("payload"));
    QVERIFY(!QFileInfo::exists(dir.filePath("other.AppImage")));
}

void InstallTest::extractRefuses_data()
{
    QTest::addColumn<QString>("name");
    QTest::addColumn<unsigned>("type");
    QTest::addColumn<QString>("link");
    QTest::addColumn<QString>("message");
    QTest::newRow("dot dot") << "../Evil.AppImage" << unsigned(AE_IFREG) << "" << "outside";
    QTest::newRow("dot dot inside") << "a/../../Evil.AppImage" << unsigned(AE_IFREG) << "" << "outside";
    QTest::newRow("absolute") << "/tmp/Evil.AppImage" << unsigned(AE_IFREG) << "" << "outside";
    QTest::newRow("symlink") << "Evil.AppImage" << unsigned(AE_IFLNK) << "/etc/passwd" << "link";
}

void InstallTest::extractRefuses()
{
    QFETCH(QString, name);
    QFETCH(unsigned, type);
    QFETCH(QString, link);
    QFETCH(QString, message);
    QTemporaryDir dir;
    QDir().mkpath(dir.filePath("out"));
    makeZip(dir.filePath("a.zip"), {{name, "payload", type, link}});
    const auto result = AppImageInstall::extractOne(dir.filePath("a.zip"), "*.AppImage", dir.filePath("out"), nullptr);
    QVERIFY(result.name.isEmpty());
    QVERIFY2(result.error.contains(message), qPrintable(result.error));
    QVERIFY(names(dir.filePath("out")).isEmpty());
    QVERIFY(!QFileInfo::exists(dir.filePath("Evil.AppImage")));
}

void InstallTest::extractNoMatch()
{
    QTemporaryDir dir;
    makeZip(dir.filePath("a.zip"), {{"readme.txt", "hi"}});
    auto result = AppImageInstall::extractOne(dir.filePath("a.zip"), "*.AppImage", dir.path(), nullptr);
    QVERIFY(result.error.contains(QStringLiteral("no file matching")));
    QVERIFY(write(dir.filePath("junk.zip"), "not an archive at all"));
    result = AppImageInstall::extractOne(dir.filePath("junk.zip"), "*.AppImage", dir.path(), nullptr);
    QVERIFY(!result.error.isEmpty());
    std::atomic_bool cancel = true;
    result = AppImageInstall::extractOne(dir.filePath("a.zip"), "*", dir.path(), &cancel);
    QVERIFY(result.name.isEmpty() && result.error.isEmpty());
}

namespace {
AppImageDownload::Result runDownload(AppImageDownload &download)
{
    QSignalSpy spy(&download, &AppImageDownload::finished);
    download.start();
    if (spy.isEmpty())
        spy.wait(10000);
    return spy.isEmpty() ? AppImageDownload::Result{} : spy.first().first().value<AppImageDownload::Result>();
}

ReleaseAsset assetFor(const FakeHttpServer &server, const QString &path, const QByteArray &bytes, bool checksum = true)
{
    ReleaseAsset asset;
    asset.id = "7";
    asset.name = path.section(u'/', -1);
    asset.url = server.url(path);
    asset.size = bytes.size();
    if (checksum)
        asset.sha256 = sha(bytes);
    return asset;
}
}

void InstallTest::desktopRewrite()
{
    const QByteArray source = "[Desktop Entry]\nName=mGBA\nExec=mgba-qt %f\nTryExec=mgba-qt\nIcon=mgba\nX-AppImage-Version=1\n"
                              "\n[Desktop Action new]\nName=New\nExec=\"mgba-qt\" --new\n";
    const QByteArray out = DesktopEntry::rewrite(source, "/home/u/My Apps/mGBA.AppImage", "/home/u/.local/share/icons/lattis/mgba.png");
    QCOMPARE(out,
             QByteArray("[Desktop Entry]\nName=mGBA\nExec=\"/home/u/My Apps/mGBA.AppImage\" %f\n"
                        "Icon=/home/u/.local/share/icons/lattis/mgba.png\n"
                        "\n[Desktop Action new]\nName=New\nExec=\"/home/u/My Apps/mGBA.AppImage\" --new\n"));

    const QByteArray noIcon = DesktopEntry::rewrite("[Desktop Entry]\nName=A\nExec=a\n", "/a.AppImage", "/i.png");
    QCOMPARE(noIcon, QByteArray("[Desktop Entry]\nIcon=/i.png\nName=A\nExec=\"/a.AppImage\"\n"));
}

void InstallTest::ownEntry()
{
    QTemporaryDir data;
    const QString appImage = data.filePath(QStringLiteral("My Apps/Lattis.AppImage"));
    QDir().mkpath(data.filePath(QStringLiteral("applications")));
    QFile entry(data.filePath(QStringLiteral("applications/lattis-lattis.desktop")));
    QVERIFY(entry.open(QIODevice::WriteOnly));
    entry.write(("[Desktop Entry]\nName=Lattis\nExec=\"" + appImage + "\"\n").toUtf8());
    entry.close();
    QDir().mkpath(data.filePath(QStringLiteral("icons/lattis")));
    QFile icon(data.filePath(QStringLiteral("icons/lattis/lattis.png")));
    QVERIFY(icon.open(QIODevice::WriteOnly));
    icon.close();

    QString error;
    QVERIFY(DesktopEntry::ensureOwn(appImage, data.path(), {}, &error).desktop.isEmpty());
    QVERIFY(error.isEmpty());

    DesktopEntry::removeOwn(data.path());
    QVERIFY(!entry.exists());
    QVERIFY(!icon.exists());

    const QString applications = data.filePath(QStringLiteral("applications"));
    QCOMPARE(AppUpdates::managedBy(appImage, applications), QString());
    QVERIFY(entry.open(QIODevice::WriteOnly));
    entry.close();
    QFile manager(applications + QStringLiteral("/gearlever_lattis.desktop"));
    QVERIFY(manager.open(QIODevice::WriteOnly));
    manager.write(("Exec=" + appImage).toUtf8());
    manager.close();
    QCOMPARE(AppUpdates::managedBy(appImage, applications), QStringLiteral("Gear Lever"));
}

void InstallTest::moveAppImage()
{
    QTemporaryDir root;
    const QString from = root.filePath(QStringLiteral("Downloads"));
    const QString to = root.filePath(QStringLiteral("Applications"));
    const QString applications = root.filePath(QStringLiteral("applications"));
    QDir().mkpath(from);
    QDir().mkpath(applications);
    const QString appImage = from + QStringLiteral("/Lattis.AppImage");
    QFile file(appImage);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("x");
    file.close();

    QCOMPARE(AppLocations::moveBlocker(appImage, to, applications), QString());
    QVERIFY(!AppLocations::moveBlocker(from + QStringLiteral("/missing"), to, applications).isEmpty());

    QFile manager(applications + QStringLiteral("/gearlever_lattis.desktop"));
    QVERIFY(manager.open(QIODevice::WriteOnly));
    manager.write(("Exec=" + appImage).toUtf8());
    manager.close();
    QVERIFY(AppLocations::moveBlocker(appImage, to, applications).contains(QStringLiteral("Gear Lever")));
    QVERIFY(manager.remove());

    QString moved;
    QCOMPARE(AppLocations::moveInto(appImage, to, &moved), QString());
    QCOMPARE(moved, to + QStringLiteral("/Lattis.AppImage"));
    QVERIFY(QFile::exists(moved));
    QVERIFY(!QFile::exists(appImage));

    QVERIFY(file.open(QIODevice::WriteOnly));
    file.close();
    QVERIFY(!AppLocations::moveBlocker(appImage, to, applications).isEmpty());
    QVERIFY(!AppLocations::moveInto(appImage, to, &moved).isEmpty());
    QVERIFY(QFile::exists(appImage));
}

void InstallTest::downloadInstalls()
{
    FakeHttpServer server;
    const QByteArray bytes(300000, 'x');
    server.on("/d/Emu.AppImage", {200, bytes});
    QTemporaryDir home;
    const QString folder = home.filePath("Applications");
    QNetworkAccessManager network;
    AppImageDownload download(&network, assetFor(server, "/d/Emu.AppImage", bytes), {}, folder);
    download.setRelaxed(true);
    QSignalSpy progress(&download, &AppImageDownload::progress);
    QSignalSpy stages(&download, &AppImageDownload::stageChanged);

    const auto result = runDownload(download);
    QVERIFY2(result.ok, qPrintable(result.error));
    QCOMPARE(result.path, folder + "/Emu.AppImage");
    QCOMPARE(result.sha256, sha(bytes));
    QCOMPARE(names(folder), QStringList{"Emu.AppImage"});
    const QFileInfo info(result.path);
    QCOMPARE(info.permissions() & QFileDevice::ExeOwner, QFileDevice::ExeOwner);
    QCOMPARE(info.size(), qint64(bytes.size()));
    QVERIFY(!progress.isEmpty());
    QCOMPARE(progress.last().at(0).toLongLong(), qint64(bytes.size()));
    QCOMPARE(stages.size(), 1); // Installing
    QCOMPARE(download.stage(), AppImageDownload::Stage::Installing);
}

void InstallTest::downloadChecksum()
{
    FakeHttpServer server;
    server.on("/d/Emu.AppImage", {200, "tampered!"});
    QTemporaryDir home;
    ReleaseAsset asset = assetFor(server, "/d/Emu.AppImage", "genuine!!");
    QNetworkAccessManager network;
    AppImageDownload download(&network, asset, {}, home.filePath("Applications"));
    download.setRelaxed(true);
    const auto result = runDownload(download);
    QVERIFY(!result.ok);
    QVERIFY(!result.cancelled);
    QVERIFY2(result.error.contains(QStringLiteral("checksum")), qPrintable(result.error));
    QVERIFY(names(home.filePath("Applications")).isEmpty());
}

void InstallTest::downloadCutShort()
{
    FakeHttpServer server;
    server.on("/d/Emu.AppImage", {200, "short"});
    QTemporaryDir home;
    ReleaseAsset asset = assetFor(server, "/d/Emu.AppImage", "a longer file", false);
    QNetworkAccessManager network;
    AppImageDownload download(&network, asset, {}, home.filePath("Applications"));
    download.setRelaxed(true);
    download.setRequireChecksum(false);
    const auto result = runDownload(download);
    QVERIFY(!result.ok);
    QVERIFY2(result.error.contains(QStringLiteral("cut short")), qPrintable(result.error));
    QVERIFY(names(home.filePath("Applications")).isEmpty());
}

void InstallTest::downloadRefusesPlainHttp()
{
    FakeHttpServer server;
    server.on("/d/Emu.AppImage", {200, "data"});
    QTemporaryDir home;
    QNetworkAccessManager network;
    AppImageDownload download(&network, assetFor(server, "/d/Emu.AppImage", "data"), {}, home.filePath("Applications"));
    const auto result = runDownload(download);
    QVERIFY(!result.ok);
    QVERIFY2(result.error.contains(QStringLiteral("HTTPS")), qPrintable(result.error));
    QVERIFY(server.requests.isEmpty());
    QVERIFY(!QFileInfo::exists(home.filePath("Applications")) || names(home.filePath("Applications")).isEmpty());
}

void InstallTest::downloadRefusesNoChecksum()
{
    FakeHttpServer server;
    server.on("/d/Emu.AppImage", {200, "data"});
    QTemporaryDir home;
    QNetworkAccessManager network;
    {
        AppImageDownload download(&network, assetFor(server, "/d/Emu.AppImage", "data", false), {},
                                  home.filePath("Applications"));
        download.setRelaxed(true);
        const auto result = runDownload(download);
        QVERIFY(!result.ok);
        QCOMPARE(result.error, AppImageInstall::noChecksumText());
        QVERIFY(server.requests.isEmpty()); // nothing is downloaded before the user chooses
        QVERIFY(!QFileInfo::exists(home.filePath("Applications")) || names(home.filePath("Applications")).isEmpty());
    }
    {
        AppImageDownload download(&network, assetFor(server, "/d/Emu.AppImage", "data", false), {},
                                  home.filePath("Applications"));
        download.setRelaxed(true);
        download.setRequireChecksum(false);
        const auto result = runDownload(download);
        QVERIFY2(result.ok, qPrintable(result.error));
        QCOMPARE(names(home.filePath("Applications")), QStringList{"Emu.AppImage"});
    }
}

void InstallTest::downloadRefusesUnsafeNames()
{
    FakeHttpServer server;
    server.on("/d/x", {200, "data"});
    QTemporaryDir home;
    QNetworkAccessManager network;
    for (const QString &name : {QStringLiteral("../x"), QStringLiteral("/../x"), QStringLiteral("a/b"), QStringLiteral(".hidden"),
                                QStringLiteral(".."), QStringLiteral("a\\b")}) {
        ReleaseAsset asset = assetFor(server, "/d/x", "data");
        asset.name = name;
        AppImageDownload download(&network, asset, {}, home.filePath("Applications"));
        download.setRelaxed(true);
        const auto result = runDownload(download);
        QVERIFY2(!result.ok, qPrintable(name));
    }
    QVERIFY(server.requests.isEmpty());
    QVERIFY(!QFileInfo::exists(home.filePath("x")) && !QFileInfo::exists(home.filePath("Applications/x")));
}

void InstallTest::releaseDropsUnsafeNames()
{
    const QByteArray json = R"({"tag_name":"v1","assets":[
        {"id":1,"name":"../x.AppImage","browser_download_url":"https://h/a"},
        {"id":2,"name":"a/b.AppImage","browser_download_url":"https://h/b"},
        {"id":3,"name":".hidden.AppImage","browser_download_url":"https://h/c"},
        {"id":4,"name":"ok.AppImage","browser_download_url":"https://h/d"}]})";
    const auto source = ReleaseSources::parse(QStringLiteral("github:a/b"));
    QVERIFY(source);
    QString error;
    const auto release = ReleaseSources::parseRelease(*source, json, &error);
    QVERIFY2(release, qPrintable(error));
    QCOMPARE(release->assets.size(), 1);
    QCOMPARE(release->assets.first().name, QStringLiteral("ok.AppImage"));
}

void InstallTest::downloadNotFound()
{
    FakeHttpServer server;
    QTemporaryDir home;
    QNetworkAccessManager network;
    AppImageDownload download(&network, assetFor(server, "/missing/Emu.AppImage", "data"), {}, home.filePath("Applications"));
    download.setRelaxed(true);
    const auto result = runDownload(download);
    QVERIFY(!result.ok);
    QVERIFY(!result.error.isEmpty());
    QVERIFY(names(home.filePath("Applications")).isEmpty());
}

void InstallTest::downloadCancelled()
{
    FakeHttpServer server;
    server.on("/d/Emu.AppImage", {200, "data", 5000});
    QTemporaryDir home;
    QNetworkAccessManager network;
    AppImageDownload download(&network, assetFor(server, "/d/Emu.AppImage", "data"), {}, home.filePath("Applications"));
    download.setRelaxed(true);
    QSignalSpy spy(&download, &AppImageDownload::finished);
    download.start();
    QTest::qWait(150);
    QVERIFY(spy.isEmpty());
    QVERIFY(QFileInfo::exists(home.filePath("Applications/.Emu.AppImage.partial")));
    download.cancel();
    if (spy.isEmpty())
        spy.wait(5000);
    QCOMPARE(spy.size(), 1);
    const auto result = spy.first().first().value<AppImageDownload::Result>();
    QVERIFY(result.cancelled);
    QVERIFY(!result.ok);
    QVERIFY(result.error.isEmpty());
    QVERIFY(names(home.filePath("Applications")).isEmpty());
}

void InstallTest::downloadFromZip()
{
    QTemporaryDir src;
    makeZip(src.filePath("pkg.zip"), {{"README", "r"}, {"Emu.AppImage", "the appimage"}});
    QFile zip(src.filePath("pkg.zip"));
    QVERIFY(zip.open(QIODevice::ReadOnly));
    const QByteArray bytes = zip.readAll();

    FakeHttpServer server;
    server.on("/d/pkg.zip", {200, bytes});
    QTemporaryDir home;
    const QString folder = home.filePath("Applications");
    QNetworkAccessManager network;
    AppImageDownload download(&network, assetFor(server, "/d/pkg.zip", bytes), "*.AppImage", folder);
    download.setRelaxed(true);
    QSignalSpy stages(&download, &AppImageDownload::stageChanged);
    const auto result = runDownload(download);
    QVERIFY2(result.ok, qPrintable(result.error));
    QCOMPARE(result.path, folder + "/Emu.AppImage");
    QCOMPARE(names(folder), QStringList{"Emu.AppImage"});
    QCOMPARE(QFileInfo(result.path).permissions() & QFileDevice::ExeOwner, QFileDevice::ExeOwner);
    QCOMPARE(stages.size(), 2); // Extracting, Installing

    // A zip without the file leaves nothing behind.
    makeZip(src.filePath("bad.zip"), {{"../Emu.AppImage", "x"}});
    QFile bad(src.filePath("bad.zip"));
    QVERIFY(bad.open(QIODevice::ReadOnly));
    const QByteArray badBytes = bad.readAll();
    server.on("/d/bad.zip", {200, badBytes});
    AppImageDownload second(&network, assetFor(server, "/d/bad.zip", badBytes), "*.AppImage", folder);
    second.setRelaxed(true);
    const auto refused = runDownload(second);
    QVERIFY(!refused.ok);
    QVERIFY2(refused.error.contains(QStringLiteral("outside")), qPrintable(refused.error));
    QCOMPARE(names(folder), QStringList{"Emu.AppImage"});
}

void InstallTest::downloadKeepsExisting()
{
    FakeHttpServer server;
    server.on("/d/Emu.AppImage", {200, "new"});
    QTemporaryDir home;
    const QString folder = home.filePath("Applications");
    QVERIFY(write(folder + "/Emu.AppImage", "old"));
    QNetworkAccessManager network;
    AppImageDownload download(&network, assetFor(server, "/d/Emu.AppImage", "new"), {}, folder);
    download.setRelaxed(true);
    const auto result = runDownload(download);
    QVERIFY(!result.ok);
    QVERIFY2(result.error.contains(QStringLiteral("already")), qPrintable(result.error));
    QCOMPARE(names(folder), QStringList{"Emu.AppImage"});
    QFile file(folder + "/Emu.AppImage");
    QVERIFY(file.open(QIODevice::ReadOnly));
    QCOMPARE(file.readAll(), QByteArray("old"));
}

void InstallTest::downloadPaced()
{
    FakeHttpServer server;
    const QByteArray bytes(1000, 'p');
    server.on("/d/Emu.AppImage", {200, bytes});
    QTemporaryDir home;
    QNetworkAccessManager network;
    AppImageDownload download(&network, assetFor(server, "/d/Emu.AppImage", bytes), {}, home.filePath("Applications"));
    download.setRelaxed(true);
    download.setPace(100, 10);
    QSignalSpy progress(&download, &AppImageDownload::progress);
    const auto result = runDownload(download);
    QVERIFY2(result.ok, qPrintable(result.error));
    QVERIFY2(progress.size() >= 10, qPrintable(QString::number(progress.size())));
    QCOMPARE(QFileInfo(result.path).size(), qint64(bytes.size()));
}

void InstallTest::recordRoundTrip()
{
    QTemporaryDir state;
    const QString path = InstallRecords::path(state.filePath("lattis"));
    QCOMPARE(path, state.filePath("lattis/installs.toml"));
    QVERIFY(InstallRecords::read(path).ok);
    QVERIFY(InstallRecords::read(path).value.isEmpty());

    InstallRecords::Records records;
    records.insert("mgba", {"appimage", "/home/x/Applications/mGBA-0.10.5.appimage", "0.10.5", false, "", "12",
                            "2026-09-01T10:02:00Z", "2026-10-22"});
    records.insert("cemu", {"appimage", "/home/x/Applications/Cemu \"2.6\".AppImage", "v2.6", false, "abc", "3", "", "2026-10-21"});
    QString error;
    QVERIFY2(InstallRecords::write(path, records, &error), qPrintable(error));
    const auto back = InstallRecords::read(path);
    QVERIFY(back.ok);
    QCOMPARE(back.value, records);
    QVERIFY(back.warnings.isEmpty());
}

void InstallTest::recordRules()
{
    QTemporaryDir dir;
    QVERIFY(write(dir.filePath("Emu.AppImage"), "x"));
    InstallRecord app;
    app.route = "appimage";
    app.file = dir.filePath("Emu.AppImage");
    QVERIFY(InstallRecords::stillThere(app, {}));
    app.file = dir.filePath("moved.AppImage");
    QVERIFY(!InstallRecords::stillThere(app, {}));
    app.file = dir.path();
    QVERIFY(!InstallRecords::stillThere(app, {}));

    InstallRecord flatpak;
    flatpak.route = "flatpak";
    flatpak.file = "io.mgba.mGBA";
    QVERIFY(InstallRecords::stillThere(flatpak, {"io.mgba.mGBA"}));
    QVERIFY(!InstallRecords::stillThere(flatpak, {"org.other.App"}));
    flatpak.route = "tarball";
    QVERIFY(!InstallRecords::stillThere(flatpak, {"io.mgba.mGBA"}));
}

void InstallTest::recordBroken()
{
    const auto bad = InstallRecords::parse("[mgba\nroute = ");
    QVERIFY(!bad.ok);
    QVERIFY(!bad.warnings.isEmpty());
    const auto partial = InstallRecords::parse("[mgba]\nroute = \"appimage\"\n\n[cemu]\nfile = \"/x\"\n\nstray = 1\n");
    QVERIFY(partial.ok);
    QVERIFY(partial.value.isEmpty()); // each needs a route and a file
}

namespace {
struct InstallerHome
{
    QTemporaryDir root;
    QString home = root.filePath("home");
    QString state = root.filePath("state");
    QString cache = root.filePath("cache");
    QString fixtures = root.filePath("fixtures");
    QString trashDir = root.filePath("trash");
    QString appDir = home + "/.local/share/flatpak/app";
    int redetected = 0;
    QString running;
    QStringList trashed;
    QList<QStringList> folderWhenTrashed;
    QList<EmulatorDef> defs;

    InstallerHome()
    {
        EmulatorDef mgba;
        mgba.id = "mgba";
        mgba.name = "mGBA";
        mgba.flatpak = "io.mgba.mGBA";
        mgba.release = "github:mgba-emu/mgba";
        mgba.releaseAsset = "mGBA-*-appimage-x64.appimage";
        mgba.preset = true;
        EmulatorDef dolphin;
        dolphin.id = "dolphin";
        dolphin.name = "Dolphin";
        dolphin.preset = true;
        EmulatorDef duck;
        duck.id = "duckstation";
        duck.name = "DuckStation";
        duck.release = "github:stenzek/duckstation";
        duck.releaseAsset = "DuckStation-x64.AppImage";
        duck.preset = true;
        defs = {mgba, dolphin, duck};
        QDir().mkpath(fixtures);

        // install makes the app's folder, uninstall removes it; a "slow" file makes install wait; a "fail" file fails it.
        const QString script = root.filePath("flatpak.sh");
        write(script,
                      QByteArray("#!/bin/sh\necho \"$@\" >> '") + root.filePath("flatpak.log").toUtf8() + "'\n"
                      "case \"$1\" in\n"
                      "install)\n"
                      "  if [ -e '" + root.filePath("flatpak.fail").toUtf8() + "' ]; then echo 'error: Nothing matches' >&2; exit 1; fi\n"
                      "  if [ -e '" + root.filePath("flatpak.slow").toUtf8() + "' ]; then exec sleep 30; fi\n"
                      "  mkdir -p '" + appDir.toUtf8() + "'/\"$6\"/current;;\n"
                      "uninstall) rm -rf '" + appDir.toUtf8() + "'/\"$5\";;\n"
                      "esac\n");
        QFile::setPermissions(script, QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner);
        qputenv("LATTIS_FLATPAK", script.toUtf8());
    }

    ~InstallerHome() { qunsetenv("LATTIS_FLATPAK"); }

    QStringList flatpakLog() const
    {
        QFile file(root.filePath("flatpak.log"));
        if (!file.open(QIODevice::ReadOnly))
            return {};
        return QString::fromUtf8(file.readAll()).split(u'\n', Qt::SkipEmptyParts);
    }

    void flag(const char *name, bool on)
    {
        if (on)
            QVERIFY(write(root.filePath(name), "x"));
        else
            QFile::remove(root.filePath(name));
    }

    Installer::Setup setup()
    {
        return {state, cache, [this] { return defs; },
                [this] {
                    InstallLocations where;
                    where.home = home;
                    where.applicationsDirs = {home + "/Applications", home + "/AppImages"};
                    where.flatpakDirs = {appDir};
                    return where;
                },
                [this] { ++redetected; }, {},
                [this](const QString &path, QString *) {
                    trashed.append(QFileInfo(path).fileName());
                    folderWhenTrashed.append(names(home + "/Applications"));
                    QDir().mkpath(trashDir);
                    return QFile::rename(path, trashDir + "/" + QFileInfo(path).fileName());
                },
                [this](const QString &) { return running; }};
    }

    void release(const QByteArray &asset, bool withDigest = true, const QString &tag = "0.10.5",
                 const QString &name = "mGBA-0.10.5-appimage-x64.appimage", int id = 12,
                 const QString &date = "2026-09-01T10:02:00Z")
    {
        QJsonObject a{{"id", id},
                      {"name", name},
                      {"browser_download_url", "https://github.com/mgba-emu/mgba/releases/download/x/" + name},
                      {"size", asset.size()},
                      {"updated_at", date}};
        if (withDigest)
            a.insert("digest", "sha256:" + sha(asset));
        const QJsonObject root{{"tag_name", tag}, {"assets", QJsonArray{a}}};
        QVERIFY(write(fixtures + "/github-mgba-emu-mgba.json", QJsonDocument(root).toJson()));
        QVERIFY(write(fixtures + "/" + name, asset));
    }

    void duckRelease(const QByteArray &asset, int id)
    {
        const QJsonObject a{{"id", id},
                            {"name", "DuckStation-x64.AppImage"},
                            {"browser_download_url", "https://github.com/stenzek/duckstation/releases/download/latest/x"},
                            {"size", asset.size()},
                            {"digest", "sha256:" + sha(asset)},
                            {"updated_at", "2026-10-01T10:00:00Z"}};
        const QJsonObject root{{"tag_name", "latest"}, {"assets", QJsonArray{a}}};
        QVERIFY(write(fixtures + "/github-stenzek-duckstation.json", QJsonDocument(root).toJson()));
        QVERIFY(write(fixtures + "/DuckStation-x64.AppImage", asset));
    }
};
}

void InstallTest::installerAsksWithoutChecksum()
{
    InstallerHome h;
    const QByteArray asset = "#!/bin/sh\nsleep 600\n" + QByteArray(5000, '#');
    h.release(asset, false);
    Installer installer(h.setup());
    installer.setFixturesDir(h.fixtures);
    installer.setFixtureDelay(0);
    installer.setFixtureTick(0);
    const auto row = [&] { return installer.emulators().value("mgba").toMap(); };

    installer.install("mgba");
    QTRY_COMPARE_WITH_TIMEOUT(row().value("state").toString(), QStringLiteral("failed"), 5000);
    QVERIFY(row().value("unchecked").toBool());
    QCOMPARE(row().value("error").toString(), AppImageInstall::noChecksumText());
    QVERIFY(names(h.home + "/Applications").isEmpty());
    QVERIFY(InstallRecords::read(InstallRecords::path(h.state)).value.isEmpty());

    // Asking again asks again: the choice is for one download.
    installer.retry("mgba");
    QTRY_VERIFY_WITH_TIMEOUT(row().value("unchecked").toBool(), 5000);

    installer.installAnyway("mgba");
    QTRY_VERIFY_WITH_TIMEOUT(row().value("installedByApp").toBool(), 10000);
    QVERIFY(!row().value("unchecked").toBool());
    QCOMPARE(names(h.home + "/Applications"), QStringList{"mGBA-0.10.5-appimage-x64.appimage"});

    // Install anyway on a job that didn't ask does nothing.
    installer.installAnyway("mgba");
    QCOMPARE(row().value("state").toString(), QStringLiteral("idle"));
}

void InstallTest::installerInstalls()
{
    InstallerHome h;
    const QByteArray asset = "#!/bin/sh\nsleep 600\n" + QByteArray(5000, '#');
    h.release(asset);
    Installer installer(h.setup());
    installer.setFixturesDir(h.fixtures);
    installer.setFixtureDelay(0);
    installer.setFixtureTick(5);

    QVariantMap row = installer.emulators().value("mgba").toMap();
    QCOMPARE(row.value("state").toString(), QStringLiteral("idle"));
    QCOMPARE(row.value("routes").toStringList(), (QStringList{"appimage", "flatpak"}));
    QCOMPARE(row.value("source").toString(), QStringLiteral("GitHub"));
    QVERIFY(!row.value("installedByApp").toBool());
    QVERIFY2(!installer.emulators().contains("dolphin"), "an emulator with no route and no record has no row");

    QStringList states;
    QObject::connect(&installer, &Installer::emulatorsChanged, [&] {
        states.append(installer.emulators().value("mgba").toMap().value("state").toString());
    });
    installer.install("mgba");
    QTRY_VERIFY_WITH_TIMEOUT(installer.emulators().value("mgba").toMap().value("installedByApp").toBool(), 10000);

    row = installer.emulators().value("mgba").toMap();
    QCOMPARE(row.value("state").toString(), QStringLiteral("idle"));
    QCOMPARE(row.value("version").toString(), QStringLiteral("0.10.5"));
    QVERIFY(states.contains("checking") || states.contains("downloading"));
    QVERIFY(states.contains("downloading"));
    QCOMPARE(h.redetected, 1);

    const QString file = h.home + "/Applications/mGBA-0.10.5-appimage-x64.appimage";
    QVERIFY(QFileInfo(file).isExecutable());
    QCOMPARE(QFileInfo(file).size(), qint64(asset.size()));
    QCOMPARE(names(h.home + "/Applications"), QStringList{"mGBA-0.10.5-appimage-x64.appimage"});

    const auto records = InstallRecords::read(InstallRecords::path(h.state)).value;
    QCOMPARE(records.size(), 1);
    QCOMPARE(records.value("mgba").file, file);
    QCOMPARE(records.value("mgba").tag, QStringLiteral("0.10.5"));
    QCOMPARE(records.value("mgba").sha256, sha(asset));
    QCOMPARE(records.value("mgba").assetId, QStringLiteral("12"));
    QVERIFY(!records.value("mgba").installed.isEmpty());

    // The file moves away (Gear Lever): the app stops claiming it.
    QVERIFY(QFile::rename(file, h.home + "/Applications/moved.appimage"));
    installer.refresh();
    QVERIFY(!installer.emulators().value("mgba").toMap().value("installedByApp").toBool());
    QVERIFY(installer.emulators().contains("mgba"));
}

void InstallTest::installerFailsClearly()
{
    InstallerHome h;
    Installer installer(h.setup());
    installer.setFixturesDir(h.fixtures);
    installer.setFixtureDelay(0);
    installer.setFixtureTick(0);
    const auto state = [&] { return installer.emulators().value("mgba").toMap(); };

    // No canned release.
    installer.install("mgba");
    QTRY_COMPARE_WITH_TIMEOUT(state().value("state").toString(), QStringLiteral("failed"), 5000);
    QVERIFY(!state().value("error").toString().isEmpty());

    // Trying again runs the same install, which works once the release is there.
    h.release("the real file");
    QVERIFY(write(h.fixtures + "/mGBA-0.10.5-appimage-x64.appimage", "the real file"));
    installer.retry("mgba");
    QTRY_VERIFY_WITH_TIMEOUT(state().value("installedByApp").toBool(), 5000);
    QCOMPARE(h.redetected, 1);
    installer.uninstall("mgba");
    QTRY_VERIFY_WITH_TIMEOUT(!state().value("installedByApp").toBool(), 5000);
    h.redetected = 0;
    QFile::remove(InstallRecords::path(h.state));
    QFile::remove(h.fixtures + "/github-mgba-emu-mgba.json");
    QFile::remove(h.fixtures + "/mGBA-0.10.5-appimage-x64.appimage");

    // A release with no matching file.
    QVERIFY(write(h.fixtures + "/github-mgba-emu-mgba.json", json("{'tag_name': '1', 'assets': []}")));
    installer.install("mgba");
    QTRY_COMPARE_WITH_TIMEOUT(state().value("error").toString().contains("no download named"), true, 5000);

    // A checksum that doesn't match.
    h.release("the real file");
    QVERIFY(write(h.fixtures + "/mGBA-0.10.5-appimage-x64.appimage", "thE reAl fiLe"));
    installer.install("mgba");
    QTRY_COMPARE_WITH_TIMEOUT(state().value("error").toString().contains("checksum"), true, 5000);
    QVERIFY(!QFileInfo::exists(h.home + "/Applications") || names(h.home + "/Applications").isEmpty());

    // A route that isn't there, and one nobody asked for.
    installer.install("mgba", "tarball");
    QCOMPARE(state().value("state").toString(), QStringLiteral("failed"));
    installer.install("nothing");
    QVERIFY(!installer.emulators().contains("nothing"));
    QCOMPARE(h.redetected, 0);
    QVERIFY(!QFileInfo::exists(InstallRecords::path(h.state)));

    // No canned release answer for Dolphin's community builds.
    installer.install("dolphin");
    QTRY_COMPARE_WITH_TIMEOUT(installer.emulators().value("dolphin").toMap().value("state").toString(),
                              QStringLiteral("failed"), 5000);
}

void InstallTest::installerCancels()
{
    InstallerHome h;
    h.release(QByteArray(4000, 'c'));
    Installer installer(h.setup());
    installer.setFixturesDir(h.fixtures);
    installer.setFixtureDelay(0);
    installer.setFixtureTick(50);
    const auto state = [&] { return installer.emulators().value("mgba").toMap().value("state").toString(); };

    installer.install("mgba");
    QTRY_COMPARE_WITH_TIMEOUT(state(), QStringLiteral("downloading"), 5000);
    installer.cancel("mgba");
    QTRY_COMPARE_WITH_TIMEOUT(state(), QStringLiteral("idle"), 5000);
    QTest::qWait(300);
    QCOMPARE(state(), QStringLiteral("idle"));
    QVERIFY(names(h.home + "/Applications").isEmpty());
    QCOMPARE(h.redetected, 0);
    QVERIFY(!QFileInfo::exists(InstallRecords::path(h.state)));

    // Cancelled before the release answered.
    installer.setFixtureDelay(200);
    installer.install("mgba");
    QCOMPARE(state(), QStringLiteral("checking"));
    installer.cancel("mgba");
    QTest::qWait(400);
    QCOMPARE(state(), QStringLiteral("idle"));
    QVERIFY(names(h.home + "/Applications").isEmpty());

    // And it can install afterwards.
    installer.setFixtureDelay(0);
    installer.setFixtureTick(0);
    installer.install("mgba");
    QTRY_VERIFY_WITH_TIMEOUT(installer.emulators().value("mgba").toMap().value("installedByApp").toBool(), 5000);
}

void InstallTest::flatpakHelpers()
{
    QVERIFY(FlatpakInstall::validId("io.mgba.mGBA"));
    QVERIFY(FlatpakInstall::validId("org.DolphinEmu.dolphin-emu"));
    QVERIFY(!FlatpakInstall::validId(""));
    QVERIFY(!FlatpakInstall::validId("-rf"));
    QVERIFY(!FlatpakInstall::validId("a b"));
    QVERIFY(!FlatpakInstall::validId("a/b"));

    const auto steps = FlatpakInstall::installSteps("io.mgba.mGBA");
    QCOMPARE(steps.size(), 2);
    QCOMPARE(steps.at(0), (QStringList{"remote-add", "--user", "--if-not-exists", "flathub",
                                       "https://dl.flathub.org/repo/flathub.flatpakrepo"}));
    QCOMPARE(steps.at(1), (QStringList{"install", "--user", "--noninteractive", "-y", "flathub", "io.mgba.mGBA"}));
    QCOMPARE(FlatpakInstall::uninstallStep("io.mgba.mGBA"),
             (QStringList{"uninstall", "--user", "--noninteractive", "-y", "io.mgba.mGBA"}));

    QCOMPARE(FlatpakInstall::failureText("warning\nerror: no match\n\n", "out", 1), QStringLiteral("error: no match"));
    QCOMPARE(FlatpakInstall::failureText("", "Installing x\n", 1), QStringLiteral("Installing x"));
    QVERIFY(FlatpakInstall::failureText("", "", 3).contains(QStringLiteral("code 3")));

    qputenv("LATTIS_FLATPAK", "/somewhere/fake-flatpak");
    QCOMPARE(FlatpakInstall::program(), QStringLiteral("/somewhere/fake-flatpak"));
    qunsetenv("LATTIS_FLATPAK");
}

void InstallTest::flatpakJob()
{
    QTemporaryDir dir;
    const QString script = dir.filePath("flatpak.sh");
    const QString log = dir.filePath("log");
    QVERIFY(write(script, "#!/bin/sh\necho \"$@\" >> '" + log.toUtf8() + "'\n"
                          "if [ \"$1\" = install ]; then echo 'Installing...'; echo 'error: boom' >&2; exit 1; fi\n"));
    QFile::setPermissions(script, QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner);

    {
        FlatpakJob job(script, FlatpakInstall::installSteps("io.mgba.mGBA"), {});
        QSignalSpy done(&job, &FlatpakJob::finished);
        job.start();
        if (done.isEmpty())
            done.wait(5000);
        QVERIFY(!done.isEmpty());
        const auto result = done.first().first().value<FlatpakJob::Result>();
        QVERIFY(!result.ok);
        QVERIFY(!result.cancelled);
        QCOMPARE(result.error, QStringLiteral("error: boom"));
    }
    QFile file(log);
    QVERIFY(file.open(QIODevice::ReadOnly));
    QCOMPARE(file.readAll().split('\n').size(), 3); // both steps ran, in order

    {
        FlatpakJob job(script, {FlatpakInstall::uninstallStep("io.mgba.mGBA")}, {});
        QSignalSpy done(&job, &FlatpakJob::finished);
        job.start();
        if (done.isEmpty())
            done.wait(5000);
        QVERIFY(!done.isEmpty());
        QVERIFY(done.first().first().value<FlatpakJob::Result>().ok);
    }
    {
        FlatpakJob job(dir.filePath("missing"), {FlatpakInstall::uninstallStep("x")}, {});
        QSignalSpy done(&job, &FlatpakJob::finished);
        job.start();
        if (done.isEmpty())
            done.wait(5000);
        QVERIFY(!done.isEmpty());
        const auto result = done.first().first().value<FlatpakJob::Result>();
        QVERIFY(!result.ok);
        QVERIFY2(result.error.contains(QStringLiteral("Couldn't run flatpak")), qPrintable(result.error));
    }
}

void InstallTest::flatpakJobCancels()
{
    QTemporaryDir dir;
    const QString script = dir.filePath("slow.sh");
    QVERIFY(write(script, "#!/bin/sh\nexec sleep 30\n"));
    QFile::setPermissions(script, QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner);
    FlatpakJob job(script, FlatpakInstall::installSteps("io.mgba.mGBA"), {});
    QSignalSpy done(&job, &FlatpakJob::finished);
    job.start();
    QTest::qWait(150);
    job.cancel();
    if (done.isEmpty())
        done.wait(5000);
    QVERIFY(!done.isEmpty());
    QVERIFY(done.first().first().value<FlatpakJob::Result>().cancelled);
    QCOMPARE(done.size(), 1);
}

void InstallTest::installerFlatpak()
{
    InstallerHome h;
    Installer installer(h.setup());
    const auto row = [&] { return installer.emulators().value("mgba").toMap(); };
    QVERIFY(!row().value("noFlatpak").toBool());

    installer.install("mgba", "flatpak");
    QCOMPARE(row().value("state").toString(), QStringLiteral("installing"));
    QCOMPARE(row().value("progress").toDouble(), -1.0);
    QTRY_VERIFY_WITH_TIMEOUT(row().value("installedByApp").toBool(), 10000);
    QCOMPARE(row().value("state").toString(), QStringLiteral("idle"));
    QCOMPARE(row().value("route").toString(), QStringLiteral("flatpak"));
    QCOMPARE(row().value("file").toString(), QStringLiteral("io.mgba.mGBA"));
    QCOMPARE(h.redetected, 1);
    QCOMPARE(h.flatpakLog(), (QStringList{"remote-add --user --if-not-exists flathub https://dl.flathub.org/repo/flathub.flatpakrepo",
                                          "install --user --noninteractive -y flathub io.mgba.mGBA"}));
    const auto records = InstallRecords::read(InstallRecords::path(h.state)).value;
    QCOMPARE(records.value("mgba").route, QStringLiteral("flatpak"));
    QCOMPARE(records.value("mgba").file, QStringLiteral("io.mgba.mGBA"));

    installer.uninstall("mgba");
    QCOMPARE(row().value("state").toString(), QStringLiteral("removing"));
    QTRY_VERIFY_WITH_TIMEOUT(!row().value("installedByApp").toBool(), 10000);
    QCOMPARE(row().value("state").toString(), QStringLiteral("idle"));
    QCOMPARE(h.flatpakLog().last(), QStringLiteral("uninstall --user --noninteractive -y io.mgba.mGBA"));
    QVERIFY(InstallRecords::read(InstallRecords::path(h.state)).value.isEmpty());
    QVERIFY(!QFileInfo::exists(h.appDir + "/io.mgba.mGBA"));
    QCOMPARE(h.redetected, 2);
    QVERIFY(h.trashed.isEmpty());
}

void InstallTest::installerFlatpakFails()
{
    InstallerHome h;
    Installer installer(h.setup());
    const auto row = [&] { return installer.emulators().value("mgba").toMap(); };

    h.flag("flatpak.fail", true);
    installer.install("mgba", "flatpak");
    QTRY_COMPARE_WITH_TIMEOUT(row().value("state").toString(), QStringLiteral("failed"), 10000);
    QCOMPARE(row().value("error").toString(), QStringLiteral("error: Nothing matches"));
    QVERIFY(!QFileInfo::exists(InstallRecords::path(h.state)));
    QCOMPARE(h.redetected, 0);

    h.flag("flatpak.fail", false);
    h.flag("flatpak.slow", true);
    installer.install("mgba", "flatpak");
    QTest::qWait(300);
    QCOMPARE(row().value("state").toString(), QStringLiteral("installing"));
    installer.cancel("mgba");
    QTRY_COMPARE_WITH_TIMEOUT(row().value("state").toString(), QStringLiteral("idle"), 10000);
    QVERIFY(!row().value("installedByApp").toBool());
    QVERIFY(!QFileInfo::exists(InstallRecords::path(h.state)));
    QCOMPARE(h.redetected, 0);
}

void InstallTest::installerNoFlatpak()
{
    InstallerHome h;
    qunsetenv("LATTIS_FLATPAK");
    QTemporaryDir empty;
    const QByteArray path = qgetenv("PATH");
    qputenv("PATH", empty.path().toUtf8());
    Installer installer(h.setup());
    const QVariantMap row = installer.emulators().value("mgba").toMap();
    QVERIFY(row.value("noFlatpak").toBool());
    QCOMPARE(row.value("routes").toStringList(), QStringList{"appimage"});
    installer.install("mgba", "flatpak");
    const QVariantMap failed = installer.emulators().value("mgba").toMap();
    qputenv("PATH", path);
    QCOMPARE(failed.value("state").toString(), QStringLiteral("failed"));
    QVERIFY2(failed.value("error").toString().contains(QStringLiteral("isn't installed")),
             qPrintable(failed.value("error").toString()));
}

// The trash refused the first update's old build, so it sits in its aside folder: the next update must not remove it.
void InstallTest::twoUpdatesKeepBothOldBuilds()
{
    QTemporaryDir home;
    const QString folder = home.filePath("Applications");
    QVERIFY(write(folder + "/Emu.AppImage", "build one"));
    QNetworkAccessManager network;
    QStringList asides;
    for (const char *next : {"build two", "build three"}) {
        FakeHttpServer server;
        server.on("/d/Emu.AppImage", {200, next});
        AppImageDownload download(&network, assetFor(server, "/d/Emu.AppImage", next), {}, folder);
        download.setRelaxed(true);
        download.setReplace(true);
        const auto result = runDownload(download);
        QVERIFY2(result.ok, qPrintable(result.error));
        QVERIFY(!asides.contains(result.replaced));
        asides.append(result.replaced);
    }
    QCOMPARE(asides.at(0), folder + "/.Emu.AppImage.replaced/Emu.AppImage");
    QCOMPARE(asides.at(1), folder + "/.Emu.AppImage.replaced/Emu.AppImage.2");
    QFile first(asides.at(0));
    QVERIFY(first.open(QIODevice::ReadOnly));
    QCOMPARE(first.readAll(), QByteArray("build one"));
    QFile second(asides.at(1));
    QVERIFY(second.open(QIODevice::ReadOnly));
    QCOMPARE(second.readAll(), QByteArray("build two"));
    QCOMPARE(AppImageInstall::asidePath(folder, "Emu.AppImage"), folder + "/.Emu.AppImage.replaced/Emu.AppImage.3");
}

void InstallTest::aTrashThatRefusesIsTold()
{
    QTemporaryDir home;
    const QString folder = home.filePath("Applications");
    const QString aside = folder + "/.Emu.AppImage.replaced/Emu.AppImage";
    QVERIFY(write(aside, "old"));
    const auto refuses = [](const QString &, QString *error) {
        *error = QStringLiteral("no trash on this drive");
        return false;
    };
    const QString problem = AppImageInstall::trashOld(aside, true, refuses);
    QVERIFY2(problem.contains("no trash on this drive") && problem.contains(folder + "/.Emu.AppImage.replaced"),
             qPrintable(problem));
    QVERIFY(QFileInfo::exists(aside));
    QVERIFY(!AppImageInstall::trashOld(aside, true, {}).isEmpty());

    const auto accepts = [](const QString &path, QString *) { return QFile::remove(path); };
    QCOMPARE(AppImageInstall::trashOld(aside, true, accepts), QString());
    QVERIFY(!QFileInfo::exists(folder + "/.Emu.AppImage.replaced"));
}

void InstallTest::replacingKeepsTheOldUntilTheNewIsIn()
{
    FakeHttpServer server;
    server.on("/d/Emu.AppImage", {200, "new"});
    QTemporaryDir home;
    const QString folder = home.filePath("Applications");
    QVERIFY(write(folder + "/Emu.AppImage", "old"));
    QNetworkAccessManager network;
    AppImageDownload download(&network, assetFor(server, "/d/Emu.AppImage", "new"), {}, folder);
    download.setRelaxed(true);
    download.setReplace(true);
    const auto result = runDownload(download);
    QVERIFY2(result.ok, qPrintable(result.error));
    QCOMPARE(result.path, folder + "/Emu.AppImage");
    QCOMPARE(result.replaced, folder + "/.Emu.AppImage.replaced/Emu.AppImage");
    QFile now(result.path);
    QVERIFY(now.open(QIODevice::ReadOnly));
    QCOMPARE(now.readAll(), QByteArray("new"));
    QFile old(result.replaced);
    QVERIFY(old.open(QIODevice::ReadOnly));
    QCOMPARE(old.readAll(), QByteArray("old"));

    // A failed download leaves the old one where it was.
    FakeHttpServer broken;
    broken.on("/d/Emu.AppImage", {404, "no"});
    AppImageDownload failing(&network, assetFor(broken, "/d/Emu.AppImage", "new"), {}, folder);
    failing.setRelaxed(true);
    failing.setReplace(true);
    QVERIFY(!runDownload(failing).ok);
    QFile kept(folder + "/Emu.AppImage");
    QVERIFY(kept.open(QIODevice::ReadOnly));
    QCOMPARE(kept.readAll(), QByteArray("new"));
}

void InstallTest::installerChecksUpdates()
{
    InstallerHome h;
    h.release("the first build");
    Installer installer(h.setup());
    installer.setFixturesDir(h.fixtures);
    installer.setFixtureDelay(0);
    installer.setFixtureTick(0);
    const auto row = [&] { return installer.emulators().value("mgba").toMap(); };
    installer.install("mgba");
    QTRY_VERIFY_WITH_TIMEOUT(row().value("installedByApp").toBool(), 10000);

    // The same release: nothing newer.
    installer.checkUpdates("mgba");
    QCOMPARE(row().value("state").toString(), QStringLiteral("checking"));
    QTRY_COMPARE_WITH_TIMEOUT(row().value("state").toString(), QStringLiteral("idle"), 5000);
    QCOMPARE(row().value("updateVersion").toString(), QString());
    QVERIFY(row().value("upToDate").toBool());
    QCOMPARE(row().value("project").toString(), QStringLiteral("mgba-emu/mgba"));

    // A new build with a different checksum.
    h.release("the second build", true, "0.10.6", "mGBA-0.10.6-appimage-x64.appimage", 13, "2026-10-01T10:00:00Z");
    installer.checkUpdates();
    QTRY_COMPARE_WITH_TIMEOUT(row().value("updateVersion").toString(), QStringLiteral("0.10.6"), 5000);
    QVERIFY(!row().value("upToDate").toBool());
    QCOMPARE(row().value("version").toString(), QStringLiteral("0.10.5"));
    QCOMPARE(names(h.home + "/Applications"), QStringList{"mGBA-0.10.5-appimage-x64.appimage"});

    // The update list carries what the updates dialog shows.
    const QVariantList updates = installer.updates();
    QCOMPARE(updates.size(), 1);
    const QVariantMap update = updates.first().toMap();
    QCOMPARE(update.value("id").toString(), QStringLiteral("mgba"));
    QCOMPARE(update.value("version").toString(), QStringLiteral("0.10.6"));
    QCOMPARE(update.value("from").toString(), QStringLiteral("0.10.5"));
    QVERIFY(update.contains("notes"));
    QVERIFY(update.value("size").toDouble() > 0);

    // A quiet look at a release it can't read says nothing.
    QFile::remove(h.fixtures + "/github-mgba-emu-mgba.json");
    installer.checkUpdates();
    QTRY_COMPARE_WITH_TIMEOUT(row().value("state").toString(), QStringLiteral("idle"), 5000);
    QCOMPARE(row().value("error").toString(), QString());
    // Asked for, the same failure is shown.
    installer.checkUpdates("mgba");
    QTRY_COMPARE_WITH_TIMEOUT(row().value("state").toString(), QStringLiteral("failed"), 5000);
    QVERIFY(!row().value("error").toString().isEmpty());
    QVERIFY(row().value("installedByApp").toBool());

    // Only what Lattis installed is looked at.
    installer.cancel("mgba");
    installer.checkUpdates("dolphin");
    QVERIFY(!installer.emulators().contains("dolphin"));
}

void InstallTest::installerAskedCheckSaysWhenDone()
{
    InstallerHome h;
    h.release("the first build");
    Installer installer(h.setup());
    installer.setFixturesDir(h.fixtures);
    installer.setFixtureDelay(0);
    installer.setFixtureTick(0);
    QCOMPARE(installer.checkUpdatesAsked(), 0);
    installer.install("mgba");
    QTRY_VERIFY_WITH_TIMEOUT(installer.emulators().value("mgba").toMap().value("installedByApp").toBool(), 10000);

    QSignalSpy done(&installer, &Installer::updatesChecked);
    QCOMPARE(installer.checkUpdatesAsked(), 1);
    QTRY_COMPARE_WITH_TIMEOUT(done.count(), 1, 5000);
    QCOMPARE(done.takeFirst(), (QVariantList{1, 0}));

    QFile::remove(h.fixtures + "/github-mgba-emu-mgba.json");
    QCOMPARE(installer.checkUpdatesAsked(), 1);
    QTRY_COMPARE_WITH_TIMEOUT(done.count(), 1, 5000);
    QCOMPARE(done.takeFirst(), (QVariantList{1, 1}));
}

void InstallTest::installerChecksByFileForARollingTag()
{
    InstallerHome h;
    h.duckRelease("build one", 1);
    Installer installer(h.setup());
    installer.setFixturesDir(h.fixtures);
    installer.setFixtureDelay(0);
    installer.setFixtureTick(0);
    const auto row = [&] { return installer.emulators().value("duckstation").toMap(); };
    installer.install("duckstation");
    QTRY_VERIFY_WITH_TIMEOUT(row().value("installedByApp").toBool(), 10000);
    QCOMPARE(row().value("version").toString(), QStringLiteral("latest"));

    installer.checkUpdates();
    QTRY_COMPARE_WITH_TIMEOUT(row().value("state").toString(), QStringLiteral("idle"), 5000);
    QCOMPARE(row().value("updateVersion").toString(), QString());

    // The tag is the same; the file isn't.
    h.duckRelease("build two", 2);
    installer.checkUpdates();
    QTRY_COMPARE_WITH_TIMEOUT(row().value("updateVersion").toString(), QStringLiteral("latest"), 5000);

    // With no checksum, the asset's id and date decide (mGBA's release lists none).
    InstallerHome g;
    g.release("one", false);
    Installer plain(g.setup());
    plain.setFixturesDir(g.fixtures);
    plain.setFixtureDelay(0);
    plain.setFixtureTick(0);
    const auto mgba = [&] { return plain.emulators().value("mgba").toMap(); };
    plain.install("mgba");
    QTRY_VERIFY_WITH_TIMEOUT(mgba().value("unchecked").toBool(), 5000);
    plain.installAnyway("mgba");
    QTRY_VERIFY_WITH_TIMEOUT(mgba().value("installedByApp").toBool(), 10000);
    plain.checkUpdates();
    QTRY_COMPARE_WITH_TIMEOUT(mgba().value("state").toString(), QStringLiteral("idle"), 5000);
    QCOMPARE(mgba().value("updateVersion").toString(), QString());
    g.release("two", false, "0.10.5", "mGBA-0.10.5-appimage-x64.appimage", 12, "2026-09-02T10:00:00Z");
    plain.checkUpdates();
    QTRY_COMPARE_WITH_TIMEOUT(mgba().value("updateVersion").toString(), QStringLiteral("0.10.5"), 5000);
}

void InstallTest::installerUsesTheReleaseCache()
{
    InstallerHome h;
    h.release("the first build");
    {
        Installer first(h.setup());
        first.setFixturesDir(h.fixtures);
        first.setFixtureDelay(0);
        first.setFixtureTick(0);
        first.install("mgba");
        QTRY_VERIFY_WITH_TIMEOUT(first.emulators().value("mgba").toMap().value("installedByApp").toBool(), 10000);
    }

    // An answer less than an hour old is used without asking the network (which this test must not reach).
    const QByteArray cached = json("{'tag_name': '0.11.0', 'published_at': '2026-10-20T10:00:00Z', 'assets': [\n"
                                   "  {'id': 99, 'name': 'mGBA-0.11.0-appimage-x64.appimage', 'size': 5,\n"
                                   "   'browser_download_url': 'https://github.com/mgba-emu/mgba/releases/download/0.11.0/x',\n"
                                   "   'digest': 'sha256:ffff', 'updated_at': '2026-10-20T10:00:00Z'}]}");
    QVERIFY(write(h.cache + "/releases/github-mgba-emu-mgba.json", cached));
    Installer installer(h.setup());
    installer.checkUpdates("mgba");
    QTRY_COMPARE_WITH_TIMEOUT(installer.emulators().value("mgba").toMap().value("updateVersion").toString(),
                              QStringLiteral("0.11.0"), 5000);
}

void InstallTest::installerUpdates()
{
    InstallerHome h;
    h.release("the first build");
    Installer installer(h.setup());
    installer.setFixturesDir(h.fixtures);
    installer.setFixtureDelay(0);
    installer.setFixtureTick(0);
    const auto row = [&] { return installer.emulators().value("mgba").toMap(); };
    installer.install("mgba");
    QTRY_VERIFY_WITH_TIMEOUT(row().value("installedByApp").toBool(), 10000);

    h.release("the second build", true, "0.10.6", "mGBA-0.10.6-appimage-x64.appimage", 13, "2026-10-01T10:00:00Z");
    installer.checkUpdates();
    QTRY_COMPARE_WITH_TIMEOUT(row().value("updateVersion").toString(), QStringLiteral("0.10.6"), 5000);

    installer.update("mgba");
    QTRY_COMPARE_WITH_TIMEOUT(row().value("version").toString(), QStringLiteral("0.10.6"), 10000);
    QCOMPARE(row().value("state").toString(), QStringLiteral("idle"));
    QCOMPARE(row().value("updateVersion").toString(), QString());
    QCOMPARE(names(h.home + "/Applications"), QStringList{"mGBA-0.10.6-appimage-x64.appimage"});
    QCOMPARE(h.trashed, QStringList{"mGBA-0.10.5-appimage-x64.appimage"});
    // The new file was already in the folder when the old one went.
    QVERIFY(h.folderWhenTrashed.first().contains("mGBA-0.10.6-appimage-x64.appimage"));
    QCOMPARE(names(h.trashDir), QStringList{"mGBA-0.10.5-appimage-x64.appimage"});
    const auto record = InstallRecords::read(InstallRecords::path(h.state)).value.value("mgba");
    QCOMPARE(record.file, h.home + "/Applications/mGBA-0.10.6-appimage-x64.appimage");
    QCOMPARE(record.assetId, QStringLiteral("13"));
    QCOMPARE(h.redetected, 2);

    // A failing update changes nothing.
    h.release("the third build", true, "0.10.7", "mGBA-0.10.7-appimage-x64.appimage", 14, "2026-10-02T10:00:00Z");
    QVERIFY(write(h.fixtures + "/mGBA-0.10.7-appimage-x64.appimage", "thE thiRd buiLd"));
    installer.update("mgba");
    QTRY_COMPARE_WITH_TIMEOUT(row().value("state").toString(), QStringLiteral("failed"), 5000);
    QVERIFY(row().value("error").toString().contains("checksum"));
    QCOMPARE(names(h.home + "/Applications"), QStringList{"mGBA-0.10.6-appimage-x64.appimage"});
    QCOMPARE(h.trashed.size(), 1);
    QCOMPARE(row().value("version").toString(), QStringLiteral("0.10.6"));
}

void InstallTest::installerUpdatesInPlace()
{
    InstallerHome h;
    h.duckRelease("build one", 1);
    Installer installer(h.setup());
    installer.setFixturesDir(h.fixtures);
    installer.setFixtureDelay(0);
    installer.setFixtureTick(0);
    const auto row = [&] { return installer.emulators().value("duckstation").toMap(); };
    installer.install("duckstation");
    QTRY_VERIFY_WITH_TIMEOUT(row().value("installedByApp").toBool(), 10000);

    // The new file has the old one's name.
    h.duckRelease("build two", 2);
    installer.checkUpdates("duckstation");
    QTRY_COMPARE_WITH_TIMEOUT(row().value("updateVersion").toString(), QStringLiteral("latest"), 5000);
    installer.update("duckstation");
    QTRY_COMPARE_WITH_TIMEOUT(row().value("updateVersion").toString(), QString(), 10000);
    QCOMPARE(row().value("state").toString(), QStringLiteral("idle"));

    const QString file = h.home + "/Applications/DuckStation-x64.AppImage";
    QCOMPARE(names(h.home + "/Applications"), QStringList{"DuckStation-x64.AppImage"});
    QFile now(file);
    QVERIFY(now.open(QIODevice::ReadOnly));
    QCOMPARE(now.readAll(), QByteArray("build two"));
    QCOMPARE(h.trashed, QStringList{"DuckStation-x64.AppImage"});
    QVERIFY(h.folderWhenTrashed.first().contains("DuckStation-x64.AppImage"));
    QFile old(h.trashDir + "/DuckStation-x64.AppImage");
    QVERIFY(old.open(QIODevice::ReadOnly));
    QCOMPARE(old.readAll(), QByteArray("build one"));
    QCOMPARE(InstallRecords::read(InstallRecords::path(h.state)).value.value("duckstation").assetId, QStringLiteral("2"));
}

namespace {
struct GameUpdateHome : InstallerHome
{
    QString file = home + "/Games/hero/Hero.AppImage";
    QString runningTitle;

    GameUpdateHome() { QVERIFY(write(file, "build one")); }

    Installer::Setup gameSetup()
    {
        Installer::Setup s = setup();
        s.games = [this] {
            return QList<Installer::GameTarget>{{"hero", "Hero", file, "github:dev/hero", "Hero-*.AppImage.zsync", false}};
        };
        s.gameRunningTitle = [this](const QString &) { return runningTitle; };
        return s;
    }

    void gameRelease(const QByteArray &asset, bool withDigest = true, const QString &tag = "2.0", int id = 7,
                     const QString &name = "Hero-2.0.AppImage")
    {
        QJsonObject a{{"id", id},
                      {"name", name},
                      {"browser_download_url", "https://github.com/dev/hero/releases/download/x/" + name},
                      {"size", asset.size()},
                      {"updated_at", "2026-11-01T10:00:00Z"}};
        if (withDigest)
            a.insert("digest", "sha256:" + sha(asset));
        const QJsonObject body{{"tag_name", tag}, {"body", "Notes"}, {"assets", QJsonArray{a}}};
        QVERIFY(write(fixtures + "/github-dev-hero.json", QJsonDocument(body).toJson()));
        QVERIFY(write(fixtures + "/" + name, asset));
    }

    QByteArray current() const
    {
        QFile f(file);
        return f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray();
    }

    GameUpdateRecord record() const
    {
        return GameUpdateRecords::read(GameUpdateRecords::path(state)).value.value("hero");
    }
};
}

void InstallTest::gameUpdateRecords()
{
    QTemporaryDir dir;
    const QString path = GameUpdateRecords::path(dir.path());
    QVERIFY(path.endsWith("/game-updates.toml"));
    GameUpdateRecords::Records records;
    records.insert("hero", {"/g/Hero.AppImage", 1234, "abc", "7", "2026-11-01T10:00:00Z", "2.0", "2026-11-17"});
    QString error;
    QVERIFY2(GameUpdateRecords::write(path, records, &error), qPrintable(error));
    const auto read = GameUpdateRecords::read(path);
    QVERIFY(read.ok);
    QCOMPARE(read.value, records);
    QVERIFY(GameUpdateRecords::read(dir.filePath("none.toml")).value.isEmpty());
    QVERIFY(!GameUpdateRecords::parse("[hero\nfile = ").ok);
    QVERIFY(GameUpdateRecords::parse("[hero]\nsha256 = \"x\"\n").value.isEmpty()); // needs a file
}

void InstallTest::installerChecksGames()
{
    // Equal checksum: up to date, and the record learns the release.
    {
        GameUpdateHome h;
        h.gameRelease("build one");
        Installer installer(h.gameSetup());
        installer.setFixturesDir(h.fixtures);
        installer.setFixtureDelay(0);
        const auto row = [&] { return installer.games().value("hero").toMap(); };
        installer.checkGame("hero");
        QTRY_VERIFY_WITH_TIMEOUT(row().value("upToDate").toBool(), 5000);
        QVERIFY(installer.updates().isEmpty());
        QCOMPARE(row().value("match").toString(), QStringLiteral("Hero-2.0.AppImage"));
        QCOMPARE(h.record().sha256, sha("build one"));
        QCOMPARE(h.record().tag, QStringLiteral("2.0"));
        QCOMPARE(h.record().assetId, QStringLiteral("7"));
        QCOMPARE(h.record().file, h.file);
        QCOMPARE(h.record().size, qint64(9));
    }
    // A different checksum: an update, with the file's own hash recorded so the next check needs no hashing.
    {
        GameUpdateHome h;
        h.gameRelease("build two");
        Installer installer(h.gameSetup());
        installer.setFixturesDir(h.fixtures);
        installer.setFixtureDelay(0);
        installer.checkGame("hero");
        QTRY_COMPARE_WITH_TIMEOUT(installer.updates().size(), 1, 5000);
        const QVariantMap update = installer.updates().first().toMap();
        QCOMPARE(update.value("id").toString(), QStringLiteral("game:hero"));
        QCOMPARE(update.value("kind").toString(), QStringLiteral("game"));
        QCOMPARE(update.value("gameId").toString(), QStringLiteral("hero"));
        QCOMPARE(update.value("name").toString(), QStringLiteral("Hero"));
        QCOMPARE(update.value("version").toString(), QStringLiteral("2.0"));
        QCOMPARE(update.value("notes").toString(), QStringLiteral("Notes"));
        QCOMPARE(h.record().sha256, sha("build one"));
        QVERIFY(h.record().tag.isEmpty());
        QCOMPARE(h.current(), QByteArray("build one"));

        // The record answers now: the file can change on disk without the release being read as the same.
        installer.checkGame("hero");
        QTest::qWait(100);
        QCOMPARE(installer.updates().size(), 1);
    }
    // No digest in the release: the version is unknown, and an update is offered.
    {
        GameUpdateHome h;
        h.gameRelease("build one", false);
        Installer installer(h.gameSetup());
        installer.setFixturesDir(h.fixtures);
        installer.setFixtureDelay(0);
        installer.checkGame("hero");
        QTRY_COMPARE_WITH_TIMEOUT(installer.updates().size(), 1, 5000);
        QCOMPARE(installer.updates().first().toMap().value("from").toString(), QString());
        QVERIFY(h.record().file.isEmpty());
    }
    // A failure Check now left stays: the quiet check doesn't start over it.
    {
        GameUpdateHome h;
        h.gameRelease("build two");
        Installer installer(h.gameSetup());
        installer.setFixturesDir(QDir(h.fixtures).filePath(QStringLiteral("missing")));
        installer.setFixtureDelay(0);
        const auto state = [&] { return installer.games().value("hero").toMap().value("state").toString(); };
        installer.checkGame("hero");
        QTRY_COMPARE_WITH_TIMEOUT(state(), QStringLiteral("failed"), 5000);
        QCOMPARE(installer.checkUpdatesAsked(), 0);
        QTest::qWait(100);
        QCOMPARE(state(), QStringLiteral("failed"));
        QVERIFY(!Installer::gameUpdating(QStringLiteral("hero")));
    }
    // A game with no source, or whose file is gone, is skipped by the quiet check.
    {
        GameUpdateHome h;
        h.gameRelease("build two");
        QVERIFY(QFile::remove(h.file));
        Installer installer(h.gameSetup());
        installer.setFixturesDir(h.fixtures);
        installer.setFixtureDelay(0);
        QCOMPARE(installer.checkUpdatesAsked(), 0);
    }
}

void InstallTest::installerUpdatesGames()
{
    GameUpdateHome h;
    h.gameRelease("build two");
    Installer installer(h.gameSetup());
    installer.setFixturesDir(h.fixtures);
    installer.setFixtureDelay(0);
    installer.setFixtureTick(0);
    const auto row = [&] { return installer.games().value("hero").toMap(); };
    QSignalSpy updated(&installer, &Installer::emulatorUpdated);
    QCOMPARE(installer.checkUpdatesAsked(), 1);
    QTRY_COMPARE_WITH_TIMEOUT(installer.updates().size(), 1, 5000);

    // Replaced in place: same path and name, the old build in the trash, made executable.
    installer.updateGame("hero");
    QTRY_VERIFY_WITH_TIMEOUT(row().value("upToDate").toBool(), 10000);
    QCOMPARE(h.current(), QByteArray("build two"));
    QCOMPARE(names(h.home + "/Games/hero"), QStringList{"Hero.AppImage"});
    QVERIFY(QFileInfo(h.file).isExecutable());
    QCOMPARE(h.trashed, QStringList{"Hero.AppImage"});
    QFile old(h.trashDir + "/Hero.AppImage");
    QVERIFY(old.open(QIODevice::ReadOnly));
    QCOMPARE(old.readAll(), QByteArray("build one"));
    QVERIFY(installer.updates().isEmpty());
    QCOMPARE(row().value("version").toString(), QStringLiteral("2.0"));
    QCOMPARE(h.record().sha256, sha("build two"));
    QCOMPARE(h.record().assetId, QStringLiteral("7"));
    QCOMPARE(h.record().size, qint64(9));
    QCOMPARE(updated.size(), 1);
    QCOMPARE(updated.first().first().toString(), QStringLiteral("Hero"));
    QCOMPARE(updated.first().at(1).toString(), QStringLiteral("2.0"));
    QCOMPARE(h.redetected, 1);

    // The record now answers the next check without hashing: the same release is up to date, a newer one isn't.
    installer.checkGame("hero");
    QTRY_VERIFY_WITH_TIMEOUT(row().value("upToDate").toBool(), 5000);
    h.gameRelease("build three", true, "2.1", 8, "Hero-2.1.AppImage");
    installer.checkGame("hero");
    QTRY_COMPARE_WITH_TIMEOUT(row().value("updateVersion").toString(), QStringLiteral("2.1"), 5000);

    // A download that fails its checksum leaves the file and the record alone.
    QVERIFY(write(h.fixtures + "/Hero-2.1.AppImage", "bad bytes!!"));
    installer.updateGame("hero");
    QTRY_COMPARE_WITH_TIMEOUT(row().value("state").toString(), QStringLiteral("failed"), 5000);
    QVERIFY(row().value("error").toString().contains("checksum"));
    QCOMPARE(h.current(), QByteArray("build two"));
    QCOMPARE(names(h.home + "/Games/hero"), QStringList{"Hero.AppImage"});
    QCOMPARE(h.trashed.size(), 1);
    QCOMPARE(h.record().sha256, sha("build two"));
}

void InstallTest::installerRefusesGameUpdates()
{
    GameUpdateHome h;
    h.gameRelease("build two");
    Installer installer(h.gameSetup());
    installer.setFixturesDir(h.fixtures);
    installer.setFixtureDelay(0);
    installer.setFixtureTick(0);
    const auto row = [&] { return installer.games().value("hero").toMap(); };

    h.runningTitle = "Hero";
    installer.updateGame("hero");
    QCOMPARE(row().value("state").toString(), QStringLiteral("failed"));
    QCOMPARE(row().value("error").toString(), QStringLiteral("Close Hero first"));
    QCOMPARE(h.current(), QByteArray("build one"));
    installer.cancel("game:hero");
    h.runningTitle.clear();

    // Gear Lever keeps this file and its menu entry, and would put the old build back.
    QVERIFY(write(h.home + "/.local/share/applications/gearlever_hero.desktop",
                  "[Desktop Entry]\nExec=" + h.file.toUtf8() + "\n"));
    installer.updateGame("hero");
    QCOMPARE(row().value("state").toString(), QStringLiteral("failed"));
    QCOMPARE(row().value("error").toString(), QStringLiteral("Gear Lever manages this AppImage; update it there"));
    QCOMPARE(h.current(), QByteArray("build one"));
    QVERIFY(h.trashed.isEmpty());
}

void InstallTest::installerUninstalls()
{
    InstallerHome h;
    h.release("the build");
    Installer installer(h.setup());
    installer.setFixturesDir(h.fixtures);
    installer.setFixtureDelay(0);
    installer.setFixtureTick(0);
    const auto row = [&] { return installer.emulators().value("mgba").toMap(); };

    // Not installed by the app: refused, and nothing is touched.
    QVERIFY(write(h.home + "/Applications/mGBA-mine.appimage", "mine"));
    installer.uninstall("mgba");
    QCOMPARE(row().value("state").toString(), QStringLiteral("failed"));
    QVERIFY(row().value("error").toString().contains(QStringLiteral("wasn't installed by Lattis")));
    QVERIFY(h.trashed.isEmpty());
    installer.cancel("mgba");

    installer.install("mgba");
    QTRY_VERIFY_WITH_TIMEOUT(row().value("installedByApp").toBool(), 10000);
    installer.uninstall("mgba");
    QVERIFY(!row().value("installedByApp").toBool());
    QCOMPARE(row().value("state").toString(), QStringLiteral("idle"));
    QCOMPARE(h.trashed, QStringList{"mGBA-0.10.5-appimage-x64.appimage"});
    QCOMPARE(names(h.home + "/Applications"), QStringList{"mGBA-mine.appimage"});
    QVERIFY(InstallRecords::read(InstallRecords::path(h.state)).value.isEmpty());
    QCOMPARE(h.redetected, 2);
}

void InstallTest::installerRefusesWhileRunning()
{
    InstallerHome h;
    h.release("the build");
    Installer installer(h.setup());
    installer.setFixturesDir(h.fixtures);
    installer.setFixtureDelay(0);
    installer.setFixtureTick(0);
    const auto row = [&] { return installer.emulators().value("mgba").toMap(); };
    installer.install("mgba");
    QTRY_VERIFY_WITH_TIMEOUT(row().value("installedByApp").toBool(), 10000);

    h.running = "Mario Tennis Aces";
    installer.uninstall("mgba");
    QCOMPARE(row().value("state").toString(), QStringLiteral("failed"));
    QCOMPARE(row().value("error").toString(), QStringLiteral("Close Mario Tennis Aces first"));
    installer.cancel("mgba");

    h.release("newer", true, "0.10.6", "mGBA-0.10.6-appimage-x64.appimage", 13, "2026-10-01T10:00:00Z");
    installer.update("mgba");
    QCOMPARE(row().value("error").toString(), QStringLiteral("Close Mario Tennis Aces first"));
    QVERIFY(row().value("installedByApp").toBool());
    QCOMPARE(names(h.home + "/Applications"), QStringList{"mGBA-0.10.5-appimage-x64.appimage"});
    QVERIFY(h.trashed.isEmpty());
    installer.cancel("mgba");

    // Installing never conflicts, and a closed game lifts the refusal.
    h.running.clear();
    installer.uninstall("mgba");
    QVERIFY(!row().value("installedByApp").toBool());
}


namespace {
struct TarEntry
{
    QString name;
    QByteArray data;
    QString link;
};

QByteArray makeTarXz(const QList<TarEntry> &entries)
{
    QTemporaryDir dir;
    const QString path = dir.filePath("ff.tar.xz");
    struct archive *a = archive_write_new();
    archive_write_set_format_pax_restricted(a);
    archive_write_add_filter_xz(a);
    archive_write_open_filename(a, QFile::encodeName(path).constData());
    for (const TarEntry &e : entries) {
        struct archive_entry *entry = archive_entry_new();
        archive_entry_set_pathname(entry, e.name.toUtf8().constData());
        archive_entry_set_perm(entry, 0755);
        if (!e.link.isEmpty()) {
            archive_entry_set_filetype(entry, AE_IFLNK);
            archive_entry_set_symlink(entry, e.link.toUtf8().constData());
        } else {
            archive_entry_set_filetype(entry, AE_IFREG);
            archive_entry_set_size(entry, e.data.size());
        }
        archive_write_header(a, entry);
        if (e.link.isEmpty())
            archive_write_data(a, e.data.constData(), size_t(e.data.size()));
        archive_entry_free(entry);
    }
    archive_write_close(a);
    archive_write_free(a);
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}

const QString kFfmpegAsset = QStringLiteral("ffmpeg-master-latest-linux64-lgpl-shared.tar.xz");
const QByteArray kGoodFfmpeg = "#!/bin/sh\necho 'ffmpeg version test'\n";

QList<TarEntry> ffmpegArchive(const QByteArray &program = kGoodFfmpeg)
{
    const QString top = QStringLiteral("ffmpeg-master-latest-linux64-lgpl-shared/");
    return {{top + "bin/ffmpeg", program, {}},
            {top + "bin/ffplay", "not kept", {}},
            {top + "lib/libavutil.so.61.20.100", "lib", {}},
            {top + "lib/libavutil.so.61", {}, "libavutil.so.61.20.100"},
            {top + "lib/libavutil.so", {}, "libavutil.so.61"},
            {top + "include/ffmpeg.h", "not kept", {}},
            {top + "LICENSE.txt", "LGPL", {}}};
}

struct FfmpegHome
{
    FakeHttpServer server;
    QTemporaryDir dir;
    QString tools = dir.filePath("state/tools");

    // The release lists the archive with `listedSha` (the archive's own when empty).
    void release(const QByteArray &archive, const QString &listedSha = {}, const QString &asset = kFfmpegAsset)
    {
        QJsonObject item{{"id", 1},
                         {"name", asset},
                         {"browser_download_url", server.url("/d/ff.tar.xz").toString()},
                         {"size", archive.size()},
                         {"digest", "sha256:" + (listedSha.isEmpty() ? sha(archive) : listedSha)},
                         {"updated_at", "2026-10-26T00:00:00Z"}};
        const QJsonObject release{{"tag_name", "latest"}, {"published_at", "2026-10-26T00:00:00Z"},
                                  {"assets", QJsonArray{item}}};
        server.clear("/release");
        server.on("/release", {200, QJsonDocument(release).toJson()});
        server.clear("/d/ff.tar.xz");
        server.on("/d/ff.tar.xz", {200, archive});
    }

    void setUp(FfmpegTool &tool)
    {
        tool.setReleaseUrl(server.url("/release"));
        tool.setRelaxed(true);
    }
};

struct PathOverride
{
    explicit PathOverride(const QByteArray &path) : saved(qgetenv("PATH")) { qputenv("PATH", path); }
    ~PathOverride() { qputenv("PATH", saved); }
    QByteArray saved;
};

bool busy(const FfmpegTool &tool)
{
    return tool.state() == QLatin1String("checking") || tool.state() == QLatin1String("downloading")
           || tool.state() == QLatin1String("installing");
}
}

void InstallTest::ffmpegAssetNames()
{
    QCOMPARE(FfmpegInstall::assetName("x86_64"), QStringLiteral("ffmpeg-master-latest-linux64-lgpl-shared.tar.xz"));
    QCOMPARE(FfmpegInstall::assetName("arm64"), QStringLiteral("ffmpeg-master-latest-linuxarm64-lgpl-shared.tar.xz"));
    QCOMPARE(FfmpegInstall::assetName("aarch64"), FfmpegInstall::assetName("arm64"));
    QVERIFY(FfmpegInstall::assetName("riscv64").isEmpty());
}

void InstallTest::ffmpegInstalls()
{
    PathOverride path("/nonexistent");
    FfmpegHome h;
    h.release(makeTarXz(ffmpegArchive()));
    FfmpegTool tool({h.tools, {}});
    h.setUp(tool);
    QVERIFY(!tool.available());
    QVERIFY(!tool.ownCopy());

    QStringList states;
    QObject::connect(&tool, &FfmpegTool::changed, [&] {
        if (states.isEmpty() || states.last() != tool.state())
            states.append(tool.state());
    });
    tool.install();
    QCOMPARE(tool.state(), QStringLiteral("checking"));
    QTRY_VERIFY_WITH_TIMEOUT(!busy(tool), 10000);

    QVERIFY2(tool.error().isEmpty(), qPrintable(tool.error()));
    QCOMPARE(tool.state(), QStringLiteral("idle"));
    QStringList expected{"checking", "downloading", "installing", "idle"};
    QCOMPARE(states, expected);
    QVERIFY(tool.ownCopy());
    QCOMPARE(tool.path(), h.tools + "/ffmpeg/bin/ffmpeg");
    QVERIFY(QFileInfo(tool.path()).isExecutable());
    QCOMPARE(names(h.tools), QStringList{"ffmpeg"});
    QCOMPARE(names(h.tools + "/ffmpeg"), (QStringList{"LICENSE.txt", "bin", "lib"}));
    QCOMPARE(names(h.tools + "/ffmpeg/bin"), QStringList{"ffmpeg"});
    QCOMPARE(names(h.tools + "/ffmpeg/lib"), (QStringList{"libavutil.so.61", "libavutil.so.61.20.100"}));
    QVERIFY(QFileInfo(h.tools + "/ffmpeg/lib/libavutil.so.61").isSymLink());
    QCOMPARE(QFileInfo(h.tools + "/ffmpeg/lib/libavutil.so.61").symLinkTarget(),
             h.tools + "/ffmpeg/lib/libavutil.so.61.20.100");
}

void InstallTest::ffmpegRefusesWrongChecksum()
{
    PathOverride path("/nonexistent");
    FfmpegHome h;
    h.release(makeTarXz(ffmpegArchive()), QString(64, 'a'));
    FfmpegTool tool({h.tools, {}});
    h.setUp(tool);
    tool.install();
    QTRY_VERIFY_WITH_TIMEOUT(!busy(tool), 10000);

    QCOMPARE(tool.state(), QStringLiteral("failed"));
    QVERIFY2(tool.error().contains(QStringLiteral("checksum")), qPrintable(tool.error()));
    QVERIFY(!tool.available());
    QVERIFY(names(h.tools).isEmpty());

    // The button comes back: a second try with the right checksum works.
    h.release(makeTarXz(ffmpegArchive()));
    tool.install();
    QTRY_VERIFY_WITH_TIMEOUT(!busy(tool), 10000);
    QCOMPARE(tool.state(), QStringLiteral("idle"));
    QVERIFY(tool.available());
    QVERIFY(tool.error().isEmpty());
}

void InstallTest::ffmpegRefusesBadReleases()
{
    PathOverride path("/nonexistent");
    FfmpegHome h;
    FfmpegTool tool({h.tools, {}});
    h.setUp(tool);

    // No answer at all.
    tool.install();
    QTRY_VERIFY_WITH_TIMEOUT(!busy(tool), 10000);
    QCOMPARE(tool.state(), QStringLiteral("failed"));

    // The release has no asset for this CPU.
    h.release(makeTarXz(ffmpegArchive()), {}, "ffmpeg-n9.0-latest-linux64-lgpl-shared-9.0.tar.xz");
    tool.install();
    QTRY_VERIFY_WITH_TIMEOUT(!busy(tool), 10000);
    QCOMPARE(tool.state(), QStringLiteral("failed"));
    QVERIFY2(tool.error().contains(kFfmpegAsset), qPrintable(tool.error()));

    // No checksum listed: a program isn't run on trust.
    QJsonObject item{{"id", 1}, {"name", kFfmpegAsset}, {"browser_download_url", h.server.url("/d/ff.tar.xz").toString()},
                     {"size", 10}, {"digest", QJsonValue::Null}};
    h.server.clear("/release");
    h.server.on("/release", {200, QJsonDocument(QJsonObject{{"tag_name", "latest"}, {"assets", QJsonArray{item}}}).toJson()});
    tool.install();
    QTRY_VERIFY_WITH_TIMEOUT(!busy(tool), 10000);
    QCOMPARE(tool.state(), QStringLiteral("failed"));
    QVERIFY2(tool.error().contains(QStringLiteral("checksum")), qPrintable(tool.error()));
    QVERIFY(names(h.tools).isEmpty());
}

void InstallTest::ffmpegRefusesBadArchives()
{
    PathOverride path("/nonexistent");
    FfmpegHome h;
    FfmpegTool tool({h.tools, {}});
    h.setUp(tool);

    h.release(makeTarXz({{"top/../../evil", "x", {}}}));
    tool.install();
    QTRY_VERIFY_WITH_TIMEOUT(!busy(tool), 10000);
    QCOMPARE(tool.state(), QStringLiteral("failed"));
    QVERIFY2(tool.error().contains(QStringLiteral("outside")), qPrintable(tool.error()));
    QVERIFY(names(h.tools).isEmpty());

    // A link out of the library folder.
    h.release(makeTarXz({{"top/bin/ffmpeg", kGoodFfmpeg, {}}, {"top/lib/libx.so.1", {}, "/etc/passwd"}}));
    tool.install();
    QTRY_VERIFY_WITH_TIMEOUT(!busy(tool), 10000);
    QCOMPARE(tool.state(), QStringLiteral("failed"));
    QVERIFY2(tool.error().contains(QStringLiteral("link")), qPrintable(tool.error()));
    QVERIFY(names(h.tools).isEmpty());

    // No program in it.
    h.release(makeTarXz({{"top/LICENSE.txt", "LGPL", {}}}));
    tool.install();
    QTRY_VERIFY_WITH_TIMEOUT(!busy(tool), 10000);
    QCOMPARE(tool.state(), QStringLiteral("failed"));
    QVERIFY2(tool.error().contains(QStringLiteral("bin/ffmpeg")), qPrintable(tool.error()));
    QVERIFY(names(h.tools).isEmpty());
}

void InstallTest::ffmpegKeepsTheOldCopyWhenTheNewOneWontStart()
{
    PathOverride path("/nonexistent");
    FfmpegHome h;
    h.release(makeTarXz(ffmpegArchive()));
    FfmpegTool tool({h.tools, {}});
    h.setUp(tool);
    tool.install();
    QTRY_VERIFY_WITH_TIMEOUT(!busy(tool), 10000);
    QVERIFY(tool.ownCopy());

    h.release(makeTarXz(ffmpegArchive("#!/bin/sh\necho 'no such library' >&2\nexit 127\n")));
    tool.install();
    QTRY_VERIFY_WITH_TIMEOUT(!busy(tool), 10000);
    QCOMPARE(tool.state(), QStringLiteral("failed"));
    QVERIFY2(tool.error().contains(QStringLiteral("doesn't start")), qPrintable(tool.error()));
    QVERIFY2(tool.error().contains(QStringLiteral("no such library")), qPrintable(tool.error()));
    QVERIFY(tool.ownCopy());
    QFile program(tool.path());
    QVERIFY(program.open(QIODevice::ReadOnly));
    QCOMPARE(program.readAll(), kGoodFfmpeg);
    QCOMPARE(names(h.tools), QStringList{"ffmpeg"});
}

void InstallTest::ffmpegCancelLeavesNothing()
{
    PathOverride path("/nonexistent");
    FfmpegHome h;
    const QByteArray archive = makeTarXz(ffmpegArchive());
    FfmpegTool tool({h.tools, {}});
    h.setUp(tool);

    // While the release is being asked for.
    h.release(archive);
    h.server.clear("/release");
    h.server.on("/release", {200, "{}", 5000});
    tool.install();
    QCOMPARE(tool.state(), QStringLiteral("checking"));
    tool.cancel();
    QCOMPARE(tool.state(), QStringLiteral("idle"));
    QVERIFY(tool.error().isEmpty());

    // While the archive is downloading.
    h.release(archive);
    h.server.clear("/d/ff.tar.xz");
    h.server.on("/d/ff.tar.xz", {200, archive, 5000});
    tool.install();
    QTRY_COMPARE_WITH_TIMEOUT(tool.state(), QStringLiteral("downloading"), 10000);
    QTRY_VERIFY_WITH_TIMEOUT(!names(h.tools).isEmpty(), 5000);
    tool.cancel();
    QCOMPARE(tool.state(), QStringLiteral("idle"));
    QVERIFY(names(h.tools).isEmpty());
    QVERIFY(!tool.available());

    // A failure is cleared by cancel() too.
    h.release(archive, QString(64, 'b'));
    tool.install();
    QTRY_VERIFY_WITH_TIMEOUT(!busy(tool), 10000);
    QCOMPARE(tool.state(), QStringLiteral("failed"));
    tool.cancel();
    QCOMPARE(tool.state(), QStringLiteral("idle"));
    QVERIFY(tool.error().isEmpty());
}

void InstallTest::ffmpegRemoves()
{
    PathOverride path("/nonexistent");
    FfmpegHome h;
    h.release(makeTarXz(ffmpegArchive()));
    FfmpegTool tool({h.tools, {}});
    h.setUp(tool);
    tool.install();
    QTRY_VERIFY_WITH_TIMEOUT(!busy(tool), 10000);
    QVERIFY(tool.ownCopy());

    tool.remove();
    QCOMPARE(tool.state(), QStringLiteral("idle"));
    QVERIFY(!tool.ownCopy());
    QVERIFY(!tool.available());
    QVERIFY(!QFileInfo::exists(h.tools + "/ffmpeg"));
}

void InstallTest::ffmpegPrefersTheOwnCopy()
{
    FfmpegHome h;
    const QString bin = h.dir.filePath("bin");
    QVERIFY(write(bin + "/ffmpeg", kGoodFfmpeg));
    QVERIFY(QFile::setPermissions(bin + "/ffmpeg", QFileDevice::ReadOwner | QFileDevice::ExeOwner));
    PathOverride path(bin.toUtf8());

    QCOMPARE(FfmpegTool::resolve(h.tools), bin + "/ffmpeg");
    FfmpegTool tool({h.tools, {}});
    QVERIFY(tool.available());
    QVERIFY(!tool.ownCopy());
    QCOMPARE(tool.path(), bin + "/ffmpeg");

    h.release(makeTarXz(ffmpegArchive()));
    h.setUp(tool);
    tool.install();
    QTRY_VERIFY_WITH_TIMEOUT(!busy(tool), 10000);
    QVERIFY(tool.ownCopy());
    QCOMPARE(tool.path(), h.tools + "/ffmpeg/bin/ffmpeg");
    QCOMPARE(FfmpegTool::resolve(h.tools), h.tools + "/ffmpeg/bin/ffmpeg");

    tool.remove();
    QCOMPARE(tool.path(), bin + "/ffmpeg");
}

void InstallTest::versionCompare_data()
{
    QTest::addColumn<QString>("candidate");
    QTest::addColumn<QString>("current");
    QTest::addColumn<bool>("newer");
    QTest::newRow("patch") << "1.1.2" << "1.1.1" << true;
    QTest::newRow("tag prefix") << "v1.2.0" << "1.1.1" << true;
    QTest::newRow("numbers, not text") << "1.10" << "1.9" << true;
    QTest::newRow("shorter is padded") << "1.2" << "1.2.0" << false;
    QTest::newRow("longer is padded") << "1.2.0.1" << "1.2" << true;
    QTest::newRow("equal") << "1.1.1" << "1.1.1" << false;
    QTest::newRow("older") << "1.1.0" << "1.1.1" << false;
    QTest::newRow("older major") << "0.99" << "1.0" << false;
    QTest::newRow("bad tag") << "latest" << "1.1.1" << false;
    QTest::newRow("prerelease tag") << "1.2.0-rc1" << "1.1.1" << false;
    QTest::newRow("empty tag") << "" << "1.1.1" << false;
    QTest::newRow("bad current") << "1.2" << "dev" << false;
}

void InstallTest::versionCompare()
{
    QFETCH(QString, candidate);
    QFETCH(QString, current);
    QFETCH(bool, newer);
    QCOMPARE(Versions::newer(candidate, current), newer);
}

void InstallTest::releaseNotes()
{
    QString error;
    const auto source = ReleaseSources::parse("github:o/r");
    const auto release = ReleaseSources::parseRelease(*source, json("{'tag_name': 'v1', 'body': '## New\\n- one', 'assets': []}"),
                                                      &error);
    QVERIFY(release);
    QCOMPARE(release->notes, QStringLiteral("## New\n- one"));
}

void InstallTest::downloadTargetName()
{
    FakeHttpServer server;
    server.on("/d/Lattis-1.2.0-x86_64.AppImage", {200, "new"});
    QTemporaryDir home;
    const QString folder = home.filePath("apps");
    QVERIFY(write(folder + "/lattis.appimage", "old"));
    QNetworkAccessManager network;
    AppImageDownload download(&network, assetFor(server, "/d/Lattis-1.2.0-x86_64.AppImage", "new"), {}, folder);
    download.setRelaxed(true);
    download.setReplace(true);
    download.setTargetName("lattis.appimage");
    const auto result = runDownload(download);
    QVERIFY2(result.ok, qPrintable(result.error));
    QCOMPARE(result.path, folder + "/lattis.appimage");
    QCOMPARE(result.replaced, folder + "/.lattis.appimage.replaced/lattis.appimage");
    QCOMPARE(names(folder), (QStringList{".lattis.appimage.replaced", "lattis.appimage"}));
    QFile old(result.replaced);
    QVERIFY(old.open(QIODevice::ReadOnly));
    QCOMPARE(old.readAll(), QByteArray("old"));
    QFile now(folder + "/lattis.appimage");
    QVERIFY(now.open(QIODevice::ReadOnly));
    QCOMPARE(now.readAll(), QByteArray("new"));
}

namespace {
const QString kUpdateAsset = QStringLiteral("Lattis-1.2.0-x86_64.AppImage");

struct UpdateHome
{
    FakeHttpServer server;
    QTemporaryDir dir;
    QString folder = dir.filePath("apps");
    QString appImage = folder + "/lattis.appimage";
    QString state = dir.filePath("state");
    QString cache = dir.filePath("cache");
    QString applications = dir.filePath("applications");
    QStringList trashed;
    QStringList skipped;
    QString game;
    QByteArray releaseJson;
    int restarts = 0;
    bool restartWorks = true;
    bool enabled = true;
    bool autoCheck = true;

    UpdateHome() { write(appImage, "old"); }

    // The release lists the AppImage with `listedSha` (its own when empty); `digest` false lists none.
    void release(const QByteArray &image, const QString &tag = QStringLiteral("v1.2.0"), bool digest = true,
                 const QString &listedSha = {})
    {
        QJsonObject item{{"id", 1},
                         {"name", kUpdateAsset},
                         {"browser_download_url", server.url("/d/lattis").toString()},
                         {"size", image.size()},
                         {"updated_at", "2026-10-29T00:00:00Z"}};
        if (digest)
            item["digest"] = "sha256:" + (listedSha.isEmpty() ? sha(image) : listedSha);
        const QJsonObject zsync{{"id", 2},
                                {"name", kUpdateAsset + ".zsync"},
                                {"browser_download_url", server.url("/d/zsync").toString()},
                                {"size", 3}};
        const QJsonObject release{{"tag_name", tag},
                                  {"published_at", "2026-10-29T00:00:00Z"},
                                  {"body", "## What's new\n- a thing"},
                                  {"assets", QJsonArray{zsync, item}}};
        releaseJson = QJsonDocument(release).toJson();
        server.clear("/release");
        server.on("/release", {200, releaseJson});
        server.clear("/d/lattis");
        server.on("/d/lattis", {200, image});
    }

    AppUpdate::Setup setup()
    {
        AppUpdate::Setup s;
        s.stateDir = state;
        s.cacheDir = cache;
        s.enabled = enabled;
        s.appImage = appImage;
        s.applicationsDir = applications;
        s.autoCheck = [this] { return autoCheck; };
        s.skippedVersion = [this] { return skipped.value(skipped.size() - 1); };
        s.setSkippedVersion = [this](const QString &v) { skipped.append(v); };
        s.trash = [this](const QString &path, QString *) {
            trashed.append(path);
            return QFile::remove(path);
        };
        s.runningGame = [this] { return game; };
        s.restart = [this](QString *error) {
            ++restarts;
            if (!restartWorks)
                *error = QStringLiteral("no");
            return restartWorks;
        };
        return s;
    }

    void use(AppUpdate &update)
    {
        update.setReleaseUrl(server.url("/release"));
        update.setRelaxed(true);
        update.setCurrentVersion("1.1.1");
    }
};

bool busy(const AppUpdate &update)
{
    return update.state() == QLatin1String("checking") || update.state() == QLatin1String("downloading");
}

QByteArray fileText(const QString &path)
{
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}
}

void InstallTest::appUpdateFindsANewerRelease()
{
    UpdateHome h;
    h.release("new image");
    AppUpdate update(h.setup());
    h.use(update);
    QVERIFY(update.available());
    QVERIFY(update.canReplace());
    QVERIFY(!update.hasUpdate());
    QVERIFY(update.latestVersion().isEmpty());

    update.check();
    QCOMPARE(update.state(), QStringLiteral("checking"));
    QTRY_VERIFY_WITH_TIMEOUT(!busy(update), 10000);
    QVERIFY2(update.error().isEmpty(), qPrintable(update.error()));
    QCOMPARE(update.state(), QStringLiteral("idle"));
    QVERIFY(update.hasUpdate());
    QCOMPARE(update.latestVersion(), QStringLiteral("1.2.0"));
    QCOMPARE(update.size(), qint64(9));
    QVERIFY(update.notes().contains(QStringLiteral("a thing")));
    QCOMPARE(update.releasePage(), QUrl("https://github.com/mines-cmd/lattis/releases/tag/v1.2.0"));
    QVERIFY(!update.skipped());
    QVERIFY(QFileInfo(h.state + "/app-update.toml").isFile());
    QCOMPARE(fileText(h.appImage), QByteArray("old"));
}

void InstallTest::appUpdateIgnoresOlderAndEqual()
{
    UpdateHome h;
    AppUpdate update(h.setup());
    h.use(update);
    for (const QString &tag : {QStringLiteral("v1.1.1"), QStringLiteral("v1.1.0"), QStringLiteral("nightly")}) {
        h.release("img", tag);
        update.check();
        QTRY_VERIFY_WITH_TIMEOUT(!busy(update), 10000);
        QVERIFY(!update.hasUpdate());
        QVERIFY(update.latestVersion().isEmpty());
    }
    // An API error is a failure; a quiet check only logs it.
    h.server.clear("/release");
    h.server.on("/release", {200, json("{'message': 'API rate limit exceeded'}")});
    update.check();
    QTRY_VERIFY_WITH_TIMEOUT(!busy(update), 10000);
    QCOMPARE(update.state(), QStringLiteral("failed"));
    QVERIFY(update.error().contains(QStringLiteral("rate limit")));
    update.cancel();
    QCOMPARE(update.state(), QStringLiteral("idle"));
    update.check(true);
    QTRY_VERIFY_WITH_TIMEOUT(!busy(update), 10000);
    QCOMPARE(update.state(), QStringLiteral("idle"));
    QVERIFY(update.error().isEmpty());
}

void InstallTest::appUpdateUnavailableDoesNothing()
{
    UpdateHome h;
    h.release("new image");
    h.enabled = false;
    AppUpdate update(h.setup());
    h.use(update);
    QVERIFY(!update.available());
    QVERIFY(!update.canReplace());
    update.check();
    QCOMPARE(update.state(), QStringLiteral("idle"));
    update.startDailyCheck(0);
    QTest::qWait(100);
    update.update();
    QCOMPARE(update.state(), QStringLiteral("idle"));
    QVERIFY(h.server.requests.isEmpty());

    // Not an AppImage either.
    UpdateHome other;
    other.appImage.clear();
    AppUpdate plain(other.setup());
    QVERIFY(!plain.available());
}

void InstallTest::appUpdateRefusesWithoutChecksum()
{
    UpdateHome h;
    h.release("new image", "v1.2.0", false);
    AppUpdate update(h.setup());
    h.use(update);
    update.check();
    QTRY_VERIFY_WITH_TIMEOUT(!busy(update), 10000);
    QVERIFY(update.hasUpdate());
    QVERIFY(update.noChecksum()); // the dialog says so before Update is clicked
    update.update();
    QCOMPARE(update.state(), QStringLiteral("failed"));
    QVERIFY2(update.error().contains(QStringLiteral("checksum")), qPrintable(update.error()));
    QCOMPARE(fileText(h.appImage), QByteArray("old"));
    QVERIFY(h.server.requestsTo("/d/lattis").isEmpty());
    QCOMPARE(names(h.folder), QStringList{"lattis.appimage"});
}

void InstallTest::appUpdateWrongChecksumKeepsTheOldFile()
{
    UpdateHome h;
    h.release("new image", "v1.2.0", true, QString(64, 'a'));
    AppUpdate update(h.setup());
    h.use(update);
    update.check();
    QTRY_VERIFY_WITH_TIMEOUT(!busy(update), 10000);
    update.update();
    QCOMPARE(update.state(), QStringLiteral("downloading"));
    QTRY_VERIFY_WITH_TIMEOUT(!busy(update), 10000);
    QCOMPARE(update.state(), QStringLiteral("failed"));
    QVERIFY2(update.error().contains(QStringLiteral("checksum")), qPrintable(update.error()));
    QCOMPARE(fileText(h.appImage), QByteArray("old"));
    QCOMPARE(names(h.folder), QStringList{"lattis.appimage"});
    QVERIFY(h.trashed.isEmpty());
}

void InstallTest::appUpdateReplacesKeepingThePath()
{
    UpdateHome h;
    h.release("new image");
    AppUpdate update(h.setup());
    h.use(update);
    update.check();
    QTRY_VERIFY_WITH_TIMEOUT(!busy(update), 10000);

    QStringList states;
    QObject::connect(&update, &AppUpdate::changed, [&] {
        if (states.isEmpty() || states.last() != update.state())
            states.append(update.state());
    });
    update.update();
    QTRY_VERIFY_WITH_TIMEOUT(!busy(update), 10000);
    QVERIFY2(update.error().isEmpty(), qPrintable(update.error()));
    QCOMPARE(update.state(), QStringLiteral("ready"));
    QCOMPARE(states, (QStringList{"downloading", "ready"}));

    // The new file sits at the running file's path, the old one went to the trash.
    QCOMPARE(fileText(h.appImage), QByteArray("new image"));
    QVERIFY(QFileInfo(h.appImage).isExecutable());
    QCOMPARE(names(h.folder), QStringList{"lattis.appimage"});
    QCOMPARE(h.trashed, QStringList{h.folder + "/.lattis.appimage.replaced/lattis.appimage"});

    // Nothing more is checked or downloaded while it waits for a restart.
    update.check();
    update.update();
    QCOMPARE(update.state(), QStringLiteral("ready"));
    QCOMPARE(h.server.requestsTo("/release").size(), 1);
    QCOMPARE(h.server.requestsTo("/d/lattis").size(), 1);
}

void InstallTest::appUpdateCancels()
{
    UpdateHome h;
    h.release(QByteArray(2000, 'x'));
    AppUpdate update(h.setup());
    h.use(update);
    update.setPace(100, 20);
    update.check();
    QTRY_VERIFY_WITH_TIMEOUT(!busy(update), 10000);
    update.update();
    QTRY_VERIFY_WITH_TIMEOUT(update.progress() > 0, 10000);
    update.cancel();
    QTRY_VERIFY_WITH_TIMEOUT(!busy(update), 10000);
    QCOMPARE(update.state(), QStringLiteral("idle"));
    QVERIFY(update.error().isEmpty());
    QCOMPARE(fileText(h.appImage), QByteArray("old"));
    QCOMPARE(names(h.folder), QStringList{"lattis.appimage"});
    QVERIFY(h.trashed.isEmpty());
    QVERIFY(update.hasUpdate());
}

void InstallTest::appUpdateCantReplace()
{
    {
        // A folder that can't be written to.
        UpdateHome h;
        h.release("new image");
        QFile::setPermissions(h.folder, QFileDevice::ReadOwner | QFileDevice::ExeOwner);
        const bool writable = QFileInfo(h.folder).isWritable();
        AppUpdate update(h.setup());
        h.use(update);
        QFile::setPermissions(h.folder, QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner);
        if (writable)
            QSKIP("the folder stays writable (running as root)");
        QVERIFY(update.available());
        QVERIFY(!update.canReplace());
        QVERIFY(!update.replaceBlocker().isEmpty());
        update.check();
        QTRY_VERIFY_WITH_TIMEOUT(!busy(update), 10000);
        QVERIFY(update.hasUpdate());
        QVERIFY(update.releasePage().isValid());
        QFile::setPermissions(h.folder, QFileDevice::ReadOwner | QFileDevice::ExeOwner);
        update.update();
        QCOMPARE(update.state(), QStringLiteral("failed"));
        QVERIFY(h.server.requestsTo("/d/lattis").isEmpty());
        QFile::setPermissions(h.folder, QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner);
    }
    {
        // Gear Lever and AppImageLauncher keep the file in step with their menu entry.
        UpdateHome h;
        QCOMPARE(AppUpdates::replaceBlocker(h.appImage, h.applications), QString());
        QVERIFY(write(h.applications + "/gearlever_lattis.desktop", "[Desktop Entry]\nExec=" + h.appImage.toUtf8() + "\n"));
        QVERIFY(AppUpdates::replaceBlocker(h.appImage, h.applications).contains(QStringLiteral("Gear Lever")));
        AppUpdate update(h.setup());
        QVERIFY(update.available());
        QVERIFY(!update.canReplace());

        QVERIFY(QFile::remove(h.applications + "/gearlever_lattis.desktop"));
        QVERIFY(write(h.applications + "/appimagekit_abc-lattis.desktop", "Exec=" + h.appImage.toUtf8() + " %U\n"));
        QVERIFY(AppUpdates::replaceBlocker(h.appImage, h.applications).contains(QStringLiteral("AppImageLauncher")));

        // Another file's entry doesn't count.
        QVERIFY(write(h.applications + "/appimagekit_abc-lattis.desktop", "Exec=/elsewhere/lattis.appimage\n"));
        QCOMPARE(AppUpdates::replaceBlocker(h.appImage, h.applications), QString());
        QVERIFY(!AppUpdates::replaceBlocker(h.folder + "/missing.appimage", h.applications).isEmpty());
    }
}

void InstallTest::appUpdateSkipsAVersion()
{
    UpdateHome h;
    h.release("new image");
    AppUpdate update(h.setup());
    h.use(update);
    update.skip();
    QVERIFY(h.skipped.isEmpty());
    update.check();
    QTRY_VERIFY_WITH_TIMEOUT(!busy(update), 10000);
    QVERIFY(!update.skipped());
    update.skip();
    QCOMPARE(h.skipped, QStringList{"1.2.0"});
    QVERIFY(update.skipped());
    QVERIFY(update.hasUpdate());

    // A newer release than the skipped one shows again.
    h.release("newer image", "v1.3.0");
    update.check();
    QTRY_VERIFY_WITH_TIMEOUT(!busy(update), 10000);
    QCOMPARE(update.latestVersion(), QStringLiteral("1.3.0"));
    QVERIFY(!update.skipped());
}

void InstallTest::appUpdateRestart()
{
    UpdateHome h;
    h.release("new image");
    AppUpdate update(h.setup());
    h.use(update);
    update.restart();
    QCOMPARE(h.restarts, 0);
    update.check();
    QTRY_VERIFY_WITH_TIMEOUT(!busy(update), 10000);
    update.restart();
    QCOMPARE(h.restarts, 0);
    update.update();
    QTRY_VERIFY_WITH_TIMEOUT(!busy(update), 10000);
    QCOMPARE(update.state(), QStringLiteral("ready"));

    // The download was allowed while a game runs; the restart is not.
    h.game = QStringLiteral("Hades");
    QCOMPARE(update.runningGame(), QStringLiteral("Hades"));
    update.restart();
    QCOMPARE(h.restarts, 0);

    h.game.clear();
    h.restartWorks = false;
    update.restart();
    QCOMPARE(h.restarts, 1);
    QVERIFY(update.error().contains(QStringLiteral("no")));
    QCOMPARE(update.state(), QStringLiteral("ready"));
    h.restartWorks = true;
    update.restart();
    QCOMPARE(h.restarts, 2);
}

void InstallTest::appUpdateWhatsNew()
{
    UpdateHome h;
    h.release("new image");
    {
        AppUpdate update(h.setup());
        h.use(update);
        QVERIFY(update.whatsNewVersion().isEmpty());
        update.check();
        QTRY_VERIFY_WITH_TIMEOUT(!busy(update), 10000);
        QVERIFY(update.whatsNewVersion().isEmpty());
        update.update();
        QTRY_VERIFY_WITH_TIMEOUT(!busy(update), 10000);
        QCOMPARE(update.state(), QStringLiteral("ready"));
        // Still the old version running ("Later"): nothing to show yet.
        QVERIFY(update.whatsNewVersion().isEmpty());
    }

    {
        AppUpdate still(h.setup());
        still.setCurrentVersion("1.1.1");
        QVERIFY(still.whatsNewVersion().isEmpty());
    }

    AppUpdate next(h.setup());
    next.setCurrentVersion("1.2.0");
    QCOMPARE(next.whatsNewVersion(), QStringLiteral("1.2.0"));
    QVERIFY(next.whatsNewNotes().contains(QStringLiteral("a thing")));
    next.dismissWhatsNew();
    QVERIFY(next.whatsNewVersion().isEmpty());

    AppUpdate after(h.setup());
    after.setCurrentVersion("1.2.0");
    QVERIFY(after.whatsNewVersion().isEmpty());
}

void InstallTest::appUpdateDailyCheck()
{
    UpdateHome h;
    h.release("new image");
    AppUpdate update(h.setup());
    h.use(update);

    h.autoCheck = false;
    update.startDailyCheck(10);
    QTest::qWait(200);
    QVERIFY(h.server.requests.isEmpty());

    h.autoCheck = true;
    update.startDailyCheck(10);
    QTRY_VERIFY_WITH_TIMEOUT(update.hasUpdate(), 10000);
    QCOMPARE(h.server.requestsTo("/release").size(), 1);

    // The last check is recent, so the next start doesn't ask again.
    update.startDailyCheck(10);
    QTest::qWait(200);
    QCOMPARE(h.server.requestsTo("/release").size(), 1);

    // A day later it does.
    QVERIFY(write(h.state + "/app-update.toml", "last_check = \"2020-01-01T00:00:00Z\"\n"));
    update.startDailyCheck(10);
    QTRY_VERIFY_WITH_TIMEOUT(h.server.requestsTo("/release").size() == 2, 10000);
}

namespace {
// Answers `--appimage-extract <pattern>` as an AppImage's runtime does: what matches `pattern` in `payload` is
// copied into ./squashfs-root, links as links. Each pattern asked for goes to `log`.
struct FakeAppImage
{
    QTemporaryDir dir;
    QString path = dir.filePath("Emu.AppImage");
    QString payload = dir.filePath("payload");
    QString log = dir.filePath("extract.log");
    QString data = dir.filePath("data");

    explicit FakeAppImage(int exitCode = 0)
    {
        QDir().mkpath(payload);
        writeFile(path,
                  "#!/bin/sh\n"
                  "[ \"$1\" = --appimage-extract ] || exit 2\n"
                  "printf '%s\\n' \"$2\" >> '" + log.toUtf8() + "'\n"
                  "out=\"$(pwd)/squashfs-root\"\n"
                  "mkdir -p \"$out\"\n"
                  "cd '" + payload.toUtf8() + "' || exit 1\n"
                  "for f in $2; do\n"
                  "  [ -e \"$f\" ] || [ -L \"$f\" ] || continue\n"
                  "  mkdir -p \"$out/$(dirname \"$f\")\"\n"
                  "  rm -f \"$out/$f\"\n"
                  "  cp -P \"$f\" \"$out/$f\"\n"
                  "done\n"
                  "exit " + QByteArray::number(exitCode) + "\n",
                  true);
    }

    void file(const QString &name, const QByteArray &bytes) { writeFile(payload + u'/' + name, bytes); }

    void link(const QString &name, const QString &target)
    {
        const QString at = payload + u'/' + name;
        QVERIFY(QDir().mkpath(QFileInfo(at).absolutePath()));
        QCOMPARE(::symlink(QFile::encodeName(target).constData(), QFile::encodeName(at).constData()), 0);
    }

    QStringList extracted() const { return QString::fromUtf8(readFile(log)).split(u'\n', Qt::SkipEmptyParts); }

    DesktopEntry::Made make(QString *error) const { return DesktopEntry::make("emu", path, data, error); }
};

// The application proxy, for code whose https: address is fixed; put back when the test ends.
struct ProxyThrough
{
    explicit ProxyThrough(const QNetworkProxy &proxy) { QNetworkProxy::setApplicationProxy(proxy); }
    ~ProxyThrough() { QNetworkProxy::setApplicationProxy(QNetworkProxy::NoProxy); }
};

struct AppVersion
{
    explicit AppVersion(const QString &version) : saved(QCoreApplication::applicationVersion())
    {
        QCoreApplication::setApplicationVersion(version);
    }
    ~AppVersion() { QCoreApplication::setApplicationVersion(saved); }
    QString saved;
};

qsizetype gets(const FakeHttpServer &server)
{
    return std::count_if(server.requests.cbegin(), server.requests.cend(),
                         [](const FakeHttpServer::Request &r) { return r.method == "GET"; });
}

bool gone(const QString &pid)
{
    return !pid.isEmpty() && !QFileInfo::exists(QStringLiteral("/proc/") + pid);
}

const QByteArray kMgbaApi = "/repos/mgba-emu/mgba/releases/latest";
const QByteArray kMgbaAsset = "/mgba-emu/mgba/releases/download/x/mGBA-0.10.5-appimage-x64.appimage";
const QByteArray kLattisApi = "/repos/mines-cmd/lattis/releases/latest";
}

void InstallTest::menuEntryRefusals()
{
    {
        // An AppImage that won't run.
        FakeAppImage app(1);
        app.file("emu.desktop", "[Desktop Entry]\nExec=emu\n");
        QString error;
        QVERIFY(app.make(&error).desktop.isEmpty());
        QCOMPARE(error, QStringLiteral("couldn't read the menu entry out of Emu.AppImage"));
        QVERIFY(!QFileInfo::exists(app.data));
    }
    {
        // No .desktop file in it.
        FakeAppImage app;
        app.file("usr/share/applications/emu.desktop", "[Desktop Entry]\nExec=emu\n");
        QString error;
        QVERIFY(app.make(&error).desktop.isEmpty());
        QCOMPARE(error, QStringLiteral("Emu.AppImage has no .desktop file"));
        QVERIFY(!QFileInfo::exists(app.data));
    }
    {
        // Links that leave the image are not followed, whether absolute or relative.
        for (const QString &target : {QStringLiteral("/etc/hostname"), QStringLiteral("../outside.desktop")}) {
            FakeAppImage app;
            app.link("emu.desktop", target);
            writeFile(app.dir.filePath("outside.desktop"), "[Desktop Entry]\nExec=outside\n");
            QString error;
            QVERIFY(app.make(&error).desktop.isEmpty());
            QCOMPARE(error, QStringLiteral("Emu.AppImage has no .desktop file"));
            QCOMPARE(app.extracted(), QStringList{"*.desktop"});
            QVERIFY(!QFileInfo::exists(app.data));
        }
    }
    {
        // A loop of links gives up after a few hops.
        FakeAppImage app;
        app.link("a.desktop", "b.desktop");
        app.link("b.desktop", "a.desktop");
        QString error;
        QVERIFY(app.make(&error).desktop.isEmpty());
        QCOMPARE(error, QStringLiteral("Emu.AppImage has no .desktop file"));
        QCOMPARE(app.extracted().size(), 5);
    }
    {
        // The entry can't be written: the icon it copied goes too.
        FakeAppImage app;
        app.file("emu.desktop", "[Desktop Entry]\nExec=emu\nIcon=emu\n");
        app.file("emu.png", "PNG");
        writeFile(app.data + "/applications", "not a folder");
        QString error;
        const DesktopEntry::Made made = app.make(&error);
        QVERIFY(made.desktop.isEmpty());
        QVERIFY(made.icon.isEmpty());
        QVERIFY2(error.contains(QStringLiteral("couldn't make")), qPrintable(error));
        QVERIFY(!QFileInfo::exists(app.data + "/icons/lattis/emu.png"));
        QCOMPARE(readFile(app.data + "/applications"), QByteArray("not a folder"));
    }
    {
        // An icon that isn't in the image keeps the entry's own Icon line.
        FakeAppImage app;
        app.file("emu.desktop", "[Desktop Entry]\nExec=emu\nIcon=emu\n");
        QString error;
        const DesktopEntry::Made made = app.make(&error);
        QVERIFY2(error.isEmpty(), qPrintable(error));
        QVERIFY(made.icon.isEmpty());
        QVERIFY(readFile(made.desktop).contains("\nIcon=emu\n"));
        QCOMPARE(app.extracted(), (QStringList{"*.desktop", "emu.*", ".DirIcon"}));
        QVERIFY(!QFileInfo::exists(app.data + "/icons"));
    }
    {
        // An absolute Icon is the system's: nothing is extracted for it.
        FakeAppImage app;
        app.file("emu.desktop", "[Desktop Entry]\nExec=emu\nIcon=/usr/share/pixmaps/emu.png\n");
        QString error;
        const DesktopEntry::Made made = app.make(&error);
        QVERIFY(made.icon.isEmpty());
        QVERIFY(readFile(made.desktop).contains("\nIcon=/usr/share/pixmaps/emu.png\n"));
        QCOMPARE(app.extracted(), QStringList{"*.desktop"});
    }
}

void InstallTest::menuEntryFromAnAppImage()
{
    FakeAppImage app;
    app.file("emu.desktop", "[Desktop Entry]\nName=Emu\nExec=emu %U\nTryExec=emu\nIcon=emu\nX-AppImage-Version=1.0\n\n"
                            "[Desktop Action Window]\nName=New window\nExec=emu --window\n");
    app.file("emu.png", "PNG");
    QString error;
    const DesktopEntry::Made made = app.make(&error);
    QVERIFY2(error.isEmpty(), qPrintable(error));
    const QString entry = app.data + "/applications/lattis-emu.desktop";
    const QString icon = app.data + "/icons/lattis/emu.png";
    QCOMPARE(made.desktop, entry);
    QCOMPARE(made.icon, icon);
    QCOMPARE(readFile(icon), QByteArray("PNG"));
    QCOMPARE(readFile(entry), ("[Desktop Entry]\nName=Emu\nExec=\"" + app.path + "\" %U\nIcon=" + icon
                               + "\n\n[Desktop Action Window]\nName=New window\nExec=\"" + app.path + "\" --window\n")
                                  .toUtf8());
    QCOMPARE(app.extracted(), (QStringList{"*.desktop", "emu.*"}));

    // Made again after an update: the new icon replaces the old.
    app.file("emu.png", "PNG 2");
    QCOMPARE(app.make(&error).icon, icon);
    QCOMPARE(readFile(icon), QByteArray("PNG 2"));
    QCOMPARE(QDir(app.data + "/icons/lattis").entryList(QDir::Files), QStringList{"emu.png"});

    // Lattis's own entry with an icon of its own reads only the .desktop file, once.
    const QString data = app.dir.filePath("own");
    const DesktopEntry::Made own = DesktopEntry::ensureOwn(app.path, data, "/icons/given.png", &error);
    QCOMPARE(own.desktop, data + "/applications/lattis-lattis.desktop");
    QVERIFY(own.icon.isEmpty());
    QVERIFY(readFile(own.desktop).contains("\nIcon=/icons/given.png\n"));
    QCOMPARE(app.extracted().size(), 5);
    QCOMPARE(app.extracted().last(), QStringLiteral("*.desktop"));
    QVERIFY(DesktopEntry::ensureOwn(app.path, data, "/icons/given.png", &error).desktop.isEmpty());
    QCOMPARE(app.extracted().size(), 5);
}

void InstallTest::menuEntryFollowsLinks()
{
    // The usual layout: the root's .desktop and .DirIcon are links into usr/, which the first extraction leaves
    // dangling.
    FakeAppImage app;
    app.file("usr/share/applications/emu-real.desktop", "[Desktop Entry]\nName=Emu\nExec=emu\nIcon=emu\n");
    app.link("usr/share/applications/emu.desktop", "emu-real.desktop");
    app.link("emu.desktop", "usr/share/applications/emu.desktop");
    app.file("usr/share/icons/hicolor/scalable/apps/emu.svg", "SVG");
    app.link(".DirIcon", "usr/share/icons/hicolor/scalable/apps/emu.svg");
    QString error;
    const DesktopEntry::Made made = app.make(&error);
    QVERIFY2(error.isEmpty(), qPrintable(error));
    const QString icon = app.data + "/icons/lattis/emu.svg";
    QCOMPARE(made.icon, icon);
    QCOMPARE(readFile(icon), QByteArray("SVG"));
    QVERIFY(!QFileInfo(icon).isSymLink());
    QCOMPARE(readFile(made.desktop), ("[Desktop Entry]\nName=Emu\nExec=\"" + app.path + "\"\nIcon=" + icon + "\n").toUtf8());
    QCOMPARE(app.extracted(), (QStringList{"*.desktop", "usr/share/applications/emu.desktop",
                                           "usr/share/applications/emu-real.desktop", "emu.*", ".DirIcon",
                                           "usr/share/icons/hicolor/scalable/apps/emu.svg"}));
}

void InstallTest::installerAsksTheReleaseApi()
{
    FakeHttpServer github;
    QVERIFY(github.useTls(TLS_FIXTURE, true));
    ProxyThrough proxy(github.proxyAddress());
    AppVersion version("4.5.6");
    InstallerHome h;
    const QByteArray image = "#!/bin/sh\n" + QByteArray(3000, '#');
    h.release(image);
    const QByteArray answer = readFile(h.fixtures + "/github-mgba-emu-mgba.json");
    github.on(kMgbaApi, {200, answer});
    github.on(kMgbaAsset, {200, image});

    Installer installer(h.setup());
    const auto row = [&] { return installer.emulators().value("mgba").toMap(); };
    installer.install("mgba");
    QTRY_VERIFY_WITH_TIMEOUT(row().value("installedByApp").toBool() || row().value("state").toString() == "failed", 10000);
    QVERIFY2(row().value("installedByApp").toBool(), qPrintable(row().value("error").toString()));

    QCOMPARE(github.requests.first().method, QByteArray("CONNECT"));
    QCOMPARE(github.requests.first().target, QByteArray("api.github.com:443"));
    const auto api = github.requestsTo(kMgbaApi);
    QCOMPARE(api.size(), 1);
    QCOMPARE(api.first().headers.value("accept"), QByteArray("application/vnd.github+json"));
    QCOMPARE(api.first().headers.value("user-agent"), (QCoreApplication::applicationName() + "/4.5.6").toUtf8());
    QCOMPARE(fileText(h.home + "/Applications/mGBA-0.10.5-appimage-x64.appimage"), image);
    // A fresh answer is kept for the quiet checks.
    QCOMPARE(fileText(h.cache + "/releases/github-mgba-emu-mgba.json"), answer);
}

void InstallTest::installerRefusesARedirectToHttp()
{
    FakeHttpServer github;
    QVERIFY(github.useTls(TLS_FIXTURE, true));
    ProxyThrough proxy(github.proxyAddress());
    InstallerHome h;
    const QByteArray image = "#!/bin/sh\n" + QByteArray(3000, '#');
    h.release(image);
    const QByteArray answer = readFile(h.fixtures + "/github-mgba-emu-mgba.json");
    github.on(kMgbaApi, {302, {}, 0, "Location: http://api.github.com/moved\r\n"});
    github.on("/moved", {200, answer});
    github.on(kMgbaAsset, {200, image});

    Installer installer(h.setup());
    const auto row = [&] { return installer.emulators().value("mgba").toMap(); };
    installer.install("mgba");
    QTRY_COMPARE_WITH_TIMEOUT(row().value("state").toString(), QStringLiteral("failed"), 10000);
    const QString error = row().value("error").toString();
    QVERIFY2(error.startsWith(QStringLiteral("Couldn't check the latest release: ")), qPrintable(error));
    QVERIFY2(error.contains(QStringLiteral("redirect"), Qt::CaseInsensitive), qPrintable(error));
    QCOMPARE(gets(github), 1);
    QVERIFY(!QFileInfo::exists(h.home + "/Applications"));
    QVERIFY(!QFileInfo::exists(h.cache + "/releases/github-mgba-emu-mgba.json"));

    // The rule is about the scheme: a redirect that stays on https is followed.
    github.clear(kMgbaApi);
    github.on(kMgbaApi, {302, {}, 0, "Location: https://api.github.com/moved\r\n"});
    installer.retry("mgba");
    QTRY_VERIFY_WITH_TIMEOUT(row().value("installedByApp").toBool() || row().value("state").toString() == "failed", 10000);
    QVERIFY2(row().value("installedByApp").toBool(), qPrintable(row().value("error").toString()));
    QCOMPARE(github.requestsTo("/moved").size(), 1);
}

void InstallTest::installerTimesOut()
{
    FakeHttpServer github;
    QVERIFY(github.useTls(TLS_FIXTURE, true));
    ProxyThrough proxy(github.proxyAddress());
    InstallerHome h;
    h.release("image");
    github.on(kMgbaApi, {200, readFile(h.fixtures + "/github-mgba-emu-mgba.json"), 5000});

    Installer installer(h.setup());
    installer.setTransferTimeout(200);
    const auto row = [&] { return installer.emulators().value("mgba").toMap(); };
    QElapsedTimer clock;
    clock.start();
    installer.install("mgba");
    // The answer would come after 5 s: failing before that is the timeout's doing.
    QTRY_COMPARE_WITH_TIMEOUT(row().value("state").toString(), QStringLiteral("failed"), 4000);
    QVERIFY(clock.elapsed() < 4000);
    const QString error = row().value("error").toString();
    QVERIFY2(error.startsWith(QStringLiteral("Couldn't check the latest release: ")) && error.contains(QStringLiteral("timed out")),
             qPrintable(error));
    QCOMPARE(github.requestsTo(kMgbaApi).size(), 1);
    QVERIFY(!QFileInfo::exists(h.home + "/Applications"));
}

void InstallTest::appUpdateAsksTheReleaseApi()
{
    FakeHttpServer tls;
    QVERIFY(tls.useTls(TLS_FIXTURE));
    AppVersion version("4.5.6");
    UpdateHome h;
    h.release("new image");
    tls.on("/release", {200, h.releaseJson});
    AppUpdate update(h.setup());
    h.use(update);
    update.setReleaseUrl(tls.url("/release"));
    update.check();
    QTRY_VERIFY_WITH_TIMEOUT(!busy(update), 10000);
    QVERIFY2(update.error().isEmpty(), qPrintable(update.error()));
    QCOMPARE(update.latestVersion(), QStringLiteral("1.2.0"));
    const auto asked = tls.requestsTo("/release");
    QCOMPARE(asked.size(), 1);
    QCOMPARE(asked.first().headers.value("accept"), QByteArray("application/vnd.github+json"));
    QCOMPARE(asked.first().headers.value("user-agent"), (QCoreApplication::applicationName() + "/4.5.6").toUtf8());
    QVERIFY(h.server.requests.isEmpty());
}

void InstallTest::appUpdateRefusesARedirectToHttp()
{
    FakeHttpServer tls;
    QVERIFY(tls.useTls(TLS_FIXTURE));
    UpdateHome h;
    h.release("new image");
    tls.on("/release", {302, {}, 0, "Location: " + h.server.url("/release").toEncoded() + "\r\n"});
    AppUpdate update(h.setup());
    h.use(update);
    update.setReleaseUrl(tls.url("/release"));
    update.check();
    QTRY_VERIFY_WITH_TIMEOUT(!busy(update), 10000);
    QCOMPARE(update.state(), QStringLiteral("failed"));
    QVERIFY2(update.error().startsWith(QStringLiteral("Couldn't check for a newer Lattis: ")), qPrintable(update.error()));
    QVERIFY2(update.error().contains(QStringLiteral("redirect"), Qt::CaseInsensitive), qPrintable(update.error()));
    QVERIFY(!update.hasUpdate());
    QVERIFY(h.server.requests.isEmpty());

    // The daily check only logs it.
    update.check(true);
    QTRY_VERIFY_WITH_TIMEOUT(!busy(update), 10000);
    QCOMPARE(update.state(), QStringLiteral("idle"));
    QVERIFY(update.error().isEmpty());
    QVERIFY(h.server.requests.isEmpty());

    // A redirect that stays on https is followed.
    tls.clear("/release");
    tls.on("/release", {302, {}, 0, "Location: " + tls.url("/moved").toEncoded() + "\r\n"});
    tls.on("/moved", {200, h.releaseJson});
    update.check();
    QTRY_VERIFY_WITH_TIMEOUT(!busy(update), 10000);
    QVERIFY2(update.error().isEmpty(), qPrintable(update.error()));
    QVERIFY(update.hasUpdate());
}

void InstallTest::appUpdateTimesOut()
{
    UpdateHome h;
    h.release("new image");
    h.server.clear("/release");
    h.server.on("/release", {200, h.releaseJson, 5000});
    AppUpdate update(h.setup());
    h.use(update);
    update.setTransferTimeout(200);
    QElapsedTimer clock;
    clock.start();
    update.check();
    // The answer would come after 5 s: failing before that is the timeout's doing.
    QTRY_COMPARE_WITH_TIMEOUT(update.state(), QStringLiteral("failed"), 4000);
    QVERIFY(clock.elapsed() < 4000);
    QVERIFY2(update.error().startsWith(QStringLiteral("Couldn't check for a newer Lattis: "))
                 && update.error().contains(QStringLiteral("timed out")),
             qPrintable(update.error()));
    QVERIFY(!update.hasUpdate());
}

void InstallTest::appUpdateUsesTheCachedAnswer()
{
    FakeHttpServer github;
    QVERIFY(github.useTls(TLS_FIXTURE, true));
    ProxyThrough proxy(github.proxyAddress());
    UpdateHome h;
    h.release("new image");
    const QString cached = h.cache + "/releases/app-update.json";
    QVERIFY(write(cached, h.releaseJson));
    AppUpdate update(h.setup());
    update.setCurrentVersion("1.1.1");

    // Less than an hour old: used without asking.
    update.check();
    QTRY_VERIFY_WITH_TIMEOUT(!busy(update), 10000);
    QVERIFY2(update.error().isEmpty(), qPrintable(update.error()));
    QCOMPARE(update.latestVersion(), QStringLiteral("1.2.0"));
    QVERIFY(github.requests.isEmpty());
    QVERIFY(QFileInfo::exists(h.state + "/app-update.toml"));

    // Older: asked again, and the answer replaces it.
    QFile file(cached);
    QVERIFY(file.open(QIODevice::ReadWrite));
    QVERIFY(file.setFileTime(QDateTime::currentDateTime().addSecs(-7200), QFileDevice::FileModificationTime));
    file.close();
    h.release("newer image", "v1.3.0");
    github.on(kLattisApi, {200, h.releaseJson});
    update.check();
    QTRY_VERIFY_WITH_TIMEOUT(!busy(update), 10000);
    QVERIFY2(update.error().isEmpty(), qPrintable(update.error()));
    QCOMPARE(update.latestVersion(), QStringLiteral("1.3.0"));
    QCOMPARE(github.requestsTo(kLattisApi).size(), 1);
    QCOMPARE(fileText(cached), h.releaseJson);

    // One that doesn't read as a release is deleted, and the check says so.
    QVERIFY(write(cached, "<html>"));
    update.check();
    QTRY_VERIFY_WITH_TIMEOUT(!busy(update), 10000);
    QCOMPARE(update.state(), QStringLiteral("failed"));
    QVERIFY2(update.error().startsWith(QStringLiteral("Couldn't check for a newer Lattis: ")), qPrintable(update.error()));
    QVERIFY(!QFileInfo::exists(cached));
    QCOMPARE(github.requestsTo(kLattisApi).size(), 1);
}

void InstallTest::appUpdateTrashRefuses()
{
    UpdateHome h;
    h.release("new image");
    AppUpdate::Setup setup = h.setup();
    setup.trash = [](const QString &, QString *error) {
        *error = QStringLiteral("no trash on this drive");
        return false;
    };
    AppUpdate update(setup);
    h.use(update);
    update.check();
    QTRY_VERIFY_WITH_TIMEOUT(!busy(update), 10000);
    update.update();
    QTRY_VERIFY_WITH_TIMEOUT(!busy(update), 10000);

    // The update itself worked; the notice says where the old build is.
    QCOMPARE(update.state(), QStringLiteral("ready"));
    QVERIFY2(update.error().isEmpty(), qPrintable(update.error()));
    const QString aside = h.folder + "/.lattis.appimage.replaced";
    QVERIFY2(update.notice().contains(QStringLiteral("no trash on this drive")) && update.notice().contains(aside),
             qPrintable(update.notice()));
    QCOMPARE(fileText(h.appImage), QByteArray("new image"));
    QCOMPARE(fileText(aside + "/lattis.appimage"), QByteArray("old"));
}

void InstallTest::installerUpdateTrashRefuses()
{
    InstallerHome h;
    h.release("the first build");
    Installer::Setup setup = h.setup();
    setup.trash = [](const QString &, QString *error) {
        *error = QStringLiteral("no trash on this drive");
        return false;
    };
    Installer installer(setup);
    installer.setFixturesDir(h.fixtures);
    installer.setFixtureDelay(0);
    installer.setFixtureTick(0);
    QSignalSpy messages(&installer, &Installer::message);
    const auto row = [&] { return installer.emulators().value("mgba").toMap(); };
    installer.install("mgba");
    QTRY_VERIFY_WITH_TIMEOUT(row().value("installedByApp").toBool(), 10000);

    h.release("the second build", true, "0.10.6", "mGBA-0.10.6-appimage-x64.appimage", 13, "2026-10-01T10:00:00Z");
    installer.checkUpdates();
    QTRY_COMPARE_WITH_TIMEOUT(row().value("updateVersion").toString(), QStringLiteral("0.10.6"), 5000);
    installer.update("mgba");
    QTRY_COMPARE_WITH_TIMEOUT(row().value("version").toString(), QStringLiteral("0.10.6"), 10000);
    QCOMPARE(row().value("state").toString(), QStringLiteral("idle"));
    QCOMPARE(messages.size(), 1);
    const QString text = messages.first().first().toString();
    QVERIFY2(text.contains(QStringLiteral("mGBA-0.10.5-appimage-x64.appimage")) && text.contains(QStringLiteral("no trash on this drive"))
                 && text.contains(h.home + "/Applications"),
             qPrintable(text));
    QCOMPARE(names(h.home + "/Applications"),
             (QStringList{"mGBA-0.10.5-appimage-x64.appimage", "mGBA-0.10.6-appimage-x64.appimage"}));
}

void InstallTest::appLocationMoves()
{
    QTemporaryDir root;
    const QString home = root.filePath("home");
    const QString appImage = home + "/Downloads/Lattis.AppImage";
    const QString applications = root.filePath("applications");
    QVERIFY(write(appImage, "image"));
    QStringList moved;
    const AppLocation::Setup setup{appImage, home, applications, [&](const QString &path) { moved.append(path); }};

    {
        // Gear Lever keeps it where it is.
        QVERIFY(write(applications + "/gearlever_lattis.desktop", "Exec=" + appImage.toUtf8() + "\n"));
        AppLocation location(setup);
        QVERIFY(location.available());
        QVERIFY(!location.canMove());
        QVERIFY2(location.move().contains(QStringLiteral("Gear Lever")), qPrintable(location.blocker()));
        QCOMPARE(fileText(appImage), QByteArray("image"));
        QVERIFY(moved.isEmpty());
        QVERIFY(QFile::remove(applications + "/gearlever_lattis.desktop"));
    }
    {
        // The folder can't be made.
        QVERIFY(write(home + "/Applications", "a file"));
        AppLocation location(setup);
        QCOMPARE(location.folder(), home + "/Applications");
        QVERIFY(location.canMove());
        const QString error = location.move();
        QVERIFY2(error.startsWith(QStringLiteral("Couldn't create")), qPrintable(error));
        QCOMPARE(fileText(appImage), QByteArray("image"));
        QVERIFY(moved.isEmpty());
        QVERIFY(QFile::remove(home + "/Applications"));
    }
    {
        // Moved into the folder that exists; what remembers the old path hears the new one.
        QVERIFY(QDir().mkpath(home + "/AppImages"));
        AppLocation location(setup);
        QCOMPARE(location.folder(), home + "/AppImages");
        QSignalSpy changed(&location, &AppLocation::changed);
        QCOMPARE(location.move(), QString());
        QCOMPARE(moved, QStringList{home + "/AppImages/Lattis.AppImage"});
        QCOMPARE(fileText(moved.first()), QByteArray("image"));
        QVERIFY(!QFileInfo::exists(appImage));
        QVERIFY(location.inFolder());
        QVERIFY(!location.canMove());
        QVERIFY(!changed.isEmpty());
        QCOMPARE(location.move(), QStringLiteral("Lattis is already there"));
        QCOMPARE(moved.size(), 1);
    }
    {
        // A file that went missing since.
        AppLocation location(setup);
        QVERIFY(!location.canMove());
        QCOMPARE(location.move(), QStringLiteral("The AppImage file isn't there"));
        QCOMPARE(moved.size(), 1);
    }
}

void InstallTest::flatpakJobEnvironment()
{
    QTemporaryDir dir;
    const QString script = dir.filePath("flatpak.sh");
    const QString out = dir.filePath("env");
    writeFile(script, "#!/bin/sh\necho \"FOO=$FOO LEAK=$LATTIS_TEST_LEAK\" > '" + out.toUtf8() + "'\n", true);
    qputenv("LATTIS_TEST_LEAK", "from the app");
    const auto run = [](FlatpakJob &job) {
        QSignalSpy done(&job, &FlatpakJob::finished);
        job.start();
        if (done.isEmpty())
            done.wait(5000);
        return done.isEmpty() ? FlatpakJob::Result{} : done.first().first().value<FlatpakJob::Result>();
    };

    // Only what it is given; an entry without a name is dropped.
    FlatpakJob given(script, {{"list"}}, {"FOO=a=b", "noequals", "=x", "PATH=/usr/bin:/bin"});
    QVERIFY(run(given).ok);
    QCOMPARE(readFile(out), QByteArray("FOO=a=b LEAK=\n"));

    // None given: the app's own.
    FlatpakJob own(script, {{"list"}}, {});
    QVERIFY(run(own).ok);
    QCOMPARE(readFile(out), QByteArray("FOO= LEAK=from the app\n"));
    qunsetenv("LATTIS_TEST_LEAK");
}

void InstallTest::flatpakJobKillsWhatIgnoresTerm()
{
    QTemporaryDir dir;
    const QString script = dir.filePath("flatpak.sh");
    const QString pidFile = dir.filePath("pid");
    writeFile(script, "#!/bin/sh\ntrap '' TERM\necho $$ > '" + pidFile.toUtf8() + "'\nexec sleep 30\n", true);
    const auto started = [&] {
        if (!QTest::qWaitFor([&] { return readFile(pidFile).endsWith('\n'); }, 5000))
            return QString();
        const QString pid = QString::fromLatin1(readFile(pidFile).trimmed());
        QFile::remove(pidFile);
        return pid;
    };

    {
        // Cancel asks it to stop; it doesn't, and is killed after the grace.
        FlatpakJob job(script, FlatpakInstall::installSteps("io.mgba.mGBA"), {});
        QSignalSpy done(&job, &FlatpakJob::finished);
        job.start();
        const QString pid = started();
        QVERIFY(!pid.isEmpty());
        QElapsedTimer clock;
        clock.start();
        job.cancel();
        QVERIFY(done.wait(6000));
        QVERIFY(clock.elapsed() >= 2500);
        const auto result = done.first().first().value<FlatpakJob::Result>();
        QVERIFY(result.cancelled);
        QVERIFY(!result.ok);
        QVERIFY(result.error.isEmpty());
        QVERIFY(gone(pid));
        QTest::qWait(50);
        QCOMPARE(done.size(), 1);
        // The second step never ran.
        QVERIFY(!QFileInfo::exists(pidFile));
    }
    {
        // Destroyed while it runs: killed, and nothing is said.
        auto job = std::make_unique<FlatpakJob>(script, FlatpakInstall::installSteps("io.mgba.mGBA"), QStringList{});
        QSignalSpy done(job.get(), &FlatpakJob::finished);
        job->start();
        const QString pid = started();
        QVERIFY(!pid.isEmpty());
        job.reset();
        QVERIFY(gone(pid));
        QVERIFY(done.isEmpty());
    }
}

void InstallTest::ffmpegCancelWhileVerifying()
{
    const QByteArray sleep = QStandardPaths::findExecutable("sleep").toUtf8();
    QVERIFY(!sleep.isEmpty());
    PathOverride path("/nonexistent");
    FfmpegHome h;
    h.release(makeTarXz(ffmpegArchive()));
    FfmpegTool tool({h.tools, {}});
    h.setUp(tool);
    tool.install();
    QTRY_VERIFY_WITH_TIMEOUT(!busy(tool), 10000);
    QVERIFY(tool.ownCopy());

    // The new copy is run with -version before it replaces anything; this one hangs there.
    const QString pidFile = h.dir.filePath("pid");
    h.release(makeTarXz(ffmpegArchive("#!/bin/sh\necho $$ > '" + pidFile.toUtf8() + "'\nexec " + sleep + " 30\n")));
    tool.install();
    QTRY_VERIFY_WITH_TIMEOUT(readFile(pidFile).endsWith('\n'), 10000);
    QCOMPARE(tool.state(), QStringLiteral("installing"));
    const QString pid = QString::fromLatin1(readFile(pidFile).trimmed());
    tool.cancel();
    QCOMPARE(tool.state(), QStringLiteral("idle"));
    QVERIFY(tool.error().isEmpty());
    QVERIFY(gone(pid));
    QCOMPARE(names(h.tools), QStringList{"ffmpeg"});
    QCOMPARE(fileText(tool.path()), kGoodFfmpeg);
    QTest::qWait(100);
    QCOMPARE(tool.state(), QStringLiteral("idle"));
}

QTEST_GUILESS_MAIN(InstallTest)
#include "install_test.moc"
