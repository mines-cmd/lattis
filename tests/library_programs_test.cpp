#include "librarytestbase.h"

// The library tests on Proton and Linux games: shortcuts, adding programs, their install folders.
class LibraryProgramsTest : public LibraryTestBase
{
    Q_OBJECT

private slots:
    void protonGames();
    void protonEditing();
    void protonKeysElsewhere();
    void linuxGamesInList();
    void linuxGameInstalledStates();
    void linuxGameKeys();
    void protonGamesNeedSteam();
    void addExistingShortcuts();
    void steamAndShortcutsReportNewGames();
    void addShortcutsReadOnly();
    void shortcutRestore();
    void shortcutFollowsBothSides();
    void shortcutQueuedWhileSteamRuns();
    void shortcutsReadOnly();
    void protonInstallDir();
    void addProgram();
    void addProgramRefused();
    void addProgramWhileSteamRuns();
    void addProgramReadOnly();
    void linuxInstallDir();
    void linuxProgramKind();
    void addLinuxProgram();
    void addLinuxProgramRefused();
    void addLinuxProgramReadOnly();
    void protonArtToSteam();
};

void LibraryProgramsTest::protonGames()
{
    installFakeSteam();
    // Cave Story is the fake Steam's shortcut 2900000001, with play time and a last played.
    const QString folder = m_installs.home + QStringLiteral("/Games/Cave Story");
    writeFile(folder + QStringLiteral("/Doukutsu.exe"), QByteArray(1000, 'x'));
    writeFile(folder + QStringLiteral("/data/stage.pbm"), QByteArray(234, 'x'));
    writeFile(gamePath("proton-cave-story"), "title = \"Cave Story\"\nplatform = \"proton\"\n"
                                             "exe = \"~/Games/Cave Story/Doukutsu.exe\"\ninstall_dir = \"~/Games/Cave Story\"\n"
                                             "shortcut = 2900000001\nproton = \"GE-Proton10-4\"\n");
    // Uninstalled and kept: its program is still there, but it isn't installed.
    writeFile(gamePath("proton-kept"), "title = \"Kept\"\nplatform = \"proton\"\n"
                                       "exe = \"~/Games/Cave Story/Doukutsu.exe\"\nshortcut = 3000000000\n"
                                       "installed = false\n");
    auto library = openLibrary();
    QVERIFY2(library->problems().isEmpty(), qPrintable(library->problems().join(u'\n')));

    const Game *cave = library->game(QStringLiteral("proton-cave-story"));
    QVERIFY(cave);
    QVERIFY(cave->installed);
    QCOMPARE(cave->platformName, QStringLiteral("Proton"));
    QCOMPARE(cave->exe, folder + QStringLiteral("/Doukutsu.exe"));
    QCOMPARE(cave->installDir, folder);
    QCOMPARE(cave->sizeBytes, 1234);
    QCOMPARE(cave->shortcut, 2900000001u);
    QCOMPARE(cave->proton, QStringLiteral("GE-Proton10-4"));
    // Steam's, from the signed app ID's block and the shortcut.
    QCOMPARE(cave->playMinutes, 575);
    QCOMPARE(cave->lastPlayed, QDateTime::fromSecsSinceEpoch(1788000000));
    const QVariantMap row = library->gameAt(library->rowOfGame(QStringLiteral("proton-cave-story")));
    QVERIFY(row.value("installed").toBool());
    QVERIFY(!row.value("gameNotFound").toBool());

    // "Installed only" hides the one not found (the demo's Diablo IV) and the one kept.
    QCOMPARE(library->rowOfGame(QStringLiteral("proton-diablo-iv")), -1);
    QCOMPARE(library->rowOfGame(QStringLiteral("proton-kept")), -1);
    library->setInstalledOnly(false);
    const QVariantMap diablo = library->gameAt(library->rowOfGame(QStringLiteral("proton-diablo-iv")));
    QVERIFY(!diablo.value("installed").toBool());
    QVERIFY(diablo.value("gameNotFound").toBool());
    const QVariantMap kept = library->gameAt(library->rowOfGame(QStringLiteral("proton-kept")));
    QVERIFY(!kept.value("installed").toBool());
    QVERIFY(!kept.value("gameNotFound").toBool());
    QVERIFY(library->game(QStringLiteral("proton-kept"))->uninstalled);

    // The size is added up once: from the cache on the next start, and not again while the
    // folder is the same.
    library.reset();
    writeFile(folder + QStringLiteral("/data/save.dat"), QByteArray(100, 'x'));
    library = std::make_unique<Library>(m_data, m_cache);
    QCOMPARE(library->game(QStringLiteral("proton-cave-story"))->sizeBytes, 1234);
    library.reset();
    library = openLibrary();
    QCOMPARE(library->game(QStringLiteral("proton-cave-story"))->sizeBytes, 1234);
    // Another folder is added up.
    QCOMPARE(library->setGameValue(QStringLiteral("proton-cave-story"), QStringLiteral("install_dir"),
                                   QStringLiteral("~/Games/Cave Story/data")),
             QString());
    library->store().refresh();
    library->store().waitForIdle();
    QCoreApplication::processEvents();
    QCOMPARE(library->game(QStringLiteral("proton-cave-story"))->sizeBytes, 334);

    // The program gone: "Game not found".
    QVERIFY(QFile::remove(folder + QStringLiteral("/Doukutsu.exe")));
    library->store().refresh();
    library->store().waitForIdle();
    QCoreApplication::processEvents();
    QVERIFY(!library->game(QStringLiteral("proton-cave-story"))->installed);
    QVERIFY(!library->game(QStringLiteral("proton-cave-story"))->uninstalled);
}

void LibraryProgramsTest::protonEditing()
{
    installFakeSteam();
    const QString folder = m_installs.home + QStringLiteral("/Games/Cave Story");
    writeFile(folder + QStringLiteral("/Doukutsu.exe"), "x");
    writeFile(folder + QStringLiteral("/v2/Doukutsu.exe"), "x");
    writeFile(gamePath("proton-cave-story"), "title = \"Cave Story\"\nplatform = \"proton\"\n"
                                             "exe = \"~/Games/Cave Story/Doukutsu.exe\"\ninstall_dir = \"~/Games/Cave Story\"\n"
                                             "shortcut = 2900000001\nproton = \"GE-Proton10-4\"\n");
    writeFile(gamePath("proton-kept"), "title = \"Kept\"\nplatform = \"proton\"\n"
                                       "exe = \"~/Games/Cave Story/Doukutsu.exe\"\nshortcut = 3000000000\n"
                                       "installed = false\n");
    auto library = openLibrary();

    // The keys as written, the tools and presets for the Launch card, Steam's play time.
    const QVariantMap cave = library->gameForEditing(QStringLiteral("proton-cave-story"));
    QCOMPARE(cave.value("exe").toString(), QStringLiteral("~/Games/Cave Story/Doukutsu.exe"));
    QCOMPARE(cave.value("exePath").toString(), folder + QStringLiteral("/Doukutsu.exe"));
    QCOMPARE(cave.value("installDir").toString(), QStringLiteral("~/Games/Cave Story"));
    QCOMPARE(cave.value("installPath").toString(), folder);
    QCOMPARE(cave.value("startDir").toString(), QString());
    QCOMPARE(cave.value("proton").toString(), QStringLiteral("GE-Proton10-4"));
    QVERIFY(!cave.value("protonTools").toList().isEmpty());
    QCOMPARE(cave.value("playMinutes").toInt(), 575);
    QVERIFY(!cave.value("gameNotFound").toBool());
    QVERIFY(!cave.value("uninstalled").toBool());
    QVERIFY(library->gameForEditing(QStringLiteral("proton-diablo-iv")).value("gameNotFound").toBool());
    const QVariantMap kept = library->gameForEditing(QStringLiteral("proton-kept"));
    QVERIFY(kept.value("uninstalled").toBool());
    QVERIFY(!kept.value("gameNotFound").toBool());

    // What the editor saves: the program, a start folder, then back to the program's own.
    QCOMPARE(library->saveGame(QStringLiteral("proton-cave-story"),
                               {{QStringLiteral("exe"), QStringLiteral("~/Games/Cave Story/v2/Doukutsu.exe")},
                                {QStringLiteral("start_dir"), QStringLiteral("~/Games/Cave Story")}}),
             QString());
    QCoreApplication::processEvents();
    QByteArray file = readFile(gamePath("proton-cave-story"));
    QVERIFY(file.contains("exe = \"~/Games/Cave Story/v2/Doukutsu.exe\"\n"));
    QVERIFY(file.contains("start_dir = \"~/Games/Cave Story\"\n"));
    QCOMPARE(library->gameForEditing(QStringLiteral("proton-cave-story")).value("startDir").toString(),
             QStringLiteral("~/Games/Cave Story"));
    QCOMPARE(library->saveGame(QStringLiteral("proton-cave-story"), {{QStringLiteral("start_dir"), QVariant()}}), QString());
    QVERIFY(!readFile(gamePath("proton-cave-story")).contains("start_dir"));

    // Located again from the editor: installed = false goes.
    QCOMPARE(library->saveGame(QStringLiteral("proton-kept"),
                               {{QStringLiteral("exe"), QStringLiteral("~/Games/Cave Story/v2/Doukutsu.exe")},
                                {QStringLiteral("installed"), QVariant()}}),
             QString());
    file = readFile(gamePath("proton-kept"));
    QVERIFY(!file.contains("installed"));
    QVERIFY(!library->game(QStringLiteral("proton-kept"))->uninstalled);
}

void LibraryProgramsTest::protonKeysElsewhere()
{
    installFakeSteam();
    // Proton's keys on another platform, and play time on a Proton game.
    writeFile(gamePath("switch-pikmin-4"), readFile(gamePath("switch-pikmin-4"))
                                               + "exe = \"~/x.exe\"\nshortcut = 3000000000\ninstalled = false\n");
    writeFile(gamePath("proton-played"), "title = \"Played\"\nplatform = \"proton\"\nexe = \"~/x.exe\"\n"
                                         "shortcut = 3000000000\nplay_minutes = 60\n");
    writeFile(gamePath("proton-bare"), "title = \"Bare\"\nplatform = \"proton\"\n");
    auto library = openLibrary();
    QCOMPARE(library->problems(),
             (QStringList{
                 "library/games/proton-bare.toml: no exe; add exe = \"<the game's .exe>\"",
                 "library/games/proton-bare.toml: no shortcut; add it from Steam with Settings → Steam → "
                 "Add existing shortcuts",
                 "library/games/proton-played.toml: play_minutes and last_played are Steam's for a Proton game; ignored",
                 "library/games/switch-pikmin-4.toml: exe, installed are for Proton and Linux games; ignored",
                 "library/games/switch-pikmin-4.toml: shortcut is for Proton games; ignored",
             }));
    // Ignored: Pikmin is installed like before.
    QVERIFY(library->game(QStringLiteral("switch-pikmin-4"))->installed);
    QCOMPARE(library->game(QStringLiteral("proton-played"))->playMinutes, 0);
}

void LibraryProgramsTest::linuxGamesInList()
{
    auto library = openLibrary();
    library->setInstalledOnly(false);
    QVERIFY2(library->problems().isEmpty(), qPrintable(library->problems().join(u'\n')));

    // SuperTuxKart: its program is a script in the data dir, so it's installed, with the
    // install folder added up, and its play time from its file.
    const Game *kart = library->game(QStringLiteral("linux-supertuxkart"));
    QVERIFY(kart);
    const QString folder = m_data + QStringLiteral("/linux-games/SuperTuxKart");
    QCOMPARE(kart->platformName, QStringLiteral("Linux"));
    QVERIFY(kart->installed);
    QVERIFY(!kart->uninstalled);
    QCOMPARE(kart->exe, folder + QStringLiteral("/run_game.sh"));
    QCOMPARE(kart->installDir, folder);
    QCOMPARE(kart->sizeBytes, QFileInfo(folder + QStringLiteral("/run_game.sh")).size());
    QCOMPARE(kart->playMinutes, 640);
    QCOMPARE(kart->lastPlayed, QDateTime::fromString(QStringLiteral("2026-09-20T21:14:00-03:00"), Qt::ISODate));
    QCOMPARE(kart->steamPlayMinutes, -1);

    // Factorio: its program isn't there ("Game not found"), and it isn't uninstalled.
    const Game *factorio = library->game(QStringLiteral("linux-factorio"));
    QVERIFY(factorio);
    QVERIFY(!factorio->installed);
    QVERIFY(!factorio->uninstalled);
    QCOMPARE(factorio->exe, m_installs.home + QStringLiteral("/Games/factorio/bin/x64/factorio"));
    QCOMPARE(factorio->installDir, m_installs.home + QStringLiteral("/Games/factorio"));
    QCOMPARE(factorio->sizeBytes, 0);
    QCOMPARE(factorio->playMinutes, 2310);

    // Installed later: the next scan finds it.
    const QString exe = m_installs.home + QStringLiteral("/Games/factorio/bin/x64/factorio");
    QVERIFY(QDir().mkpath(QFileInfo(exe).absolutePath()));
    writeFile(exe, "#!/bin/sh\n");
    QVERIFY(QFile::setPermissions(exe, QFile::permissions(exe) | QFile::ExeUser));
    library->store().refresh();
    library->store().waitForIdle();
    QCoreApplication::processEvents();
    QVERIFY(library->game(QStringLiteral("linux-factorio"))->installed);
    QCOMPARE(library->game(QStringLiteral("linux-factorio"))->sizeBytes, 10);
}

void LibraryProgramsTest::linuxGameInstalledStates()
{
    // Not executable: not installed until it is.
    const QString exe = m_data + QStringLiteral("/linux-games/SuperTuxKart/run_game.sh");
    QVERIFY(QFile::setPermissions(exe, QFile::ReadOwner | QFile::WriteOwner));
    // Kept after Uninstall: not installed, whatever its program.
    writeFile(gamePath("linux-kept"), "title = \"Kept\"\nplatform = \"linux\"\n"
                                      "exe = \"linux-games/SuperTuxKart/run_game.sh\"\ninstalled = false\n");
    writeFile(gamePath("linux-bare"), "title = \"Bare\"\nplatform = \"linux\"\n");
    auto library = openLibrary();
    QVERIFY(!library->game(QStringLiteral("linux-supertuxkart"))->installed);
    QVERIFY(!library->game(QStringLiteral("linux-supertuxkart"))->uninstalled);
    QVERIFY(!library->game(QStringLiteral("linux-kept"))->installed);
    QVERIFY(library->game(QStringLiteral("linux-kept"))->uninstalled);
    QVERIFY(!library->game(QStringLiteral("linux-bare"))->installed);
    QCOMPARE(library->problems(),
             QStringList{"library/games/linux-bare.toml: no exe; add exe = \"<the game's program>\""});

    QVERIFY(QFile::setPermissions(exe, QFile::permissions(exe) | QFile::ExeOwner | QFile::ExeUser));
    library->store().refresh();
    library->store().waitForIdle();
    QCoreApplication::processEvents();
    QVERIFY(library->game(QStringLiteral("linux-supertuxkart"))->installed);
    QVERIFY(!library->game(QStringLiteral("linux-kept"))->installed); // still kept
}

void LibraryProgramsTest::linuxGameKeys()
{
    // A Linux game takes launch_options; Proton's own keys are ignored, and so is
    // launch_options on a ROM game.
    writeFile(gamePath("linux-supertuxkart"),
              readFile(gamePath("linux-supertuxkart"))
                  + "launch_options = \"mangohud %command% --no-start-screen\"\nstart_dir = \"linux-games\"\n"
                    "proton = \"proton_experimental\"\nshortcut = 3000000000\nskip_steam_art = [\"hero\"]\n");
    writeFile(gamePath("switch-pikmin-4"),
              readFile(gamePath("switch-pikmin-4")) + "launch_options = \"%command%\"\n");
    auto library = openLibrary();
    QCOMPARE(library->problems(),
             (QStringList{
                 "library/games/linux-supertuxkart.toml: proton, skip_steam_art are for Steam and Proton games; ignored",
                 "library/games/linux-supertuxkart.toml: shortcut is for Proton games; ignored",
                 "library/games/switch-pikmin-4.toml: launch_options is for Steam, Proton and Linux games; ignored",
             }));
    const Game *kart = library->game(QStringLiteral("linux-supertuxkart"));
    QCOMPARE(kart->launchOptions, QStringLiteral("mangohud %command% --no-start-screen"));
    QCOMPARE(kart->proton, QString());
    QCOMPARE(kart->shortcut, 0u);
    QVERIFY(kart->installed);
    QCOMPARE(library->game(QStringLiteral("switch-pikmin-4"))->launchOptions, QString());
}

void LibraryProgramsTest::protonGamesNeedSteam()
{
    // Steam off: a problem, once for all of them.
    writeFile(m_data + QStringLiteral("/settings.toml"), "[steam]\nenabled = false\n");
    auto library = openLibrary();
    QCOMPARE(library->problems(),
             QStringList{"library/games/proton-diablo-iv.toml (and 1 more game(s)): Proton games need Steam, and "
                         "the Steam import is off in settings.toml"});
    library.reset();
    // Not found.
    writeFile(m_data + QStringLiteral("/settings.toml"), "");
    library = openLibrary();
    QCOMPARE(library->problems(),
             QStringList{"library/games/proton-diablo-iv.toml (and 1 more game(s)): Proton games need Steam, which "
                         "isn't found"});
}

void LibraryProgramsTest::addExistingShortcuts()
{
    const QString root = installFakeSteam();
    // The demo's Proton games aren't this Steam's shortcuts: they'd be added to it.
    QFile::remove(gamePath("proton-diablo-iv"));
    QFile::remove(gamePath("proton-final-fantasy-xiv-online"));
    auto library = openLibrary();
    QCOMPARE(library->steam().value("shortcuts").toInt(), 3);
    QCOMPARE(library->steam().value("shortcutsInLibrary").toInt(), 0);

    // Every shortcut, checked when Steam runs it with a compatibility tool.
    const QVariantList shortcuts = library->steamShortcuts();
    QCOMPARE(shortcuts.size(), 3);
    const QVariantMap cave = shortcuts.at(0).toMap();
    QCOMPARE(cave.value("appId").toLongLong(), 2900000001);
    QCOMPARE(cave.value("name").toString(), QStringLiteral("Cave Story"));
    QCOMPARE(cave.value("target").toString(), QStringLiteral("/home/user/Games/Cave Story/Doukutsu.exe"));
    QCOMPARE(cave.value("tool").toString(), QStringLiteral("GE-Proton10-4"));
    QVERIFY(cave.value("checked").toBool());
    QVERIFY(cave.value("art").toString().endsWith(QStringLiteral("/grid/2900000001_hero.png")));
    const QVariantMap smash = shortcuts.at(2).toMap();
    QVERIFY(!smash.value("checked").toBool());
    QVERIFY(smash.value("tool").toString().isEmpty());
    QCOMPARE(smash.value("target").toString(), QStringLiteral("/home/user/AppImages/ryujinx.appimage --fullscreen "
                                                              "\"/home/user/Games/switch/Smash.nsp\""));
    QVERIFY(!shortcuts.at(1).toMap().value("checked").toBool());

    // Added: a game file each, keeping the shortcut's app ID and values.
    QCOMPARE(library->addSteamShortcuts({qint64(2900000001), qint64(3300000003)}), QString());
    const QByteArray today = QDate::currentDate().toString(Qt::ISODate).toUtf8();
    QCOMPARE(readFile(gamePath("proton-cave-story")),
             "title = \"Cave Story\"\nplatform = \"proton\"\nadded = " + today
                 + "\nlaunch_options = \" mangohud %command%\"\nproton = \"GE-Proton10-4\"\n"
                   "exe = \"/home/user/Games/Cave Story/Doukutsu.exe\"\ninstall_dir = \"/home/user/Games/Cave Story\"\n"
                   "shortcut = 2900000001\n");
    // The arguments inside Steam's exe stay in Steam; the game file has the program.
    QCOMPARE(readFile(gamePath("proton-super-smash-bros-ultimate")),
             "title = \"Super Smash Bros. Ultimate\"\nplatform = \"proton\"\nadded = " + today
                 + "\nexe = \"/home/user/AppImages/ryujinx.appimage\"\ninstall_dir = \"/home/user/AppImages\"\n"
                   "shortcut = 3300000003\n");
    QVERIFY(readFile(m_data + QStringLiteral("/library/platforms.toml")).contains("id = \"proton\""));

    // Recorded as they are in Steam, so nothing waits for Steam.
    const QString record = SteamSync::recordPath(m_state, AppPaths::dataDirKey(m_data));
    QStringList warnings;
    const SteamSync::Record r = SteamSync::readRecord(record, libraryId(), &warnings);
    QVERIFY(warnings.isEmpty());
    QCOMPARE(r.shortcuts.value(2900000001),
             (SteamSync::ShortcutRecord{QStringLiteral("Cave Story"), QStringLiteral("/home/user/Games/Cave Story/Doukutsu.exe"),
                                        QStringLiteral("/home/user/Games/Cave Story/"),
                                        QStringLiteral(" mangohud %command%"), QStringLiteral("GE-Proton10-4")}));
    QVERIFY(r.shortcuts.contains(3300000003));
    QVERIFY(!r.shortcuts.contains(3100000002));
    library->store().waitForIdle();
    QCoreApplication::processEvents();
    QVERIFY(library->store().snapshot().steamQueued.isEmpty());
    QVERIFY(library->problems().isEmpty());

    // The next scan copied Cave Story's grid art; its play time is Steam's.
    QVERIFY(QFileInfo::exists(m_data + QStringLiteral("/art/proton-cave-story/hero.png")));
    QCOMPARE(library->game(QStringLiteral("proton-cave-story"))->playMinutes, 575);
    QCOMPARE(library->steam().value("shortcutsInLibrary").toInt(), 2);
    // Only the one left to add is offered; adding one twice does nothing.
    QCOMPARE(library->steamShortcuts().size(), 1);
    QCOMPARE(library->addSteamShortcuts({qint64(2900000001)}), QString());
    QVERIFY(!QFileInfo::exists(gamePath("proton-cave-story-2")));

    // Steam's own files are only read.
    QCOMPARE(readFile(root + QStringLiteral("/userdata/39734273/config/shortcuts.vdf")),
             readFile(QStringLiteral(STEAM_DATA_DIR "/userdata/39734273/config/shortcuts.vdf")));
    QCOMPARE(readFile(root + QStringLiteral("/config/config.vdf")),
             readFile(QStringLiteral(STEAM_DATA_DIR "/config/config.vdf")));
}

void LibraryProgramsTest::steamAndShortcutsReportNewGames()
{
    const QString root = installFakeSteam();
    QFile::remove(gamePath("proton-diablo-iv"));
    QFile::remove(gamePath("proton-final-fantasy-xiv-online"));
    const QString manifest = root + QStringLiteral("/steamapps/appmanifest_2231450.acf");
    const QByteArray pizzaTower = readFile(manifest);
    QVERIFY(QFile::remove(manifest));
    auto library = openLibrary();
    QSignalSpy added(&library->store(), &LibraryStore::gamesAdded);
    const auto scan = [&] {
        library->store().waitForIdle();
        QCoreApplication::processEvents();
    };

    // A game Steam installs.
    writeFile(manifest, pizzaTower);
    library->store().refresh();
    scan();
    QCOMPARE(added.size(), 1);
    QCOMPARE(added.takeFirst().first().toStringList(), QStringList{"steam-2231450"});

    // Shortcuts brought in: reported by the scan that follows, which copies their grid art.
    QCOMPARE(library->addSteamShortcuts({qint64(2900000001)}), QString());
    QCOMPARE(added.size(), 0);
    scan(); // outdated by the batch, so scanned again
    QTRY_COMPARE_WITH_TIMEOUT(added.size(), 1, 5000);
    QCOMPARE(added.takeFirst().first().toStringList(), QStringList{"proton-cave-story"});
    QVERIFY(library->game(QStringLiteral("proton-cave-story"))->heroUrl.isValid());

    // "+" with an .exe isn't: the editor has its lookup.
    const QString exe = m_installs.home + QStringLiteral("/Games/Hollow Knight/hollow_knight.exe");
    writeFile(exe, "MZ");
    const QVariantMap knight = library->addGame(exe, QStringLiteral("proton"), QStringLiteral("Hollow Knight"), {}, {});
    QCOMPARE(knight.value("error").toString(), QString());
    scan();
    QTRY_VERIFY_WITH_TIMEOUT(!library->store().scanning(), 5000);
    QCOMPARE(added.size(), 0);
}

void LibraryProgramsTest::addShortcutsReadOnly()
{
    installFakeSteam();
    writeFile(m_data + QStringLiteral("/library/library.toml"), "version = 99\n");
    auto library = openLibrary();
    QVERIFY(library->readOnly());
    QCOMPARE(library->steamShortcuts().size(), 3);
    QVERIFY(!library->addSteamShortcuts({qint64(2900000001)}).isEmpty());
    QVERIFY(!QFileInfo::exists(gamePath("proton-cave-story")));
    QVERIFY(!QFileInfo::exists(SteamSync::recordPath(m_state, AppPaths::dataDirKey(m_data))));
    QVERIFY(!QFileInfo::exists(m_data + QStringLiteral("/art/proton-cave-story")));
}

void LibraryProgramsTest::shortcutRestore()
{
    // A data dir restored with a new state dir and Steam closed: the demo's Proton games
    // aren't this Steam's shortcuts, so the first scan adds them, keeping their app IDs.
    const QString root = installFakeSteam();
    const QString vdf = root + kShortcutsVdf;
    const QByteArray before = readFile(vdf);
    auto library = openLibrary();
    QTRY_COMPARE_WITH_TIMEOUT(shortcutsIn(vdf).size(), 5, 5000);
    QTRY_VERIFY_WITH_TIMEOUT(!pending(*library, QStringLiteral("proton-diablo-iv")), 5000);
    QVERIFY(!pending(*library, QStringLiteral("proton-final-fantasy-xiv-online")));

    // The three that were there keep their bytes.
    const BinaryVdf::Document old = BinaryVdf::parse(before);
    const BinaryVdf::Document now = BinaryVdf::parse(readFile(vdf));
    for (qsizetype i = 0; i < 3; ++i)
        QCOMPARE(now.root.child("shortcuts")->children.at(i), old.root.child("shortcuts")->children.at(i));

    // The game files' values, paths absolute, StartDir as Steam writes it.
    const SteamShortcut ffxiv = shortcutIn(vdf, 2458135266u);
    QCOMPARE(ffxiv.name, QStringLiteral("Final Fantasy XIV Online"));
    QCOMPARE(ffxiv.exe, m_data + QStringLiteral("/pc-games/Final Fantasy XIV Online/boot/ffxivboot.exe"));
    QCOMPARE(ffxiv.startDir, m_data + QStringLiteral("/pc-games/Final Fantasy XIV Online/boot/"));
    const SteamShortcut diablo = shortcutIn(vdf, 3110441846u);
    QCOMPARE(diablo.exe, m_installs.home + QStringLiteral("/Games/Diablo IV/Diablo IV Launcher.exe"));
    QCOMPARE(diablo.launchOptions, QStringLiteral("mangohud %command%"));
    // Their Proton versions follow them into config.vdf.
    const Vdf::Document config = Vdf::parse(readFile(root + QStringLiteral("/config/config.vdf")));
    const QStringList mapping = {QStringLiteral("InstallConfigStore"), QStringLiteral("Software"), QStringLiteral("Valve"),
                                 QStringLiteral("Steam"), QStringLiteral("CompatToolMapping")};
    for (const QString &id : {QStringLiteral("2458135266"), QStringLiteral("3110441846")})
        QCOMPARE(config.root.find(mapping + QStringList{id, QStringLiteral("name")})->value, QStringLiteral("proton_experimental"));

    QStringList warnings;
    const SteamSync::Record record =
        SteamSync::readRecord(SteamSync::recordPath(m_state, AppPaths::dataDirKey(m_data)), libraryId(), &warnings);
    QCOMPARE(record.shortcuts.value(2458135266).proton, QStringLiteral("proton_experimental"));
    QCOMPARE(record.shortcuts.value(3110441846).launchOptions, QStringLiteral("mangohud %command%"));
    QCOMPARE(readFile(m_state + QStringLiteral("/steam-backup") + kShortcutsVdf), before);
}

void LibraryProgramsTest::shortcutFollowsBothSides()
{
    const QString root = installFakeSteam();
    QFile::remove(gamePath("proton-diablo-iv"));
    QFile::remove(gamePath("proton-final-fantasy-xiv-online"));
    const QString vdf = root + kShortcutsVdf;
    auto library = openLibrary();
    QCOMPARE(library->addSteamShortcuts({qint64(2900000001)}), QString());
    library->store().waitForIdle();
    QCoreApplication::processEvents();
    const QString cave = QStringLiteral("proton-cave-story");
    QVERIFY(!pending(*library, cave));

    // Changed here with Steam closed: written into its shortcut and config.vdf right away.
    QCOMPARE(library->setGameValue(cave, QStringLiteral("title"), QStringLiteral("Doukutsu Monogatari")), QString());
    QTRY_COMPARE_WITH_TIMEOUT(shortcutIn(vdf, 2900000001u).name, QStringLiteral("Doukutsu Monogatari"), 5000);
    QCOMPARE(library->setGameValue(cave, QStringLiteral("proton"), QStringLiteral("proton_9")), QString());
    QTRY_VERIFY_WITH_TIMEOUT(readFile(root + QStringLiteral("/config/config.vdf"))
                                 .contains("\"2900000001\"\n\t\t\t\t\t{\n\t\t\t\t\t\t\"name\"\t\t\"proton_9\""),
                             5000);
    QTRY_VERIFY_WITH_TIMEOUT(!pending(*library, cave), 5000);
    // The other shortcuts are as they were.
    QCOMPARE(shortcutIn(vdf, 3100000002u).name, QStringLiteral("Monster Hunter 4 Ultimate"));

    // Changed in Steam: copied into the game file.
    editShortcuts(vdf, [](BinaryVdf::Node *root) {
        SteamShortcut s = SteamShortcuts::read(*root).at(0);
        s.launchOptions = QStringLiteral("gamemoderun %command%");
        QVERIFY(SteamShortcuts::update(root, s));
    });
    QTRY_VERIFY_WITH_TIMEOUT(readFile(gamePath(cave)).contains("launch_options = \"gamemoderun %command%\"\n"), 5000);
    QVERIFY(!pending(*library, cave));

    // Deleted in Steam: not installed, and not added back.
    editShortcuts(vdf, [](BinaryVdf::Node *root) { QVERIFY(SteamShortcuts::remove(root, 2900000001u)); });
    QTRY_VERIFY_WITH_TIMEOUT(readFile(gamePath(cave)).contains("installed = false\n"), 5000);
    QTest::qWait(300);
    QCOMPARE(shortcutsIn(vdf).size(), 2);
    QVERIFY(!pending(*library, cave));
    QVERIFY(!library->game(cave)->installed);

    // Located again: its shortcut comes back with the same app ID.
    QCOMPARE(library->locateProgram(cave, m_data + QStringLiteral("/pc-games/Final Fantasy XIV Online/boot/ffxivboot.exe")),
             QString());
    QTRY_COMPARE_WITH_TIMEOUT(shortcutsIn(vdf).size(), 3, 5000);
    QCOMPARE(shortcutIn(vdf, 2900000001u).name, QStringLiteral("Doukutsu Monogatari"));
}

void LibraryProgramsTest::shortcutQueuedWhileSteamRuns()
{
    const QString root = installFakeSteam();
    const QString vdf = root + kShortcutsVdf;
    const QByteArray before = readFile(vdf);
    const auto steam = startFakeSteam(root);
    auto library = openLibrary();
    const QString ffxiv = QStringLiteral("proton-final-fantasy-xiv-online");

    // New to this Steam while it runs: queued to be added, shortcuts.vdf untouched.
    QTRY_VERIFY_WITH_TIMEOUT(pending(*library, ffxiv), 5000);
    QVERIFY(library->gameAt(library->rowOfGame(ffxiv)).value("shortcutQueued").toBool());
    QVariantMap entry;
    for (const QVariant &g : library->steam().value("queue").toList()) {
        if (g.toMap().value("id") == ffxiv)
            entry = g.toMap();
    }
    const QVariantMap change = entry.value("changes").toList().value(0).toMap();
    QCOMPARE(change.value("name").toString(), QStringLiteral("Shortcut"));
    QCOMPARE(change.value("to").toString(), QStringLiteral("Added"));
    QCOMPARE(readFile(vdf), before);

    // Steam closes: added.
    steam->kill();
    steam->waitForFinished();
    QTRY_VERIFY_WITH_TIMEOUT(!pending(*library, ffxiv), 5000);
    QCOMPARE(shortcutIn(vdf, 2458135266u).name, QStringLiteral("Final Fantasy XIV Online"));
    QVERIFY(!library->gameAt(library->rowOfGame(ffxiv)).value("shortcutQueued").toBool());
}

void LibraryProgramsTest::shortcutsReadOnly()
{
    const QString root = installFakeSteam();
    const QString vdf = root + kShortcutsVdf;
    const QByteArray before = readFile(vdf);
    const QString grid = root + QStringLiteral("/userdata/39734273/config/grid");
    const QStringList gridBefore = QDir(grid).entryList(QDir::Files | QDir::Hidden);
    // A demo Proton game with art Steam has none of.
    QImage hero(64, 32, QImage::Format_RGB32);
    hero.fill(Qt::red);
    QDir().mkpath(m_data + QStringLiteral("/art/proton-diablo-iv"));
    QVERIFY(hero.save(m_data + QStringLiteral("/art/proton-diablo-iv/hero.png")));
    writeFile(m_data + QStringLiteral("/library/library.toml"), "version = 99\n");
    auto library = openLibrary();
    QVERIFY(library->readOnly());
    library->store().waitForIdle();
    QCoreApplication::processEvents();
    QTest::qWait(300);
    QCOMPARE(readFile(vdf), before);
    // No grid files either.
    QCOMPARE(QDir(grid).entryList(QDir::Files | QDir::Hidden), gridBefore);
}

void LibraryProgramsTest::protonInstallDir()
{
    const QString home = QStringLiteral("/home/user");
    QCOMPARE(::protonInstallDir(QStringLiteral("/home/user/Games/Mortal Shell II/MortalShell2.exe"), home),
             QStringLiteral("/home/user/Games/Mortal Shell II"));
    QCOMPARE(::protonInstallDir(QStringLiteral("/home/user/Games/Hades/Binaries/Win64/Hades.exe"), home),
             QStringLiteral("/home/user/Games/Hades"));
    QCOMPARE(::protonInstallDir(QStringLiteral("/mnt/g/Celeste/game/bin/x64/start.exe"), home),
             QStringLiteral("/mnt/g/Celeste"));
    // Never up to the home folder or /.
    QCOMPARE(::protonInstallDir(QStringLiteral("/home/user/bin/tool.exe"), home), QStringLiteral("/home/user/bin"));
    QCOMPARE(::protonInstallDir(QStringLiteral("/bin/tool.exe"), home), QStringLiteral("/bin"));
}

void LibraryProgramsTest::addProgram()
{
    const QString root = installFakeSteam();
    const QString vdf = root + kShortcutsVdf;
    writeFile(m_data + QStringLiteral("/settings.toml"), "[steam]\ndefault_presets = [\"mangohud\"]\n");
    const QString exe = m_installs.home + QStringLiteral("/Games/Hollow Knight/Binaries/Win64/hollow_knight.exe");
    writeFile(exe, "MZ");
    auto library = openLibrary();
    QTRY_VERIFY_WITH_TIMEOUT(library->store().snapshot().steamQueued.isEmpty(), 5000); // the demo's shortcuts added
    QVERIFY(library->romNameFilters().contains(QStringLiteral("Windows programs (*.exe *.EXE)")));
    QVERIFY(library->romNameFilters().first().contains(QStringLiteral("*.exe")));

    // A Proton game: titled after the folder above the generic ones, with the new games' defaults.
    const QVariantMap pick = library->inspectRom(QUrl::fromLocalFile(exe).toString());
    QVERIFY(pick.value("proton").toBool());
    QCOMPARE(pick.value("platforms").toStringList(), QStringList{"proton"});
    QCOMPARE(pick.value("title").toString(), QStringLiteral("Hollow Knight"));
    QCOMPARE(pick.value("installDir").toString(), m_installs.home + QStringLiteral("/Games/Hollow Knight"));
    QCOMPARE(pick.value("problem").toString(), QString());
    QCOMPARE(pick.value("existing").toString(), QString());
    QCOMPARE(pick.value("shortcut").toLongLong(), 0);
    QCOMPARE(pick.value("protonVersion").toString(), QStringLiteral("proton_experimental")); // Steam's for other titles
    QCOMPARE(pick.value("launchOptions").toString(), QStringLiteral("mangohud %command%"));
    QVERIFY(!pick.value("protonTools").toList().isEmpty());

    // Added: a game file with a new shortcut, which goes into Steam right away (it's closed).
    const QVariantMap added = library->addGame(exe, QStringLiteral("proton"), QStringLiteral("Hollow Knight"),
                                               {{"launch_options", "mangohud %command%"}, {"proton", "GE-Proton10-4"}}, {});
    QCOMPARE(added.value("error").toString(), QString());
    const QString id = added.value("id").toString();
    QCOMPARE(id, QStringLiteral("proton-hollow-knight"));
    const GameFile file = library->store().snapshot().content.games.value(id);
    QVERIFY(file.shortcut > 0);
    QCOMPARE(quint32(file.shortcut), SteamShortcuts::newAppId(SteamShortcuts::exeField(exe), QStringLiteral("Hollow Knight"), {}));
    const QByteArray today = QDate::currentDate().toString(Qt::ISODate).toUtf8();
    QCOMPARE(readFile(gamePath(id)),
             "title = \"Hollow Knight\"\nplatform = \"proton\"\nadded = " + today
                 + "\nlaunch_options = \"mangohud %command%\"\nproton = \"GE-Proton10-4\"\n"
                   "exe = \"~/Games/Hollow Knight/Binaries/Win64/hollow_knight.exe\"\n"
                   "install_dir = \"~/Games/Hollow Knight\"\nshortcut = "
                 + QByteArray::number(file.shortcut) + "\n");
    QTRY_VERIFY_WITH_TIMEOUT(!pending(*library, id), 5000);
    const SteamShortcut s = shortcutIn(vdf, quint32(file.shortcut));
    QCOMPARE(s.name, QStringLiteral("Hollow Knight"));
    QCOMPARE(s.exe, exe);
    QCOMPARE(s.launchOptions, QStringLiteral("mangohud %command%"));
    QVERIFY(readFile(root + QStringLiteral("/config/config.vdf")).contains(QByteArray::number(file.shortcut)));
    QVERIFY(library->game(id)->installed);

    // Picked again: it's that game.
    QCOMPARE(library->inspectRom(exe).value("existing").toString(), id);
    const QVariantMap again = library->addGame(exe, QStringLiteral("proton"), QStringLiteral("Hollow Knight"), {}, {});
    QVERIFY(again.value("existing").toBool());
    QCOMPARE(again.value("id").toString(), id);
    QVERIFY(!QFileInfo::exists(gamePath("proton-hollow-knight-2")));

    // An install folder chosen in the dialog, written like the app writes paths.
    const QString other = m_installs.home + QStringLiteral("/Games/Celeste/game/bin/x64/Celeste.exe");
    writeFile(other, "MZ");
    QCOMPARE(library->inspectRom(other).value("title").toString(), QStringLiteral("Celeste"));
    const QVariantMap celeste = library->addGame(other, QStringLiteral("proton"), QStringLiteral("Celeste"),
                                                 {{"install_dir", m_installs.home + QStringLiteral("/Games/Celeste/game")}}, {});
    QCOMPARE(celeste.value("error").toString(), QString());
    QVERIFY(readFile(gamePath("proton-celeste")).contains("install_dir = \"~/Games/Celeste/game\"\n"));
    // Nothing chosen for Proton: runs natively, no compatibility tool.
    QVERIFY(!readFile(gamePath("proton-celeste")).contains("proton = "));
}

void LibraryProgramsTest::addProgramRefused()
{
    installFakeSteam();
    auto library = openLibrary();
    // A shortcut Steam has that isn't in the library: the picker, not a second shortcut.
    const QString cave = QStringLiteral("/home/user/Games/Cave Story/Doukutsu.exe");
    QCOMPARE(library->inspectRom(cave).value("shortcut").toLongLong(), 2900000001);
    const QString copy = m_installs.home + QStringLiteral("/Games/Cave Story/Doukutsu.exe");
    writeFile(copy, "MZ");
    QCOMPARE(library->inspectRom(copy).value("shortcut").toLongLong(), 0);
    // A demo game's program: that game.
    const QString ffxiv = m_data + QStringLiteral("/pc-games/Final Fantasy XIV Online/boot/ffxivboot.exe");
    QCOMPARE(library->inspectRom(ffxiv).value("existing").toString(), QStringLiteral("proton-final-fantasy-xiv-online"));
    // No title, or not a file.
    QVERIFY(!library->addGame(copy, QStringLiteral("proton"), QStringLiteral(" "), {}, {}).value("error").toString().isEmpty());
    QVERIFY(!library->addGame(copy + "x", QStringLiteral("proton"), QStringLiteral("X"), {}, {}).value("error").toString().isEmpty());
    library.reset();

    // Without Steam: a problem, and no .exe in the picker's filter.
    writeFile(m_data + QStringLiteral("/settings.toml"), "[steam]\nenabled = false\n");
    library = openLibrary();
    QVERIFY(!library->inspectRom(copy).value("problem").toString().isEmpty());
    QVERIFY(!library->romNameFilters().join(u' ').contains(QStringLiteral("exe")));
    const QVariantMap added = library->addGame(copy, QStringLiteral("proton"), QStringLiteral("Cave Story"), {}, {});
    QVERIFY(!added.value("error").toString().isEmpty());
    QVERIFY(!QFileInfo::exists(gamePath("proton-cave-story")));
}

void LibraryProgramsTest::addProgramWhileSteamRuns()
{
    const QString root = installFakeSteam();
    const QString vdf = root + kShortcutsVdf;
    const QByteArray before = readFile(vdf);
    const auto steam = startFakeSteam(root);
    const QString exe = m_installs.home + QStringLiteral("/Games/Hollow Knight/hollow_knight.exe");
    writeFile(exe, "MZ");
    auto library = openLibrary();
    const QVariantMap added = library->addGame(exe, QStringLiteral("proton"), QStringLiteral("Hollow Knight"), {}, {});
    const QString id = added.value("id").toString();
    QCOMPARE(added.value("error").toString(), QString());
    // Queued to be added; shortcuts.vdf untouched until Steam closes.
    QVERIFY(pending(*library, id));
    QVERIFY(library->gameAt(library->rowOfGame(id)).value("shortcutQueued").toBool());
    QTest::qWait(300);
    QCOMPARE(readFile(vdf), before);
    steam->kill();
    steam->waitForFinished();
    QTRY_VERIFY_WITH_TIMEOUT(!pending(*library, id), 5000);
    QCOMPARE(shortcutIn(vdf, quint32(library->store().snapshot().content.games.value(id).shortcut)).exe, exe);
}

void LibraryProgramsTest::addProgramReadOnly()
{
    installFakeSteam();
    writeFile(m_data + QStringLiteral("/library/library.toml"), "version = 99\n");
    const QString exe = m_installs.home + QStringLiteral("/Games/Hollow Knight/hollow_knight.exe");
    writeFile(exe, "MZ");
    auto library = openLibrary();
    QVERIFY(library->readOnly());
    const QVariantMap added = library->addGame(exe, QStringLiteral("proton"), QStringLiteral("Hollow Knight"), {}, {});
    QVERIFY(!added.value("error").toString().isEmpty());
    QVERIFY(added.value("id").toString().isEmpty());
    QVERIFY(!QFileInfo::exists(gamePath("proton-hollow-knight")));
}

void LibraryProgramsTest::linuxInstallDir()
{
    const QString home = QStringLiteral("/home/user");
    QCOMPARE(::linuxInstallDir(QStringLiteral("/home/user/Games/SuperTux/run.sh"), home),
             QStringLiteral("/home/user/Games/SuperTux"));
    QCOMPARE(::linuxInstallDir(QStringLiteral("/home/user/Games/Celeste/bin/x86_64/Celeste.x86_64"), home),
             QStringLiteral("/home/user/Games/Celeste"));
    QCOMPARE(::linuxInstallDir(QStringLiteral("/mnt/g/Factorio/bin/Linux64/factorio"), home), QStringLiteral("/mnt/g/Factorio"));
    QCOMPARE(::linuxInstallDir(QStringLiteral("/mnt/g/Portal/game/lib/linux/x86/start"), home), QStringLiteral("/mnt/g/Portal"));
    // Never up to the home folder or /.
    QCOMPARE(::linuxInstallDir(QStringLiteral("/home/user/bin/game.sh"), home), QStringLiteral("/home/user/bin"));
    QCOMPARE(::linuxInstallDir(QStringLiteral("/lib/game"), home), QStringLiteral("/lib"));
    // Proton's generic folders stay as they were.
    QCOMPARE(::protonInstallDir(QStringLiteral("/mnt/g/Game/linux/game.exe"), home), QStringLiteral("/mnt/g/Game/linux"));
}

void LibraryProgramsTest::linuxProgramKind()
{
    const QString dir = m_dir->filePath(QStringLiteral("programs/"));
    const auto kind = [&](const QString &name, const QByteArray &bytes) {
        writeFile(dir + name, bytes);
        return ::linuxProgramKind(dir + name);
    };
    constexpr quint16 exec = 2, dyn = 3, rel = 1, x86_64 = 62, i386 = 3, ppc64 = 21;
    constexpr quint32 interp = 3, load = 1;
    QCOMPARE(kind("static", elf(exec, x86_64)), LinuxProgramKind::Elf);
    QCOMPARE(kind("pie.x86_64", elf(dyn, x86_64, interp)), LinuxProgramKind::Elf);
    QCOMPARE(kind("game.x86", elf(dyn, i386, interp, false)), LinuxProgramKind::Elf);
    QCOMPARE(kind("libgame", elf(dyn, x86_64, load)), LinuxProgramKind::SharedLibrary);
    // Named like a library: one, even with an interpreter (libc.so.6 has one).
    QCOMPARE(kind("libc.so.6", elf(dyn, x86_64, interp)), LinuxProgramKind::SharedLibrary);
    QCOMPARE(kind("game.o", elf(rel, x86_64)), LinuxProgramKind::None);
    // Another processor's, like a PS3 game's EBOOT: a ROM, not a program.
    QCOMPARE(kind("EBOOT.ELF", elf(exec, ppc64, load, true, true)), LinuxProgramKind::None);
    // AppImage's runtime marks the header.
    QByteArray appImage = elf(exec, x86_64);
    appImage.replace(8, 3, QByteArray("AI\x02", 3));
    QCOMPARE(kind("Game.AppImage", appImage), LinuxProgramKind::AppImage);
    QCOMPARE(kind("run.sh", "#!/bin/sh\nexec ./game\n"), LinuxProgramKind::Script);
    QCOMPARE(kind("start", "#!/usr/bin/env bash\n"), LinuxProgramKind::Script);
    QCOMPARE(kind("nobang.sh", "./game\n"), LinuxProgramKind::None);
    QCOMPARE(kind("short", "\x7f" "ELF"), LinuxProgramKind::None);
    QCOMPARE(kind("empty", ""), LinuxProgramKind::None);
    QCOMPARE(::linuxProgramKind(dir + QStringLiteral("missing")), LinuxProgramKind::None);
    QCOMPARE(::linuxProgramKind(dir), LinuxProgramKind::None);
}

void LibraryProgramsTest::addLinuxProgram()
{
    writeFile(m_data + QStringLiteral("/settings.toml"),
              demoSettings() + "default_presets = [\"mangohud\", \"wayland\"]\n");
    const QString script = m_installs.home + QStringLiteral("/Games/SuperTux/bin/x86_64/run.sh");
    writeFile(script, "#!/bin/sh\nsleep 1\n");
    QFile::setPermissions(script, QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ReadGroup);
    auto library = openLibrary();
    QVERIFY(library->romNameFilters().contains(QStringLiteral("Linux programs (*.sh *.x86_64 *.x86 *.AppImage *.appimage *.bin)")));
    QCOMPARE(library->romNameFilters().last(), QStringLiteral("All files (*)"));

    // A Linux game: titled after the folder above the generic ones, with the default presets
    // but those only Proton reads.
    const QVariantMap pick = library->inspectRom(QUrl::fromLocalFile(script).toString());
    QVERIFY(pick.value("linux").toBool());
    QVERIFY(!pick.value("proton").toBool());
    QCOMPARE(pick.value("kind").toString(), QStringLiteral("script"));
    QCOMPARE(pick.value("platforms").toStringList(), QStringList{"linux"});
    QCOMPARE(pick.value("choices").toList().value(0).toMap().value("name").toString(), QStringLiteral("Linux"));
    QCOMPARE(pick.value("choices").toList().value(0).toMap().value("icon").toString(), QStringLiteral("linux"));
    QCOMPARE(pick.value("title").toString(), QStringLiteral("SuperTux"));
    QCOMPARE(pick.value("installDir").toString(), m_installs.home + QStringLiteral("/Games/SuperTux"));
    QCOMPARE(pick.value("problem").toString(), QString());
    QCOMPARE(pick.value("existing").toString(), QString());
    QVERIFY(!pick.value("executable").toBool());
    QCOMPARE(pick.value("launchOptions").toString(), QStringLiteral("mangohud %command%"));
    QStringList presets;
    for (const QVariant &p : pick.value("steamPresets").toList())
        presets.append(p.toMap().value("id").toString());
    QVERIFY(presets.contains(QStringLiteral("mangohud")));
    QVERIFY(presets.contains(QStringLiteral("gamemode")));
    QVERIFY(!presets.contains(QStringLiteral("wayland")));

    // Added, made executable: a game file, installed right away.
    const QVariantMap added = library->addGame(script, QStringLiteral("linux"), QStringLiteral("SuperTux"),
                                               {{"launch_options", "mangohud %command%"}, {"makeExecutable", true}}, {});
    QCOMPARE(added.value("error").toString(), QString());
    const QString id = added.value("id").toString();
    QCOMPARE(id, QStringLiteral("linux-supertux"));
    QVERIFY(QFileInfo(script).isExecutable());
    QVERIFY(QFileInfo(script).permissions() & QFileDevice::ExeGroup);
    QVERIFY(!(QFileInfo(script).permissions() & QFileDevice::ExeOther)); // others can't read it
    const QByteArray today = QDate::currentDate().toString(Qt::ISODate).toUtf8();
    QCOMPARE(readFile(gamePath(id)),
             "title = \"SuperTux\"\nplatform = \"linux\"\nadded = " + today
                 + "\nlaunch_options = \"mangohud %command%\"\n"
                   "exe = \"~/Games/SuperTux/bin/x86_64/run.sh\"\ninstall_dir = \"~/Games/SuperTux\"\n");
    QTRY_VERIFY_WITH_TIMEOUT(library->game(id) && library->game(id)->installed, 5000);

    // Picked again: it's that game.
    QCOMPARE(library->inspectRom(script).value("existing").toString(), id);
    const QVariantMap again = library->addGame(script, QStringLiteral("linux"), QStringLiteral("SuperTux"), {}, {});
    QVERIFY(again.value("existing").toBool());
    QCOMPARE(again.value("id").toString(), id);
    QVERIFY(!QFileInfo::exists(gamePath("linux-supertux-2")));

    // Without an extension ("All files"), found by its content; already executable. An install
    // folder and start folder chosen in the dialog, written like the app writes paths.
    QByteArray program = elf(3, 62, 3);
    const QString terraria = m_installs.home + QStringLiteral("/Games/Terraria/bin/x64/Terraria");
    writeFile(terraria, program);
    QFile::setPermissions(terraria, QFile::permissions(terraria) | QFileDevice::ExeOwner);
    const QVariantMap elfPick = library->inspectRom(terraria);
    QCOMPARE(elfPick.value("kind").toString(), QStringLiteral("elf"));
    QVERIFY(elfPick.value("executable").toBool());
    QCOMPARE(elfPick.value("title").toString(), QStringLiteral("Terraria"));
    const QVariantMap f = library->addGame(terraria, QStringLiteral("linux"), QStringLiteral("Terraria"),
                                           {{"install_dir", m_installs.home + QStringLiteral("/Games/Terraria/bin")},
                                            {"start_dir", m_installs.home + QStringLiteral("/Games/Terraria/")},
                                            {"launch_options", ""}},
                                           {});
    QCOMPARE(f.value("error").toString(), QString());
    const QByteArray written = readFile(gamePath("linux-terraria"));
    QVERIFY(written.contains("install_dir = \"~/Games/Terraria/bin\"\n"));
    QVERIFY(written.contains("start_dir = \"~/Games/Terraria\"\n"));
    QVERIFY(!written.contains("launch_options"));

    // An AppImage, not made executable when asked not to: added, but not installed until it is.
    QByteArray appImage = elf(2, 62);
    appImage.replace(8, 3, QByteArray("AI\x02", 3));
    const QString celeste = m_installs.home + QStringLiteral("/Apps/Celeste.AppImage");
    writeFile(celeste, appImage);
    QCOMPARE(library->inspectRom(celeste).value("kind").toString(), QStringLiteral("appimage"));
    const QVariantMap c = library->addGame(celeste, QStringLiteral("linux"), QStringLiteral("Celeste"),
                                           {{"makeExecutable", false}}, {});
    QCOMPARE(c.value("error").toString(), QString());
    QVERIFY(!QFileInfo(celeste).isExecutable());
    library->store().waitForIdle();
    QCoreApplication::processEvents();
    QVERIFY(!library->game(c.value("id").toString())->installed);
}

void LibraryProgramsTest::addLinuxProgramRefused()
{
    auto library = openLibrary();
    const QString dir = m_installs.home + QStringLiteral("/Games/Odd/");
    const auto problem = [&](const QString &name, const QByteArray &bytes) {
        writeFile(dir + name, bytes);
        const QVariantMap pick = library->inspectRom(dir + name);
        return pick.value("linux").toBool() ? QString() : pick.value("problem").toString();
    };
    QVERIFY(problem("libgame.so", elf(3, 62, 1)).contains(QStringLiteral("shared library")));
    QVERIFY(problem("libsteam_api.so.1", elf(3, 62, 3)).contains(QStringLiteral("shared library")));
    QVERIFY(problem("start.sh", "./game\n").contains(QStringLiteral("#!")));
    QVERIFY(problem("game.x86_64", "not a program").contains(QStringLiteral("isn't a program")));
    QVERIFY(problem("Game.AppImage", "").contains(QStringLiteral("isn't a program")));
    // A .bin that isn't a program is a ROM, as before.
    writeFile(dir + QStringLiteral("Crash.bin"), "rom");
    const QVariantMap rom = library->inspectRom(dir + QStringLiteral("Crash.bin"));
    QVERIFY(!rom.value("linux").toBool());
    QCOMPARE(rom.value("problem").toString(), QString());
    // A demo game's program: that game.
    const QString tux = m_data + QStringLiteral("/linux-games/SuperTuxKart/run_game.sh");
    QCOMPARE(library->inspectRom(tux).value("existing").toString(), QStringLiteral("linux-supertuxkart"));
    QVERIFY(library->addGame(tux, QStringLiteral("linux"), QStringLiteral("Tux"), {}, {}).value("existing").toBool());
    // Not a program, no title, or not a file.
    const QString script = dir + QStringLiteral("run.sh");
    writeFile(script, "#!/bin/sh\n");
    QVERIFY(!library->addGame(dir + QStringLiteral("start.sh"), QStringLiteral("linux"), QStringLiteral("Odd"), {}, {})
                 .value("error").toString().isEmpty());
    QVERIFY(!library->addGame(dir + QStringLiteral("libgame.so"), QStringLiteral("linux"), QStringLiteral("Odd"), {}, {})
                 .value("error").toString().isEmpty());
    QVERIFY(!library->addGame(script, QStringLiteral("linux"), QStringLiteral(" "), {}, {}).value("error").toString().isEmpty());
    QVERIFY(!library->addGame(script + "x", QStringLiteral("linux"), QStringLiteral("Odd"), {}, {}).value("error").toString().isEmpty());
    QVERIFY(!QFileInfo::exists(gamePath("linux-odd")));
}

void LibraryProgramsTest::addLinuxProgramReadOnly()
{
    writeFile(m_data + QStringLiteral("/library/library.toml"), "version = 99\n");
    const QString script = m_installs.home + QStringLiteral("/Games/SuperTux/run.sh");
    writeFile(script, "#!/bin/sh\n");
    auto library = openLibrary();
    QVERIFY(library->readOnly());
    const QVariantMap added = library->addGame(script, QStringLiteral("linux"), QStringLiteral("SuperTux"),
                                               {{"makeExecutable", true}}, {});
    QVERIFY(!added.value("error").toString().isEmpty());
    QVERIFY(added.value("id").toString().isEmpty());
    QVERIFY(!QFileInfo::exists(gamePath("linux-supertux")));
    QVERIFY(!QFileInfo(script).isExecutable());
}

void LibraryProgramsTest::protonArtToSteam()
{
    const QString root = installFakeSteam();
    const QString vdf = root + kShortcutsVdf;
    const QString grid = root + QStringLiteral("/userdata/39734273/config/grid/");
    const QString exe = m_installs.home + QStringLiteral("/Games/Hollow Knight/hollow_knight.exe");
    writeFile(exe, "MZ");
    auto library = openLibrary();
    QTRY_VERIFY_WITH_TIMEOUT(library->store().snapshot().steamQueued.isEmpty(), 5000);

    const quint32 appId =
        SteamShortcuts::newAppId(SteamShortcuts::exeField(exe), QStringLiteral("Hollow Knight"), {});
    const QString id = QString::number(appId);
    const auto image = [&](const QString &path, Qt::GlobalColor color) {
        QImage i(64, 32, QImage::Format_RGB32);
        i.fill(color);
        QDir().mkpath(QFileInfo(path).absolutePath());
        QVERIFY(i.save(path));
    };
    // The cover the user gave it in Steam, and the art picked in the "+" dialog.
    image(grid + id + QStringLiteral("p.png"), Qt::black);
    const QByteArray usersCover = readFile(grid + id + QStringLiteral("p.png"));
    const QString pics = m_dir->filePath(QStringLiteral("pics"));
    image(pics + QStringLiteral("/hero.png"), Qt::red);
    image(pics + QStringLiteral("/cover.png"), Qt::green);
    image(pics + QStringLiteral("/icon.jpg"), Qt::blue);
    const QVariantMap added = library->addGame(exe, QStringLiteral("proton"), QStringLiteral("Hollow Knight"),
                                               {{"proton", "GE-Proton10-4"}},
                                               {{"hero", pics + QStringLiteral("/hero.png")},
                                                {"cover", pics + QStringLiteral("/cover.png")},
                                                {"icon", pics + QStringLiteral("/icon.jpg")}});
    QCOMPARE(added.value("error").toString(), QString());
    const QString game = added.value("id").toString();
    QCOMPARE(quint32(library->store().snapshot().content.games.value(game).shortcut), appId);

    // Into grid/ right away, but for the cover Steam already has; the shortcut takes the icon.
    QTRY_VERIFY_WITH_TIMEOUT(QFileInfo::exists(grid + id + QStringLiteral("_hero.png")), 5000);
    QTRY_VERIFY_WITH_TIMEOUT(QFileInfo::exists(grid + id + QStringLiteral("_icon.png")), 5000);
    QCOMPARE(readFile(grid + id + QStringLiteral("_hero.png")), readFile(pics + QStringLiteral("/hero.png")));
    QCOMPARE(readFile(grid + id + QStringLiteral("p.png")), usersCover);
    QTRY_COMPARE_WITH_TIMEOUT(shortcutIn(vdf, appId).icon, grid + id + QStringLiteral("_icon.png"), 5000);
    QVERIFY(!pending(*library, game));
    QVERIFY(library->store().wroteGridArt(appId, QStringLiteral("hero")));
    QVERIFY(!library->store().wroteGridArt(appId, QStringLiteral("cover")));

    // Removed here: the launcher's hero goes, and isn't copied back from Steam.
    QCOMPARE(library->removeArt(game, QStringLiteral("hero")), QString());
    QTRY_VERIFY_WITH_TIMEOUT(!QFileInfo::exists(grid + id + QStringLiteral("_hero.png")), 5000);
    library->store().waitForIdle();
    QVERIFY(library->store().snapshot().art.value(game).hero.isEmpty());
    // The user's cover stays in Steam, so the game skips it from now on.
    QCOMPARE(library->removeArt(game, QStringLiteral("cover")), QString());
    QCOMPARE(library->store().snapshot().content.games.value(game).skipSteamArt, QStringList{"cover"});
    // The icon removed: its file goes, and the shortcut stops pointing at it.
    QCOMPARE(library->removeArt(game, QStringLiteral("icon")), QString());
    QTRY_COMPARE_WITH_TIMEOUT(shortcutIn(vdf, appId).icon, QString(), 5000);
    QVERIFY(!QFileInfo::exists(grid + id + QStringLiteral("_icon.png")));
    QCOMPARE(readFile(grid + id + QStringLiteral("p.png")), usersCover);

    // A new hero: written again.
    QCOMPARE(library->setArt(game, QStringLiteral("hero"), pics + QStringLiteral("/cover.png")), QString());
    QTRY_COMPARE_WITH_TIMEOUT(readFile(grid + id + QStringLiteral("_hero.png")), readFile(pics + QStringLiteral("/cover.png")),
                              5000);
}

QTEST_GUILESS_MAIN(LibraryProgramsTest)

#include "library_programs_test.moc"
