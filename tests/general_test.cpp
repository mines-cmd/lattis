#include "autostart.h"
#include "general.h"
#include "generalsettings.h"
#include "tomlpatch.h"

#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

class GeneralTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void parserDefaults();
    void parser_data();
    void parser();
    void writeRoundTrip();
    void startFullscreenSetting();
    void downloadBarCollapsedSetting();
    void railHiddenSetting();
    void systemLocaleSetting();
    void reduceMotionSetting();
    void railHiddenButtonsSetting();
    void railButtonOrderSetting();
    void startAtLogin();
    void staleAutostartRewritten();

private:
    // The autostart folder goes to a scratch config folder, never the real one.
    QTemporaryDir m_config;
};

void GeneralTest::parserDefaults()
{
    // Empty bytes: ok, all defaults.
    const auto empty = parseGeneralSettings({}, QStringLiteral("settings.toml"));
    QVERIFY(empty.ok);
    QCOMPARE(empty.value, GeneralSettings());

    // No [general] section.
    const auto other = parseGeneralSettings("[launch]\nwhile_running = \"stay\"\n", QStringLiteral("settings.toml"));
    QVERIFY(other.ok);
    QCOMPARE(other.value, GeneralSettings());
    QVERIFY(other.warnings.isEmpty());

    // Syntax error.
    const auto broken = parseGeneralSettings("[general\n", QStringLiteral("settings.toml"));
    QVERIFY(!broken.ok);
    QVERIFY(broken.warnings.first().startsWith(QStringLiteral("settings.toml:1:")));
}

void GeneralTest::parser_data()
{
    QTest::addColumn<QByteArray>("toml");
    QTest::addColumn<bool>("startFullscreen");
    QTest::addColumn<QString>("warning");

    QTest::newRow("true") << QByteArray("[general]\nstart_fullscreen = true\n")
                          << true << QString();
    QTest::newRow("false") << QByteArray("[general]\nstart_fullscreen = false\n")
                           << false << QString();
    QTest::newRow("other sections") << QByteArray("[launch]\nwhile_running = \"stay\"\n[general]\nstart_fullscreen = true\n")
                                    << true << QString();
    QTest::newRow("wrong type") << QByteArray("[general]\nstart_fullscreen = 1\n")
                                << false
                                << QStringLiteral("settings.toml:2: start_fullscreen must be true or false");
    QTest::newRow("unknown key") << QByteArray("[general]\nstart_fullscren = true\n")
                                 << false
                                 << QStringLiteral("settings.toml:2: unknown key start_fullscren — did you mean \"start_fullscreen\"?");
    QTest::newRow("not a section") << QByteArray("general = 1\n")
                                   << false
                                   << QStringLiteral("settings.toml:1: general must be a [general] section");
}

void GeneralTest::parser()
{
    QFETCH(QByteArray, toml);
    QFETCH(bool, startFullscreen);
    QFETCH(QString, warning);

    const auto r = parseGeneralSettings(toml, QStringLiteral("settings.toml"));
    QVERIFY(r.ok);
    QCOMPARE(r.value.startFullscreen, startFullscreen);
    QCOMPARE(r.warnings, warning.isEmpty() ? QStringList() : QStringList{warning});
}

void GeneralTest::writeRoundTrip()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("settings.toml"));

    // Write a settings.toml with a comment to verify comments survive.
    QFile f(path);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write("# my settings\n[launch]\nwhile_running = \"stay\"\n");
    f.close();

    // Setting true adds [general] with start_fullscreen = true; comment survives.
    QString error;
    QVERIFY(TomlPatch::setValueInFile(path, QStringLiteral("general"),
                                      QStringLiteral("start_fullscreen"), QStringLiteral("true"), &error));
    QVERIFY(error.isEmpty());
    QFile f2(path);
    QVERIFY(f2.open(QIODevice::ReadOnly));
    const QByteArray after = f2.readAll();
    f2.close();
    QVERIFY(after.contains("# my settings"));
    const auto parsed = parseGeneralSettings(after, QStringLiteral("settings.toml"));
    QVERIFY(parsed.ok);
    QVERIFY(parsed.value.startFullscreen);
    QVERIFY(parsed.warnings.isEmpty());

    // Setting false (the default) removes the key.
    QVERIFY(TomlPatch::removeValueInFile(path, QStringLiteral("general"),
                                         QStringLiteral("start_fullscreen"), &error));
    QVERIFY(error.isEmpty());
    QFile f3(path);
    QVERIFY(f3.open(QIODevice::ReadOnly));
    const QByteArray removed = f3.readAll();
    f3.close();
    const auto parsed2 = parseGeneralSettings(removed, QStringLiteral("settings.toml"));
    QVERIFY(parsed2.ok);
    QVERIFY(!parsed2.value.startFullscreen);
    QVERIFY(parsed2.warnings.isEmpty());
}

void GeneralTest::initTestCase()
{
    QVERIFY(m_config.isValid());
    qputenv("XDG_CONFIG_HOME", QFile::encodeName(m_config.path()));
    QCOMPARE(Autostart::autostartFolder(), m_config.path() + QStringLiteral("/autostart"));
}

void GeneralTest::startFullscreenSetting()
{
    QTemporaryDir data;
    General general(data.path());
    QVERIFY(!general.startFullscreen());
    QVERIFY(general.problems().isEmpty());
    QCOMPARE(general.dataDir(), data.path());

    // Theme writes the file at startup when it's missing.
    QFile settings(data.filePath(QStringLiteral("settings.toml")));
    QVERIFY(settings.open(QIODevice::WriteOnly));
    settings.close();
    QSignalSpy changed(&general, &General::settingsChanged);
    QCOMPARE(general.setStartFullscreen(true), QString());
    QVERIFY(general.startFullscreen());
    QCOMPARE(changed.size(), 1);
    QVERIFY(settings.open(QIODevice::ReadOnly));
    QVERIFY(settings.readAll().contains("start_fullscreen = true"));
    settings.close();

    // A hand edit shows on reload; an unchanged file signals nothing.
    general.reload();
    QCOMPARE(changed.size(), 1);
    QVERIFY(settings.open(QIODevice::WriteOnly | QIODevice::Truncate));
    settings.write("[general]\nstart_fullscreen = \"yes\"\n");
    settings.close();
    general.reload();
    QCOMPARE(changed.size(), 2);
    QVERIFY(!general.startFullscreen());
    QCOMPARE(general.problems(), QStringList{"settings.toml:2: start_fullscreen must be true or false"});

    QCOMPARE(general.setStartFullscreen(false), QString());
    QVERIFY(general.problems().isEmpty());
    QVERIFY(settings.open(QIODevice::ReadOnly));
    QVERIFY(!settings.readAll().contains("start_fullscreen"));
}

void GeneralTest::downloadBarCollapsedSetting()
{
    QTemporaryDir data;
    General general(data.path());
    QVERIFY(!general.downloadBarCollapsed());

    const auto read = parseGeneralSettings("[general]\ndownload_bar_collapsed = true\n", QStringLiteral("settings.toml"));
    QVERIFY(read.value.downloadBarCollapsed);
    QVERIFY(read.warnings.isEmpty());
    const auto bad = parseGeneralSettings("[general]\ndownload_bar_collapsed = 1\n", QStringLiteral("settings.toml"));
    QVERIFY(!bad.value.downloadBarCollapsed);
    QCOMPARE(bad.warnings, QStringList{"settings.toml:2: download_bar_collapsed must be true or false"});

    // Only its own key is written; the rest of the file stays as it was.
    QFile settings(data.filePath(QStringLiteral("settings.toml")));
    QVERIFY(settings.open(QIODevice::WriteOnly));
    settings.write("# mine\n[general]\nstart_fullscreen = true\n\n[launch]\nwhile_running = \"stay\"\n");
    settings.close();
    general.reload();
    QSignalSpy changed(&general, &General::settingsChanged);
    QCOMPARE(general.setDownloadBarCollapsed(true), QString());
    QVERIFY(general.downloadBarCollapsed());
    QVERIFY(general.startFullscreen());
    QCOMPARE(changed.size(), 1);
    QVERIFY(settings.open(QIODevice::ReadOnly));
    const QByteArray on = settings.readAll();
    settings.close();
    QVERIFY(on.contains("# mine"));
    QVERIFY(on.contains("start_fullscreen = true"));
    QVERIFY(on.contains("download_bar_collapsed = true"));
    QVERIFY(on.contains("while_running = \"stay\""));

    QCOMPARE(general.setDownloadBarCollapsed(false), QString());
    QVERIFY(!general.downloadBarCollapsed());
    QVERIFY(settings.open(QIODevice::ReadOnly));
    const QByteArray off = settings.readAll();
    settings.close();
    QVERIFY(!off.contains("download_bar_collapsed"));
    QVERIFY(off.contains("start_fullscreen = true"));
    QVERIFY(off.contains("# mine"));
}

void GeneralTest::railHiddenSetting()
{
    QTemporaryDir data;
    General general(data.path());
    QVERIFY(!general.railHidden());

    const auto read = parseGeneralSettings("[general]\nrail_hidden = true\n", QStringLiteral("settings.toml"));
    QVERIFY(read.value.railHidden);
    QVERIFY(read.warnings.isEmpty());
    const auto bad = parseGeneralSettings("[general]\nrail_hidden = 1\n", QStringLiteral("settings.toml"));
    QVERIFY(!bad.value.railHidden);
    QCOMPARE(bad.warnings, QStringList{"settings.toml:2: rail_hidden must be true or false"});

    QFile settings(data.filePath(QStringLiteral("settings.toml")));
    QVERIFY(settings.open(QIODevice::WriteOnly));
    settings.write("# mine\n[general]\nstart_fullscreen = true\n\n[launch]\nwhile_running = \"stay\"\n");
    settings.close();
    general.reload();
    QSignalSpy changed(&general, &General::railHiddenChanged);
    QSignalSpy window(&general, &General::settingsChanged);
    QCOMPARE(general.setRailHidden(true), QString());
    QVERIFY(general.railHidden());
    QVERIFY(general.startFullscreen());
    QCOMPARE(changed.size(), 1);
    QCOMPARE(window.size(), 0);
    QVERIFY(settings.open(QIODevice::ReadOnly));
    const QByteArray on = settings.readAll();
    settings.close();
    QVERIFY(on.contains("# mine"));
    QVERIFY(on.contains("start_fullscreen = true"));
    QVERIFY(on.contains("rail_hidden = true"));
    QVERIFY(on.contains("while_running = \"stay\""));

    QCOMPARE(general.setRailHidden(false), QString());
    QVERIFY(!general.railHidden());
    QVERIFY(settings.open(QIODevice::ReadOnly));
    const QByteArray off = settings.readAll();
    settings.close();
    QVERIFY(!off.contains("rail_hidden"));
    QVERIFY(off.contains("start_fullscreen = true"));
    QVERIFY(off.contains("# mine"));
}

void GeneralTest::reduceMotionSetting()
{
    QTemporaryDir data;
    General general(data.path());
    QVERIFY(!general.reduceMotion());

    const auto bad = parseGeneralSettings("[general]\nreduce_motion = 1\n", QStringLiteral("settings.toml"));
    QVERIFY(!bad.value.reduceMotion);
    QCOMPARE(bad.warnings, QStringList{"settings.toml:2: reduce_motion must be true or false"});

    // It has its own signal: settingsChanged re-shows the main window.
    QFile settings(data.filePath(QStringLiteral("settings.toml")));
    QVERIFY(settings.open(QIODevice::WriteOnly));
    settings.close();
    QSignalSpy changed(&general, &General::settingsChanged);
    QSignalSpy motionChanged(&general, &General::reduceMotionChanged);
    QCOMPARE(general.setReduceMotion(true), QString());
    QVERIFY(general.reduceMotion());
    QCOMPARE(changed.size(), 0);
    QCOMPARE(motionChanged.size(), 1);
    QVERIFY(settings.open(QIODevice::ReadOnly));
    QVERIFY(settings.readAll().contains("reduce_motion = true"));
    settings.close();

    QCOMPARE(general.setReduceMotion(false), QString());
    QVERIFY(!general.reduceMotion());
    QCOMPARE(motionChanged.size(), 2);
    QVERIFY(settings.open(QIODevice::ReadOnly));
    QVERIFY(!settings.readAll().contains("reduce_motion"));
}

void GeneralTest::systemLocaleSetting()
{
    QTemporaryDir data;
    General general(data.path());
    QVERIFY(general.systemLocale());

    QFile settings(data.filePath(QStringLiteral("settings.toml")));
    QVERIFY(settings.open(QIODevice::WriteOnly));
    settings.close();
    QCOMPARE(general.setSystemLocale(false), QString());
    QVERIFY(!general.systemLocale());
    QVERIFY(settings.open(QIODevice::ReadOnly));
    QVERIFY(settings.readAll().contains("system_locale = false"));
    settings.close();

    QCOMPARE(general.setSystemLocale(true), QString());
    QVERIFY(general.systemLocale());
    QVERIFY(settings.open(QIODevice::ReadOnly));
    QVERIFY(!settings.readAll().contains("system_locale"));
}

void GeneralTest::railHiddenButtonsSetting()
{
    QTemporaryDir data;
    General general(data.path());
    QVERIFY(general.railHiddenButtons().isEmpty());

    const auto bad = parseGeneralSettings("[general]\nrail_hidden_buttons = [\"settings\", \"add_game\"]\n",
                                          QStringLiteral("settings.toml"));
    QCOMPARE(bad.value.railHiddenButtons, QStringList{"add_game"});
    QCOMPARE(bad.warnings.size(), 1);

    QFile settings(data.filePath(QStringLiteral("settings.toml")));
    QVERIFY(settings.open(QIODevice::WriteOnly));
    settings.close();
    QSignalSpy changed(&general, &General::settingsChanged);
    QSignalSpy buttonsChanged(&general, &General::railHiddenButtonsChanged);
    QCOMPARE(general.setRailButtonShown(QStringLiteral("open_settings"), false), QString());
    QCOMPARE(general.setRailButtonShown(QStringLiteral("add_game"), false), QString());
    QCOMPARE(general.railHiddenButtons(), (QStringList{"add_game", "open_settings"}));
    QCOMPARE(changed.size(), 0);
    QCOMPARE(buttonsChanged.size(), 2);

    QCOMPARE(general.setRailButtonShown(QStringLiteral("add_game"), true), QString());
    QCOMPARE(general.setRailButtonShown(QStringLiteral("open_settings"), true), QString());
    QVERIFY(general.railHiddenButtons().isEmpty());
    QVERIFY(settings.open(QIODevice::ReadOnly));
    QVERIFY(!settings.readAll().contains("rail_hidden_buttons"));
}

void GeneralTest::railButtonOrderSetting()
{
    QTemporaryDir data;
    General general(data.path());
    QCOMPARE(general.railButtonOrder(), railButtons());

    const auto partial = parseGeneralSettings("[general]\nrail_button_order = [\"open_settings\", \"nope\"]\n",
                                              QStringLiteral("settings.toml"));
    QCOMPARE(partial.warnings.size(), 1);
    QCOMPARE(orderedRailButtons(partial.value.railButtonOrder),
             (QStringList{"open_settings", "add_game", "play_statistics"}));

    QFile settings(data.filePath(QStringLiteral("settings.toml")));
    QVERIFY(settings.open(QIODevice::WriteOnly));
    settings.close();
    QSignalSpy changed(&general, &General::settingsChanged);
    QSignalSpy orderChanged(&general, &General::railButtonOrderChanged);
    const QStringList moved{"play_statistics", "open_settings", "add_game"};
    QCOMPARE(general.setRailButtonOrder(moved), QString());
    QCOMPARE(general.railButtonOrder(), moved);
    QCOMPARE(changed.size(), 0);
    QCOMPARE(orderChanged.size(), 1);

    general.setRailButtonOrder({"add_game"});
    QCOMPARE(general.railButtonOrder(), moved);

    QCOMPARE(general.setRailButtonOrder(railButtons()), QString());
    QCOMPARE(general.railButtonOrder(), railButtons());
    QVERIFY(settings.open(QIODevice::ReadOnly));
    QVERIFY(!settings.readAll().contains("rail_button_order"));
}

void GeneralTest::startAtLogin()
{
    QTemporaryDir data;
    General general(data.path());
    QVERIFY(!general.startAtLogin());

    QSignalSpy changed(&general, &General::loginChanged);
    QCOMPARE(general.setStartAtLogin(true), QString());
    QVERIFY(general.startAtLogin());
    QVERIFY(general.startAtLoginOurs());
    QCOMPARE(changed.size(), 1);
    const Autostart::Entry entry = Autostart::readEntry(Autostart::autostartFolder());
    QCOMPARE(entry.exec, Autostart::entryExec(Autostart::currentProgram(), data.path()));

    // Turned off in the desktop's own settings: there, but not starting.
    QFile file(Autostart::autostartFolder() + QStringLiteral("/lattis.desktop"));
    QVERIFY(file.open(QIODevice::Append));
    file.write("Hidden=true\n");
    file.close();
    general.reload();
    QVERIFY(!general.startAtLogin());
    QCOMPARE(changed.size(), 2);

    QCOMPARE(general.setStartAtLogin(false), QString());
    QVERIFY(!file.exists());
    QVERIFY(!general.startAtLogin());
}

void GeneralTest::staleAutostartRewritten()
{
    QTemporaryDir data;
    const QString folder = Autostart::autostartFolder();
    const QString current = Autostart::entryExec(Autostart::currentProgram(), data.path());

    // Ours, from a program that has moved since: rewritten.
    QCOMPARE(Autostart::writeEntry(folder, QStringLiteral("/old/place/lattis"), data.path()), QString());
    {
        General general(data.path());
        general.refreshAutostart();
        QCOMPARE(Autostart::readEntry(folder).exec, current);
        QVERIFY(general.startAtLoginOurs());
    }

    // Someone else's entry of the same name: left as it is.
    QFile file(folder + QStringLiteral("/lattis.desktop"));
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
    file.write("[Desktop Entry]\nType=Application\nExec=/usr/bin/lattis --minimized\n");
    file.close();
    General general(data.path());
    general.refreshAutostart();
    QVERIFY(general.startAtLogin());
    QVERIFY(!general.startAtLoginOurs());
    QCOMPARE(Autostart::readEntry(folder).exec, QStringLiteral("/usr/bin/lattis --minimized"));
    QVERIFY(Autostart::removeEntry(folder).isEmpty());
}

QTEST_MAIN(GeneralTest)
#include "general_test.moc"
