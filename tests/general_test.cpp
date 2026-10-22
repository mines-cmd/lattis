#include "autostart.h"
#include "general.h"
#include "gamepagemigration.h"
#include "gamepagesettings.h"
#include "generalsettings.h"
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
    void railHiddenSetting();
    void systemLocaleSetting();
    void reduceMotionSetting();
    void railHiddenButtonsSetting();
    void railButtonOrderSetting();
    void gamePageDefaults();
    void gamePageParser();
    void gamePageOrderKeepsNewNames();
    void gamePageSettersWrite();
    void gamePageMigration();
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


void GeneralTest::gamePageDefaults()
{
    const auto empty = parseGamePageSettings({}, QStringLiteral("settings.toml"));
    QVERIFY(empty.ok);
    QCOMPARE(empty.value, GamePageSettings());
    QCOMPARE(empty.value.chips,
             (QStringList{"play_time", "last_played", "steam_waiting", "steam_update"}));
    QCOMPARE(empty.value.buttons, (QStringList{"favorite", "open_platform", "folder", "settings"}));
    QCOMPARE(empty.value.infoRows,
             (QStringList{"release_date", "rating", "status", "install_size", "platform"}));
    QCOMPARE(empty.value.recentPeriod, QStringLiteral("two_weeks"));
    QVERIFY(empty.value.showDescription);
    QVERIFY(empty.value.showTags);

    const auto other = parseGamePageSettings("[general]\nreduce_motion = true\n", QStringLiteral("settings.toml"));
    QCOMPARE(other.value, GamePageSettings());
    QVERIFY(other.warnings.isEmpty());

    QVERIFY(!parseGamePageSettings("[game_page\n", QStringLiteral("settings.toml")).ok);
}

void GeneralTest::gamePageParser()
{
    const auto r = parseGamePageSettings("[game_page]\nchips = [\"rating\", \"play_time\", \"rating\"]\n"
                                         "buttons = []\ninfo_rows = [\"times_played\", \"date_added\"]\n"
                                         "recent_period = \"month\"\nshow_description = false\nshow_tags = false\n",
                                         QStringLiteral("settings.toml"));
    QVERIFY(r.warnings.isEmpty());
    QCOMPARE(r.value.chips, (QStringList{"rating", "play_time"}));
    QVERIFY(r.value.buttons.isEmpty());
    QCOMPARE(r.value.infoRows, (QStringList{"times_played", "date_added"}));
    QCOMPARE(r.value.recentPeriod, QStringLiteral("month"));
    QVERIFY(!r.value.showDescription);
    QVERIFY(!r.value.showTags);

    const auto bad = parseGamePageSettings("[game_page]\nchips = [\"play_tim\", \"menu\"]\nbuttons = \"favorite\"\n"
                                           "info_rows = [\"steam_update\"]\nrecent_period = \"day\"\n"
                                           "show_tags = 1\nchip = []\n",
                                           QStringLiteral("settings.toml"));
    QCOMPARE(bad.warnings.size(), 7);
    // The keys are read in alphabetical order, so look for each message.
    const QString all = bad.warnings.join(u'\n');
    QVERIFY(all.contains(QStringLiteral("unknown name \"play_tim\" in chips")));
    QVERIFY(all.contains(QStringLiteral("did you mean \"play_time\"")));
    QVERIFY(all.contains(QStringLiteral("unknown name \"menu\" in chips")));
    QVERIFY(all.contains(QStringLiteral("buttons must be a list")));
    QVERIFY(all.contains(QStringLiteral("unknown name \"steam_update\" in info_rows")));
    QVERIFY(all.contains(QStringLiteral("recent_period must be")));
    QVERIFY(all.contains(QStringLiteral("show_tags must be true or false")));
    QVERIFY(all.contains(QStringLiteral("unknown key chip")));
    // What isn't valid leaves the default.
    QCOMPARE(bad.value.buttons, GamePageSettings().buttons);
    QCOMPARE(bad.value.recentPeriod, QStringLiteral("two_weeks"));
    QVERIFY(bad.value.showTags);
}

void GeneralTest::gamePageOrderKeepsNewNames()
{
    GamePageSettings s;
    s.buttons = {"settings", "favorite"};
    // Without an order, the shown ones come first, in their listed order, then the rest as the catalog has them.
    QCOMPARE(gamePageOrder(s, GamePage::buttons),
             (QStringList{"settings", "favorite", "open_platform", "folder", "save_folder", "menu"}));

    // Hidden ones keep their slots; the shown ones' listed order wins over the order's.
    s.buttonsOrder = {"folder", "favorite", "menu", "settings"};
    QCOMPARE(gamePageOrder(s, GamePage::buttons),
             (QStringList{"folder", "settings", "menu", "favorite", "open_platform", "save_folder"}));

    // A name a later version adds follows the listed ones.
    s.infoRows = {"rating"};
    s.infoRowsOrder = {"status", "rating"};
    const QStringList rows = gamePageOrder(s, GamePage::infoRows);
    QCOMPARE(rows.first(), QStringLiteral("status"));
    QCOMPARE(rows.size(), gamePageChoices(GamePage::infoRows).size());
    QCOMPARE(rows.at(1), QStringLiteral("rating"));

    // Chips offer the Steam chips; the info panel doesn't.
    QVERIFY(gamePageChoices(GamePage::chips).contains(QStringLiteral("steam_waiting")));
    QVERIFY(!gamePageChoices(GamePage::infoRows).contains(QStringLiteral("steam_waiting")));
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
                                      {"menu", "settings", "save_folder", "folder", "open_platform", "favorite"}),
             QString());
    QCOMPARE(general.gamePageButtons(), (QStringList{"menu", "settings", "folder", "open_platform", "favorite"}));
    QCOMPARE(general.gamePageOrder(GamePage::buttons),
             (QStringList{"menu", "settings", "save_folder", "folder", "open_platform", "favorite"}));
    QVERIFY(fileText().contains("buttons_order"));
    // Not every name, or one that isn't a choice, changes nothing.
    QCOMPARE(general.setGamePageOrder(GamePage::buttons, {"menu"}), QString());
    QCOMPARE(general.setGamePageShown(GamePage::buttons, QStringLiteral("nope"), true), QString());
    QCOMPARE(general.setGamePageShown(QStringLiteral("elsewhere"), QStringLiteral("menu"), true), QString());
    QCOMPARE(general.gamePageButtons().first(), QStringLiteral("menu"));

    // Back to the defaults leaves no keys.
    QCOMPARE(general.setGamePageOrder(GamePage::buttons, {"favorite", "open_platform", "folder", "settings", "save_folder", "menu"}),
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

    QCOMPARE(general.setShowTags(false), QString());
    QVERIFY(!general.showTags());
    QVERIFY(general.showDescription());
    QVERIFY(fileText().contains("show_tags = false"));
    QCOMPARE(general.setShowDescription(false), QString());
    QVERIFY(!general.showDescription());
    QCOMPARE(general.setShowTags(true), QString());
    QCOMPARE(general.setShowDescription(true), QString());
    QVERIFY(!fileText().contains("show_"));
    QVERIFY(general.problems().isEmpty());

    // A hand edit shows on reload, and a mistake in it is a problem.
    QVERIFY(settings.open(QIODevice::WriteOnly | QIODevice::Truncate));
    settings.write("[game_page]\nchips = [\"nope\"]\n");
    settings.close();
    general.reload();
    QVERIFY(general.gamePageChips().isEmpty());
    QCOMPARE(general.problems().size(), 1);
    QVERIFY(general.problems().first().startsWith(QStringLiteral("settings.toml:2: unknown name \"nope\"")));
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
    QVERIFY(r.value.showTags);
    QVERIFY(!r.value.showDescription);
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

    // A list in the info panel's own order.
    QVERIFY(QFile::remove(marker));
    QVariantMap rows;
    rows.insert(QStringLiteral("info_rows"), QVariantList{QStringLiteral("status"), QStringLiteral("platform")});
    QVERIFY(GamePageMigration::run(settings, marker, rows, &error));
    QCOMPARE(parsed().value.infoRows, (QStringList{"status", "platform"}));
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
