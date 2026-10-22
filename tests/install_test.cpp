#include "appimageinstall.h"
#include "fakehttpserver.h"
#include "installer.h"
#include "installrecord.h"
#include "releasesource.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

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
    void initTestCase() { qRegisterMetaType<AppImageDownload::Result>(); }

    void sourceParses_data();
    void sourceParses();
    void sourceUrls();
    void githubRelease();
    void gitlabRelease();
    void releaseErrors();
    void assetPatterns_data();
    void assetPatterns();
    void assetPreference();
    void targetFolder();
    void archiveNames();

    void extractsOneFile();
    void extractRefuses_data();
    void extractRefuses();
    void extractNoMatch();

    void downloadInstalls();
    void downloadChecksum();
    void downloadCutShort();
    void downloadRefusesPlainHttp();
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
    records.insert("mgba", {"appimage", "/home/x/Applications/mGBA-0.10.5.appimage", "0.10.5", "", "12",
                            "2026-09-01T10:02:00Z", "2026-10-22"});
    records.insert("cemu", {"appimage", "/home/x/Applications/Cemu \"2.6\".AppImage", "v2.6", "abc", "3", "", "2026-10-21"});
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
    int redetected = 0;
    QList<EmulatorDef> defs;

    InstallerHome()
    {
        EmulatorDef mgba;
        mgba.id = "mgba";
        mgba.name = "mGBA";
        mgba.release = "github:mgba-emu/mgba";
        mgba.releaseAsset = "mGBA-*-appimage-x64.appimage";
        mgba.preset = true;
        EmulatorDef dolphin;
        dolphin.id = "dolphin";
        dolphin.name = "Dolphin";
        dolphin.preset = true;
        defs = {mgba, dolphin};
        QDir().mkpath(fixtures);
    }

    Installer::Setup setup()
    {
        return {state, cache, [this] { return defs; },
                [this] {
                    InstallLocations where;
                    where.home = home;
                    where.applicationsDirs = {home + "/Applications", home + "/AppImages"};
                    where.flatpakDirs = {home + "/.local/share/flatpak/app"};
                    return where;
                },
                [this] { ++redetected; }};
    }

    void release(const QByteArray &asset, bool withDigest = true)
    {
        QJsonObject a{{"id", 12},
                      {"name", "mGBA-0.10.5-appimage-x64.appimage"},
                      {"browser_download_url", "https://github.com/mgba-emu/mgba/releases/download/0.10.5/x"},
                      {"size", asset.size()},
                      {"updated_at", "2026-09-01T10:02:00Z"}};
        if (withDigest)
            a.insert("digest", "sha256:" + sha(asset));
        const QJsonObject root{{"tag_name", "0.10.5"}, {"assets", QJsonArray{a}}};
        QVERIFY(write(fixtures + "/github-mgba-emu-mgba.json", QJsonDocument(root).toJson()));
        QVERIFY(write(fixtures + "/mGBA-0.10.5-appimage-x64.appimage", asset));
    }
};
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
    QCOMPARE(row.value("routes").toStringList(), QStringList{"appimage"});
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
    installer.install("mgba", "flatpak");
    QCOMPARE(state().value("state").toString(), QStringLiteral("failed"));
    installer.install("nothing");
    QVERIFY(!installer.emulators().contains("nothing"));
    QCOMPARE(h.redetected, 0);
    QVERIFY(!QFileInfo::exists(InstallRecords::path(h.state)));

    // Dolphin has no AppImage.
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

QTEST_GUILESS_MAIN(InstallTest)
#include "install_test.moc"
