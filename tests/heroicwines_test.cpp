#include "heroicwines.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>

namespace {
void touch(const QString &path)
{
    QVERIFY(QDir().mkpath(QFileInfo(path).absolutePath()));
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
}
}

class HeroicWinesTest : public QObject
{
    Q_OBJECT

private slots:
    void listsHeroicThenSteamThenSystem()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString root = dir.path() + QStringLiteral("/heroic");
        touch(root + QStringLiteral("/tools/proton/Proton-GE-9/proton"));
        touch(root + QStringLiteral("/tools/proton/empty-folder/readme"));
        touch(root + QStringLiteral("/tools/wine/Wine-GE-8/bin/wine"));
        touch(root + QStringLiteral("/tools/wine/Wine-GE-8/bin/wineserver"));
        const QString steam = dir.path() + QStringLiteral("/compatibilitytools.d");
        touch(steam + QStringLiteral("/GE-Proton11-11/proton"));
        touch(dir.path() + QStringLiteral("/usr/bin/wine"));

        const QList<HeroicWineChoice> choices =
            HeroicWines::find(root, {steam, dir.path() + QStringLiteral("/missing")}, dir.path() + QStringLiteral("/usr/bin/wine"));
        QCOMPARE(choices.size(), 4);
        QCOMPARE(choices.at(0).wine.name, QStringLiteral("Proton-GE-9"));
        QCOMPARE(choices.at(0).wine.type, QStringLiteral("proton"));
        QCOMPARE(choices.at(0).wine.bin, root + QStringLiteral("/tools/proton/Proton-GE-9/proton"));
        QCOMPARE(choices.at(1).wine.type, QStringLiteral("wine"));
        QCOMPARE(choices.at(1).wine.wineserver, root + QStringLiteral("/tools/wine/Wine-GE-8/bin/wineserver"));
        QCOMPARE(choices.at(2).source, QStringLiteral("steam"));
        QCOMPARE(choices.at(2).wine.name, QStringLiteral("GE-Proton11-11"));
        QCOMPARE(choices.at(3).source, QStringLiteral("system"));
        QVERIFY(choices.at(3).wine.wineserver.isEmpty());
    }

    void nothingFoundIsEmpty()
    {
        QVERIFY(HeroicWines::find(QStringLiteral("/nonexistent/heroic"), {}, {}).isEmpty());
    }
};

QTEST_APPLESS_MAIN(HeroicWinesTest)
#include "heroicwines_test.moc"
