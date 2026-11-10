#include "heroicsaves.h"

#include <QDir>
#include <QTest>

#ifndef HEROIC_FIXTURE
#error HEROIC_FIXTURE must name tests/data/heroic
#endif

namespace {
const QString kRoot = QStringLiteral(HEROIC_FIXTURE);
const QString kUser = QStringLiteral("tester");
const QString kPrefix = QStringLiteral("/home/test/Games/Heroic/Prefixes/Stranded Deep");

HeroicSavesInfo read(const QString &appName)
{
    return HeroicSaves::read(kRoot, appName, kUser);
}
}

class HeroicSavesTest : public QObject
{
    Q_OBJECT

private slots:
    void protonUsesSteamuserAndItsOwnPrefix()
    {
        const HeroicSavesInfo info = read(QStringLiteral("StrandedApp"));
        QCOMPARE(info.prefix, kPrefix);
        QCOMPARE(info.wineUser, QStringLiteral("steamuser"));
        QCOMPARE(info.cloudSaveFolder, QStringLiteral("{AppData}/../LocalLow/Beam Team Games/Stranded Deep/"));
        QVERIFY(info.warnings.isEmpty());
        QVERIFY(info.autoSync);
        QVERIFY(!info.native);
    }

    void autoSyncIsOffWhenNeitherTheGameNorTheDefaultsSetIt()
    {
        QVERIFY(!read(QStringLiteral("WineApp")).autoSync);
    }

    void wineUsesTheSystemUserAndExpandsATildePrefix()
    {
        const HeroicSavesInfo info = read(QStringLiteral("WineApp"));
        QCOMPARE(info.prefix, QDir::homePath() + QStringLiteral("/Games/Heroic/Prefixes/Wine Game"));
        QCOMPARE(info.wineUser, kUser);
        QCOMPARE(info.cloudSaveFolder, QStringLiteral("{UserDir}/My Games/Wine Game"));
    }

    void aGameWithoutItsOwnPrefixUsesTheDefaultOne()
    {
        const HeroicSavesInfo info = read(QStringLiteral("SharedApp"));
        QCOMPARE(info.prefix, QStringLiteral("/home/test/Games/Heroic/Prefixes/default"));
        // The default's wine version is Wine, not Proton.
        QCOMPARE(info.wineUser, kUser);
    }

    void aGameWithoutAConfigFileGetsTheDefaults()
    {
        const HeroicSavesInfo info = read(QStringLiteral("InstallApp"));
        QCOMPARE(info.prefix, QStringLiteral("/home/test/Games/Heroic/Prefixes/default"));
        QCOMPARE(info.cloudSaveFolder, QStringLiteral("{InstallDir}/Saves"));
    }

    void noMetadataMeansNoCloudSaveFolder()
    {
        const HeroicSavesInfo info = read(QStringLiteral("NoMetaApp"));
        QCOMPARE(info.prefix, QStringLiteral("/home/test/Games/Heroic/Prefixes/A Short Hike"));
        QCOMPARE(info.wineUser, QStringLiteral("steamuser"));
        QVERIFY(info.cloudSaveFolder.isEmpty());
        QVERIFY(info.warnings.isEmpty());
    }

    void metadataWithoutTheAttributeMeansNoFolder()
    {
        const HeroicSavesInfo info = read(QStringLiteral("NoSaveApp"));
        QVERIFY(info.cloudSaveFolder.isEmpty());
        QVERIFY(info.warnings.isEmpty());
    }

    void badFilesWarnAndFallBack()
    {
        const HeroicSavesInfo info = read(QStringLiteral("BadApp"));
        QCOMPARE(info.warnings.size(), 2);
        QVERIFY(info.cloudSaveFolder.isEmpty());
        QCOMPARE(info.prefix, QStringLiteral("/home/test/Games/Heroic/Prefixes/default"));
    }

    void aMissingRootReadsNothing()
    {
        const HeroicSavesInfo info = HeroicSaves::read(kRoot + QStringLiteral("/nowhere"), QStringLiteral("X"), kUser);
        QVERIFY(info.prefix.isEmpty());
        QVERIFY(info.cloudSaveFolder.isEmpty());
        QVERIFY(info.warnings.isEmpty());
    }

    void expandsEpicsVariablesIntoTheWinePrefix()
    {
        const QString user = kPrefix + QStringLiteral("/drive_c/users/steamuser");
        const auto expand = [&](const QString &folder) {
            return HeroicSaves::expandCloudSaveFolder(folder, kPrefix, QStringLiteral("steamuser"),
                                                      QStringLiteral("/games/Game"));
        };
        QCOMPARE(expand(QStringLiteral("{AppData}/../LocalLow/Beam Team Games/Stranded Deep/")),
                 user + QStringLiteral("/AppData/LocalLow/Beam Team Games/Stranded Deep"));
        QCOMPARE(expand(QStringLiteral("{UserDir}/My Games/G")), user + QStringLiteral("/Documents/My Games/G"));
        QCOMPARE(expand(QStringLiteral("{usersavedgames}/G")), user + QStringLiteral("/Saved Games/G"));
        QCOMPARE(expand(QStringLiteral("{AppData}/G/{EpicID}")), user + QStringLiteral("/AppData/Local/G/*"));
        QCOMPARE(expand(QStringLiteral("{InstallDir}/Saves")), QStringLiteral("/games/Game/Saves"));
        QCOMPARE(expand(QStringLiteral("{AppData}\\G\\Saves")), user + QStringLiteral("/AppData/Local/G/Saves"));
    }

    void installDirNeedsNoPrefix()
    {
        QCOMPARE(HeroicSaves::expandCloudSaveFolder(QStringLiteral("{InstallDir}/Saves"), {}, {},
                                                    QStringLiteral("/games/Game")),
                 QStringLiteral("/games/Game/Saves"));
    }

    void unresolvedVariablesGiveNothing()
    {
        const QString installDir = QStringLiteral("/games/Game");
        QVERIFY(HeroicSaves::expandCloudSaveFolder(QStringLiteral("{AppData}/G"), {}, {}, installDir).isEmpty());
        QVERIFY(HeroicSaves::expandCloudSaveFolder(QStringLiteral("{InstallDir}/G"), kPrefix, QStringLiteral("u"), {})
                    .isEmpty());
        QVERIFY(HeroicSaves::expandCloudSaveFolder(QStringLiteral("{Nope}/G"), kPrefix, QStringLiteral("u"), installDir)
                    .isEmpty());
        QVERIFY(HeroicSaves::expandCloudSaveFolder({}, kPrefix, QStringLiteral("u"), installDir).isEmpty());
    }
};

QTEST_APPLESS_MAIN(HeroicSavesTest)

#include "heroicsaves_test.moc"
