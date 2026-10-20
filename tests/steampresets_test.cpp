#include "steampresets.h"

#include <QTest>

using namespace Qt::StringLiterals;

class SteamPresetsTest : public QObject
{
    Q_OBJECT

private slots:
    void defaults();
    void parseSection();
    void userPresets();
    void overrideBuiltIn();
    void badValues_data();
    void badValues();
    void unknownDefaultPreset();
    void words();
    void describe();
    void isOn_data();
    void isOn();
    void turnOn_data();
    void turnOn();
    void turnOff_data();
    void turnOff();
    void roundTrip_data();
    void roundTrip();
};

namespace {

Parsed<SteamSettings> parse(const QByteArray &text)
{
    return parseSteamSettings(text, u"settings.toml"_s);
}

SteamPreset builtIn(const QString &id)
{
    for (const SteamPreset &p : builtInSteamPresets()) {
        if (p.id == id)
            return p;
    }
    qFatal("no preset %s", qPrintable(id));
}

SteamPreset preset(const QStringList &env, const QStringList &wrapper, const QStringList &args)
{
    SteamPreset p;
    p.id = u"test"_s;
    for (const QString &e : env)
        p.env.append({e.section(u'=', 0, 0), e.section(u'=', 1)});
    p.wrapper = wrapper;
    p.args = args;
    return p;
}

// The presets the string tests use, by name.
SteamPreset named(const QString &name)
{
    if (name == u"hdr")
        return preset({u"PROTON_ENABLE_HDR=1"_s, u"DXVK_HDR=1"_s}, {}, {});
    if (name == u"gamescope")
        return preset({}, {u"gamescope"_s, u"-f"_s, u"--"_s}, {});
    if (name == u"skip")
        return preset({}, {}, {u"-skipintro"_s});
    if (name == u"spaced")
        return preset({u"WINEDLLOVERRIDES=dinput8=n,b"_s, u"TITLE=My Game"_s}, {u"/opt/my tools/run"_s}, {});
    return builtIn(name);
}

} // namespace

void SteamPresetsTest::defaults()
{
    const Parsed<SteamSettings> parsed = parse("[launch]\nwhile_running = \"stay\"\n");
    QVERIFY(parsed.ok);
    QVERIFY(parsed.warnings.isEmpty());
    QVERIFY(parsed.value.enabled);
    QVERIFY(parsed.value.path.isEmpty());
    QVERIFY(parsed.value.user.isEmpty());
    QVERIFY(parsed.value.defaultPresets.isEmpty());
    QCOMPARE(parsed.value.presetIds(), (QStringList{u"mangohud"_s, u"gamemode"_s, u"wayland"_s}));
    QCOMPARE(parsed.value.preset(u"gamemode"_s)->wrapper, QStringList{u"gamemoderun"_s});
    QCOMPARE(parsed.value.preset(u"wayland"_s)->env.first().first, u"PROTON_ENABLE_WAYLAND"_s);
    QVERIFY(parsed.value.preset(u"mangohud"_s)->builtIn);
    QVERIFY(!parsed.value.preset(u"missing"_s));
}

void SteamPresetsTest::parseSection()
{
    const Parsed<SteamSettings> parsed = parse("[steam]\n"
                                               "enabled = false\n"
                                               "path = \"~/.local/share/Steam\"\n"
                                               "user = 151861781\n"
                                               "default_presets = [\"mangohud\", \"wayland\", \"mangohud\"]\n"
                                               "default_proton = \"GE-Proton10-4\"\n");
    QVERIFY(parsed.warnings.isEmpty());
    QCOMPARE(parsed.value.defaultProton, u"GE-Proton10-4"_s);
    QVERIFY(parse("[steam]\n").value.defaultProton.isEmpty());
    QVERIFY(!parsed.value.enabled);
    QCOMPARE(parsed.value.path, u"~/.local/share/Steam"_s);
    QCOMPARE(parsed.value.user, u"151861781"_s);
    QCOMPARE(parsed.value.defaultPresets, (QStringList{u"mangohud"_s, u"wayland"_s}));
    QCOMPARE(parse("[steam]\nuser = \"151861781\"\n").value.user, u"151861781"_s);
}

void SteamPresetsTest::userPresets()
{
    const Parsed<SteamSettings> parsed = parse("[steam]\n"
                                               "default_presets = [\"gamescope\", \"hdr\"]\n"
                                               "\n"
                                               "[steam.presets.gamescope]\n"
                                               "name = \"Gamescope\"\n"
                                               "wrapper = [\"gamescope\", \"-f\", \"--\"]\n"
                                               "\n"
                                               "[steam.presets.hdr]\n"
                                               "name = \"HDR\"\n"
                                               "env = { PROTON_ENABLE_HDR = \"1\", DXVK_HDR = \"1\" }\n"
                                               "\n"
                                               "[steam.presets.skip]\n"
                                               "args = \"-skipintro -nosplash\"\n");
    QVERIFY2(parsed.warnings.isEmpty(), qPrintable(parsed.warnings.join(u'\n')));
    const SteamSettings &s = parsed.value;
    // Defaults can name presets defined further down.
    QCOMPARE(s.defaultPresets, (QStringList{u"gamescope"_s, u"hdr"_s}));
    QCOMPARE(s.presets.size(), 6);
    QCOMPARE(s.preset(u"gamescope"_s)->name, u"Gamescope"_s);
    QCOMPARE(s.preset(u"gamescope"_s)->wrapper, (QStringList{u"gamescope"_s, u"-f"_s, u"--"_s}));
    QVERIFY(!s.preset(u"gamescope"_s)->builtIn);
    QCOMPARE(s.preset(u"hdr"_s)->env.size(), 2);
    QCOMPARE(s.preset(u"hdr"_s)->describe(), u"DXVK_HDR=1 PROTON_ENABLE_HDR=1 %command%"_s);
    // A string is split into words; no name falls back to the id.
    QCOMPARE(s.preset(u"skip"_s)->args, (QStringList{u"-skipintro"_s, u"-nosplash"_s}));
    QCOMPARE(s.preset(u"skip"_s)->name, u"skip"_s);
}

void SteamPresetsTest::overrideBuiltIn()
{
    const Parsed<SteamSettings> parsed = parse("[steam.presets.mangohud]\n"
                                               "wrapper = [\"mangohud\", \"--dlsym\"]\n"
                                               "[steam.presets.wayland]\n"
                                               "name = \"Native Wayland\"\n");
    QVERIFY2(parsed.warnings.isEmpty(), qPrintable(parsed.warnings.join(u'\n')));
    const SteamSettings &s = parsed.value;
    QCOMPARE(s.presets.size(), 3);
    const SteamPreset *mangohud = s.preset(u"mangohud"_s);
    QCOMPARE(mangohud->wrapper, (QStringList{u"mangohud"_s, u"--dlsym"_s}));
    QCOMPARE(mangohud->name, u"MangoHud"_s); // kept from the built-in
    QVERIFY(mangohud->builtIn);
    const SteamPreset *wayland = s.preset(u"wayland"_s);
    QCOMPARE(wayland->name, u"Native Wayland"_s);
    QCOMPARE(wayland->env, builtIn(u"wayland"_s).env);
}

void SteamPresetsTest::badValues_data()
{
    QTest::addColumn<QByteArray>("text");
    QTest::addColumn<QString>("warning");
    QTest::newRow("not a section") << QByteArray("steam = 1\n") << u"settings.toml:1: steam must be a [steam] section"_s;
    QTest::newRow("enabled") << QByteArray("[steam]\nenabled = \"yes\"\n")
                             << u"settings.toml:2: enabled must be true or false"_s;
    QTest::newRow("path") << QByteArray("[steam]\npath = 3\n")
                          << u"settings.toml:2: path must be a folder, like \"~/.local/share/Steam\""_s;
    QTest::newRow("user") << QByteArray("[steam]\nuser = true\n")
                          << u"settings.toml:2: user must be an account id, the number of a folder in Steam's userdata"_s;
    QTest::newRow("defaults") << QByteArray("[steam]\ndefault_presets = \"mangohud\"\n")
                              << u"settings.toml:2: default_presets must be a list of preset ids, like [\"mangohud\"]"_s;
    QTest::newRow("default proton") << QByteArray("[steam]\ndefault_proton = true\n")
                                    << u"settings.toml:2: default_proton must be a compatibility tool's internal name, "
                                       "like \"proton_experimental\""_s;
    QTest::newRow("unknown key") << QByteArray("[steam]\nenable = true\n")
                                 << u"settings.toml:2: unknown key enable — did you mean \"enabled\"?"_s;
    QTest::newRow("empty preset") << QByteArray("[steam.presets.nothing]\nname = \"Nothing\"\n")
                                  << u"settings.toml:1: preset nothing adds nothing: give it env, wrapper or args"_s;
    QTest::newRow("bad id") << QByteArray("[steam.presets.My-Preset]\nargs = [\"-x\"]\n")
                            << u"settings.toml:1: preset id My-Preset must be lowercase letters, digits, - and _"_s;
    QTest::newRow("env type") << QByteArray("[steam.presets.x]\nenv = \"A=1\"\n")
                              << u"settings.toml:2: env must be a table of variables, like { DXVK_HDR = \"1\" }"_s;
    QTest::newRow("env value") << QByteArray("[steam.presets.x]\nenv = { A = 1 }\nargs = [\"-x\"]\n")
                               << u"settings.toml:2: A must be text, like \"1\""_s;
    QTest::newRow("env name") << QByteArray("[steam.presets.x]\nenv = { \"A-B\" = \"1\" }\nargs = [\"-x\"]\n")
                              << u"settings.toml:2: A-B isn't a variable name"_s;
    QTest::newRow("wrapper") << QByteArray("[steam.presets.x]\nwrapper = [1, 2]\n")
                             << u"settings.toml:2: wrapper must be a list of words, like [\"gamescope\", \"-f\", \"--\"]"_s;
    QTest::newRow("preset key") << QByteArray("[steam.presets.x]\nwraper = [\"x\"]\nargs = [\"-x\"]\n")
                                << u"settings.toml:2: unknown key wraper — did you mean \"wrapper\"?"_s;
    QTest::newRow("syntax") << QByteArray("[steam\n") << QString();
}

void SteamPresetsTest::badValues()
{
    QFETCH(QByteArray, text);
    QFETCH(QString, warning);
    const Parsed<SteamSettings> parsed = parse(text);
    QVERIFY(!parsed.warnings.isEmpty());
    if (!warning.isEmpty())
        QCOMPARE(parsed.warnings.first(), warning);
    // Whatever was wrong, the defaults stand.
    QVERIFY(parsed.value.enabled);
    QCOMPARE(parsed.value.presets.mid(0, 3), builtInSteamPresets());
}

void SteamPresetsTest::unknownDefaultPreset()
{
    const Parsed<SteamSettings> parsed = parse("[steam]\ndefault_presets = [\"mangohd\", \"gamemode\"]\n");
    QCOMPARE(parsed.warnings, QStringList{u"settings.toml:2: unknown preset mangohd — did you mean \"mangohud\"?"_s});
    QCOMPARE(parsed.value.defaultPresets, QStringList{u"gamemode"_s});
}

void SteamPresetsTest::words()
{
    QCOMPARE(LaunchOptions::words(u"  A=1  B=\"two words\" 'it''s' a\\ b %command%\t-x "_s),
             (QStringList{u"A=1"_s, u"B=two words"_s, u"its"_s, u"a b"_s, u"%command%"_s, u"-x"_s}));
    QCOMPARE(LaunchOptions::words(u"\"a \\\"quoted\\\" \\\\ word\""_s), QStringList{u"a \"quoted\" \\ word"_s});
    QVERIFY(LaunchOptions::words(u"   "_s).isEmpty());
}

void SteamPresetsTest::describe()
{
    QCOMPARE(builtIn(u"mangohud"_s).describe(), u"mangohud %command%"_s);
    QCOMPARE(builtIn(u"gamemode"_s).describe(), u"gamemoderun %command%"_s);
    QCOMPARE(builtIn(u"wayland"_s).describe(), u"PROTON_ENABLE_WAYLAND=1 %command%"_s);
    QCOMPARE(named(u"skip"_s).describe(), u"%command% -skipintro"_s);
    QCOMPARE(named(u"spaced"_s).describe(),
             u"WINEDLLOVERRIDES=dinput8=n,b TITLE=\"My Game\" \"/opt/my tools/run\" %command%"_s);
}

void SteamPresetsTest::isOn_data()
{
    QTest::addColumn<QString>("options");
    QTest::addColumn<QString>("preset");
    QTest::addColumn<bool>("on");
    QTest::newRow("empty") << QString() << u"mangohud"_s << false;
    QTest::newRow("typed in Steam") << u"mangohud %command%"_s << u"mangohud"_s << true;
    QTest::newRow("other wrapper first") << u"gamemoderun mangohud %command%"_s << u"mangohud"_s << true;
    QTest::newRow("both") << u"gamemoderun mangohud %command%"_s << u"gamemode"_s << true;
    QTest::newRow("after command is an argument") << u"%command% mangohud"_s << u"mangohud"_s << false;
    QTest::newRow("no command") << u"mangohud"_s << u"mangohud"_s << false;
    QTest::newRow("a part of a word") << u"mangohud2 %command%"_s << u"mangohud"_s << false;
    QTest::newRow("quoted") << u"\"mangohud\" %command%"_s << u"mangohud"_s << true;
    QTest::newRow("variable") << u"PROTON_ENABLE_WAYLAND=1 %command%"_s << u"wayland"_s << true;
    QTest::newRow("variable, other value") << u"PROTON_ENABLE_WAYLAND=0 %command%"_s << u"wayland"_s << false;
    QTest::newRow("variable among others") << u"DXVK_HUD=1 PROTON_ENABLE_WAYLAND=1 PROTON_LOG=1 mangohud %command% -x"_s
                                           << u"wayland"_s << true;
    QTest::newRow("variable after a wrapper") << u"mangohud PROTON_ENABLE_WAYLAND=1 %command%"_s << u"wayland"_s
                                              << false;
    QTest::newRow("half of hdr") << u"DXVK_HDR=1 %command%"_s << u"hdr"_s << false;
    QTest::newRow("hdr, any order") << u"DXVK_HDR=1 PROTON_ENABLE_HDR=1 %command%"_s << u"hdr"_s << true;
    QTest::newRow("sequence") << u"gamescope -f -- %command%"_s << u"gamescope"_s << true;
    QTest::newRow("sequence broken") << u"gamescope -f -W 1920 -- %command%"_s << u"gamescope"_s << false;
    QTest::newRow("argument") << u"%command% -skipintro"_s << u"skip"_s << true;
    QTest::newRow("argument without command") << u"-skipintro"_s << u"skip"_s << true;
    QTest::newRow("argument before command") << u"-skipintro %command%"_s << u"skip"_s << false;
}

void SteamPresetsTest::isOn()
{
    QFETCH(QString, options);
    QFETCH(QString, preset);
    QFETCH(bool, on);
    QCOMPARE(LaunchOptions::isOn(options, named(preset)), on);
}

void SteamPresetsTest::turnOn_data()
{
    QTest::addColumn<QString>("options");
    QTest::addColumn<QString>("preset");
    QTest::addColumn<QString>("result");
    QTest::newRow("empty") << QString() << u"mangohud"_s << u"mangohud %command%"_s;
    QTest::newRow("blank") << u"  "_s << u"wayland"_s << u"PROTON_ENABLE_WAYLAND=1 %command%"_s;
    QTest::newRow("arguments only") << u"-skipintro"_s << u"mangohud"_s << u"mangohud %command% -skipintro"_s;
    QTest::newRow("already on") << u"DXVK_HUD=1  mangohud %command%"_s << u"mangohud"_s
                                << u"DXVK_HUD=1  mangohud %command%"_s;
    QTest::newRow("wrapper next to the command") << u"A=1 gamemoderun %command% -x"_s << u"mangohud"_s
                                                 << u"A=1 gamemoderun mangohud %command% -x"_s;
    QTest::newRow("variable first") << u"A=1 mangohud %command% -x"_s << u"wayland"_s
                                    << u"PROTON_ENABLE_WAYLAND=1 A=1 mangohud %command% -x"_s;
    QTest::newRow("variable replaced") << u"A=1 PROTON_ENABLE_WAYLAND=0 %command%"_s << u"wayland"_s
                                       << u"A=1 PROTON_ENABLE_WAYLAND=1 %command%"_s;
    QTest::newRow("missing half") << u"DXVK_HDR=1 %command%"_s << u"hdr"_s
                                  << u"PROTON_ENABLE_HDR=1 DXVK_HDR=1 %command%"_s;
    QTest::newRow("argument") << u"mangohud %command% -x"_s << u"skip"_s << u"mangohud %command% -skipintro -x"_s;
    QTest::newRow("argument, no command") << u"-x"_s << u"skip"_s << u"%command% -skipintro -x"_s;
    QTest::newRow("quoted words") << QString() << u"spaced"_s
                                  << u"WINEDLLOVERRIDES=dinput8=n,b TITLE=\"My Game\" \"/opt/my tools/run\" %command%"_s;
}

void SteamPresetsTest::turnOn()
{
    QFETCH(QString, options);
    QFETCH(QString, preset);
    QFETCH(QString, result);
    const QString on = LaunchOptions::turnOn(options, named(preset));
    QCOMPARE(on, result);
    QVERIFY(LaunchOptions::isOn(on, named(preset)));
}

void SteamPresetsTest::turnOff_data()
{
    QTest::addColumn<QString>("options");
    QTest::addColumn<QString>("preset");
    QTest::addColumn<QString>("result");
    QTest::newRow("only it") << u"mangohud %command%"_s << u"mangohud"_s << QString();
    QTest::newRow("typed with extra spaces") << u"  mangohud   %command%  "_s << u"mangohud"_s << u"  "_s;
    QTest::newRow("keeps arguments") << u"mangohud %command% -x"_s << u"mangohud"_s << u"-x"_s;
    QTest::newRow("keeps other wrapper") << u"gamemoderun mangohud %command% -x"_s << u"mangohud"_s
                                         << u"gamemoderun %command% -x"_s;
    QTest::newRow("keeps variables") << u"A=1 mangohud %command%"_s << u"mangohud"_s << u"A=1 %command%"_s;
    QTest::newRow("variable in the middle") << u"A=1 PROTON_ENABLE_WAYLAND=1 B=2 %command%"_s << u"wayland"_s
                                            << u"A=1 B=2 %command%"_s;
    QTest::newRow("all variables") << u"PROTON_ENABLE_HDR=1 A=1 DXVK_HDR=1 %command%"_s << u"hdr"_s
                                   << u"A=1 %command%"_s;
    QTest::newRow("sequence") << u"gamescope -f -- mangohud %command%"_s << u"gamescope"_s << u"mangohud %command%"_s;
    QTest::newRow("last argument") << u"mangohud %command% -skipintro"_s << u"skip"_s << u"mangohud %command%"_s;
    QTest::newRow("not on") << u"mangohud2 %command%"_s << u"mangohud"_s << u"mangohud2 %command%"_s;
    QTest::newRow("other value kept") << u"PROTON_ENABLE_WAYLAND=0 %command%"_s << u"wayland"_s
                                      << u"PROTON_ENABLE_WAYLAND=0 %command%"_s;
    QTest::newRow("argument, no command") << u"-skipintro -x"_s << u"skip"_s << u"-x"_s;
}

void SteamPresetsTest::turnOff()
{
    QFETCH(QString, options);
    QFETCH(QString, preset);
    QFETCH(QString, result);
    const QString off = LaunchOptions::turnOff(options, named(preset));
    QCOMPARE(off, result);
    QVERIFY(!LaunchOptions::isOn(off, named(preset)));
}

void SteamPresetsTest::roundTrip_data()
{
    QTest::addColumn<QString>("options");
    const QStringList strings = {QString(),
                                 u"-skipintro"_s,
                                 u"%command%"_s,
                                 u"PROTON_LOG=1 %command% -x"_s,
                                 u"gamemoderun %command%"_s,
                                 u"A=1  B=\"two words\"   gamemoderun %command%   -window \"C:\\\\x\""_s,
                                 u"-x -y"_s};
    for (const QString &s : strings)
        QTest::newRow(qPrintable(s.isEmpty() ? u"empty"_s : s)) << s;
}

void SteamPresetsTest::roundTrip()
{
    QFETCH(QString, options);
    for (const QString &name : {u"mangohud"_s, u"wayland"_s, u"hdr"_s, u"gamescope"_s, u"skip"_s, u"spaced"_s}) {
        const SteamPreset p = named(name);
        if (LaunchOptions::isOn(options, p))
            continue;
        const QString on = LaunchOptions::turnOn(options, p);
        QVERIFY2(LaunchOptions::isOn(on, p), qPrintable(name + u": "_s + on));
        const QString off = LaunchOptions::turnOff(on, p);
        // %command% with nothing before it is dropped; Steam treats both the same.
        const QString expected = options == u"%command%" ? QString() : options;
        QCOMPARE(off, expected);
    }
    // Two presets, turned off in either order.
    const SteamPreset a = named(u"mangohud"_s);
    const SteamPreset b = named(u"wayland"_s);
    const QString both = LaunchOptions::turnOn(LaunchOptions::turnOn(options, a), b);
    const QString expected = options == u"%command%" ? QString() : options;
    QCOMPARE(LaunchOptions::turnOff(LaunchOptions::turnOff(both, a), b), expected);
    QCOMPARE(LaunchOptions::turnOff(LaunchOptions::turnOff(both, b), a), expected);
}

QTEST_MAIN(SteamPresetsTest)
#include "steampresets_test.moc"
