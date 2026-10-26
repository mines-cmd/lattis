#include "steammedia.h"

#include <QDir>
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
    void remuxMakesOneFile();
    void remuxWithoutFfmpeg();
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

QTEST_MAIN(SteamMediaTest)
#include "steammedia_test.moc"
