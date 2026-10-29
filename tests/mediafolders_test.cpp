#include "mediafolders.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QTemporaryDir>
#include <QTimeZone>
#include <QTest>

#include <memory>

using namespace Qt::StringLiterals;

class MediaFoldersTest : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void picturesAndVideosInNestedFolders();
    void hiddenFilesAndFoldersSkipped();
    void homeFolderPaths();
    void aMissingFolderIsSkipped();
    void symlinkedFoldersAreNotFollowed();
    void newestFirst();
    void overlappingFoldersListAFileOnce();
    void theLimitKeepsTheNewest();

private:
    QString belmont() const { return u"~/Videos/OBS/Belmont's Curse"_s; }
    QStringList names(const MediaFolders::Listing &listing) const;

    std::unique_ptr<QTemporaryDir> m_home;
};

namespace {
void copyTree(const QString &from, const QString &to)
{
    QDir().mkpath(to);
    QDirIterator it(from, QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden);
    while (it.hasNext()) {
        it.next();
        const QString target = to + u'/' + it.fileName();
        if (it.fileInfo().isDir())
            copyTree(it.filePath(), target);
        else
            QFile::copy(it.filePath(), target);
    }
}

// QFile::setFileTime() needs the file open; appending leaves its contents alone.
bool setTime(const QString &path, const QDateTime &time)
{
    QFile file(path);
    return file.open(QIODevice::Append) && file.setFileTime(time, QFileDevice::FileModificationTime);
}

void touch(const QString &path, const QDateTime &time)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("x");
    file.close();
    QVERIFY(setTime(path, time));
}
}

void MediaFoldersTest::init()
{

    m_home = std::make_unique<QTemporaryDir>();
    QVERIFY(m_home->isValid());
    copyTree(QStringLiteral(MEDIA_HOME_DIR), m_home->path());
}

QStringList MediaFoldersTest::names(const MediaFolders::Listing &listing) const
{
    QStringList out;
    for (const MediaFolders::File &f : listing.files)
        out.append(f.name);
    out.sort();
    return out;
}

void MediaFoldersTest::picturesAndVideosInNestedFolders()
{
    const MediaFolders::Listing listing = MediaFolders::list({belmont()}, m_home->path());
    QCOMPARE(names(listing),
             (QStringList{u"2026-10-01 20-00-00.mkv"_s, u"2026-10-02 21-00-00.mp4"_s, u"SHOT.PNG"_s, u"fight.webm"_s}));
    QVERIFY(!listing.limited);
    for (const MediaFolders::File &f : listing.files) {
        QCOMPARE(f.video, f.name != u"SHOT.PNG"_s);
        QVERIFY(f.time.isValid());
        QVERIFY(QFileInfo(f.path).isFile());
        QCOMPARE(QFileInfo(f.path).absolutePath(), f.folder);
    }

    const MediaFolders::Listing shots = MediaFolders::list({u"~/Pictures/Shots"_s}, m_home->path());
    QCOMPARE(names(shots), (QStringList{u"a.jpg"_s, u"b.jpeg"_s, u"c.webp"_s, u"d.bmp"_s, u"e.mov"_s}));
}

void MediaFoldersTest::hiddenFilesAndFoldersSkipped()
{
    const QStringList found = names(MediaFolders::list({belmont()}, m_home->path()));
    QVERIFY(!found.contains(u".hidden.png"_s));
    QVERIFY(!found.contains(u"inside.png"_s));
}

void MediaFoldersTest::homeFolderPaths()
{
    const MediaFolders::Listing viaTilde = MediaFolders::list({u"~/Pictures/Shots"_s}, m_home->path());
    const MediaFolders::Listing absolute = MediaFolders::list({m_home->path() + u"/Pictures/Shots"_s}, m_home->path());
    QCOMPARE(viaTilde.files, absolute.files);
    QCOMPARE(viaTilde.files.size(), 5);
    QCOMPARE(MediaFolders::list({u"~"_s}, m_home->path()).files.size(), 9);
}

void MediaFoldersTest::aMissingFolderIsSkipped()
{
    const MediaFolders::Listing listing = MediaFolders::list({u"~/Unplugged"_s, u""_s, u"~/Pictures/Shots"_s}, m_home->path());
    QCOMPARE(listing.files.size(), 5);
    QVERIFY(MediaFolders::list({u"~/Unplugged"_s}, m_home->path()).files.isEmpty());
    QVERIFY(MediaFolders::list({}, m_home->path()).files.isEmpty());
}

void MediaFoldersTest::symlinkedFoldersAreNotFollowed()
{
    const QString shots = m_home->path() + u"/Pictures/Shots"_s;
    QVERIFY(QFile::link(m_home->path(), shots + u"/loop"_s));
    QVERIFY(QFile::link(m_home->path() + u"/Videos"_s, shots + u"/videos"_s));
    const MediaFolders::Listing listing = MediaFolders::list({u"~/Pictures/Shots"_s}, m_home->path());
    QCOMPARE(listing.files.size(), 5);
    // A folder the user names themselves can be a link.
    QVERIFY(QFile::link(shots, m_home->path() + u"/ShotsLink"_s));
    QCOMPARE(MediaFolders::list({u"~/ShotsLink"_s}, m_home->path()).files.size(), 5);
}

void MediaFoldersTest::newestFirst()
{
    const QDateTime base(QDate(2026, 10, 1), QTime(12, 0), QTimeZone::UTC);
    const QString shots = m_home->path() + u"/Pictures/Shots/"_s;
    const QStringList order{u"c.webp"_s, u"a.jpg"_s, u"e.mov"_s, u"b.jpeg"_s, u"d.bmp"_s};
    for (int i = 0; i < order.size(); ++i)
        QVERIFY(setTime(shots + order[i], base.addDays(10 - i)));
    const MediaFolders::Listing listing = MediaFolders::list({u"~/Pictures/Shots"_s}, m_home->path());
    QStringList got;
    for (const MediaFolders::File &f : listing.files)
        got.append(f.name);
    QCOMPARE(got, order);
    QCOMPARE(listing.files.first().time, base.addDays(10));
}

void MediaFoldersTest::overlappingFoldersListAFileOnce()
{
    const MediaFolders::Listing listing = MediaFolders::list(
        {u"~/Pictures"_s, u"~/Pictures/Shots"_s, u"~/Pictures/Shots"_s}, m_home->path());
    QCOMPARE(listing.files.size(), 5);
}

void MediaFoldersTest::theLimitKeepsTheNewest()
{
    const QDateTime base(QDate(2026, 1, 1), QTime(0, 0), QTimeZone::UTC);
    const QString big = m_home->path() + u"/Big/"_s;
    const int total = MediaFolders::kLimit + 25;
    for (int i = 0; i < total; ++i)
        touch(big + u"sub%1/%2.png"_s.arg(i % 3).arg(i), base.addSecs(i));
    const MediaFolders::Listing listing = MediaFolders::list({u"~/Big"_s}, m_home->path());
    QCOMPARE(listing.files.size(), MediaFolders::kLimit);
    QVERIFY(listing.limited);
    QCOMPARE(listing.files.first().name, u"%1.png"_s.arg(total - 1));
    QCOMPARE(listing.files.last().name, u"25.png"_s);
    QVERIFY(!MediaFolders::list({u"~/Pictures/Shots"_s}, m_home->path()).limited);
}

QTEST_MAIN(MediaFoldersTest)
#include "mediafolders_test.moc"
