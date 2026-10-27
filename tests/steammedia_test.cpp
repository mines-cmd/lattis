#include "steammedia.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QProcess>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>

using namespace Qt::StringLiterals;

namespace {
const QString kFakeSteam = QStringLiteral(STEAM_DATA_DIR);

SteamInstall fakeInstall(const QString &user = u"39734273"_s)
{
    SteamInstall install;
    install.kind = SteamInstall::Kind::Native;
    install.root = kFakeSteam;
    install.userId = user;
    return install;
}
}

class SteamMediaTest : public QObject
{
    Q_OBJECT

private slots:
    void durations();
    void durations_data();
    void lengthTexts();
    void newestFirst();
    void brokenClipSkipped();
    void lengthFromManifest();
    void otherUsersClipsAreNotShown();
    void gameWithoutClips();
    void noSteamUser();
    void movedRecordingFolder();
    void remuxMakesOneFile();
    void remuxWithoutFfmpeg();
    void screenshotsNewestFirst();
    void screenshotsOfNoOne();
    void listAllOnePass();
    void listAllWithoutSteamUser();
};

void SteamMediaTest::durations_data()
{
    QTest::addColumn<QString>("text");
    QTest::addColumn<qint64>("ms");
    QTest::newRow("seconds") << u"PT30.0S"_s << qint64(30000);
    QTest::newRow("whole seconds") << u"PT4S"_s << qint64(4000);
    QTest::newRow("minutes") << u"PT1M3.5S"_s << qint64(63500);
    QTest::newRow("hours") << u"PT1H2M3S"_s << qint64(3723000);
    QTest::newRow("empty") << QString() << qint64(0);
    QTest::newRow("nothing after PT") << u"PT"_s << qint64(0);
    QTest::newRow("not a duration") << u"30 seconds"_s << qint64(0);
}

void SteamMediaTest::durations()
{
    QFETCH(QString, text);
    QFETCH(qint64, ms);
    QCOMPARE(SteamMedia::parseDurationMs(text), ms);
}

void SteamMediaTest::lengthTexts()
{
    QCOMPARE(SteamMedia::lengthText(33000), u"0:33"_s);
    QCOMPARE(SteamMedia::lengthText(33600), u"0:34"_s);
    QCOMPARE(SteamMedia::lengthText(63000), u"1:03"_s);
    QCOMPARE(SteamMedia::lengthText(3723000), u"1:02:03"_s);
}

void SteamMediaTest::newestFirst()
{
    const QList<SteamMedia::Clip> clips = SteamMedia::listClips(fakeInstall(), 8000);
    QCOMPARE(clips.size(), 2);
    QCOMPARE(clips[0].id, u"clip_8000_20260301_120000"_s);
    QCOMPARE(clips[1].id, u"clip_8000_20260215_090000"_s);
    QCOMPARE(clips[0].time, QDateTime(QDate(2026, 3, 1), QTime(12, 0)));
    QVERIFY(QFile::exists(clips[0].thumbnail));
    QVERIFY(QFile::exists(clips[0].videoDir + u"/session.mpd"_s));
}

void SteamMediaTest::brokenClipSkipped()
{
    for (const SteamMedia::Clip &c : SteamMedia::listClips(fakeInstall(), 8000))
        QVERIFY2(c.id != u"clip_8000_20260101_080000"_s, "a clip without a manifest was listed");
}

void SteamMediaTest::lengthFromManifest()
{
    const QList<SteamMedia::Clip> clips = SteamMedia::listClips(fakeInstall(), 8000);
    QCOMPARE(clips[0].durationMs, qint64(4000));
    QCOMPARE(clips[1].durationMs, qint64(3000));
}

void SteamMediaTest::otherUsersClipsAreNotShown()
{
    QVERIFY(SteamMedia::listClips(fakeInstall(u"39734274"_s), 8000).isEmpty());
}

void SteamMediaTest::gameWithoutClips()
{
    QVERIFY(SteamMedia::listClips(fakeInstall(), 1493710).isEmpty());
    QVERIFY(SteamMedia::listClips(fakeInstall(), 0).isEmpty());
}

void SteamMediaTest::noSteamUser()
{
    QVERIFY(SteamMedia::listClips({}, 8000).isEmpty());
    SteamInstall noUser = fakeInstall();
    noUser.userId.clear();
    QVERIFY(SteamMedia::listClips(noUser, 8000).isEmpty());
}

void SteamMediaTest::movedRecordingFolder()
{
    QTemporaryDir root;
    QVERIFY(root.isValid());
    const QString fixture = kFakeSteam + u"/userdata/39734273/gamerecordings"_s;
    const QString config = root.path() + u"/userdata/1/config"_s;
    QVERIFY(QDir().mkpath(config));
    QFile vdf(config + u"/localconfig.vdf"_s);
    QVERIFY(vdf.open(QIODevice::WriteOnly));
    vdf.write("\"UserLocalConfigStore\"\n{\n\t\"GameRecording\"\n\t{\n\t\t\"BackgroundRecordPath\"\t\t\""
              + fixture.toUtf8() + "\"\n\t}\n}\n");
    vdf.close();

    SteamInstall install = fakeInstall(u"1"_s);
    install.root = root.path();
    QCOMPARE(SteamMedia::listClips(install, 8000).size(), 2);

    // The setting can name the default folder itself; its clips are listed once.
    QVERIFY(QFile::link(fixture, root.path() + u"/userdata/1/gamerecordings"_s));
    QCOMPARE(SteamMedia::listClips(install, 8000).size(), 2);
}

void SteamMediaTest::remuxMakesOneFile()
{
    const QString ffmpeg = QStandardPaths::findExecutable(u"ffmpeg"_s);
    const QString ffprobe = QStandardPaths::findExecutable(u"ffprobe"_s);
    if (ffmpeg.isEmpty() || ffprobe.isEmpty())
        QSKIP("ffmpeg and ffprobe aren't installed");

    QTemporaryDir dir;
    const QString out = dir.filePath(u"clip.mp4"_s);
    const SteamMedia::Clip clip = SteamMedia::listClips(fakeInstall(), 8000).first();
    QCOMPARE(SteamMedia::remuxClip(clip, ffmpeg, out), QString());
    QVERIFY(QFile::exists(out));
    QCOMPARE(QDir(dir.path()).entryList(QDir::Files).size(), 1); // the two joined streams are gone

    QProcess probe;
    probe.start(ffprobe, {u"-v"_s, u"error"_s, u"-show_entries"_s, u"format=duration:stream=codec_type"_s, u"-of"_s,
                          u"default=nw=1"_s, out});
    QVERIFY(probe.waitForFinished(10000));
    const QString text = QString::fromUtf8(probe.readAllStandardOutput());
    QVERIFY2(text.contains(u"codec_type=video"_s) && text.contains(u"codec_type=audio"_s), qPrintable(text));
    const double seconds = text.section(u"duration="_s, 1).section(u'\n', 0, 0).toDouble();
    QVERIFY2(seconds > 3.5 && seconds < 4.5, qPrintable(text));
}

void SteamMediaTest::remuxWithoutFfmpeg()
{
    QTemporaryDir dir;
    const SteamMedia::Clip clip = SteamMedia::listClips(fakeInstall(), 8000).first();
    QVERIFY(!SteamMedia::remuxClip(clip, {}, dir.filePath(u"clip.mp4"_s)).isEmpty());
    QVERIFY(!SteamMedia::remuxClip(clip, dir.filePath(u"missing"_s), dir.filePath(u"clip.mp4"_s)).isEmpty());
    QVERIFY(QDir(dir.path()).entryList(QDir::Files).isEmpty());
}

void SteamMediaTest::screenshotsNewestFirst()
{
    const QList<SteamMedia::Screenshot> shots = SteamMedia::listScreenshots(fakeInstall(), 8000);
    QCOMPARE(shots.size(), 2); // not notes.txt, nor the thumbnails folder
    QCOMPARE(shots[0].id, u"20260310083000_1.jpg"_s);
    QCOMPARE(shots[1].id, u"20260220100000_1.jpg"_s);
    QCOMPARE(shots[0].time, QDateTime(QDate(2026, 3, 10), QTime(8, 30)));
    QVERIFY(QFile::exists(shots[0].path));
}

void SteamMediaTest::screenshotsOfNoOne()
{
    QVERIFY(SteamMedia::listScreenshots(fakeInstall(u"39734274"_s), 8000).isEmpty());
    QVERIFY(SteamMedia::listScreenshots(fakeInstall(), 1493710).isEmpty());
    QVERIFY(SteamMedia::listScreenshots({}, 8000).isEmpty());
}

namespace {
void copyFolder(const QString &from, const QString &to)
{
    QDirIterator it(from, QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const QString source = it.next();
        const QString target = to + source.mid(from.size());
        QDir().mkpath(QFileInfo(target).absolutePath());
        QFile::copy(source, target);
    }
}
}

void SteamMediaTest::listAllOnePass()
{
    // Two games with media (8000 as it is, 8001 a copy with one clip and one screenshot) and an app ID the library
    // may not know: listAll() reports every app ID; deciding which games exist is the caller's.
    QTemporaryDir root;
    QVERIFY(root.isValid());
    const QString user = root.path() + u"/userdata/39734273"_s;
    const QString source = kFakeSteam + u"/userdata/39734273"_s;
    copyFolder(source, user);
    copyFolder(source + u"/gamerecordings/clips/clip_8000_20260215_090000"_s,
               user + u"/gamerecordings/clips/clip_8001_20260215_090000"_s);
    copyFolder(source + u"/760/remote/8000/screenshots/thumbnails"_s, user + u"/760/remote/8001/screenshots/thumbnails"_s);
    QVERIFY(QFile::copy(source + u"/760/remote/8000/screenshots/20260220100000_1.jpg"_s,
                        user + u"/760/remote/8001/screenshots/20260220100000_1.jpg"_s));
    QVERIFY(QDir().mkpath(user + u"/760/remote/99999/screenshots"_s));
    QVERIFY(QFile::copy(source + u"/760/remote/8000/screenshots/20260220100000_1.jpg"_s,
                        user + u"/760/remote/99999/screenshots/20260105090000_1.jpg"_s));
    QVERIFY(QDir().mkpath(user + u"/760/remote/not-an-app/screenshots"_s));
    QVERIFY(QFile::copy(source + u"/760/remote/8000/screenshots/20260220100000_1.jpg"_s,
                        user + u"/760/remote/not-an-app/screenshots/20260105090000_1.jpg"_s));

    SteamInstall install = fakeInstall();
    install.root = root.path();
    const QHash<quint64, SteamMedia::AppMedia> all = SteamMedia::listAll(install);
    QCOMPARE(all.size(), 3);

    // Each app's lists are what listClips() and listScreenshots() give for it.
    for (const quint64 appId : {quint64(8000), quint64(8001), quint64(99999)}) {
        const QList<SteamMedia::Clip> clips = SteamMedia::listClips(install, appId);
        const QList<SteamMedia::Screenshot> shots = SteamMedia::listScreenshots(install, appId);
        QCOMPARE(all.value(appId).clips.size(), clips.size());
        QCOMPARE(all.value(appId).screenshots.size(), shots.size());
        for (qsizetype i = 0; i < clips.size(); ++i)
            QCOMPARE(all.value(appId).clips[i].id, clips[i].id);
        for (qsizetype i = 0; i < shots.size(); ++i)
            QCOMPARE(all.value(appId).screenshots[i].id, shots[i].id);
    }
    QCOMPARE(all.value(8000).clips.size(), 2); // the clip without a manifest isn't one
    QCOMPARE(all.value(8000).screenshots.size(), 2);
    QCOMPARE(all.value(8001).clips.size(), 1);
    QCOMPARE(all.value(8001).screenshots.size(), 1);
    QCOMPARE(all.value(99999).clips.size(), 0);
    QCOMPARE(all.value(99999).screenshots.size(), 1);
    QCOMPARE(all.value(8000).clips[0].id, u"clip_8000_20260301_120000"_s);
}

void SteamMediaTest::listAllWithoutSteamUser()
{
    QVERIFY(SteamMedia::listAll({}).isEmpty());
    SteamInstall noUser = fakeInstall();
    noUser.userId.clear();
    QVERIFY(SteamMedia::listAll(noUser).isEmpty());
    QVERIFY(SteamMedia::listAll(fakeInstall(u"39734274"_s)).isEmpty());
}

QTEST_MAIN(SteamMediaTest)
#include "steammedia_test.moc"
