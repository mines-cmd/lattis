#include "librarytestbase.h"

#include <QImage>
#include <QSignalSpy>

// The display copies of big hero art (see HeroCache).
class LibraryHeroTest : public LibraryTestBase
{
    Q_OBJECT

private slots:
    void copyIsUsedForTheList();
    void copyFoundAtLoad();
    void replacedArtMakesANewCopyAndPrunesTheOld();
    void narrowPngGetsACopy();
    void narrowBaselineJpegKeepsItsUrl();

private:
    void writeHero(const QString &id, int width, const QColor &color = Qt::red, const QString &file = QStringLiteral("hero.png"));
    QString heroFolder() const { return m_cache + QStringLiteral("/heroes"); }
};

void LibraryHeroTest::writeHero(const QString &id, int width, const QColor &color, const QString &file)
{
    QDir().mkpath(m_data + QStringLiteral("/art/") + id);
    QImage image(width, width / 3, QImage::Format_RGB32);
    image.fill(color);
    QVERIFY(image.save(m_data + QStringLiteral("/art/") + id + QStringLiteral("/") + file));
}

void LibraryHeroTest::copyIsUsedForTheList()
{
    const QString id = QStringLiteral("switch-pikmin-4");
    writeHero(id, 3840);
    auto library = openLibrary();
    const int row = library->rowOfGame(id);
    QVERIFY(row >= 0);

    QTRY_VERIFY(library->gameAt(row).value("heroUrl") != library->gameAt(row).value("heroArt"));
    const QVariantMap game = library->gameAt(row);
    QVERIFY(game.value("heroUrl").toUrl().toLocalFile().startsWith(heroFolder()));
    QVERIFY(game.value("heroUrl").toUrl().query().startsWith(u"v="));
    QVERIFY(game.value("heroArt").toUrl().toLocalFile().startsWith(m_data));
    // The editor and bulk lookup keep the original.
    QCOMPARE(library->gameForEditing(id).value("art").toMap().value("hero").toString(),
             game.value("heroArt").toUrl().toString());
}

void LibraryHeroTest::copyFoundAtLoad()
{
    const QString id = QStringLiteral("switch-pikmin-4");
    writeHero(id, 3840);
    {
        auto library = openLibrary();
        QTRY_VERIFY(!QDir(heroFolder()).entryList(QDir::Files).isEmpty());
        QTRY_VERIFY(library->gameAt(library->rowOfGame(id)).value("heroUrl")
                    != library->gameAt(library->rowOfGame(id)).value("heroArt"));
    }
    Library again(m_data, m_cache);
    const QVariantMap game = again.gameAt(again.rowOfGame(id));
    QVERIFY(game.value("heroUrl").toUrl().toLocalFile().startsWith(heroFolder()));
}

void LibraryHeroTest::replacedArtMakesANewCopyAndPrunesTheOld()
{
    const QString id = QStringLiteral("switch-pikmin-4");
    writeHero(id, 3840);
    auto library = openLibrary();
    QTRY_VERIFY(QDir(heroFolder()).entryList(QDir::Files).size() == 1);
    const QString first = QDir(heroFolder()).entryList(QDir::Files).constFirst();

    QTest::qWait(1100);
    writeHero(id, 3600, Qt::blue);
    library->store().refresh();
    library->store().waitForIdle();
    QTRY_VERIFY(QDir(heroFolder()).entryList(QDir::Files) != QStringList{first});
    QTRY_COMPARE(QDir(heroFolder()).entryList(QDir::Files).size(), 1);
    QVERIFY(!QDir(heroFolder()).exists(first));
}

void LibraryHeroTest::narrowPngGetsACopy()
{
    const QString id = QStringLiteral("switch-pikmin-4");
    writeHero(id, 1920);
    auto library = openLibrary();
    const int row = library->rowOfGame(id);
    QTRY_VERIFY(library->gameAt(row).value("heroUrl") != library->gameAt(row).value("heroArt"));
    QVERIFY(library->gameAt(row).value("heroUrl").toUrl().toLocalFile().startsWith(heroFolder()));
}

void LibraryHeroTest::narrowBaselineJpegKeepsItsUrl()
{
    const QString id = QStringLiteral("switch-pikmin-4");
    writeHero(id, 1920, Qt::red, QStringLiteral("hero.jpg"));
    auto library = openLibrary();
    QTest::qWait(300);
    const QVariantMap game = library->gameAt(library->rowOfGame(id));
    QCOMPARE(game.value("heroUrl"), game.value("heroArt"));
    QVERIFY(!QFileInfo::exists(heroFolder()) || QDir(heroFolder()).entryList(QDir::Files).isEmpty());
}

QTEST_MAIN(LibraryHeroTest)
#include "library_hero_test.moc"
