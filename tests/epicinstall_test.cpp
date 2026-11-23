#include "epicinstall.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QTest>

using namespace Qt::StringLiterals;

namespace {
QByteArray capture(const QString &name)
{
    QFile file(QStringLiteral(LEGENDARY_DATA_DIR) + u'/' + name);
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}

void writeFile(const QString &path, const QByteArray &content)
{
    QVERIFY(QDir().mkpath(QFileInfo(path).absolutePath()));
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
    file.write(content);
}
}

class EpicInstallTest : public QObject
{
    Q_OBJECT

private slots:
    void parsesARealInfo();
    void infoWithoutSizesIsNothing();
    void linuxBuildChoosesLinux();
    void notJsonIsNothing();
    void readsHeroicsCache();
    void cacheEntryWithoutInstallStillHasSizes();
    void defaultFolder();
    void installArguments();
    void freeSpaceOfAFolderThatDoesNotExistYet();
    void homeIsExpanded();
};

void EpicInstallTest::parsesARealInfo()
{
    const auto info = EpicInstall::parseInfo(capture(u"info.stdout"_s));
    QVERIFY(info);
    QCOMPARE(info->downloadSize, qint64(224976506));
    QCOMPARE(info->diskSize, qint64(323783140));
    QCOMPARE(info->platform, u"Windows"_s);
    QVERIFY(!info->hasLinux);
    QCOMPARE(EpicInstall::platformFor(*info), u"Windows"_s);
}

void EpicInstallTest::infoWithoutSizesIsNothing()
{
    QVERIFY(!EpicInstall::parseInfo(R"({"game": {}, "install": null, "manifest": null})"));
    QVERIFY(!EpicInstall::parseInfo(R"({"manifest": {"num_files": 3}})"));
}

void EpicInstallTest::linuxBuildChoosesLinux()
{
    const auto info = EpicInstall::parseInfo(
        R"({"game": {"platform_versions": {"Windows": "1", "Linux": "1"}}, "manifest": {"download_size": 5, "disk_size": 9}})");
    QVERIFY(info);
    QVERIFY(info->hasLinux);
    QCOMPARE(EpicInstall::platformFor(*info), u"Linux"_s);
}

void EpicInstallTest::notJsonIsNothing()
{
    QVERIFY(!EpicInstall::parseInfo(""));
    QVERIFY(!EpicInstall::parseInfo("[Core] INFO: logging in"));
    QVERIFY(!EpicInstall::parseInfo("[1, 2]"));
}

void EpicInstallTest::readsHeroicsCache()
{
    QTemporaryDir root;
    writeFile(root.filePath(u"store_cache/legendary_install_info.json"_s),
              R"({"AppA": {"game": {}, "install": {"platform": "Windows"}, "manifest": {"download_size": 11, "disk_size": 22}},
                  "__timestamp": {"a": "b"}})");
    const auto info = EpicInstall::cachedInfo(root.path(), u"AppA"_s);
    QVERIFY(info);
    QCOMPARE(info->downloadSize, qint64(11));
    QCOMPARE(info->diskSize, qint64(22));
    QVERIFY(!EpicInstall::cachedInfo(root.path(), u"Other"_s));
    QVERIFY(!EpicInstall::cachedInfo(root.path(), u"__timestamp"_s));
    QVERIFY(!EpicInstall::cachedInfo(root.filePath(u"nowhere"_s), u"AppA"_s));
    QVERIFY(!EpicInstall::cachedInfo({}, u"AppA"_s));
}

void EpicInstallTest::cacheEntryWithoutInstallStillHasSizes()
{
    QTemporaryDir root;
    writeFile(root.filePath(u"store_cache/legendary_install_info.json"_s),
              R"({"AppA": {"game": {}, "install": null, "manifest": {"download_size": 1, "disk_size": 2}}, "AppB": null})");
    const auto info = EpicInstall::cachedInfo(root.path(), u"AppA"_s);
    QVERIFY(info);
    QVERIFY(info->platform.isEmpty());
    QVERIFY(!EpicInstall::cachedInfo(root.path(), u"AppB"_s));
}

void EpicInstallTest::defaultFolder()
{
    QTemporaryDir root;
    QCOMPARE(EpicInstall::defaultFolder(root.path()), QDir::homePath() + u"/Games/Heroic"_s);
    writeFile(root.filePath(u"config.json"_s), R"({"defaultSettings": {"defaultInstallPath": "/data/games"}})");
    QCOMPARE(EpicInstall::defaultFolder(root.path()), u"/data/games"_s);
    writeFile(root.filePath(u"config.json"_s), R"({"defaultSettings": {}})");
    QCOMPARE(EpicInstall::defaultFolder(root.path()), QDir::homePath() + u"/Games/Heroic"_s);
}

void EpicInstallTest::installArguments()
{
    QCOMPARE(EpicInstall::installArgs(u"/g"_s, u"Windows"_s),
             QStringList({u"--base-path"_s, u"/g"_s, u"--platform"_s, u"Windows"_s, u"--skip-sdl"_s}));
}

void EpicInstallTest::freeSpaceOfAFolderThatDoesNotExistYet()
{
    QTemporaryDir dir;
    const qint64 here = EpicInstall::freeSpace(dir.path());
    QVERIFY(here > 0);
    // Another process may write between the two reads.
    QVERIFY(qAbs(EpicInstall::freeSpace(dir.filePath(u"a/b/c"_s)) - here) < 64 * 1024 * 1024);
    QCOMPARE(EpicInstall::freeSpace({}), qint64(-1));
    QCOMPARE(EpicInstall::freeSpace(u"relative/path"_s), qint64(-1));
}

void EpicInstallTest::homeIsExpanded()
{
    QCOMPARE(EpicInstall::expandHome(u"~"_s), QDir::homePath());
    QCOMPARE(EpicInstall::expandHome(u"~/Games"_s), QDir::homePath() + u"/Games"_s);
    QCOMPARE(EpicInstall::expandHome(u"/x/~y"_s), u"/x/~y"_s);
    QCOMPARE(EpicInstall::expandHome({}), QString());
}

QTEST_MAIN(EpicInstallTest)
#include "epicinstall_test.moc"
