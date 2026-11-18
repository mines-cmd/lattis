#include "heroicgameconfig.h"

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QTest>

#ifndef HEROIC_FIXTURE
#error HEROIC_FIXTURE must name tests/data/heroic
#endif

namespace {
const QString kFixtures = QStringLiteral(HEROIC_FIXTURE);
const QString kApp = QStringLiteral("d6407c9e6fd54cb492b8c6635480d792");

QByteArray readFile(const QString &path)
{
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}

QJsonObject entryOf(const QString &root)
{
    return QJsonDocument::fromJson(readFile(HeroicGameConfig::path(root, kApp))).object().value(kApp).toObject();
}
}

class HeroicGameConfigTest : public QObject
{
    Q_OBJECT

    QTemporaryDir m_dir;

    QString root() const { return m_dir.path(); }

    // Puts a fixture where Heroic keeps it; an empty name leaves the game without a file.
    void install(const QString &fixture, bool withDefaults = true)
    {
        QDir(root()).removeRecursively();
        QVERIFY(QDir().mkpath(root() + QStringLiteral("/GamesConfig")));
        if (!fixture.isEmpty())
            QVERIFY(QFile::copy(kFixtures + u'/' + fixture, HeroicGameConfig::path(root(), kApp)));
        if (withDefaults)
            QVERIFY(QFile::copy(kFixtures + QStringLiteral("/config.json"), root() + QStringLiteral("/config.json")));
    }

private slots:
    void initTestCase() { QVERIFY(m_dir.isValid()); }

    void readsAFullEntry()
    {
        install(QStringLiteral("gamesconfig-full.json"));
        HeroicGameSettings s;
        QVERIFY(HeroicGameConfig::read(root(), kApp, s));
        QCOMPARE(s.launcherArgs, QStringLiteral("-testarg"));
        QVERIFY(s.showMangohud);
        QVERIFY(!s.useGameMode);
        QCOMPARE(s.wine.name, QStringLiteral("GE-Proton11-11"));
        QCOMPARE(s.wine.type, QStringLiteral("proton"));
        QVERIFY(s.wine.bin.endsWith(QStringLiteral("/GE-Proton11-11/proton")));
    }

    void emptyEntryReadsAsDefaults()
    {
        install(QStringLiteral("gamesconfig-empty.json"));
        HeroicGameSettings s;
        QVERIFY(HeroicGameConfig::read(root(), kApp, s));
        QVERIFY(s.launcherArgs.isEmpty());
        QCOMPARE(s.wine.name, QStringLiteral("Wine-GE-8"));
        QCOMPARE(s.wine.type, QStringLiteral("wine"));
    }

    void missingFileReadsAsDefaults()
    {
        install({});
        HeroicGameSettings s;
        QVERIFY(HeroicGameConfig::read(root(), kApp, s));
        QCOMPARE(s.wine.name, QStringLiteral("Wine-GE-8"));
    }

    void noDefaultsReadsAsEmpty()
    {
        install({}, false);
        HeroicGameSettings s;
        QVERIFY(HeroicGameConfig::read(root(), kApp, s));
        QCOMPARE(s.wine, HeroicWine{});
        QVERIFY(!s.showMangohud);
    }

    void badJsonIsRefusedAndLeftAlone()
    {
        install(QStringLiteral("gamesconfig-bad.json"));
        const QByteArray before = readFile(HeroicGameConfig::path(root(), kApp));
        HeroicGameSettings s;
        QString error;
        QVERIFY(!HeroicGameConfig::read(root(), kApp, s, &error));
        QVERIFY(!error.isEmpty());

        HeroicGameEdits edits;
        edits.launcherArgs = QStringLiteral("-x");
        error.clear();
        QVERIFY(!HeroicGameConfig::write(root(), kApp, edits, &error));
        QVERIFY(!error.isEmpty());
        QCOMPARE(readFile(HeroicGameConfig::path(root(), kApp)), before);
    }

    void writeChangesOnlyTheEditedKey()
    {
        install(QStringLiteral("gamesconfig-full.json"));
        const QByteArray before = readFile(HeroicGameConfig::path(root(), kApp));
        HeroicGameEdits edits;
        edits.launcherArgs = QStringLiteral("-windowed \"x\"");
        QVERIFY(HeroicGameConfig::write(root(), kApp, edits));

        QByteArray expected = before;
        expected.replace("\"-testarg\"", "\"-windowed \\\"x\\\"\"");
        QCOMPARE(readFile(HeroicGameConfig::path(root(), kApp)), expected);
    }

    void unknownKeysAndOrderSurvive()
    {
        install(QStringLiteral("gamesconfig-unknown-keys.json"));
        const QByteArray before = readFile(HeroicGameConfig::path(root(), kApp));
        HeroicGameEdits edits;
        edits.showMangohud = false;
        edits.useGameMode = true;
        edits.environment = QJsonArray{QJsonObject{{"key", "A"}, {"value", "1"}}};
        QVERIFY(HeroicGameConfig::write(root(), kApp, edits));

        const QByteArray after = readFile(HeroicGameConfig::path(root(), kApp));
        const QJsonObject entry = entryOf(root());
        QVERIFY(entry.contains(QStringLiteral("someFutureKey")));
        QCOMPARE(entry.value(QStringLiteral("someFutureKey")).toObject().value(QStringLiteral("nested")).toArray().size(),
                 2);
        QCOMPARE(entry.value(QStringLiteral("launcherArgs")).toString(), QStringLiteral("-testarg"));
        QVERIFY(!entry.value(QStringLiteral("showMangohud")).toBool());
        QVERIFY(entry.value(QStringLiteral("useGameMode")).toBool());
        QCOMPARE(entry.value(QStringLiteral("enviromentOptions")).toArray().size(), 1);
        QVERIFY(after.endsWith(before.right(60)));
        QVERIFY(after.indexOf("autoInstallDxvk") < after.indexOf("someFutureKey"));
        QVERIFY(after.indexOf("launcherArgs") < after.indexOf("wineVersion"));

        HeroicGameEdits revert;
        revert.useGameMode = false;
        revert.showMangohud = true;
        revert.environment = QJsonArray();
        QVERIFY(HeroicGameConfig::write(root(), kApp, revert));
        QCOMPARE(readFile(HeroicGameConfig::path(root(), kApp)), before);
    }

    void absentKeysAreInsertedAndKeepTheLayout()
    {
        install(QStringLiteral("gamesconfig-full.json"));
        QByteArray text = readFile(HeroicGameConfig::path(root(), kApp));
        const qsizetype from = text.indexOf("    \"launcherArgs\"");
        text.remove(from, text.indexOf('\n', from) + 1 - from);
        QFile file(HeroicGameConfig::path(root(), kApp));
        QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
        file.write(text);
        file.close();

        HeroicGameEdits edits;
        edits.launcherArgs = QStringLiteral("-new");
        QVERIFY(HeroicGameConfig::write(root(), kApp, edits));
        const QByteArray after = readFile(HeroicGameConfig::path(root(), kApp));
        QVERIFY(after.contains("\n    \"launcherArgs\": \"-new\"\n  },"));
        QCOMPARE(entryOf(root()).value(QStringLiteral("launcherArgs")).toString(), QStringLiteral("-new"));
    }

    void wineEditKeepsTheRestOfTheObject()
    {
        install(QStringLiteral("gamesconfig-full.json"));
        HeroicGameEdits edits;
        edits.wine = HeroicWine{QStringLiteral("/w/wine"), QStringLiteral("Wine-9"), QStringLiteral("wine")};
        QVERIFY(HeroicGameConfig::write(root(), kApp, edits));

        HeroicGameSettings s;
        QVERIFY(HeroicGameConfig::read(root(), kApp, s));
        QCOMPARE(s.wine, *edits.wine);
        QCOMPARE(entryOf(root()).value(QStringLiteral("winePrefix")).toString(),
                 QStringLiteral("/home/user/Games/Heroic/Prefixes/A Short Hike"));
    }

    void emptyEntryBecomesAFullEntry()
    {
        install(QStringLiteral("gamesconfig-empty.json"));
        HeroicGameEdits edits;
        edits.launcherArgs = QStringLiteral("-a");
        edits.showMangohud = true;
        QVERIFY(HeroicGameConfig::write(root(), kApp, edits));

        const QJsonObject file = QJsonDocument::fromJson(readFile(HeroicGameConfig::path(root(), kApp))).object();
        QCOMPARE(file.value(QStringLiteral("version")).toString(), QStringLiteral("v0"));
        QVERIFY(file.value(QStringLiteral("explicit")).toBool());
        const QJsonObject entry = file.value(kApp).toObject();
        QCOMPARE(entry.value(QStringLiteral("launcherArgs")).toString(), QStringLiteral("-a"));
        QVERIFY(entry.value(QStringLiteral("showMangohud")).toBool());
        QCOMPARE(entry.value(QStringLiteral("wineVersion")).toObject().value(QStringLiteral("name")).toString(),
                 QStringLiteral("Wine-GE-8"));
        QVERIFY(entry.contains(QStringLiteral("winePrefix")));
    }

    void missingFileIsCreated()
    {
        install({});
        HeroicGameEdits edits;
        edits.useGameMode = true;
        QVERIFY(HeroicGameConfig::write(root(), kApp, edits));
        HeroicGameSettings s;
        QVERIFY(HeroicGameConfig::read(root(), kApp, s));
        QVERIFY(s.useGameMode);
    }

    void editsRoundTripThroughJsonAndMerge()
    {
        HeroicGameEdits a;
        a.launcherArgs = QStringLiteral("-a");
        a.useGameMode = true;
        a.wine = HeroicWine{QStringLiteral("/w/wine"), QStringLiteral("Wine-9"), QStringLiteral("wine"),
                            QStringLiteral("/w/server")};
        const HeroicGameEdits back = HeroicGameConfig::editsFromJson(HeroicGameConfig::editsToJson(a));
        QCOMPARE(*back.launcherArgs, QStringLiteral("-a"));
        QVERIFY(*back.useGameMode);
        QVERIFY(!back.showMangohud);
        QCOMPARE(back.wine->wineserver, QStringLiteral("/w/server"));
        QVERIFY(HeroicGameConfig::isEmpty({}));
        QVERIFY(!HeroicGameConfig::isEmpty(a));

        HeroicGameEdits later;
        later.launcherArgs = QStringLiteral("-b");
        later.showMangohud = true;
        HeroicGameConfig::merge(a, later);
        QCOMPARE(*a.launcherArgs, QStringLiteral("-b"));
        QVERIFY(*a.showMangohud);
        QVERIFY(*a.useGameMode);
    }

    void otherGamesInTheFileStay()
    {
        install({});
        QFile file(HeroicGameConfig::path(root(), kApp));
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("{\n  \"other\": {\n    \"showFps\": true\n  },\n  \"version\": \"v0\",\n  \"explicit\": true\n}");
        file.close();

        HeroicGameEdits edits;
        edits.launcherArgs = QStringLiteral("-b");
        QVERIFY(HeroicGameConfig::write(root(), kApp, edits));
        const QJsonObject doc = QJsonDocument::fromJson(readFile(HeroicGameConfig::path(root(), kApp))).object();
        QVERIFY(doc.value(QStringLiteral("other")).toObject().value(QStringLiteral("showFps")).toBool());
        QCOMPARE(doc.value(kApp).toObject().value(QStringLiteral("launcherArgs")).toString(), QStringLiteral("-b"));
    }
};

QTEST_APPLESS_MAIN(HeroicGameConfigTest)

#include "heroicgameconfig_test.moc"
