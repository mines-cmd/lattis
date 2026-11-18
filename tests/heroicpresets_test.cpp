#include "heroicpresets.h"

#include <QJsonObject>
#include <QTest>

namespace {
SteamPreset preset(const QString &id)
{
    for (const SteamPreset &p : builtInSteamPresets()) {
        if (p.id == id)
            return p;
    }
    return {};
}

SteamPreset own()
{
    SteamPreset p;
    p.id = QStringLiteral("hdr");
    p.name = QStringLiteral("HDR");
    p.env = {{QStringLiteral("DXVK_HDR"), QStringLiteral("1")}};
    p.wrapper = {QStringLiteral("gamescope"), QStringLiteral("-f"), QStringLiteral("--")};
    p.args = {QStringLiteral("-windowed")};
    return p;
}
}

class HeroicPresetsTest : public QObject
{
    Q_OBJECT

private slots:
    void mangoHudAndGameModeUseHeroicsSwitches()
    {
        HeroicGameSettings s;
        QVERIFY(!HeroicPresets::isOn(s, preset(QStringLiteral("mangohud"))));
        HeroicPresets::turnOn(s, preset(QStringLiteral("mangohud")));
        QVERIFY(s.showMangohud);
        QVERIFY(!s.useGameMode);
        QVERIFY(s.wrappers.isEmpty());
        QVERIFY(HeroicPresets::isOn(s, preset(QStringLiteral("mangohud"))));
        HeroicPresets::turnOn(s, preset(QStringLiteral("gamemode")));
        QVERIFY(s.useGameMode);
        HeroicPresets::turnOff(s, preset(QStringLiteral("mangohud")));
        QVERIFY(!s.showMangohud);
        QVERIFY(s.useGameMode);
    }

    void anOwnPresetFillsEnvironmentWrapperAndArguments()
    {
        HeroicGameSettings s;
        s.launcherArgs = QStringLiteral("-testarg");
        HeroicPresets::turnOn(s, own());
        QCOMPARE(s.environment.size(), 1);
        QCOMPARE(s.environment.at(0).toObject().value(QStringLiteral("key")).toString(), QStringLiteral("DXVK_HDR"));
        QCOMPARE(s.environment.at(0).toObject().value(QStringLiteral("value")).toString(), QStringLiteral("1"));
        QCOMPARE(s.wrappers.size(), 1);
        QCOMPARE(s.wrappers.at(0).toObject().value(QStringLiteral("exe")).toString(), QStringLiteral("gamescope"));
        QCOMPARE(s.wrappers.at(0).toObject().value(QStringLiteral("args")).toString(), QStringLiteral("-f --"));
        QCOMPARE(s.launcherArgs, QStringLiteral("-testarg -windowed"));
        QVERIFY(HeroicPresets::isOn(s, own()));
    }

    void turningOnTwiceChangesNothingAndOffGivesBackTheOriginal()
    {
        HeroicGameSettings s;
        s.launcherArgs = QStringLiteral("-testarg");
        s.environment.append(QJsonObject{{QStringLiteral("key"), QStringLiteral("A")}, {QStringLiteral("value"), QStringLiteral("b")}});
        const HeroicGameSettings before = s;
        HeroicPresets::turnOn(s, own());
        const HeroicGameSettings once = s;
        HeroicPresets::turnOn(s, own());
        QCOMPARE(s.environment, once.environment);
        QCOMPARE(s.wrappers, once.wrappers);
        QCOMPARE(s.launcherArgs, once.launcherArgs);
        HeroicPresets::turnOff(s, own());
        QVERIFY(!HeroicPresets::isOn(s, own()));
        QCOMPARE(s.environment, before.environment);
        QCOMPARE(s.wrappers, before.wrappers);
        QCOMPARE(s.launcherArgs, before.launcherArgs);
    }

    void aPartlyPresentPresetIsOff()
    {
        HeroicGameSettings s;
        s.environment.append(QJsonObject{{QStringLiteral("key"), QStringLiteral("DXVK_HDR")}, {QStringLiteral("value"), QStringLiteral("1")}});
        QVERIFY(!HeroicPresets::isOn(s, own()));
    }
};

QTEST_APPLESS_MAIN(HeroicPresetsTest)
#include "heroicpresets_test.moc"
