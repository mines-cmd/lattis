#include "herocache.h"

#include <QDir>
#include <QFile>
#include <QImage>
#include <QImageWriter>
#include <QPainter>
#include <QTemporaryDir>
#include <QTest>

#include <memory>

class HeroCacheTest : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void wideArtGetsAJpeg();
    void pathChangesWithModified();
    void narrowPngGetsACopyAtItsOwnSize();
    void narrowProgressiveJpegGetsACopy();
    void narrowBaselineJpegGetsNoCopy();
    void narrowWebpGetsACopy();
    void transparentSourceBecomesAnOpaqueJpeg();
    void baselineCheckSkipsOtherSegments();
    void onlyJpegCopiesCount();
    void existingCopyIsReused();
    void unreadableSource();
    void failedWriteLeavesNothing();

private:
    QString makeImage(int width, QImage::Format format, const QString &name, int alpha = 255);
    QString makeProgressiveJpeg(int width, const QString &name);

    std::unique_ptr<QTemporaryDir> m_dir;
};

void HeroCacheTest::init()
{
    m_dir = std::make_unique<QTemporaryDir>();
    QVERIFY(m_dir->isValid());
}

QString HeroCacheTest::makeImage(int width, QImage::Format format, const QString &name, int alpha)
{
    QImage image(width, width / 3, format);
    image.fill(QColor(200, 60, 40, alpha));
    const QString path = m_dir->filePath(name);
    return image.save(path) ? path : QString();
}

QString HeroCacheTest::makeProgressiveJpeg(int width, const QString &name)
{
    QImage image(width, width / 3, QImage::Format_RGB32);
    image.fill(QColor(200, 60, 40));
    const QString path = m_dir->filePath(name);
    QImageWriter writer(path, "jpeg");
    writer.setProgressiveScanWrite(true);
    return writer.write(image) ? path : QString();
}

void HeroCacheTest::wideArtGetsAJpeg()
{
    const QString source = makeImage(3840, QImage::Format_RGB32, "wide.png");
    const QString cache = m_dir->filePath("cache");
    const HeroCache::Result result = HeroCache::make(source, "game", 5, cache);
    QVERIFY2(result.error.isEmpty(), qPrintable(result.error));
    QCOMPARE(result.path, cache + "/heroes/game-5.jpg");
    const QImage copy(result.path);
    QCOMPARE(copy.width(), HeroCache::kWidth);
    QCOMPARE(copy.height(), 3840 / 3 * HeroCache::kWidth / 3840);
    QVERIFY(QFileInfo::exists(source));
    QVERIFY(!QFileInfo::exists(result.path + ".partial"));
}

void HeroCacheTest::pathChangesWithModified()
{
    const QString source = makeImage(3000, QImage::Format_RGB32, "wide.png");
    const QString cache = m_dir->filePath("cache");
    const QString first = HeroCache::make(source, "game", 5, cache).path;
    const QString second = HeroCache::make(source, "game", 6, cache).path;
    QVERIFY(!first.isEmpty());
    QVERIFY(!second.isEmpty());
    QVERIFY(first != second);
    QCOMPARE(HeroCache::stem("a-1", 23), QStringLiteral("a-1-23"));
}

void HeroCacheTest::narrowPngGetsACopyAtItsOwnSize()
{
    for (const int width : {1920, HeroCache::kWidth}) {
        const QString source = makeImage(width, QImage::Format_RGB32, "narrow.png");
        const HeroCache::Result result = HeroCache::make(source, "game", width, m_dir->filePath("cache"));
        QVERIFY2(result.error.isEmpty(), qPrintable(result.error));
        QVERIFY(result.path.endsWith(".jpg"));
        QCOMPARE(QImage(result.path).size(), QSize(width, width / 3));
    }
}

void HeroCacheTest::narrowProgressiveJpegGetsACopy()
{
    const QString source = makeProgressiveJpeg(1920, "progressive.jpg");
    QVERIFY(!HeroCache::isBaselineJpeg(source));
    const HeroCache::Result result = HeroCache::make(source, "game", 5, m_dir->filePath("cache"));
    QVERIFY2(result.error.isEmpty(), qPrintable(result.error));
    QVERIFY(result.path.endsWith(".jpg"));
    QCOMPARE(QImage(result.path).width(), 1920);
}

void HeroCacheTest::narrowBaselineJpegGetsNoCopy()
{
    const QString source = makeImage(1920, QImage::Format_RGB32, "baseline.jpg");
    QVERIFY(HeroCache::isBaselineJpeg(source));
    const QString cache = m_dir->filePath("cache");
    const HeroCache::Result result = HeroCache::make(source, "game", 5, cache);
    QVERIFY(result.path.isEmpty());
    QVERIFY(result.error.isEmpty());
    QVERIFY(!QFileInfo::exists(cache));
}

void HeroCacheTest::narrowWebpGetsACopy()
{
    if (!QImageWriter::supportedImageFormats().contains("webp"))
        QSKIP("the webp image plugin is missing");
    const QString source = makeImage(1920, QImage::Format_RGB32, "narrow.webp");
    const HeroCache::Result result = HeroCache::make(source, "game", 5, m_dir->filePath("cache"));
    QVERIFY2(result.error.isEmpty(), qPrintable(result.error));
    QVERIFY(result.path.endsWith(".jpg"));
}

void HeroCacheTest::transparentSourceBecomesAnOpaqueJpeg()
{
    const QString source = makeImage(3840, QImage::Format_ARGB32, "alpha.png", 0);
    const HeroCache::Result result = HeroCache::make(source, "game", 5, m_dir->filePath("cache"));
    QVERIFY2(result.error.isEmpty(), qPrintable(result.error));
    QVERIFY(result.path.endsWith(".jpg"));
    const QImage copy(result.path);
    QVERIFY(!copy.hasAlphaChannel());
    QVERIFY(qRed(copy.pixel(10, 10)) < 10);
}

void HeroCacheTest::baselineCheckSkipsOtherSegments()
{
    const QString plain = makeImage(64, QImage::Format_RGB32, "plain.jpg");
    QFile file(plain);
    QVERIFY(file.open(QIODevice::ReadOnly));
    const QByteArray bytes = file.readAll();
    file.close();

    // An APP1 block between SOI and the frame header, holding bytes that look like a progressive SOF.
    QByteArray payload("Exif\0\0", 6);
    payload += QByteArray("\xFF\xC2\x00\x0B", 4);
    payload += QByteArray(300, 'x');
    const int length = payload.size() + 2;
    QByteArray withExif = bytes.left(2);
    withExif += QByteArray("\xFF\xE1", 2);
    withExif += char(length >> 8);
    withExif += char(length & 0xFF);
    withExif += payload;
    withExif += bytes.mid(2);
    const QString exifPath = m_dir->filePath("exif.jpg");
    QFile out(exifPath);
    QVERIFY(out.open(QIODevice::WriteOnly));
    out.write(withExif);
    out.close();
    QVERIFY(HeroCache::isBaselineJpeg(exifPath));

    QVERIFY(!HeroCache::isBaselineJpeg(m_dir->filePath("missing.jpg")));
    QVERIFY(!HeroCache::isBaselineJpeg(makeImage(64, QImage::Format_RGB32, "plain.png")));
}

void HeroCacheTest::onlyJpegCopiesCount()
{
    const QString cache = m_dir->filePath("cache");
    QVERIFY(QDir().mkpath(cache + "/heroes"));
    for (const QString &name : {"a-1.jpg", "b-2.png", "c-3.jpg.partial"}) {
        QFile file(cache + "/heroes/" + name);
        QVERIFY(file.open(QIODevice::WriteOnly));
    }
    QCOMPARE(HeroCache::list(cache).keys(), QStringList{"a-1"});
    HeroCache::prune(cache, {"a-1", "b-2"});
    QCOMPARE(QDir(cache + "/heroes").entryList(QDir::Files), QStringList{"a-1.jpg"});
}

void HeroCacheTest::existingCopyIsReused()
{
    const QString source = makeImage(3840, QImage::Format_RGB32, "wide.png");
    const QString cache = m_dir->filePath("cache");
    const QString first = HeroCache::make(source, "game", 5, cache).path;
    const QDateTime written = QFileInfo(first).lastModified();
    QTest::qWait(20);
    const HeroCache::Result again = HeroCache::make(source, "game", 5, cache);
    QCOMPARE(again.path, first);
    QCOMPARE(QFileInfo(first).lastModified(), written);
}

void HeroCacheTest::unreadableSource()
{
    QFile broken(m_dir->filePath("broken.jpg"));
    QVERIFY(broken.open(QIODevice::WriteOnly));
    broken.write("not an image");
    broken.close();
    const HeroCache::Result result = HeroCache::make(broken.fileName(), "game", 5, m_dir->filePath("cache"));
    QVERIFY(result.path.isEmpty());
    QVERIFY(!result.error.isEmpty());
    QVERIFY(!QFileInfo::exists(m_dir->filePath("cache")));
}

void HeroCacheTest::failedWriteLeavesNothing()
{
    const QString source = makeImage(3840, QImage::Format_RGB32, "wide.png");
    const QString cache = m_dir->filePath("cache");
    QVERIFY(QDir().mkpath(cache));
    QFile blocker(cache + "/heroes");
    QVERIFY(blocker.open(QIODevice::WriteOnly));
    blocker.close();
    const HeroCache::Result result = HeroCache::make(source, "game", 5, cache);
    QVERIFY(result.path.isEmpty());
    QVERIFY(!result.error.isEmpty());
    QCOMPARE(QDir(cache).entryList(QDir::AllEntries | QDir::NoDotAndDotDot), QStringList{"heroes"});
    QVERIFY(QFileInfo(cache + "/heroes").isFile());
}

QTEST_GUILESS_MAIN(HeroCacheTest)
#include "herocache_test.moc"
