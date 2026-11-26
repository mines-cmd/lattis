#include "appicon.h"

#include <QDir>
#include <QFile>
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

QTEST_MAIN(AppIconTest)
#include "appicon_test.moc"
