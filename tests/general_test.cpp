#include "autostart.h"
#include "general.h"
#include "gamepagemigration.h"
#include "gamepagesettings.h"
#include "generalsettings.h"
#include "overviewsettings.h"
#include "tomlpatch.h"

#include <QFile>
#include <QFileInfo>
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
    void clipVolumeSetting();
    void updateSettings();
    void appIconSetting();
    void railHiddenSetting();
    void systemLocaleSetting();
    void reduceMotionSetting();
    void railHiddenButtonsSetting();
    void railButtonOrderSetting();
    void toolbarSettings();
    void gridSettings();
    void gridGamePageLists();
    void gallerySettings();
    void overviewParser();
    void overviewResolving();
    void overviewSetters();
    void gamePageDefaults();
    void gamePageParser();
    void gamePageOrderKeepsNewNames();
    void gamePageSettersWrite();
    void gamePageMigration();
    void gamePageMigrationFailuresLeaveNoMarker();
    void startAtLogin();
    void staleAutostartRewritten();
    void loginEntryNamesTheVariableOnlyWhenSet();
    void loginEntryBelongsToItsCopy();

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

void GeneralTest::clipVolumeSetting()
{
    QTemporaryDir data;
    General general(data.path());
    QCOMPARE(general.clipVolume(), 1.0);

    QCOMPARE(parseGeneralSettings("[general]\nclip_volume = 0.4\n", QStringLiteral("settings.toml")).value.clipVolume, 0.4);
    QCOMPARE(parseGeneralSettings("[general]\nclip_volume = 0\n", QStringLiteral("settings.toml")).value.clipVolume, 0.0);
    const auto bad = parseGeneralSettings("[general]\nclip_volume = 1.5\n", QStringLiteral("settings.toml"));
    QCOMPARE(bad.value.clipVolume, 1.0);
    QCOMPARE(bad.warnings, QStringList{"settings.toml:2: clip_volume must be a number from 0 to 1"});

    QFile settings(data.filePath(QStringLiteral("settings.toml")));
    QVERIFY(settings.open(QIODevice::WriteOnly));
    settings.write("[general]\n");
    settings.close();
    general.reload();

    // The window's bindings listen to settingsChanged, so a volume change must not emit it.
    QSignalSpy settingsChanged(&general, &General::settingsChanged);
    QSignalSpy volumeChanged(&general, &General::clipVolumeChanged);
    QCOMPARE(general.setClipVolume(0.456), QString());
    QCOMPARE(general.clipVolume(), 0.46);
    QCOMPARE(volumeChanged.size(), 1);
    QCOMPARE(settingsChanged.size(), 0);
    QVERIFY(settings.open(QIODevice::ReadOnly));
    QVERIFY(settings.readAll().contains("clip_volume = 0.46"));
    settings.close();

    QCOMPARE(general.setClipVolume(1), QString());
    QCOMPARE(general.clipVolume(), 1.0);
    QVERIFY(settings.open(QIODevice::ReadOnly));
    QVERIFY(!settings.readAll().contains("clip_volume"));
}

void GeneralTest::updateSettings()
{
    const auto parse = [](const char *text) {
        return parseGeneralSettings(text, QStringLiteral("settings.toml"));
    };
    QVERIFY(parse("[general]\n").value.checkForUpdates);
    QVERIFY(parse("[general]\n").value.skippedUpdate.isEmpty());
    QVERIFY(!parse("[general]\ncheck_for_updates = false\n").value.checkForUpdates);
    QVERIFY(parse("[general]\n").value.appMenuEntry);
    QVERIFY(!parse("[general]\napp_menu_entry = false\n").value.appMenuEntry);
    QCOMPARE(parse("[general]\nskipped_update = \"1.2\"\n").value.skippedUpdate, QStringLiteral("1.2"));
    const auto bad = parse("[general]\nskipped_update = 1.2\ncheck_for_updates = \"no\"\n");
    QVERIFY(bad.value.checkForUpdates);
    QVERIFY(bad.value.skippedUpdate.isEmpty());
    QCOMPARE(bad.warnings.size(), 2);

    QTemporaryDir data;
    General general(data.path());
    QFile settings(data.filePath(QStringLiteral("settings.toml")));
    QVERIFY(settings.open(QIODevice::WriteOnly));
    settings.write("[general]\n");
    settings.close();
    general.reload();
    QVERIFY(general.checkForUpdates());
    QCOMPARE(general.setCheckForUpdates(false), QString());
    QVERIFY(!general.checkForUpdates());
    QCOMPARE(general.setSkippedUpdate("1.2"), QString());
    QCOMPARE(general.skippedUpdate(), QStringLiteral("1.2"));
    // A control character is written as an escape, so the file still parses and holds one value.
    const QString odd = QStringLiteral("1.2\n[x]\a");
    QCOMPARE(general.setSkippedUpdate(odd), QString());
    QCOMPARE(general.skippedUpdate(), odd);
    QCOMPARE(general.setSkippedUpdate("1.2"), QString());
    QVERIFY(settings.open(QIODevice::ReadOnly));
    const QByteArray written = settings.readAll();
    settings.close();
    QVERIFY(written.contains("check_for_updates = false"));
    QVERIFY(written.contains("skipped_update = \"1.2\""));

    QCOMPARE(general.setCheckForUpdates(true), QString());
    QCOMPARE(general.setSkippedUpdate({}), QString());
    QVERIFY(general.checkForUpdates());
    QVERIFY(general.skippedUpdate().isEmpty());
    QVERIFY(settings.open(QIODevice::ReadOnly));
    const QByteArray cleared = settings.readAll();
    QVERIFY(!cleared.contains("check_for_updates"));
    QVERIFY(!cleared.contains("skipped_update"));
    QVERIFY(general.problems().isEmpty());
}

void GeneralTest::appIconSetting()
{
    const auto parse = [](const char *text) {
        return parseGeneralSettings(text, QStringLiteral("settings.toml"));
    };
    QCOMPARE(parse("[general]\n").value.appIcon, QStringLiteral("theme"));
    QCOMPARE(parse("[general]\napp_icon = \"Nord\"\n").value.appIcon, QStringLiteral("Nord"));
    for (const char *bad : {"[general]\napp_icon = 3\n", "[general]\napp_icon = \"\"\n"}) {
        const auto parsed = parse(bad);
        QCOMPARE(parsed.value.appIcon, QStringLiteral("theme"));
        QCOMPARE(parsed.warnings.size(), 1);
    }

    QTemporaryDir data;
    QFile settings(data.filePath(QStringLiteral("settings.toml")));
    QVERIFY(settings.open(QIODevice::WriteOnly));
    settings.write("[general]\n");
    settings.close();
    General general(data.path());
    QSignalSpy changed(&general, &General::appIconChanged);
    QSignalSpy window(&general, &General::settingsChanged);
    QCOMPARE(general.setAppIcon(QStringLiteral("Nord")), QString());
    QCOMPARE(general.appIcon(), QStringLiteral("Nord"));
    QCOMPARE(changed.count(), 1);
    QCOMPARE(window.count(), 0);
    QVERIFY(settings.open(QIODevice::ReadOnly));
    QVERIFY(settings.readAll().contains("app_icon = \"Nord\""));
    settings.close();

    QCOMPARE(general.setAppIcon(QStringLiteral("theme")), QString());
    QCOMPARE(general.appIcon(), QStringLiteral("theme"));
    QVERIFY(settings.open(QIODevice::ReadOnly));
    QVERIFY(!settings.readAll().contains("app_icon"));
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
             (QStringList{"open_settings", "steam_friends", "add_game", "overview", "play_statistics", "gallery", "toggle_view"}));

    QFile settings(data.filePath(QStringLiteral("settings.toml")));
    QVERIFY(settings.open(QIODevice::WriteOnly));
    settings.close();
    QSignalSpy changed(&general, &General::settingsChanged);
    QSignalSpy orderChanged(&general, &General::railButtonOrderChanged);
    const QStringList moved{"overview", "play_statistics", "gallery", "toggle_view", "open_settings", "add_game", "steam_friends"};
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


void GeneralTest::toolbarSettings()
{
    QTemporaryDir data;
    General general(data.path());
    QVERIFY(!general.toolbar());
    QVERIFY(general.toolbarHiddenButtons().isEmpty());
    QCOMPARE(general.toolbarButtonOrder(), toolbarButtons());
    QCOMPARE(toolbarButtons(), (QStringList{"steam_friends", "add_game", "overview", "play_statistics", "gallery", "toggle_view", "open_settings"}));

    const auto bad = parseGeneralSettings(
        "[general]\ntoolbar = true\ntoolbar_hidden_buttons = [\"settings\", \"gallery\"]\n"
        "toolbar_button_order = [\"open_settings\", \"nope\"]\n",
        QStringLiteral("settings.toml"));
    QVERIFY(bad.value.toolbar);
    QCOMPARE(bad.value.toolbarHiddenButtons, QStringList{"gallery"});
    QCOMPARE(bad.warnings.size(), 2);
    const QString warnings = bad.warnings.join(u'\n');
    QVERIFY(warnings.contains(QLatin1String("unknown toolbar button \"settings\"")));
    QVERIFY(warnings.contains(QLatin1String("\"open_settings\"")));
    QCOMPARE(orderedToolbarButtons(bad.value.toolbarButtonOrder),
             (QStringList{"open_settings", "steam_friends", "add_game", "overview", "play_statistics", "gallery", "toggle_view"}));

    const auto wrongType = parseGeneralSettings("[general]\ntoolbar = \"yes\"\ntoolbar_button_order = 3\n",
                                                QStringLiteral("settings.toml"));
    QCOMPARE(wrongType.warnings.size(), 2);
    QVERIFY(!wrongType.value.toolbar);

    QFile settings(data.filePath(QStringLiteral("settings.toml")));
    QVERIFY(settings.open(QIODevice::WriteOnly));
    settings.close();
    QSignalSpy changed(&general, &General::settingsChanged);
    QSignalSpy toolbarChanged(&general, &General::toolbarChanged);
    QSignalSpy hiddenChanged(&general, &General::toolbarHiddenButtonsChanged);
    QSignalSpy orderChanged(&general, &General::toolbarButtonOrderChanged);

    QCOMPARE(general.setToolbar(true), QString());
    QVERIFY(general.toolbar());
    QCOMPARE(toolbarChanged.size(), 1);

    QCOMPARE(general.setToolbarButtonShown(QStringLiteral("open_settings"), false), QString());
    QCOMPARE(general.setToolbarButtonShown(QStringLiteral("add_game"), false), QString());
    QCOMPARE(general.toolbarHiddenButtons(), (QStringList{"add_game", "open_settings"}));
    QCOMPARE(hiddenChanged.size(), 2);
    QCOMPARE(general.setToolbarButtonShown(QStringLiteral("nope"), false), QString());
    QCOMPARE(hiddenChanged.size(), 2);

    const QStringList moved{"overview", "play_statistics", "gallery", "toggle_view", "open_settings", "add_game", "steam_friends"};
    QCOMPARE(general.setToolbarButtonOrder(moved), QString());
    QCOMPARE(general.toolbarButtonOrder(), moved);
    QCOMPARE(orderChanged.size(), 1);
    general.setToolbarButtonOrder({"add_game"});
    QCOMPARE(general.toolbarButtonOrder(), moved);
    QCOMPARE(changed.size(), 0);

    QCOMPARE(general.setToolbarButtonShown(QStringLiteral("add_game"), true), QString());
    QCOMPARE(general.setToolbarButtonShown(QStringLiteral("open_settings"), true), QString());
    QCOMPARE(general.setToolbarButtonOrder(toolbarButtons()), QString());
    QCOMPARE(general.setToolbar(false), QString());
    QVERIFY(settings.open(QIODevice::ReadOnly));
    const QByteArray text = settings.readAll();
    QVERIFY(!text.contains("toolbar_hidden_buttons"));
    QVERIFY(!text.contains("toolbar_button_order"));
    QVERIFY(!text.contains("toolbar"));
}

void GeneralTest::gridSettings()
{
    const GeneralSettings defaults;
    QCOMPARE(defaults.view, QStringLiteral("list"));
    QCOMPARE(defaults.gridCoverWidth, 220);
    QVERIFY(defaults.gridPanel);
    QVERIFY(!defaults.gridShowTitles);

    const auto ok = parseGeneralSettings("[general]\nview = \"grid\"\ngrid_cover_width = 300\ngrid_panel = false\n"
                                         "grid_show_titles = true\n",
                                         QStringLiteral("settings.toml"));
    QVERIFY(ok.warnings.isEmpty());
    QCOMPARE(ok.value.view, QStringLiteral("grid"));
    QCOMPARE(ok.value.gridCoverWidth, 300);
    QVERIFY(!ok.value.gridPanel);
    QVERIFY(ok.value.gridShowTitles);

    const auto bad = parseGeneralSettings("[general]\nview = \"tiles\"\ngrid_cover_width = 5\ngrid_panel = 1\n",
                                          QStringLiteral("settings.toml"));
    QCOMPARE(bad.warnings.size(), 3);
    QVERIFY(bad.warnings.join(u'\n').contains("view must be \"list\" or \"grid\""));
    QVERIFY(bad.warnings.join(u'\n').contains("grid_cover_width must be a whole number from 80 to 600"));
    QVERIFY(bad.warnings.join(u'\n').contains("grid_panel must be true or false"));
    QCOMPARE(bad.value, GeneralSettings());

    QTemporaryDir data;
    QFile settings(data.filePath(QStringLiteral("settings.toml")));
    QVERIFY(settings.open(QIODevice::WriteOnly));
    settings.write("# mine\n[general]\nstart_fullscreen = true\n");
    settings.close();
    General general(data.path());
    QSignalSpy changed(&general, &General::settingsChanged);
    QSignalSpy viewChanged(&general, &General::viewChanged);
    QSignalSpy gridChanged(&general, &General::gridChanged);
    const auto fileText = [&] {
        QFile f(settings.fileName());
        return f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray();
    };

    QCOMPARE(general.setView(QStringLiteral("grid")), QString());
    QCOMPARE(general.view(), QStringLiteral("grid"));
    QCOMPARE(viewChanged.size(), 1);
    QVERIFY(fileText().contains("view = \"grid\""));
    QCOMPARE(general.setView(QStringLiteral("tiles")), QString());
    QCOMPARE(general.view(), QStringLiteral("grid"));

    QCOMPARE(general.setGridCoverWidth(1000), QString());
    QCOMPARE(general.gridCoverWidth(), 600);
    QCOMPARE(general.setGridCoverWidth(10), QString());
    QCOMPARE(general.gridCoverWidth(), 80);
    QCOMPARE(general.setGridPanel(false), QString());
    QVERIFY(!general.gridPanel());
    QCOMPARE(general.setGridShowTitles(true), QString());
    QVERIFY(general.gridShowTitles());
    QCOMPARE(gridChanged.size(), 4);
    QCOMPARE(changed.size(), 0);

    QCOMPARE(general.setView(QStringLiteral("list")), QString());
    QCOMPARE(general.setGridCoverWidth(220), QString());
    QCOMPARE(general.setGridPanel(true), QString());
    QCOMPARE(general.setGridShowTitles(false), QString());
    const QByteArray text = fileText();
    QVERIFY(!text.contains("view"));
    QVERIFY(!text.contains("grid_"));
    QVERIFY(text.contains("# mine"));
    QVERIFY(text.contains("start_fullscreen = true"));
}

void GeneralTest::gridGamePageLists()
{
    const GamePageSettings defaults;
    QCOMPARE(defaults.gridChips, (QStringList{"play_time", "last_played"}));
    QCOMPARE(defaults.gridPanels, (QStringList{"info"}));
    QCOMPARE(gamePageChoices(GamePage::gridChips), gamePageChoices(GamePage::chips));
    QCOMPARE(gamePageChoices(GamePage::gridPanels), (QStringList{"info", "friends", "achievements", "tags"}));
    QCOMPARE(gamePageChoices(GamePage::panels), (QStringList{"info", "friends", "achievements"}));

    const auto parsed = parseGamePageSettings("[game_page]\ngrid_chips = [\"rating\", \"play_time\"]\n"
                                              "grid_chips_order = [\"play_time\", \"rating\"]\n"
                                              "grid_panels = [\"tags\", \"info\"]\n"
                                              "chips = [\"rating\"]\n",
                                              QStringLiteral("settings.toml"));
    QVERIFY(parsed.warnings.isEmpty());
    QCOMPARE(parsed.value.gridChips, (QStringList{"rating", "play_time"}));
    QCOMPARE(parsed.value.gridChipsOrder, (QStringList{"play_time", "rating"}));
    QCOMPARE(parsed.value.gridPanels, (QStringList{"tags", "info"}));
    QCOMPARE(parsed.value.chips, (QStringList{"rating"}));
    QCOMPARE(parsed.value.panels, GamePageSettings::defaultPanels());
    QCOMPARE(gamePageOrder(parsed.value, GamePage::gridChips).first(), QStringLiteral("rating"));

    const auto bad = parseGamePageSettings("[game_page]\ngrid_panels = [\"tags\", \"nope\"]\n"
                                           "panels = [\"tags\"]\n",
                                           QStringLiteral("settings.toml"));
    QCOMPARE(bad.warnings.size(), 2);
    QCOMPARE(bad.value.gridPanels, (QStringList{"tags"}));
    QVERIFY(bad.value.panels.isEmpty());

    QTemporaryDir data;
    QFile settings(data.filePath(QStringLiteral("settings.toml")));
    QVERIFY(settings.open(QIODevice::WriteOnly));
    settings.write("[general]\nstart_fullscreen = true\n");
    settings.close();
    General general(data.path());
    const auto fileText = [&] {
        QFile f(settings.fileName());
        return f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray();
    };
    QCOMPARE(general.setGamePageShown(GamePage::gridChips, QStringLiteral("rating"), true), QString());
    QCOMPARE(general.gamePageGridChips(), (QStringList{"play_time", "last_played", "rating"}));
    QCOMPARE(general.gamePageChips(), GamePageSettings::defaultChips());
    QVERIFY(fileText().contains("grid_chips = [\"play_time\", \"last_played\", \"rating\"]"));
    QCOMPARE(general.setGamePageShown(GamePage::gridPanels, QStringLiteral("tags"), true), QString());
    QCOMPARE(general.gamePageGridPanels(), (QStringList{"info", "tags"}));
    QCOMPARE(general.gamePagePanels(), GamePageSettings::defaultPanels());
    QCOMPARE(general.setGamePageShown(GamePage::panels, QStringLiteral("tags"), true), QString());
    QCOMPARE(general.gamePagePanels(), GamePageSettings::defaultPanels());

    QCOMPARE(general.setGamePageOrder(GamePage::gridPanels, {"tags", "achievements", "info", "friends"}), QString());
    QCOMPARE(general.gamePageOrder(GamePage::gridPanels),
             (QStringList{"tags", "achievements", "info", "friends"}));

    QCOMPARE(general.setGamePageShown(GamePage::gridChips, QStringLiteral("rating"), false), QString());
    QCOMPARE(general.setGamePageShown(GamePage::gridPanels, QStringLiteral("tags"), false), QString());
    QCOMPARE(general.setGamePageOrder(GamePage::gridPanels, gamePageChoices(GamePage::gridPanels)), QString());
    const QByteArray text = fileText();
    QVERIFY(!text.contains("grid_"));
}

void GeneralTest::gamePageDefaults()
{
    const auto empty = parseGamePageSettings({}, QStringLiteral("settings.toml"));
    QVERIFY(empty.ok);
    QCOMPARE(empty.value, GamePageSettings());
    QCOMPARE(empty.value.chips,
             (QStringList{"play_time", "last_played", "steam_update", "game_update", "steam_family"}));
    QCOMPARE(empty.value.buttons, (QStringList{"favorite", "open_platform", "folder", "settings"}));
    QCOMPARE(empty.value.infoRows,
             (QStringList{"release_date", "rating", "status", "install_size", "platform", "game_update"}));
    QCOMPARE(empty.value.recentPeriod, QStringLiteral("two_weeks"));
    QCOMPARE(empty.value.playTimeFormat, QStringLiteral("hours"));
    QCOMPARE(empty.value.sections, (QStringList{"description", "clips", "tags"}));

    const auto other = parseGamePageSettings("[general]\nreduce_motion = true\n", QStringLiteral("settings.toml"));
    QCOMPARE(other.value, GamePageSettings());
    QVERIFY(other.warnings.isEmpty());

    QVERIFY(!parseGamePageSettings("[game_page\n", QStringLiteral("settings.toml")).ok);

    const auto retired = parseGamePageSettings("[game_page]\nchips = [\"steam_waiting\", \"play_time\"]\n"
                                               "chips_order = [\"steam_waiting\", \"play_time\"]\n"
                                               "info_rows = [\"steam_waiting\"]\ninfo_rows_order = [\"steam_waiting\"]\n"
                                               "grid_chips_order = [\"steam_waiting\"]\n",
                                               QStringLiteral("settings.toml"));
    QVERIFY(retired.warnings.isEmpty());
    QCOMPARE(retired.value.chips, (QStringList{"play_time"}));
}

void GeneralTest::gamePageParser()
{
    const auto r = parseGamePageSettings("[game_page]\nchips = [\"rating\", \"play_time\", \"rating\"]\n"
                                         "buttons = []\ninfo_rows = [\"times_played\", \"date_added\"]\n"
                                         "recent_period = \"month\"\nplay_time_format = \"hours_minutes\"\n"
                                         "show_description = false\nshow_tags = false\nshow_clips = false\n"
                                         "chips_without_icon = [\"steam_update\", \"rating\"]\n",
                                         QStringLiteral("settings.toml"));
    QVERIFY(r.warnings.isEmpty());
    QCOMPARE(r.value.chips, (QStringList{"rating", "play_time"}));
    QVERIFY(r.value.buttons.isEmpty());
    QCOMPARE(r.value.infoRows, (QStringList{"times_played", "date_added"}));
    QCOMPARE(r.value.recentPeriod, QStringLiteral("month"));
    QCOMPARE(r.value.playTimeFormat, QStringLiteral("hours_minutes"));
    QVERIFY(r.value.sections.isEmpty());
    QCOMPARE(r.value.chipsWithoutIcon, (QStringList{"steam_update", "rating"}));

    const auto bad = parseGamePageSettings("[game_page]\nchips = [\"play_tim\", \"menu\"]\nbuttons = \"favorite\"\n"
                                           "info_rows = [\"favorite\"]\nrecent_period = \"day\"\n"
                                           "play_time_format = \"clock\"\n"
                                           "show_tags = 1\nchip = []\nchips_without_icon = [\"favorite\"]\n",
                                           QStringLiteral("settings.toml"));
    QCOMPARE(bad.warnings.size(), 9);
    // The keys are read in alphabetical order, so look for each message.
    const QString all = bad.warnings.join(u'\n');
    QVERIFY(all.contains(QStringLiteral("unknown name \"play_tim\" in chips")));
    QVERIFY(all.contains(QStringLiteral("did you mean \"play_time\"")));
    QVERIFY(all.contains(QStringLiteral("unknown name \"menu\" in chips")));
    QVERIFY(all.contains(QStringLiteral("buttons must be a list")));
    QVERIFY(all.contains(QStringLiteral("unknown name \"favorite\" in info_rows")));
    QVERIFY(all.contains(QStringLiteral("recent_period must be")));
    QVERIFY(all.contains(QStringLiteral("play_time_format must be \"hours\", \"hours_minutes\"")));
    QVERIFY(all.contains(QStringLiteral("show_tags must be true or false")));
    QVERIFY(all.contains(QStringLiteral("unknown key chip")));
    QVERIFY(all.contains(QStringLiteral("unknown name \"favorite\" in chips_without_icon")));
    // What isn't valid leaves the default.
    QCOMPARE(bad.value.buttons, GamePageSettings().buttons);
    QCOMPARE(bad.value.recentPeriod, QStringLiteral("two_weeks"));
    QCOMPARE(bad.value.playTimeFormat, QStringLiteral("hours"));
    QCOMPARE(bad.value.sections, GamePageSettings::defaultSections());

    const auto ordered = parseGamePageSettings("[game_page]\nsections = [\"clips\", \"description\"]\n"
                                               "show_description = false\n",
                                               QStringLiteral("settings.toml"));
    QVERIFY(ordered.warnings.isEmpty());
    QCOMPARE(ordered.value.sections, (QStringList{"clips", "description"}));
}

void GeneralTest::gamePageOrderKeepsNewNames()
{
    GamePageSettings s;
    s.buttons = {"settings", "favorite"};
    // Without an order, the shown ones come first, in their listed order, then the rest as the catalog has them.
    QCOMPARE(gamePageOrder(s, GamePage::buttons),
             (QStringList{"settings", "favorite", "open_platform", "folder", "save_folder", "backup", "gallery", "mod_manager", "menu"}));

    // Hidden ones keep their slots; the shown ones' listed order wins over the order's.
    s.buttonsOrder = {"folder", "favorite", "menu", "settings"};
    QCOMPARE(gamePageOrder(s, GamePage::buttons),
             (QStringList{"folder", "settings", "menu", "favorite", "open_platform", "save_folder", "backup", "gallery", "mod_manager"}));

    // A name a later version adds follows the listed ones.
    s.infoRows = {"rating"};
    s.infoRowsOrder = {"status", "rating"};
    const QStringList rows = gamePageOrder(s, GamePage::infoRows);
    QCOMPARE(rows.first(), QStringLiteral("status"));
    QCOMPARE(rows.size(), gamePageChoices(GamePage::infoRows).size());
    QCOMPARE(rows.at(1), QStringLiteral("rating"));

    // A new chip is a new info row too: the two places never drift apart.
    QCOMPARE(gamePageChoices(GamePage::chips), gamePageChoices(GamePage::infoRows));
    QVERIFY(!gamePageChoices(GamePage::infoRows).contains(QStringLiteral("steam_waiting")));
    QVERIFY(gamePageChoices(GamePage::infoRows).contains(QStringLiteral("protondb")));
    QVERIFY(!GamePageSettings::defaultInfoRows().contains(QStringLiteral("protondb")));
    QVERIFY(gamePageChoices(GamePage::chips).contains(QStringLiteral("friends_playing")));
    QVERIFY(!GamePageSettings::defaultInfoRows().contains(QStringLiteral("friends_playing")));
}

void GeneralTest::gamePageSettersWrite()
{
    QTemporaryDir data;
    QFile settings(data.filePath(QStringLiteral("settings.toml")));
    QVERIFY(settings.open(QIODevice::WriteOnly));
    settings.write("# mine\n[general]\nstart_fullscreen = true\n");
    settings.close();
    General general(data.path());
    QSignalSpy page(&general, &General::gamePageChanged);
    QSignalSpy window(&general, &General::settingsChanged);
    const auto fileText = [&] {
        QFile f(settings.fileName());
        return f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray();
    };

    QCOMPARE(general.gamePageButtons(), GamePageSettings::defaultButtons());
    QCOMPARE(general.setGamePageShown(GamePage::buttons, QStringLiteral("menu"), true), QString());
    QCOMPARE(general.gamePageButtons(), (QStringList{"favorite", "open_platform", "folder", "settings", "menu"}));
    QCOMPARE(page.size(), 1);
    QCOMPARE(window.size(), 0);
    QVERIFY(fileText().contains("buttons = [\"favorite\", \"open_platform\", \"folder\", \"settings\", \"menu\"]"));
    QVERIFY(fileText().contains("# mine"));
    QVERIFY(fileText().contains("start_fullscreen = true"));

    // Hiding one keeps its slot, so it comes back where it was.
    QCOMPARE(general.setGamePageShown(GamePage::buttons, QStringLiteral("folder"), false), QString());
    QCOMPARE(general.gamePageButtons(), (QStringList{"favorite", "open_platform", "settings", "menu"}));
    QCOMPARE(general.gamePageOrder(GamePage::buttons).indexOf(QStringLiteral("folder")), 2);
    QCOMPARE(general.setGamePageShown(GamePage::buttons, QStringLiteral("folder"), true), QString());
    QCOMPARE(general.gamePageButtons(), (QStringList{"favorite", "open_platform", "folder", "settings", "menu"}));

    // A new order is written whole, and the shown ones follow it.
    QCOMPARE(general.setGamePageOrder(GamePage::buttons,
                                      {"menu", "settings", "save_folder", "backup", "gallery", "folder", "open_platform", "favorite", "mod_manager"}),
             QString());
    QCOMPARE(general.gamePageButtons(), (QStringList{"menu", "settings", "folder", "open_platform", "favorite"}));
    QCOMPARE(general.gamePageOrder(GamePage::buttons),
             (QStringList{"menu", "settings", "save_folder", "backup", "gallery", "folder", "open_platform", "favorite", "mod_manager"}));
    QVERIFY(fileText().contains("buttons_order"));
    // Not every name, or one that isn't a choice, changes nothing.
    QCOMPARE(general.setGamePageOrder(GamePage::buttons, {"menu"}), QString());
    QCOMPARE(general.setGamePageShown(GamePage::buttons, QStringLiteral("nope"), true), QString());
    QCOMPARE(general.setGamePageShown(QStringLiteral("elsewhere"), QStringLiteral("menu"), true), QString());
    QCOMPARE(general.gamePageButtons().first(), QStringLiteral("menu"));

    // Back to the defaults leaves no keys.
    QCOMPARE(general.setGamePageOrder(GamePage::buttons, {"favorite", "open_platform", "folder", "settings", "save_folder", "backup", "gallery", "mod_manager", "menu"}),
             QString());
    QCOMPARE(general.setGamePageShown(GamePage::buttons, QStringLiteral("menu"), false), QString());
    QCOMPARE(general.gamePageButtons(), GamePageSettings::defaultButtons());
    QVERIFY(!fileText().contains("buttons"));

    QCOMPARE(general.setRecentPeriod(QStringLiteral("year")), QString());
    QCOMPARE(general.recentPeriod(), QStringLiteral("year"));
    QVERIFY(fileText().contains("recent_period = \"year\""));
    QCOMPARE(general.setRecentPeriod(QStringLiteral("fortnight")), QString());
    QCOMPARE(general.recentPeriod(), QStringLiteral("year"));
    QCOMPARE(general.setRecentPeriod(QStringLiteral("two_weeks")), QString());
    QVERIFY(!fileText().contains("recent_period"));

    QCOMPARE(general.setPlayTimeFormat(QStringLiteral("hours_minutes")), QString());
    QCOMPARE(general.playTimeFormat(), QStringLiteral("hours_minutes"));
    QVERIFY(fileText().contains("play_time_format = \"hours_minutes\""));
    QCOMPARE(general.setPlayTimeFormat(QStringLiteral("clock")), QString());
    QCOMPARE(general.playTimeFormat(), QStringLiteral("hours_minutes"));
    QCOMPARE(general.setPlayTimeFormat(QStringLiteral("hours")), QString());
    QVERIFY(!fileText().contains("play_time_format"));

    QVERIFY(general.showScreenshots());
    QCOMPARE(general.setShowScreenshots(false), QString());
    QVERIFY(!general.showScreenshots());
    QVERIFY(fileText().contains("show_screenshots = false"));
    QCOMPARE(general.setShowScreenshots(true), QString());
    QVERIFY(general.showScreenshots());
    QVERIFY(!fileText().contains("show_screenshots"));

    const QString panels = QStringLiteral("panels");
    QVERIFY(general.showAchievements());
    QCOMPARE(general.setGamePageShown(panels, QStringLiteral("achievements"), false), QString());
    QVERIFY(!general.showAchievements());
    QCOMPARE(general.gamePagePanels(), (QStringList{"info", "friends"}));
    QVERIFY(fileText().contains("panels = [\"info\", \"friends\"]"));
    QCOMPARE(general.setGamePageOrder(panels, {"achievements", "info", "friends"}), QString());
    QCOMPARE(general.gamePageOrder(panels), (QStringList{"achievements", "info", "friends"}));
    QCOMPARE(general.setGamePageShown(panels, QStringLiteral("achievements"), true), QString());
    QCOMPARE(general.gamePagePanels(), (QStringList{"achievements", "info", "friends"}));
    QCOMPARE(general.setGamePageOrder(panels, GamePageSettings::defaultPanels()), QString());
    QVERIFY(!fileText().contains("panels"));

    const auto legacy = parseGamePageSettings("[game_page]\nshow_achievements = false\n", QStringLiteral("s.toml"));
    QCOMPARE(legacy.value.panels, (QStringList{"info", "friends"}));
    const auto listed = parseGamePageSettings("[game_page]\nshow_achievements = false\npanels = [\"achievements\"]\n",
                                              QStringLiteral("s.toml"));
    QCOMPARE(listed.value.panels, (QStringList{"achievements"}));
    QVERIFY(listed.warnings.isEmpty());

    QCOMPARE(general.friendsGroups(), (QStringList{"playing", "recent", "previous"}));
    QCOMPARE(general.setFriendsGroup(QStringLiteral("recent"), false), QString());
    QCOMPARE(general.friendsGroups(), (QStringList{"playing", "previous"}));
    QVERIFY(fileText().contains("friends_groups = [\"playing\", \"previous\"]"));
    QCOMPARE(general.setFriendsGroup(QStringLiteral("playing"), false), QString());
    QCOMPARE(general.setFriendsGroup(QStringLiteral("previous"), false), QString());
    QCOMPARE(general.friendsGroups(), QStringList());
    QVERIFY(fileText().contains("friends_groups = []"));
    QCOMPARE(general.setFriendsGroup(QStringLiteral("nobody"), true), QString());
    QCOMPARE(general.setFriendsGroup(QStringLiteral("previous"), true), QString());
    QCOMPARE(general.setFriendsGroup(QStringLiteral("playing"), true), QString());
    QCOMPARE(general.setFriendsGroup(QStringLiteral("recent"), true), QString());
    QVERIFY(!fileText().contains("friends_groups"));

    QVERIFY(general.showVideo());
    QCOMPARE(general.setShowVideo(false), QString());
    QVERIFY(!general.showVideo());
    QVERIFY(fileText().contains("show_video = false"));
    QCOMPARE(general.setShowVideo(true), QString());
    QVERIFY(general.showVideo());
    QVERIFY(!fileText().contains("show_video"));

    const QString sections = QStringLiteral("sections");
    QCOMPARE(general.setGamePageShown(sections, QStringLiteral("tags"), false), QString());
    QVERIFY(!general.showTags());
    QVERIFY(general.showDescription());
    QVERIFY(fileText().contains("sections = [\"description\", \"clips\"]"));
    QCOMPARE(general.setGamePageOrder(sections, {"clips", "tags", "description"}), QString());
    QCOMPARE(general.gamePageSections(), (QStringList{"clips", "description"}));
    QCOMPARE(general.gamePageOrder(sections), (QStringList{"clips", "tags", "description"}));
    QCOMPARE(general.setGamePageShown(sections, QStringLiteral("tags"), true), QString());
    QCOMPARE(general.setGamePageOrder(sections, GamePageSettings::defaultSections()), QString());
    QVERIFY(!fileText().contains("sections"));

    QCOMPARE(general.setChipIconShown(QStringLiteral("status"), false), QString());
    QCOMPARE(general.setChipIconShown(QStringLiteral("play_time"), false), QString());
    QCOMPARE(general.chipsWithoutIcon(), (QStringList{"play_time", "status"}));
    QVERIFY(fileText().contains("chips_without_icon = [\"play_time\", \"status\"]"));
    QCOMPARE(general.setChipIconShown(QStringLiteral("menu"), false), QString());
    QCOMPARE(general.chipsWithoutIcon(), (QStringList{"play_time", "status"}));
    QCOMPARE(general.setChipIconShown(QStringLiteral("status"), true), QString());
    QCOMPARE(general.setChipIconShown(QStringLiteral("play_time"), true), QString());
    QVERIFY(general.chipsWithoutIcon().isEmpty());
    QVERIFY(!fileText().contains("chips_without_icon"));

    QCOMPARE(general.setSectionCollapsed(QStringLiteral("clips"), true), QString());
    QCOMPARE(general.setSectionCollapsed(QStringLiteral("description"), true), QString());
    QCOMPARE(general.setSectionCollapsed(QStringLiteral("status"), true), QString());
    QCOMPARE(general.collapsedSections(), (QStringList{"description", "clips"}));
    QVERIFY(fileText().contains("collapsed_sections = [\"description\", \"clips\"]"));
    QCOMPARE(general.setSectionCollapsed(QStringLiteral("description"), false), QString());
    QCOMPARE(general.setSectionCollapsed(QStringLiteral("clips"), false), QString());
    QVERIFY(general.collapsedSections().isEmpty());
    QVERIFY(!fileText().contains("collapsed_sections"));
    QVERIFY(general.problems().isEmpty());

    // A hand edit shows on reload, and a mistake in it is a problem.
    QVERIFY(settings.open(QIODevice::WriteOnly | QIODevice::Truncate));
    settings.write("[general]\nstart_fullscreen = 1\n[game_page]\nchips = [\"nope\"]\n");
    settings.close();
    QSignalSpy pageChanged(&general, &General::gamePageChanged);
    general.reload();
    QVERIFY(general.gamePageChips().isEmpty());
    QCOMPARE(general.problems().size(), 2);
    QVERIFY(general.problems().last().startsWith(QStringLiteral("settings.toml:4: unknown name \"nope\"")));
    // The Game page's own, without [general]'s.
    QCOMPARE(general.gamePageProblems(), QStringList{general.problems().last()});

    // Fixing the mistake without changing what's shown still tells the page its problems are gone.
    QVERIFY(settings.open(QIODevice::WriteOnly | QIODevice::Truncate));
    settings.write("[game_page]\nchips = []\n");
    settings.close();
    pageChanged.clear();
    general.reload();
    QVERIFY(general.gamePageProblems().isEmpty());
    QCOMPARE(pageChanged.count(), 1);
}

void GeneralTest::gamePageMigration()
{
    QTemporaryDir dir;
    const QString settings = dir.filePath(QStringLiteral("settings.toml"));
    const QString marker = dir.filePath(QStringLiteral("state/game-page-migrated"));
    const auto write = [&](const QByteArray &text) {
        QFile f(settings);
        QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Truncate));
        f.write(text);
    };
    const auto parsed = [&] {
        QFile f(settings);
        return f.open(QIODevice::ReadOnly) ? parseGamePageSettings(f.readAll(), QStringLiteral("settings.toml"))
                                           : Parsed<GamePageSettings>();
    };

    // What the style set is copied; what [game_page] already has is not.
    write("# mine\n[appearance]\nstyle = \"Mine\"\n\n[game_page]\nshow_tags = true\n");
    QVariantMap legacy;
    legacy.insert(QStringLiteral("stats"), QVariantList{QStringLiteral("last_played"), QStringLiteral("rating")});
    legacy.insert(QStringLiteral("action_buttons"), QVariantList{QStringLiteral("menu")});
    legacy.insert(QStringLiteral("show_tags"), false);
    legacy.insert(QStringLiteral("show_description"), false);
    QString error;
    QVERIFY(GamePageMigration::run(settings, marker, legacy, &error));
    QVERIFY(error.isEmpty());
    QVERIFY(QFileInfo::exists(marker));
    auto r = parsed();
    QVERIFY(r.warnings.isEmpty());
    QCOMPARE(r.value.chips, (QStringList{"last_played", "rating"}));
    QCOMPARE(r.value.buttons, QStringList{"menu"});
    QCOMPARE(r.value.infoRows, GamePageSettings::defaultInfoRows());
    QCOMPARE(r.value.sections, (QStringList{"clips", "tags"}));
    QFile f(settings);
    QVERIFY(f.open(QIODevice::ReadOnly));
    QVERIFY(f.readAll().startsWith("# mine"));
    f.close();

    // Done once: a theme that sets them later is not copied.
    write("[appearance]\nstyle = \"Mine\"\n");
    QVERIFY(!GamePageMigration::run(settings, marker, legacy, &error));
    QCOMPARE(parsed().value, GamePageSettings());
    QVERIFY(f.open(QIODevice::ReadOnly));
    QVERIFY(!f.readAll().contains("game_page"));
    f.close();

    // A style that sets none changes nothing, and still counts as done.
    QVERIFY(QFile::remove(marker));
    QVERIFY(GamePageMigration::run(settings, marker, {}, &error));
    QVERIFY(QFileInfo::exists(marker));
    QVERIFY(f.open(QIODevice::ReadOnly));
    QVERIFY(!f.readAll().contains("game_page"));
    f.close();

    // A style is a file people share: a quote, a backslash or a line break can't change the file, and only
    // names the game page knows get in.
    QVERIFY(QFile::remove(marker));
    write("# mine\n[general]\nview = \"grid\"\n");
    QVariantMap hostile;
    hostile.insert(QStringLiteral("stats"), QVariantList{QStringLiteral("a\"b"), QStringLiteral("C:\\x"),
                                                         QStringLiteral("x\"]\n[metadata]\nlook_up_new_games = true\n#"),
                                                         QStringLiteral("rating")});
    QVERIFY(GamePageMigration::run(settings, marker, hostile, &error));
    QVERIFY(QFileInfo::exists(marker));
    QVERIFY(f.open(QIODevice::ReadOnly));
    const QByteArray written = f.readAll();
    f.close();
    QVERIFY(!written.contains("metadata"));
    QVERIFY(written.contains("view = \"grid\""));
    r = parsed();
    QVERIFY(r.warnings.isEmpty());
    QCOMPARE(r.value.chips, QStringList{"rating"});

    // A list in the info panel's own order.
    QVERIFY(QFile::remove(marker));
    QVariantMap rows;
    rows.insert(QStringLiteral("info_rows"), QVariantList{QStringLiteral("status"), QStringLiteral("platform")});
    QVERIFY(GamePageMigration::run(settings, marker, rows, &error));
    QCOMPARE(parsed().value.infoRows, (QStringList{"status", "platform"}));
}

void GeneralTest::gamePageMigrationFailuresLeaveNoMarker()
{
    QTemporaryDir dir;
    const QString settings = dir.filePath(QStringLiteral("settings.toml"));
    const QString marker = dir.filePath(QStringLiteral("state/game-page-migrated"));
    const auto contents = [&] {
        QFile f(settings);
        return f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray("(missing)");
    };
    const auto write = [&](const QByteArray &text) {
        QFile f(settings);
        QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Truncate));
        f.write(text);
    };
    QVariantMap legacy;
    legacy.insert(QStringLiteral("show_tags"), false);
    QString error;

    // No settings.toml to copy into: tried again next start.
    QVERIFY(!GamePageMigration::run(settings, marker, legacy, &error));
    QCOMPARE(error, QStringLiteral("could not read %1").arg(settings));
    QVERIFY(!QFileInfo::exists(marker));
    QCOMPARE(contents(), QByteArray("(missing)"));

    // A settings.toml that doesn't parse is left as it is.
    const QByteArray broken = "[game_page\nshow_tags = true\n";
    write(broken);
    error.clear();
    QVERIFY(!GamePageMigration::run(settings, marker, legacy, &error));
    QVERIFY(!error.isEmpty());
    QVERIFY(!QFileInfo::exists(marker));
    QCOMPARE(contents(), broken);

    // A folder it can't write in.
    const QByteArray mine = "[general]\nview = \"grid\"\n";
    write(mine);
    QVERIFY(QFile::setPermissions(dir.path(), QFileDevice::ReadOwner | QFileDevice::ExeOwner));
    const bool locked = !QFileInfo(dir.path()).isWritable();
    error.clear();
    const bool ran = GamePageMigration::run(settings, marker, legacy, &error);
    QVERIFY(QFile::setPermissions(dir.path(), QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner));
    if (locked) {
        QVERIFY(!ran);
        QVERIFY(!error.isEmpty());
        QVERIFY(!QFileInfo::exists(marker));
        QCOMPARE(contents(), mine);
    }

    // The marker can't be made: settings.toml has the values, and the next start finds them there and adds nothing.
    write(mine);
    {
        QFile blocker(dir.filePath(QStringLiteral("state")));
        QVERIFY(blocker.open(QIODevice::WriteOnly));
    }
    error.clear();
    QVERIFY(!GamePageMigration::run(settings, marker, legacy, &error));
    QVERIFY(!error.isEmpty());
    QVERIFY(QFileInfo(dir.filePath(QStringLiteral("state"))).isFile());
    const QByteArray once = contents();
    QVERIFY(once.contains("show_tags = false"));
    QVERIFY(QFile::remove(dir.filePath(QStringLiteral("state"))));
    error.clear();
    QVERIFY(GamePageMigration::run(settings, marker, legacy, &error));
    QCOMPARE(contents(), once);
    QVERIFY(QFileInfo::exists(marker));
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
    QCOMPARE(entry.exec, Autostart::entryExec(Autostart::currentProgram()));

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
        QCOMPARE(Autostart::readEntry(folder).icon, QStringLiteral("lattis"));

        // The app icon changed: only the icon is rewritten.
        const QString icon = data.filePath(QStringLiteral("icons/lattis/lattis-0123456789ab-256.png"));
        general.setAppIconApplied(icon, {});
        QCOMPARE(Autostart::readEntry(folder).icon, icon);
        QCOMPARE(Autostart::readEntry(folder).exec, current);
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

namespace {
// LATTIS_DATA set for one test and put back after it.
struct DataEnv
{
    explicit DataEnv(const QString &value)
    {
        if (value.isEmpty())
            qunsetenv("LATTIS_DATA");
        else
            qputenv("LATTIS_DATA", QFile::encodeName(value));
    }
    ~DataEnv() { qunsetenv("LATTIS_DATA"); }
};
}

void GeneralTest::loginEntryNamesTheVariableOnlyWhenSet()
{
    QTemporaryDir data;
    const QString folder = Autostart::autostartFolder();
    {
        DataEnv env{QString()};
        General general(data.path());
        QCOMPARE(general.setStartAtLogin(true), QString());
        QCOMPARE(Autostart::readEntry(folder).exec, Autostart::entryExec(Autostart::currentProgram()));
        QVERIFY(Autostart::execDataDir(Autostart::readEntry(folder).exec).isEmpty());
    }
    {
        DataEnv env{data.path()};
        General general(data.path());
        QCOMPARE(general.setStartAtLogin(true), QString());
        QCOMPARE(Autostart::execDataDir(Autostart::readEntry(folder).exec), data.path());
    }
    QVERIFY(Autostart::removeEntry(folder).isEmpty());
}

void GeneralTest::loginEntryBelongsToItsCopy()
{
    QTemporaryDir mine;
    QTemporaryDir other;
    const QString folder = Autostart::autostartFolder();
    const QString old = QStringLiteral("/old/place/lattis");
    const auto execAfter = [&](const QString &named, const QString &envDir) {
        DataEnv env{envDir};
        Autostart::writeEntry(folder, old, named);
        General general(mine.path());
        general.refreshAutostart();
        return Autostart::readEntry(folder).exec;
    };
    const QString oldExec = Autostart::entryExec(old);
    const QString oldOther = Autostart::entryExec(old, other.path());
    const QString oldMine = Autostart::entryExec(old, mine.path());

    // A copy on a custom dir leaves the default copy's entry (no variable) alone...
    QCOMPARE(execAfter(QString(), mine.path()), oldExec);
    // ...and another copy's entry.
    QCOMPARE(execAfter(other.path(), mine.path()), oldOther);
    // A copy on the default dir leaves a custom copy's entry alone, but follows its own entry.
    QCOMPARE(execAfter(other.path(), QString()), oldOther);
    QCOMPARE(execAfter(QString(), QString()), Autostart::entryExec(Autostart::currentProgram()));
    // A copy on a custom dir follows an entry that names its dir, and keeps naming it.
    QCOMPARE(execAfter(mine.path(), mine.path()), Autostart::entryExec(Autostart::currentProgram(), mine.path()));
    QVERIFY(oldMine != oldExec);
    QVERIFY(Autostart::removeEntry(folder).isEmpty());
}

void GeneralTest::gallerySettings()
{
    const auto none = parseGallerySettings("", QStringLiteral("settings.toml"));
    QCOMPARE(none.value, GallerySettings{});
    QCOMPARE(none.value.group, QStringLiteral("month"));
    QCOMPARE(none.value.sort, QStringLiteral("newest"));

    const auto set = parseGallerySettings(
        "[gallery]\ngroup = \"game\"\nsort = \"oldest\"\ntile_width = 320\nfolded_sections = [\"date\", \"type\", \"date\"]\n",
        QStringLiteral("settings.toml"));
    QVERIFY(set.warnings.isEmpty());
    QCOMPARE(set.value.group, QStringLiteral("game"));
    QCOMPARE(set.value.sort, QStringLiteral("oldest"));
    QCOMPARE(set.value.tileWidth, 320);
    QCOMPARE(set.value.foldedSections, (QStringList{"date", "type"}));

    const auto bad = parseGallerySettings(
        "[gallery]\ngroup = \"year\"\ntile_width = 20\nfolded_sections = [\"dates\"]\nsize = 1\n",
        QStringLiteral("settings.toml"));
    QCOMPARE(bad.warnings.size(), 4);
    QCOMPARE(bad.value, GallerySettings{});

    QTemporaryDir data;
    QFile created(data.filePath(QStringLiteral("settings.toml")));
    QVERIFY(created.open(QIODevice::WriteOnly));
    created.close();
    General general(data.path());
    QSignalSpy changed(&general, &General::galleryChanged);
    QSignalSpy window(&general, &General::settingsChanged);
    QCOMPARE(general.setGalleryGroup(QStringLiteral("game")), QString());
    QCOMPARE(general.galleryGroup(), QStringLiteral("game"));
    QCOMPARE(general.setGallerySort(QStringLiteral("oldest")), QString());
    QCOMPARE(general.setGalleryTileWidth(300), QString());
    QCOMPARE(general.galleryTileWidth(), 300);
    QCOMPARE(general.setGallerySectionFolded(QStringLiteral("date"), true), QString());
    QCOMPARE(general.setGallerySectionFolded(QStringLiteral("type"), true), QString());
    QCOMPARE(general.galleryFoldedSections(), (QStringList{"type", "date"}));
    QCOMPARE(changed.size(), 5);
    QCOMPARE(window.size(), 0);

    general.setGalleryGroup(QStringLiteral("nope"));
    QCOMPARE(general.galleryGroup(), QStringLiteral("game"));

    QCOMPARE(general.setGalleryGroup(QStringLiteral("month")), QString());
    QCOMPARE(general.setGallerySectionFolded(QStringLiteral("date"), false), QString());
    QCOMPARE(general.setGallerySectionFolded(QStringLiteral("type"), false), QString());
    QFile settings(data.filePath(QStringLiteral("settings.toml")));
    QVERIFY(settings.open(QIODevice::ReadOnly));
    const QByteArray text = settings.readAll();
    QVERIFY(!text.contains("group"));
    QVERIFY(!text.contains("folded_sections"));
    QVERIFY(text.contains("sort = \"oldest\""));
}

void GeneralTest::overviewParser()
{
    const auto none = parseOverviewSettings("", QStringLiteral("settings.toml"));
    QCOMPARE(none.value, OverviewSettings{});
    QCOMPARE(none.value.above, (QStringList{"tiles", "continue_playing"}));

    const auto set = parseOverviewSettings(
        "[overview]\nleft = [\"tags\", \"storage\", \"tags\"]\nright = [\"facts\"]\nhidden = [\"years\", \"tiles\"]\n"
        "above = [\"continue_playing\"]\nbelow = [\"tiles\"]\n",
        QStringLiteral("settings.toml"));
    QVERIFY(set.warnings.isEmpty());
    QCOMPARE(set.value.left, (QStringList{"tags", "storage"}));
    QCOMPARE(set.value.right, QStringList{"facts"});
    QCOMPARE(set.value.hidden, (QStringList{"years", "tiles"}));
    QCOMPARE(set.value.above, QStringList{"continue_playing"});
    QCOMPARE(set.value.below, QStringList{"tiles"});

    // A card put below isn't also above by the default.
    const auto below = parseOverviewSettings("[overview]\nbelow = [\"continue_playing\"]\n", QStringLiteral("settings.toml"));
    QCOMPARE(below.value.above, QStringList{"tiles"});

    // A row card isn't a column card and the other way round; strays are dropped with a warning.
    const auto bad = parseOverviewSettings(
        "[overview]\nleft = [\"tiles\", \"storag\", \"saves\"]\nhidden = \"saves\"\nabove = [\"saves\"]\nsize = 1\n",
        QStringLiteral("settings.toml"));
    QCOMPARE(bad.warnings.size(), 5);
    QCOMPARE(bad.value.left, QStringList{"saves"});
    QVERIFY(bad.value.hidden.isEmpty());
    QVERIFY(bad.value.above.isEmpty());
}

void GeneralTest::overviewResolving()
{
    // Nothing set: the page deals the columns itself and the full-width rows keep their default places.
    const OverviewLayout automatic = resolveOverview({});
    QVERIFY(automatic.automatic);
    QVERIFY(automatic.left.isEmpty() && automatic.right.isEmpty());
    QCOMPARE(automatic.above, (QStringList{"tiles", "continue_playing"}));
    QVERIFY(automatic.below.isEmpty());

    // Unknown names and repeats drop; a card in both columns stays in the left one.
    const QStringList all = overviewColumnCards();
    OverviewSettings full;
    full.left = QStringList{"storage", "nope", "tiles", "tags", "storage"};
    full.right = QStringList{"tags", "years"};
    for (const QString &name : all)
        if (!full.left.contains(name) && !full.right.contains(name))
            full.right.append(name);
    const OverviewLayout resolved = resolveOverview(full);
    QVERIFY(!resolved.automatic);
    QCOMPARE(resolved.left, (QStringList{"storage", "tags"}));
    QCOMPARE(resolved.right.size(), all.size() - 2);
    QVERIFY(!resolved.right.contains(QStringLiteral("tags")));

    // A card in neither list goes to the end of the shorter one, counted as the cards are added.
    OverviewSettings partial;
    partial.left = QStringList{"facts", "saves", "years"};
    partial.right = QStringList{"tags"};
    const OverviewLayout added = resolveOverview(partial);
    // storage, platforms, statuses, attention, backlog, nearly_done, compatibility are missing, in catalog order
    QCOMPARE(added.right, (QStringList{"tags", "storage", "platforms", "attention", "nearly_done"}));
    QCOMPARE(added.left, (QStringList{"facts", "saves", "years", "statuses", "backlog", "compatibility"}));

    // A tie goes to the left column.
    OverviewSettings tie;
    tie.left = QStringList{"storage"};
    tie.right = QStringList{"platforms"};
    QCOMPARE(resolveOverview(tie).left.value(1), QStringLiteral("statuses"));

    // A hidden card keeps its place; a name for no card is dropped.
    OverviewSettings hidden;
    hidden.left = QStringList{"storage"};
    hidden.hidden = QStringList{"storage", "tiles", "nope", "tiles"};
    const OverviewLayout withHidden = resolveOverview(hidden);
    QVERIFY(withHidden.left.contains(QStringLiteral("storage")));
    QCOMPARE(withHidden.hidden, (QStringList{"storage", "tiles"}));

    // Only one row card named: the other keeps its default place, above.
    OverviewSettings rows;
    rows.above = QStringList{"continue_playing"};
    QCOMPARE(resolveOverview(rows).above, (QStringList{"continue_playing", "tiles"}));
    rows.above = {};
    rows.below = QStringList{"tiles", "continue_playing"};
    const OverviewLayout below = resolveOverview(rows);
    QVERIFY(below.above.isEmpty());
    QCOMPARE(below.below, (QStringList{"tiles", "continue_playing"}));
}

void GeneralTest::overviewSetters()
{
    QTemporaryDir data;
    QFile created(data.filePath(QStringLiteral("settings.toml")));
    QVERIFY(created.open(QIODevice::WriteOnly));
    created.close();
    QStringList left;
    QStringList right;
    {
    General general(data.path());
    QSignalSpy changed(&general, &General::overviewChanged);
    QSignalSpy window(&general, &General::settingsChanged);
    QVERIFY(general.overviewAutomatic());
    QCOMPARE(general.overviewAbove(), (QStringList{"tiles", "continue_playing"}));

    QCOMPARE(general.setOverviewHidden({"years", "nope", "tiles"}), QString());
    QCOMPARE(general.overviewHidden(), (QStringList{"tiles", "years"}));
    QCOMPARE(changed.size(), 1);

    QCOMPARE(general.setOverviewColumns({"facts", "storage"}, {"tags"}), QString());
    QVERIFY(!general.overviewAutomatic());
    QCOMPARE(general.overviewLeft().first(), QStringLiteral("facts"));
    QCOMPARE(general.overviewRight().first(), QStringLiteral("tags"));
    QCOMPARE(general.overviewLeft().size() + general.overviewRight().size(), overviewColumnCards().size());
    QCOMPARE(changed.size(), 2);

    QCOMPARE(general.setOverviewRows({"tiles"}, {"continue_playing"}), QString());
    QCOMPARE(general.overviewAbove(), QStringList{"tiles"});
    QCOMPARE(general.overviewBelow(), QStringList{"continue_playing"});
    QCOMPARE(window.size(), 0);

    left = general.overviewLeft();
    right = general.overviewRight();
    }

    // It survives a new reader. The first General is gone: only one may live.
    General general(data.path());
    QCOMPARE(general.overviewLeft(), left);
    QCOMPARE(general.overviewRight(), right);
    QCOMPARE(general.overviewHidden(), (QStringList{"tiles", "years"}));
    QCOMPARE(general.overviewBelow(), QStringList{"continue_playing"});

    // The default rows leave no keys.
    QCOMPARE(general.setOverviewRows({"tiles", "continue_playing"}, {}), QString());
    QFile settings(data.filePath(QStringLiteral("settings.toml")));
    QVERIFY(settings.open(QIODevice::ReadOnly));
    const QByteArray text = settings.readAll();
    settings.close();
    QVERIFY(!text.contains("above"));
    QVERIFY(!text.contains("below"));
    QVERIFY(text.contains("hidden = [\"tiles\", \"years\"]"));

    QCOMPARE(general.resetOverviewLayout(), QString());
    QVERIFY(general.overviewAutomatic());
    QVERIFY(general.overviewHidden().isEmpty());
    QVERIFY(settings.open(QIODevice::ReadOnly));
    const QByteArray cleared = settings.readAll();
    QVERIFY(!cleared.contains("left"));
    QVERIFY(!cleared.contains("hidden"));
}

QTEST_MAIN(GeneralTest)
#include "general_test.moc"
