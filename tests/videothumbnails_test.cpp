#include "videothumbnails.h"

#include <QDir>
#include <QFile>
#include <QGuiApplication>
#include <QImage>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

using namespace Qt::StringLiterals;

namespace {
const QString kVideos = QStringLiteral(MEDIA_HOME_DIR "/Videos/OBS/Belmont's Curse");
const QString kGood = kVideos + u"/2026-10-02 21-00-00.mp4"_s; // 3 s, 160x90
const QString kBroken = kVideos + u"/2026-10-01 20-00-00.mkv"_s; // empty file
}

class VideoThumbnailsTest : public QObject
{
    Q_OBJECT

private slots:
    void keyFollowsPathSizeAndTime();
    void cacheFindsWhatWasMade();
    void makesAThumbnailOnce();
    void aVideoThatWontPlayIsMarkedAndNotRetried();
    void aMissingVideoIsMarkedAndTheNextGoesOn();
    void aMarkedVideoIsNotTriedAgainAfterARestart();
    void aFileThatIsntAVideoIsMarked();
};

void VideoThumbnailsTest::keyFollowsPathSizeAndTime()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(u"a.mp4"_s);
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("123");
    file.close();
    const QString first = VideoThumbnails::key(QFileInfo(path));
    QCOMPARE(VideoThumbnails::key(QFileInfo(path)), first);

    QVERIFY(file.open(QIODevice::Append));
    file.write("4");
    file.close();
    const QString grown = VideoThumbnails::key(QFileInfo(path));
    QVERIFY(grown != first);

    QVERIFY(file.open(QIODevice::Append));
    QVERIFY(file.setFileTime(QDateTime::currentDateTime().addDays(-3), QFileDevice::FileModificationTime));
    file.close();
    QVERIFY(VideoThumbnails::key(QFileInfo(path)) != grown);
}

void VideoThumbnailsTest::cacheFindsWhatWasMade()
{
    QTemporaryDir cache;
    QVERIFY(cache.isValid());
    QVERIFY(VideoThumbnails::Cache(cache.path()).find(kGood).thumbnail.isEmpty());

    const QFileInfo good(kGood);
    QFile thumbnail(VideoThumbnails::thumbnailPath(cache.path(), good, 3000));
    QVERIFY(thumbnail.open(QIODevice::WriteOnly));
    QFile failed(VideoThumbnails::failedPath(cache.path(), QFileInfo(kBroken)));
    QVERIFY(failed.open(QIODevice::WriteOnly));

    const VideoThumbnails::Cache found(cache.path());
    QCOMPARE(found.find(kGood).thumbnail, thumbnail.fileName());
    QCOMPARE(found.find(kGood).durationMs, qint64(3000));
    QVERIFY(!found.find(kGood).failed);
    QVERIFY(found.find(kBroken).failed);
    QVERIFY(found.find(kBroken).thumbnail.isEmpty());
}

void VideoThumbnailsTest::makesAThumbnailOnce()
{
    QTemporaryDir cache;
    QVERIFY(cache.isValid());
    VideoThumbnailMaker maker(cache.path());
    QSignalSpy made(&maker, &VideoThumbnailMaker::made);
    maker.request(kGood);
    maker.request(kGood); // already queued
    QVERIFY2(made.wait(20000), "no thumbnail was made");
    QTest::qWait(100);
    QCOMPARE(made.size(), 1);

    const QString thumbnail = made.first().at(1).toString();
    const qint64 duration = made.first().at(2).toLongLong();
    QCOMPARE(made.first().at(0).toString(), kGood);
    QVERIFY(duration > 2500 && duration < 3500);
    const QImage image(thumbnail);
    QVERIFY(!image.isNull());
    QCOMPARE(image.width(), 160);
    QCOMPARE(image.height(), 90);

    const VideoThumbnails::Cached cached = VideoThumbnails::Cache(cache.path()).find(kGood);
    QCOMPARE(cached.thumbnail, thumbnail);
    QCOMPARE(cached.durationMs, duration);
}

void VideoThumbnailsTest::aVideoThatWontPlayIsMarkedAndNotRetried()
{
    QTemporaryDir cache;
    QVERIFY(cache.isValid());
    VideoThumbnailMaker maker(cache.path());
    QSignalSpy made(&maker, &VideoThumbnailMaker::made);
    maker.request(kBroken);
    maker.request(kGood);
    QVERIFY2(made.wait(20000), "the video after the broken one was never made");
    QCOMPARE(made.size(), 1);
    QCOMPARE(made.first().at(0).toString(), kGood);
    const VideoThumbnails::Cache cached(cache.path());
    QVERIFY(cached.find(kBroken).failed);
    QVERIFY(cached.find(kBroken).thumbnail.isEmpty());
}

void VideoThumbnailsTest::aMissingVideoIsMarkedAndTheNextGoesOn()
{
    QTemporaryDir cache;
    QVERIFY(cache.isValid());
    const QString gone = cache.filePath(u"gone.mp4"_s);
    VideoThumbnailMaker maker(cache.path());
    QSignalSpy made(&maker, &VideoThumbnailMaker::made);
    maker.request(gone);
    maker.request(kGood);
    QVERIFY2(made.wait(20000), "the video after the missing one was never made");
    QCOMPARE(made.size(), 1);
    QCOMPARE(made.first().at(0).toString(), kGood);
    QVERIFY(QFileInfo::exists(VideoThumbnails::failedPath(cache.path(), QFileInfo(gone))));
}

void VideoThumbnailsTest::aMarkedVideoIsNotTriedAgainAfterARestart()
{
    QTemporaryDir cache;
    QVERIFY(cache.isValid());
    QFile marker(VideoThumbnails::failedPath(cache.path(), QFileInfo(kGood)));
    QVERIFY(marker.open(QIODevice::WriteOnly));
    marker.close();

    const QString other = cache.filePath(u"copy.mp4"_s);
    QVERIFY(QFile::copy(kGood, other));
    VideoThumbnailMaker maker(cache.path());
    QSignalSpy made(&maker, &VideoThumbnailMaker::made);
    maker.request(kGood);
    maker.request(other);
    QVERIFY2(made.wait(20000), "the video after the marked one was never made");
    QTest::qWait(100);
    QCOMPARE(made.size(), 1);
    QCOMPARE(made.first().at(0).toString(), other);
    QVERIFY(VideoThumbnails::Cache(cache.path()).find(kGood).thumbnail.isEmpty());
}

void VideoThumbnailsTest::aFileThatIsntAVideoIsMarked()
{
    QTemporaryDir cache;
    QVERIFY(cache.isValid());
    const QString text = cache.filePath(u"notes.mp4"_s);
    QFile file(text);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("These are my notes, not a video.\n");
    file.close();

    VideoThumbnailMaker maker(cache.path() + u"/thumbs"_s);
    QSignalSpy made(&maker, &VideoThumbnailMaker::made);
    maker.request(text);
    maker.request(kGood);
    QVERIFY2(made.wait(20000), "the video after the text file was never made");
    QCOMPARE(made.size(), 1);
    QCOMPARE(made.first().at(0).toString(), kGood);
    const VideoThumbnails::Cache cached(cache.path() + u"/thumbs"_s);
    QVERIFY(cached.find(text).failed);
    QVERIFY(cached.find(text).thumbnail.isEmpty());
}

QTEST_MAIN(VideoThumbnailsTest)
#include "videothumbnails_test.moc"
