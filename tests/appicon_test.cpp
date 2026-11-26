#include "appicon.h"
#include "appiconsync.h"
#include "desktopentry.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QTemporaryDir>
#include <QTest>

class AppIconTest : public QObject
{
    Q_OBJECT

private slots:
    void colorsReplaceThePlaceholders();
    void eachSizeHasItsDrawing();
    void unknownPlaceholdersFail();
    void sameColorsReuseTheFiles();
    void cleanUpKeepsWhatIsUsed();
    void entryRewrittenWhenOnlyTheIconDiffers();
    void managedEntryLeftAlone();
    void userCopyFollowsTheChoice();
    void deletedThemeFallsBack();

private:
    static AppIcon::Colors other()
    {
        return {QColor("#112233"), QColor("#445566"), QColor("#778899"), QColor("#aabbcc"), QColor("#ddeeff")};
    }
};

void AppIconTest::colorsReplaceThePlaceholders()
{
    for (int size : {16, 256}) {
        const QByteArray svg = AppIcon::drawing(other(), size);
        QVERIFY(!svg.isEmpty());
        QVERIFY(!svg.contains("{{"));
        for (const char *color : {"#112233", "#445566", "#778899", "#aabbcc", "#ddeeff"})
            QVERIFY2(svg.contains(color), color);
    }

    // The neck, barks and shines are worked out from the five; for Ember they land within a few
    // steps of the hand-picked colors of the original drawing (#f6e9df, #dcbba3, #6c7fa6).
    const QByteArray ember = AppIcon::drawing(AppIcon::ember(), 256);
    QVERIFY(ember.contains("#f98644"));
    QVERIFY(ember.contains("#e9cdb8"));
    QVERIFY(ember.contains("#f8eade"));
    QVERIFY(ember.contains("#dcbba2"));
    QVERIFY(ember.contains("#6c7ea6"));
    QVERIFY(AppIcon::drawing(AppIcon::ember(), 16).contains("#f0d9c8"));
}

void AppIconTest::eachSizeHasItsDrawing()
{
    const AppIcon::Colors colors = AppIcon::ember();
    // The small drawing up to 24 px; the margin from 48 px up.
    QCOMPARE(AppIcon::drawing(colors, 24), AppIcon::drawing(colors, 16));
    QVERIFY(AppIcon::drawing(colors, 24) != AppIcon::drawing(colors, 32));
    QVERIFY(AppIcon::drawing(colors, 32).contains("viewBox=\"0 0 256 256\""));
    QVERIFY(AppIcon::drawing(colors, 48).contains("viewBox=\"-14 -14 284 284\""));

    for (int size : AppIcon::sizes()) {
        const QImage image = AppIcon::render(colors, size);
        QCOMPARE(image.size(), QSize(size, size));
        // The tile's rounded corner is clear; its middle is opaque.
        QCOMPARE(qAlpha(image.pixel(0, 0)), 0);
        QCOMPARE(qAlpha(image.pixel(size / 2, size / 2)), 255);
    }
    const QImage large = AppIcon::render(colors, 512);
    // Inside the margin at the top edge's middle; the tile starts below it.
    QCOMPARE(qAlpha(large.pixel(256, 10)), 0);
    QCOMPARE(qAlpha(large.pixel(256, 40)), 255);
}

void AppIconTest::unknownPlaceholdersFail()
{
    QVERIFY(AppIcon::fill("<svg fill=\"{{fur}}\"/>", other()).isEmpty());
    QVERIFY(AppIcon::fill("<svg fill=\"{{mix coat fur 0.5}}\"/>", other()).isEmpty());
    QVERIFY(AppIcon::fill("<svg fill=\"{{shade coat x 1}}\"/>", other()).isEmpty());
    QCOMPARE(AppIcon::fill("<svg fill=\"{{mix coat details 0}}\" stroke=\"{{coat}}\"/>", other()),
             QByteArray("<svg fill=\"#778899\" stroke=\"#778899\"/>"));
}

void AppIconTest::sameColorsReuseTheFiles()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString folder = dir.filePath("icons/lattis");

    const QString path = AppIcon::write(folder, AppIcon::ember());
    const QString key = AppIcon::key(AppIcon::ember());
    QCOMPARE(path, QDir(folder).filePath(AppIcon::fileName(key, 256)));
    QCOMPARE(QFileInfo(path).fileName(), QStringLiteral("lattis-%1-256.png").arg(key));
    for (int size : AppIcon::sizes()) {
        const QImage image(QDir(folder).filePath(AppIcon::fileName(key, size)));
        QCOMPARE(image.size(), QSize(size, size));
    }

    // A file that is already there isn't rendered again.
    const QString small = QDir(folder).filePath(AppIcon::fileName(key, 16));
    QFile marker(small);
    QVERIFY(marker.open(QIODevice::WriteOnly));
    marker.write("kept");
    marker.close();
    QCOMPARE(AppIcon::write(folder, AppIcon::ember()), path);
    QCOMPARE(QFileInfo(small).size(), 4);

    const QString otherKey = AppIcon::key(other());
    QVERIFY(otherKey != key);
    QCOMPARE(AppIcon::key(other()), otherKey);
    QCOMPARE(AppIcon::write(folder, other()), QDir(folder).filePath(AppIcon::fileName(otherKey, 256)));
    QCOMPARE(QDir(folder).entryList(QDir::Files).size(), 2 * AppIcon::sizes().size());
}

void AppIconTest::cleanUpKeepsWhatIsUsed()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString folder = dir.path();
    AppIcon::write(folder, AppIcon::ember());
    AppIcon::write(folder, other());
    // What isn't a rendered icon stays: the AppImage's own icon, an emulator's.
    for (const char *name : {"lattis.png", "lattis-eden.png", "eden.png"}) {
        QFile file(QDir(folder).filePath(QString::fromLatin1(name)));
        QVERIFY(file.open(QIODevice::WriteOnly));
    }

    const QString keep = AppIcon::key(AppIcon::ember());
    QCOMPARE(AppIcon::cleanUp(folder, {keep}), int(AppIcon::sizes().size()));
    QStringList left = QDir(folder).entryList(QDir::Files);
    QCOMPARE(left.size(), AppIcon::sizes().size() + 3);
    QVERIFY(left.contains(AppIcon::fileName(keep, 512)));
    QVERIFY(!left.contains(AppIcon::fileName(AppIcon::key(other()), 512)));
    QCOMPARE(AppIcon::cleanUp(folder, {keep}), 0);
}

namespace {
void writeText(const QString &path, const QByteArray &text)
{
    QDir().mkpath(QFileInfo(path).path());
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
    file.write(text);
}

QByteArray readText(const QString &path)
{
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}

QString gearLever(const QString &appImage, const QString &applicationsDir)
{
    const QDir dir(applicationsDir);
    for (const QString &name : dir.entryList({QStringLiteral("gearlever_*.desktop")}, QDir::Files)) {
        if (readText(dir.filePath(name)).contains(appImage.toUtf8()))
            return QStringLiteral("Gear Lever");
    }
    return {};
}
}

void AppIconTest::entryRewrittenWhenOnlyTheIconDiffers()
{
    QTemporaryDir data;
    // Missing on purpose: reading the AppImage again would fail.
    const QString appImage = data.filePath(QStringLiteral("My Apps/Lattis.AppImage"));
    const QString entry = data.filePath(QStringLiteral("applications/lattis-lattis.desktop"));
    const QString extracted = data.filePath(QStringLiteral("icons/lattis/lattis.png"));
    writeText(entry, ("[Desktop Entry]\nName=Lattis\nExec=\"" + appImage + "\" %U\nIcon=" + extracted
                      + "\n\n[Desktop Action New]\nName=New\nExec=\"" + appImage + "\" --new\n")
                         .toUtf8());
    writeText(extracted, "old");

    AppIconSync::Job job;
    job.colors = other();
    job.dataHome = data.path();
    job.appImage = appImage;
    job.managedBy = gearLever;
    const AppIconSync::Result result = AppIconSync::run(job);
    QVERIFY2(result.error.isEmpty(), qPrintable(result.error));
    QCOMPARE(result.icon, data.filePath(QStringLiteral("icons/lattis/") + AppIcon::fileName(AppIcon::key(other()), 256)));
    const QByteArray text = readText(entry);
    QVERIFY(text.contains("\nIcon=" + result.icon.toUtf8() + '\n'));
    QVERIFY(text.contains(("Exec=\"" + appImage + "\" %U\n").toUtf8()));
    QVERIFY(text.contains(("Exec=\"" + appImage + "\" --new\n").toUtf8()));
    QVERIFY(text.contains("[Desktop Action New]\nName=New\n"));
    QCOMPARE(text.count("Icon="), 1);
    QVERIFY(!QFile::exists(extracted));

    QString error;
    QVERIFY(DesktopEntry::ensureOwn(appImage, data.path(), result.icon, &error).desktop.isEmpty());
    QVERIFY(error.isEmpty());

    // Back to Ember: the entry follows, and the other colors' files go.
    job.colors = AppIcon::ember();
    const AppIconSync::Result ember = AppIconSync::run(job);
    QVERIFY(readText(entry).contains("\nIcon=" + ember.icon.toUtf8() + '\n'));
    QVERIFY(!QFile::exists(result.icon));
    QCOMPARE(QDir(ember.folder).entryList(QDir::Files).size(), AppIcon::sizes().size());

    // Turned off: the entry goes, the icon stays for the window and the login entry.
    job.menuEntry = false;
    AppIconSync::run(job);
    QVERIFY(!QFile::exists(entry));
    QVERIFY(QFile::exists(ember.icon));
}

void AppIconTest::managedEntryLeftAlone()
{
    QTemporaryDir data;
    const QString appImage = data.filePath(QStringLiteral("Apps/Lattis.AppImage"));
    const QString entry = data.filePath(QStringLiteral("applications/lattis-lattis.desktop"));
    const QByteArray original = ("[Desktop Entry]\nName=Lattis\nExec=\"" + appImage + "\"\nIcon=lattis\n").toUtf8();
    writeText(entry, original);
    const QString manager = data.filePath(QStringLiteral("applications/gearlever_lattis_1234.desktop"));
    const QByteArray managed = ("[Desktop Entry]\nName=Lattis\nExec=" + appImage + "\nIcon=gearlever\n").toUtf8();
    writeText(manager, managed);

    AppIconSync::Job job;
    job.colors = other();
    job.dataHome = data.path();
    job.appImage = appImage;
    job.managedBy = gearLever;
    const AppIconSync::Result result = AppIconSync::run(job);
    QVERIFY(result.error.isEmpty());
    QCOMPARE(result.managedBy, QStringLiteral("Gear Lever"));
    QVERIFY(QFile::exists(result.icon));
    QCOMPARE(readText(entry), original);
    QCOMPARE(readText(manager), managed);
}

void AppIconTest::userCopyFollowsTheChoice()
{
    QTemporaryDir home;
    QTemporaryDir system;
    const QString installed = system.filePath(QStringLiteral("applications/lattis.desktop"));
    writeText(installed, "[Desktop Entry]\nType=Application\nName=Lattis\nExec=/usr/bin/lattis %U\nTryExec=lattis\n"
                         "Icon=lattis\n\n[Desktop Action New]\nName=New\nIcon=document-new\nExec=lattis --new\n");
    const QString copy = DesktopEntry::userCopyPath(home.path());

    AppIconSync::Job job;
    job.colors = AppIcon::ember();
    job.dataHome = home.path();
    job.dataDirs = {system.path()};
    AppIconSync::run(job);
    QVERIFY(!QFile::exists(copy));

    job.colors = other();
    const AppIconSync::Result result = AppIconSync::run(job);
    QVERIFY2(result.error.isEmpty(), qPrintable(result.error));
    QVERIFY(DesktopEntry::isUserCopy(copy));
    const QByteArray text = readText(copy);
    QVERIFY(text.contains("\nExec=/usr/bin/lattis %U\n"));
    QVERIFY(text.contains("\nTryExec=lattis\n"));
    QVERIFY(text.contains("\nIcon=" + result.icon.toUtf8() + '\n'));
    QVERIFY(text.contains("\nIcon=document-new\n"));
    QCOMPARE(DesktopEntry::systemEntry({home.path(), system.path()}), installed);

    // Back to Ember: the copy is deleted and the system entry shows again.
    job.colors = AppIcon::ember();
    AppIconSync::run(job);
    QVERIFY(!QFile::exists(copy));
    QVERIFY(QFile::exists(installed));

    // An entry there that Lattis didn't write (an install with the home folder as its prefix) stays.
    writeText(copy, "[Desktop Entry]\nName=Lattis\nExec=/home/me/.local/bin/lattis\nIcon=lattis\n");
    job.colors = other();
    QVERIFY(AppIconSync::run(job).error.isEmpty());
    job.colors = AppIcon::ember();
    AppIconSync::run(job);
    QCOMPARE(readText(copy), QByteArray("[Desktop Entry]\nName=Lattis\nExec=/home/me/.local/bin/lattis\nIcon=lattis\n"));
}

void AppIconTest::deletedThemeFallsBack()
{
    const QStringList available = {QStringLiteral("Ember"), QStringLiteral("Nord"), QStringLiteral("Mine")};
    QCOMPARE(AppIconSync::chosenColors(QStringLiteral("theme"), QStringLiteral("Nord"), available), QStringLiteral("Nord"));
    QCOMPARE(AppIconSync::chosenColors(QStringLiteral("Mine"), QStringLiteral("Nord"), available), QStringLiteral("Mine"));
    // Deleted or renamed: Match theme.
    QCOMPARE(AppIconSync::chosenColors(QStringLiteral("Gone"), QStringLiteral("Nord"), available), QStringLiteral("Nord"));
    QCOMPARE(AppIconSync::chosenColors(QStringLiteral("mine"), QStringLiteral("Ember"), available), QStringLiteral("Ember"));
}

QTEST_MAIN(AppIconTest)
#include "appicon_test.moc"
