#include "librarytestbase.h"

#include <QImage>
#include <QImageReader>
#include <QSignalSpy>

// Finding art that is larger than the app shows it (see ArtImage::maxSide()), and shrinking it.
class LibraryArtTest : public LibraryTestBase
{
    Q_OBJECT

private slots:
    void reportsFilesOverTheLimitForEachKind_data();
    void reportsFilesOverTheLimitForEachKind();
    void tallImageIsReported();
    void unchangedFolderIsNotReadAgain();
    void changedFolderIsReadAgain();
    void foundAtLoadFromTheCache();
    void oversizedArtIsShrunkInItsFormatKeepingTransparency();
    void logoMarginsAreTrimmedOnce();
    void artWithinTheLimitIsNotTouched();
    void shownGameWaitsUntilTheSelectionMoves();
    void readOnlyLibraryIsLeftAlone();
    void shrinksOnAQuietStartWithoutACache();
    void fileReplacedDuringTheShrinkKeepsTheReplacement();

private:
    QString writeArt(const QString &id, const QString &kind, int width, int height);
    QStringList oversized(Library &library, const QString &id) const;
};

QString LibraryArtTest::writeArt(const QString &id, const QString &kind, int width, int height)
{
    const QString folder = m_data + QStringLiteral("/art/") + id;
    QDir().mkpath(folder);
    QImage image(width, height, QImage::Format_RGB32);
    image.fill(Qt::red);
    const QString path = folder + u'/' + kind + QStringLiteral(".png");
    if (!image.save(path))
        QTest::qFail("could not write the image", __FILE__, __LINE__);
    return path;
}

QStringList LibraryArtTest::oversized(Library &library, const QString &id) const
{
    const ArtCheck check = library.store().snapshot().artChecks.value(id);
    return check.art == library.store().snapshot().art.value(id) ? check.oversized : QStringList{QStringLiteral("?")};
}

void LibraryArtTest::reportsFilesOverTheLimitForEachKind_data()
{
    QTest::addColumn<QString>("kind");
    QTest::addColumn<int>("limit");
    QTest::newRow("hero") << "hero" << 3840;
    QTest::newRow("logo") << "logo" << 1280;
    QTest::newRow("cover") << "cover" << 1800;
    QTest::newRow("icon") << "icon" << 512;
}

void LibraryArtTest::reportsFilesOverTheLimitForEachKind()
{
    QFETCH(QString, kind);
    QFETCH(int, limit);
    writeArt(QStringLiteral("orphan-a"), kind, limit + 1, 10);
    writeArt(QStringLiteral("orphan-b"), kind, limit, 10);
    auto library = openLibrary();
    QCOMPARE(oversized(*library, QStringLiteral("orphan-a")), QStringList{kind});
    QCOMPARE(oversized(*library, QStringLiteral("orphan-b")), QStringList{});
}

void LibraryArtTest::tallImageIsReported()
{
    writeArt(QStringLiteral("orphan-a"), QStringLiteral("logo"), 10, 1281);
    auto library = openLibrary();
    QCOMPARE(oversized(*library, QStringLiteral("orphan-a")), QStringList{QStringLiteral("logo")});
}

void LibraryArtTest::unchangedFolderIsNotReadAgain()
{
    const QString id = QStringLiteral("orphan-a");
    const QString path = writeArt(id, QStringLiteral("logo"), 1281, 10);
    auto library = openLibrary();
    QCOMPARE(oversized(*library, id), QStringList{QStringLiteral("logo")});

    // Small now, with the same name, size and time as far as the art set can tell.
    const QDateTime time = QFileInfo(path).lastModified();
    writeArt(id, QStringLiteral("logo"), 100, 10);
    QFile file(path);
    QVERIFY(file.open(QIODevice::ReadWrite));
    QVERIFY(file.setFileTime(time, QFileDevice::FileModificationTime));
    file.close();
    // Another game's new art shows the rescan has been applied.
    writeArt(QStringLiteral("orphan-b"), QStringLiteral("icon"), 513, 10);
    library->store().refresh();
    QTRY_COMPARE(oversized(*library, QStringLiteral("orphan-b")), QStringList{QStringLiteral("icon")});
    QCOMPARE(oversized(*library, id), QStringList{QStringLiteral("logo")});
}

void LibraryArtTest::changedFolderIsReadAgain()
{
    const QString id = QStringLiteral("orphan-a");
    writeArt(id, QStringLiteral("logo"), 1281, 10);
    auto library = openLibrary();
    QCOMPARE(oversized(*library, id), QStringList{QStringLiteral("logo")});

    QTest::qWait(1100);
    writeArt(id, QStringLiteral("logo"), 100, 10);
    writeArt(id, QStringLiteral("icon"), 513, 10);
    library->store().refresh();
    library->store().waitForIdle();
    QTRY_COMPARE(oversized(*library, id), QStringList{QStringLiteral("icon")});
}

void LibraryArtTest::foundAtLoadFromTheCache()
{
    const QString id = QStringLiteral("orphan-a");
    writeArt(id, QStringLiteral("logo"), 1281, 10);
    {
        auto library = openLibrary();
        QCOMPARE(oversized(*library, id), QStringList{QStringLiteral("logo")});
    }
    Library again(m_data, m_cache);
    QCOMPARE(oversized(again, id), QStringList{QStringLiteral("logo")});
}

void LibraryArtTest::oversizedArtIsShrunkInItsFormatKeepingTransparency()
{
    const QString id = QStringLiteral("switch-pikmin-4");
    const QString path = m_data + QStringLiteral("/art/") + id + QStringLiteral("/logo.png");
    QDir().mkpath(QFileInfo(path).absolutePath());
    QImage image(2000, 100, QImage::Format_ARGB32);
    image.fill(Qt::transparent);
    QVERIFY(image.save(path));
    auto library = openLibrary();

    QTRY_COMPARE(QImageReader(path).size(), QSize(1280, 64));
    QImage shrunk(path);
    QVERIFY(shrunk.hasAlphaChannel());
    QCOMPARE(qAlpha(shrunk.pixel(10, 10)), 0);
    QTRY_VERIFY(oversized(*library, id).isEmpty());
    QVERIFY(!QFileInfo::exists(QFileInfo(path).absolutePath() + QStringLiteral("/.logo.shrink")));

    // A jpeg stays a jpeg.
    const QString jpg = m_data + QStringLiteral("/art/") + id + QStringLiteral("/icon.jpg");
    QImage big(1000, 1000, QImage::Format_RGB32);
    big.fill(Qt::blue);
    QVERIFY(big.save(jpg));
    library->store().refresh();
    QTRY_COMPARE(QImageReader(jpg).size(), QSize(512, 512));
    QCOMPARE(QImageReader(jpg).format(), QByteArray("jpeg"));
}

void LibraryArtTest::logoMarginsAreTrimmedOnce()
{
    const QString id = QStringLiteral("switch-mario-kart-8-deluxe");
    const QString folder = m_data + QStringLiteral("/art/") + id;
    QDir().mkpath(folder);
    QImage padded(300, 100, QImage::Format_ARGB32);
    padded.fill(Qt::transparent);
    for (int y = 40; y < 60; ++y)
        for (int x = 100; x < 200; ++x)
            padded.setPixel(x, y, qRgba(255, 0, 0, 255));
    const QString path = folder + QStringLiteral("/logo.png");
    QVERIFY(padded.save(path));
    QImage tight(100, 20, QImage::Format_ARGB32);
    tight.fill(Qt::red);
    const QString tightPath = m_data + QStringLiteral("/art/switch-pikmin-4/logo.png");
    QDir().mkpath(QFileInfo(tightPath).absolutePath());
    QVERIFY(tight.save(tightPath));
    const QByteArray tightBytes = readFile(tightPath);
    const QDateTime tightTime = QFileInfo(tightPath).lastModified();

    {
        auto library = openLibrary();
        QTRY_COMPARE(QImageReader(path).size(), QSize(100, 20));
        QTRY_VERIFY(library->store().artToShrink(QStringLiteral("switch-pikmin-4")).isEmpty());
        QCOMPARE(readFile(tightPath), tightBytes);
        QCOMPARE(QFileInfo(tightPath).lastModified(), tightTime);
    }

    // Checked once: a padded logo slipped in under the same time isn't looked at again.
    const QDateTime time = QFileInfo(path).lastModified();
    QVERIFY(padded.save(path));
    QFile file(path);
    QVERIFY(file.open(QIODevice::ReadWrite));
    QVERIFY(file.setFileTime(time, QFileDevice::FileModificationTime));
    file.close();
    Library again(m_data, m_cache);
    QVERIFY(again.store().artToShrink(id).isEmpty());
}

void LibraryArtTest::artWithinTheLimitIsNotTouched()
{
    const QString id = QStringLiteral("switch-pikmin-4");
    const QString small = writeArt(id, QStringLiteral("logo"), 1280, 10);
    const QByteArray bytes = readFile(small);
    const QDateTime time = QFileInfo(small).lastModified();
    auto library = openLibrary();
    QTest::qWait(300);
    QCOMPARE(readFile(small), bytes);
    QCOMPARE(QFileInfo(small).lastModified(), time);
}

void LibraryArtTest::shownGameWaitsUntilTheSelectionMoves()
{
    const QString shown = QStringLiteral("switch-pikmin-4");
    const QString other = QStringLiteral("switch-mario-kart-8-deluxe");
    auto library = openLibrary();
    library->copyHeroesNear(library->rowOfGame(shown));
    const QString shownLogo = writeArt(shown, QStringLiteral("logo"), 2000, 100);
    const QString otherLogo = writeArt(other, QStringLiteral("logo"), 2000, 100);
    library->store().refresh();
    QTRY_COMPARE(QImageReader(otherLogo).size(), QSize(1280, 64));
    QTest::qWait(300);
    QCOMPARE(QImageReader(shownLogo).size(), QSize(2000, 100));

    library->copyHeroesNear(library->rowOfGame(other));
    QTRY_COMPARE(QImageReader(shownLogo).size(), QSize(1280, 64));
}

void LibraryArtTest::readOnlyLibraryIsLeftAlone()
{
    writeFile(m_data + QStringLiteral("/library/library.toml"), "version = 99\n");
    const QString path = writeArt(QStringLiteral("switch-pikmin-4"), QStringLiteral("logo"), 2000, 100);
    auto library = openLibrary();
    QVERIFY(library->readOnly());
    QTest::qWait(300);
    QCOMPARE(QImageReader(path).size(), QSize(2000, 100));
}

void LibraryArtTest::shrinksOnAQuietStartWithoutACache()
{
    // Once settled, nothing else changes on the next start, so the scan is the only trigger.
    { auto settled = openLibrary(); }
    QDir(m_cache).removeRecursively();
    const QString path = writeArt(QStringLiteral("switch-pikmin-4"), QStringLiteral("logo"), 2000, 100);
    auto library = openLibrary();
    QTRY_COMPARE(QImageReader(path).size(), QSize(1280, 64));
}

void LibraryArtTest::fileReplacedDuringTheShrinkKeepsTheReplacement()
{
    const QString id = QStringLiteral("switch-pikmin-4");
    auto library = openLibrary();
    library->copyHeroesNear(library->rowOfGame(id)); // the queue leaves it alone
    const QString path = writeArt(id, QStringLiteral("logo"), 2000, 100);
    library->store().refresh();
    QTRY_COMPARE(oversized(*library, id), QStringList{QStringLiteral("logo")});
    QTest::qWait(300);
    QCOMPARE(QImageReader(path).size(), QSize(2000, 100));

    // The result is applied on this thread, after the replacement below.
    QVERIFY(library->store().shrinkArt(id, QStringLiteral("logo")));
    writeArt(id, QStringLiteral("logo"), 3000, 100);
    QFile file(path);
    QVERIFY(file.open(QIODevice::ReadWrite));
    QVERIFY(file.setFileTime(QDateTime::currentDateTime().addSecs(60), QFileDevice::FileModificationTime));
    file.close();
    QSignalSpy spy(&library->store(), &LibraryStore::artShrunk);
    QVERIFY(spy.wait(5000));
    QCOMPARE(QImageReader(path).size(), QSize(3000, 100));
    QVERIFY(!QFileInfo::exists(QFileInfo(path).absolutePath() + QStringLiteral("/.logo.shrink")));
}

QTEST_MAIN(LibraryArtTest)
#include "library_art_test.moc"
