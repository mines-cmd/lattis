#include "steamachievements.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>

using namespace Qt::StringLiterals;

namespace {
const QString kFakeSteam = QStringLiteral(STEAM_DATA_DIR);

SteamInstall fakeInstall(const QString &user = u"39734273"_s)
{
    SteamInstall install;
    install.kind = SteamInstall::Kind::Native;
    install.root = kFakeSteam;
    install.userId = user;
    return install;
}

QStringList names(const QList<SteamAchievements::Achievement> &list)
{
    QStringList out;
    for (const SteamAchievements::Achievement &a : list)
        out.append(a.apiName);
    return out;
}

void writeRegistry(const QString &path, const QByteArray &language)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("\"Registry\"\n{\n\t\"HKCU\"\n\t{\n\t\t\"Software\"\n\t\t{\n\t\t\t\"Valve\"\n\t\t\t{\n\t\t\t\t\"Steam\"\n"
               "\t\t\t\t{\n\t\t\t\t\t\"language\"\t\t\""
               + language + "\"\n\t\t\t\t}\n\t\t\t}\n\t\t}\n\t}\n}\n");
}
}

class SteamAchievementsTest : public QObject
{
    Q_OBJECT

private slots:
    void unlockedFirstNewestFirst();
    void bitsFromEveryStat();
    void unlockTimes();
    void hiddenFlag();
    void descriptionAndIcons();
    void languageFallsBack();
    void noProgressFileMeansAllLocked();
    void noSchemaOrNoAchievements();
    void noSteamUser();
    void otherUsersProgressIsNotUsed();
    void steamLanguageNative();
    void steamLanguageFlatpak();
    void steamLanguageUnreadable();
};

void SteamAchievementsTest::unlockedFirstNewestFirst()
{
    const auto list = SteamAchievements::readAchievements(fakeInstall(), 9001, u"english"_s);
    // LONG_WALK (second stat) was unlocked after FIRST_STEP; the locked ones follow, hidden SECRET_ROOM last.
    QCOMPARE(names(list), QStringList({"LONG_WALK", "FIRST_STEP", "FINALE", "SECRET_ROOM"}));
    QVERIFY(list[0].unlocked);
    QVERIFY(list[1].unlocked);
    QVERIFY(!list[2].unlocked);
    QVERIFY(!list[3].unlocked);
}

void SteamAchievementsTest::bitsFromEveryStat()
{
    const auto list = SteamAchievements::readAchievements(fakeInstall(), 9001, u"english"_s);
    QCOMPARE(list.size(), 4);
    // Bit 0 of the second stat is a different achievement from bit 0 of the first.
    QCOMPARE(list[0].name, u"Long Walk"_s);
    QCOMPARE(list[1].name, u"First Step"_s);
}

void SteamAchievementsTest::unlockTimes()
{
    const auto list = SteamAchievements::readAchievements(fakeInstall(), 9001, u"english"_s);
    QCOMPARE(list[0].unlockTime, QDateTime::fromSecsSinceEpoch(1750000000));
    QCOMPARE(list[1].unlockTime, QDateTime::fromSecsSinceEpoch(1700000000));
    QVERIFY(!list[2].unlockTime.isValid());
}

void SteamAchievementsTest::hiddenFlag()
{
    const auto list = SteamAchievements::readAchievements(fakeInstall(), 9001, u"english"_s);
    QVERIFY(!list[1].hidden);
    QVERIFY(list[3].hidden);
    QCOMPARE(list[3].apiName, u"SECRET_ROOM"_s);
}

void SteamAchievementsTest::descriptionAndIcons()
{
    const auto list = SteamAchievements::readAchievements(fakeInstall(), 9001, u"german"_s);
    QCOMPARE(list[1].description, u"Verlasse das Dorf."_s);
    // Colored when unlocked, gray when locked.
    QCOMPARE(list[1].iconUrl,
             u"https://shared.fastly.steamstatic.com/community_assets/images/apps/9001/firststep.jpg"_s);
    QCOMPARE(list[3].iconUrl,
             u"https://shared.fastly.steamstatic.com/community_assets/images/apps/9001/secretroom_gray.jpg"_s);
}

void SteamAchievementsTest::languageFallsBack()
{
    const auto german = SteamAchievements::readAchievements(fakeInstall(), 9001, u"german"_s);
    QCOMPARE(german[1].name, u"Erster Schritt"_s);
    // No Spanish: English where there is some, whatever text exists where there isn't.
    const auto spanish = SteamAchievements::readAchievements(fakeInstall(), 9001, u"spanish"_s);
    QCOMPARE(spanish[1].name, u"First Step"_s);
    QCOMPARE(spanish[2].apiName, u"FINALE"_s);
    QCOMPARE(spanish[2].name, u"Finale"_s);
    QCOMPARE(spanish[2].description, u"Termine le jeu."_s);
}

void SteamAchievementsTest::noProgressFileMeansAllLocked()
{
    const auto list = SteamAchievements::readAchievements(fakeInstall(), 9002, u"english"_s);
    QCOMPARE(list.size(), 1);
    QCOMPARE(list[0].name, u"Only One"_s);
    QVERIFY(!list[0].unlocked);
    QVERIFY(!list[0].unlockTime.isValid());
    QVERIFY(list[0].iconUrl.endsWith(u"onlyone_gray.jpg"_s));
}

void SteamAchievementsTest::noSchemaOrNoAchievements()
{
    QVERIFY(SteamAchievements::readAchievements(fakeInstall(), 424242, u"english"_s).isEmpty());
    QVERIFY(SteamAchievements::readAchievements(fakeInstall(), 9003, u"english"_s).isEmpty());
    QVERIFY(SteamAchievements::readAchievements(fakeInstall(), 0, u"english"_s).isEmpty());
}

void SteamAchievementsTest::noSteamUser()
{
    QVERIFY(SteamAchievements::readAchievements(fakeInstall(QString()), 9001, u"english"_s).isEmpty());
    QVERIFY(SteamAchievements::readAchievements(SteamInstall(), 9001, u"english"_s).isEmpty());
}

void SteamAchievementsTest::otherUsersProgressIsNotUsed()
{
    // The other user has no progress file for 9001: everything is locked for them.
    const auto list = SteamAchievements::readAchievements(fakeInstall(u"39734274"_s), 9001, u"english"_s);
    QCOMPARE(list.size(), 4);
    for (const SteamAchievements::Achievement &a : list)
        QVERIFY(!a.unlocked);
    QCOMPARE(names(list), QStringList({"FIRST_STEP", "LONG_WALK", "FINALE", "SECRET_ROOM"}));
}

void SteamAchievementsTest::steamLanguageNative()
{
    QTemporaryDir home;
    writeRegistry(home.filePath(u".steam/registry.vdf"_s), "german");
    QCOMPARE(SteamAchievements::steamLanguage(fakeInstall(), home.path()), u"german"_s);
}

void SteamAchievementsTest::steamLanguageFlatpak()
{
    QTemporaryDir home;
    writeRegistry(home.filePath(u".steam/registry.vdf"_s), "german");
    writeRegistry(home.filePath(u".var/app/com.valvesoftware.Steam/.steam/registry.vdf"_s), "french");
    SteamInstall install = fakeInstall();
    install.kind = SteamInstall::Kind::Flatpak;
    QCOMPARE(SteamAchievements::steamLanguage(install, home.path()), u"french"_s);
}

void SteamAchievementsTest::steamLanguageUnreadable()
{
    QTemporaryDir home;
    QCOMPARE(SteamAchievements::steamLanguage(fakeInstall(), home.path()), u"english"_s);
    writeRegistry(home.filePath(u".steam/registry.vdf"_s), "");
    QCOMPARE(SteamAchievements::steamLanguage(fakeInstall(), home.path()), u"english"_s);
}

QTEST_GUILESS_MAIN(SteamAchievementsTest)

#include "steamachievements_test.moc"
