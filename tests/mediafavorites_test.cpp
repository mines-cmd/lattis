#include "mediafavorites.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>

#include <memory>

using namespace Qt::StringLiterals;

class MediaFavoritesTest : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void roundTrip();
    void homeFolderIsWrittenAsTilde();
    void tildeEntriesAreExpanded();
    void aMissingFileIsEmpty();
    void aBadFileIsAnError();
    void wrongTypesAreSkipped();
    void aClipFolderIsAnEntry();
    void movedFilesKeepTheirEntry();
    void writeLeavesNoPartialFile();

private:
    QString file() const { return m_dir->path() + u"/library/media.toml"_s; }
    QString home() const { return m_dir->path() + u"/home"_s; }
    QByteArray bytes() const
    {
        QFile f(file());
        return f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray();
    }

    std::unique_ptr<QTemporaryDir> m_dir;
};

void MediaFavoritesTest::init()
{
    m_dir = std::make_unique<QTemporaryDir>();
    QVERIFY(m_dir->isValid());
}

void MediaFavoritesTest::roundTrip()
{
    const QSet<QString> set{u"/mnt/shots/a.png"_s, u"/mnt/shots/b \"quoted\".jpg"_s, u"/mnt/clips/c.mp4"_s};
    QVERIFY(MediaFavorites::write(file(), set, home()));
    QCOMPARE(MediaFavorites::read(file(), home()).value, set);
}

void MediaFavoritesTest::homeFolderIsWrittenAsTilde()
{
    QVERIFY(MediaFavorites::write(file(), {home() + u"/Pictures/a.png"_s, home() + u"x/b.png"_s}, home()));
    const QByteArray text = bytes();
    QVERIFY(text.contains("~/Pictures/a.png"));
    QVERIFY(!text.contains(home().toUtf8() + "/Pictures"));
    QVERIFY(text.contains(home().toUtf8() + "x/b.png"));
}

void MediaFavoritesTest::tildeEntriesAreExpanded()
{
    QDir().mkpath(m_dir->path() + u"/library"_s);
    QFile f(file());
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write("favorites = [\"~/Videos/a.mp4\", \"/abs/b.png\"]\n");
    f.close();
    const QSet<QString> expected{home() + u"/Videos/a.mp4"_s, u"/abs/b.png"_s};
    QCOMPARE(MediaFavorites::read(file(), home()).value, expected);
}

void MediaFavoritesTest::aMissingFileIsEmpty()
{
    QVERIFY(MediaFavorites::read(file(), home()).ok);
    QVERIFY(MediaFavorites::read(file(), home()).value.isEmpty());
}

void MediaFavoritesTest::aBadFileIsAnError()
{
    QDir().mkpath(m_dir->path() + u"/library"_s);
    QFile f(file());
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write("favorites = [\"a\", \n not toml");
    f.close();
    const Parsed<QSet<QString>> read = MediaFavorites::read(file(), home());
    QVERIFY(!read.ok);
    QVERIFY(read.value.isEmpty());
    QVERIFY(read.warnings.value(0).contains(u"media.toml:"_s));
}

void MediaFavoritesTest::wrongTypesAreSkipped()
{
    QDir().mkpath(m_dir->path() + u"/library"_s);
    QFile f(file());
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write("favorites = [1, \"\", \"/ok.png\", true]\n");
    f.close();
    const QSet<QString> expected{u"/ok.png"_s};
    QCOMPARE(MediaFavorites::read(file(), home()).value, expected);

    QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Truncate));
    f.write("favorites = \"/ok.png\"\n");
    f.close();
    QVERIFY(MediaFavorites::read(file(), home()).value.isEmpty());
}

void MediaFavoritesTest::aClipFolderIsAnEntry()
{
    const QString clip = home() + u"/.local/share/Steam/userdata/1/gamerecordings/clips/clip_1145360_20261012_211533"_s;
    QVERIFY(MediaFavorites::write(file(), {clip}, home()));
    QCOMPARE(MediaFavorites::read(file(), home()).value, QSet<QString>{clip});
}

void MediaFavoritesTest::movedFilesKeepTheirEntry()
{
    const QString gone = u"/mnt/old/deleted.png"_s;
    QVERIFY(MediaFavorites::write(file(), {gone, u"/mnt/new/b.png"_s}, home()));
    QVERIFY(MediaFavorites::read(file(), home()).value.contains(gone));
}

void MediaFavoritesTest::writeLeavesNoPartialFile()
{
    QVERIFY(MediaFavorites::write(file(), {u"/a.png"_s}, home()));
    QVERIFY(MediaFavorites::write(file(), {u"/b.png"_s}, home()));
    QVERIFY(!QFile::exists(file() + u".partial"_s));
    QCOMPARE(MediaFavorites::read(file(), home()).value, QSet<QString>{u"/b.png"_s});
}

QTEST_MAIN(MediaFavoritesTest)
#include "mediafavorites_test.moc"
