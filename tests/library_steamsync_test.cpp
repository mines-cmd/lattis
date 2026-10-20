#include "librarytestbase.h"

// The library tests on launch options and Proton versions synced with Steam, and undoing them.
class LibrarySteamSyncTest : public LibraryTestBase
{
    Q_OBJECT

private slots:
    void steamSyncFollowsBothSides();
    void steamSyncRestore();
    void steamSyncNewLibraryAtOldPath();
    void steamDefaultPresetsForNewGames();
    void steamApplyWhenSteamCloses();
    void steamApplyRestore();
    void steamPresetForAllAndQueue();
    void steamUndo();
};

void LibrarySteamSyncTest::steamSyncFollowsBothSides()
{
    const QString root = installFakeSteam();
    // Running throughout, so what's queued stays queued.
    const auto steam = startFakeSteam(root);
    const QString localConfig = root + QStringLiteral("/userdata/39734273/config/localconfig.vdf");
    const QString record = SteamSync::recordPath(m_state, AppPaths::dataDirKey(m_data));
    const QString tomb = QStringLiteral("steam-8000");
    const QString pizza = QStringLiteral("steam-2231450");
    auto library = openLibrary();

    // No record yet, and nothing in the game files: Steam's values are copied, so the game
    // file keeps a copy. The state dir didn't exist; the record is written there.
    QCOMPARE(launchOptionsOf(*library, tomb), QStringLiteral(R"(PROTON_LOG=1 %command% -window "C:\Games\save dir")"));
    QCOMPARE(library->store().snapshot().content.games.value(tomb).proton, QStringLiteral("GE-Proton10-4"));
    QVERIFY(!pending(*library, tomb));
    QVERIFY(!QFileInfo::exists(gamePath(pizza))); // nothing to copy
    QVERIFY(readFile(record).contains("8000 = { launch_options = \"PROTON_LOG=1 %command% -window \\\"C:\\\\Games\\\\save dir\\\"\", proton = \"GE-Proton10-4\" }\n"));
    QVERIFY(library->problems().isEmpty());

    // Changed here: queued, right away.
    QCOMPARE(library->setGameValue(pizza, QStringLiteral("launch_options"), QStringLiteral("mangohud %command%")),
             QString());
    QVERIFY(pending(*library, pizza));
    QCOMPARE(library->gameForEditing(pizza).value("launchOptions").toString(), QStringLiteral("mangohud %command%"));
    // And it stays queued: Steam still has what the record says.
    library->store().refresh();
    library->store().waitForIdle();
    QCoreApplication::processEvents();
    QVERIFY(pending(*library, pizza));
    QCOMPARE(launchOptionsOf(*library, pizza), QStringLiteral("mangohud %command%"));

    // Changed in Steam: copied into the game file.
    setSteamLaunchOptions(localConfig, 8000, QStringLiteral("-fullscreen"));
    QTRY_COMPARE_WITH_TIMEOUT(launchOptionsOf(*library, tomb), QStringLiteral("-fullscreen"), 5000);
    QVERIFY(readFile(gamePath(tomb)).contains("launch_options = \"-fullscreen\"\n"));
    QVERIFY(!pending(*library, tomb));
    QTRY_VERIFY_WITH_TIMEOUT(readFile(record).contains("8000 = { launch_options = \"-fullscreen\""), 5000);

    // Changed on both sides: Steam wins.
    QCOMPARE(library->setGameValue(tomb, QStringLiteral("launch_options"), QStringLiteral("mine")), QString());
    QVERIFY(pending(*library, tomb));
    setSteamLaunchOptions(localConfig, 8000, QStringLiteral("theirs %command%"));
    QTRY_COMPARE_WITH_TIMEOUT(launchOptionsOf(*library, tomb), QStringLiteral("theirs %command%"), 5000);
    QVERIFY(!pending(*library, tomb));

    // Cleared in Steam: the key goes from the game file.
    setSteamLaunchOptions(localConfig, 8000, QString());
    QTRY_VERIFY_WITH_TIMEOUT(!readFile(gamePath(tomb)).contains("launch_options"), 5000);
    QVERIFY(readFile(gamePath(tomb)).contains("proton = \"GE-Proton10-4\"\n"));

    // Steam's files are still only read while it runs.
    QVERIFY(pending(*library, pizza));
    QVERIFY(!readFile(localConfig).contains("mangohud"));

    // The queue comes back from the cache at the next start, before any scan.
    library.reset();
    Library cached(m_data, m_cache);
    QVERIFY(pending(cached, pizza));
    steam->kill();
    steam->waitForFinished();
}

void LibrarySteamSyncTest::steamSyncRestore()
{
    const QString root = installFakeSteam();
    writeFile(gamePath(QStringLiteral("steam-2231450")), "launch_options = \"gamemoderun %command%\"\nproton = \"proton_9\"\n");
    writeFile(gamePath(QStringLiteral("steam-8000")), "launch_options = \"PROTON_LOG=1 %command% -window \\\"C:\\\\Games\\\\save dir\\\"\"\n");
    const QByteArray localConfig = readFile(root + QStringLiteral("/userdata/39734273/config/localconfig.vdf"));
    const auto steam = startFakeSteam(root);

    // A data dir restored on a machine whose Steam doesn't have these values, and a new
    // state dir: the game files' values are queued, not overwritten with Steam's. (Written
    // when Steam closes: see steamApplyRestore.)
    auto library = openLibrary();
    QVERIFY(pending(*library, QStringLiteral("steam-2231450")));
    QCOMPARE(library->store().snapshot().steamQueued.value(QStringLiteral("steam-2231450")),
             (QStringList{"launch_options", "proton"}));
    QCOMPARE(launchOptionsOf(*library, QStringLiteral("steam-2231450")), QStringLiteral("gamemoderun %command%"));
    // Tomb Raider's launch options match Steam's; its Proton version is Steam's, copied.
    QVERIFY(!pending(*library, QStringLiteral("steam-8000")));
    QVERIFY(readFile(gamePath(QStringLiteral("steam-8000"))).contains("proton = \"GE-Proton10-4\"\n"));
    QCOMPARE(readFile(root + QStringLiteral("/userdata/39734273/config/localconfig.vdf")), localConfig);

    // Clearing the state dir again at worst queues them once more.
    library.reset();
    QVERIFY(QDir(m_state).removeRecursively());
    library = openLibrary();
    QVERIFY(pending(*library, QStringLiteral("steam-2231450")));
    QVERIFY(!pending(*library, QStringLiteral("steam-8000")));

    // A broken record counts as none.
    library.reset();
    writeFile(SteamSync::recordPath(m_state, AppPaths::dataDirKey(m_data)), "[games\n");
    library = openLibrary();
    QVERIFY(pending(*library, QStringLiteral("steam-2231450")));
    library.reset();
    steam->kill();
    steam->waitForFinished();
}

void LibrarySteamSyncTest::steamSyncNewLibraryAtOldPath()
{
    const QString root = installFakeSteam();
    const QString localConfig = root + QStringLiteral("/userdata/39734273/config/localconfig.vdf");
    const QString record = SteamSync::recordPath(m_state, AppPaths::dataDirKey(m_data));
    const QString tomb = QStringLiteral("steam-8000");
    auto library = openLibrary();
    QVERIFY(readFile(record).contains("8000 = { launch_options = \"PROTON_LOG=1"));
    library.reset();

    // The library is deleted and a new one starts at the same path, with Steam closed. The old
    // library's record would take its empty game files for cleared values and clear them in
    // Steam; instead Steam's values are copied, and Steam's files stay as they are.
    const QByteArray steamBefore = readFile(localConfig);
    const QByteArray configBefore = readFile(root + QStringLiteral("/config/config.vdf"));
    QVERIFY(QDir(m_data + QStringLiteral("/library")).removeRecursively());
    library = openLibrary();
    QTest::qWait(100); // a queued edit would be written into Steam by now
    QCOMPARE(launchOptionsOf(*library, tomb), QStringLiteral(R"(PROTON_LOG=1 %command% -window "C:\Games\save dir")"));
    QCOMPARE(library->store().snapshot().content.games.value(tomb).proton, QStringLiteral("GE-Proton10-4"));
    QVERIFY(library->store().snapshot().steamQueued.isEmpty());
    QCOMPARE(readFile(localConfig), steamBefore);
    QCOMPARE(readFile(root + QStringLiteral("/config/config.vdf")), configBefore);
    QVERIFY(readFile(record).contains(QStringLiteral("library = \"%1\"\n").arg(libraryId()).toUtf8()));
}

void LibrarySteamSyncTest::steamDefaultPresetsForNewGames()
{
    const QString root = installFakeSteam();
    writeFile(m_data + QStringLiteral("/settings.toml"), "[steam]\ndefault_presets = [\"mangohud\"]\n");
    const auto steam = startFakeSteam(root); // so the defaults stay queued
    auto library = openLibrary();

    // Both Steam games are new: they start with MangoHud, on top of Steam's launch options,
    // queued for Steam.
    QCOMPARE(launchOptionsOf(*library, QStringLiteral("steam-2231450")), QStringLiteral("mangohud %command%"));
    QCOMPARE(launchOptionsOf(*library, QStringLiteral("steam-8000")),
             QStringLiteral(R"(PROTON_LOG=1 mangohud %command% -window "C:\Games\save dir")"));
    QVERIFY(pending(*library, QStringLiteral("steam-2231450")));
    QVERIFY(pending(*library, QStringLiteral("steam-8000")));
    // The demo's Steam games were in steam.toml already: untouched.
    QVERIFY(launchOptionsOf(*library, QStringLiteral("steam-1145360")).isEmpty());
    QVERIFY(!pending(*library, QStringLiteral("steam-1145360")));

    // Turned off by hand, it stays off: only new games get the defaults.
    QCOMPARE(library->setGameValue(QStringLiteral("steam-2231450"), QStringLiteral("launch_options"), QString()),
             QString());
    QVERIFY(!pending(*library, QStringLiteral("steam-2231450")));
    const QString manifest = root + QStringLiteral("/steamapps/appmanifest_620.acf");
    writeFile(manifest, "\"AppState\"\n{\n\t\"appid\"\t\t\"620\"\n\t\"name\"\t\t\"Portal 2\"\n\t\"StateFlags\"\t\t\"4\"\n}\n");
    QTRY_COMPARE_WITH_TIMEOUT(launchOptionsOf(*library, QStringLiteral("steam-620")), QStringLiteral("mangohud %command%"), 5000);
    QVERIFY(pending(*library, QStringLiteral("steam-620")));
    QVERIFY(launchOptionsOf(*library, QStringLiteral("steam-2231450")).isEmpty());
    library.reset();
    steam->kill();
    steam->waitForFinished();
}

void LibrarySteamSyncTest::steamApplyWhenSteamCloses()
{
    const QString root = installFakeSteam();
    const QString localConfig = root + QStringLiteral("/userdata/39734273/config/localconfig.vdf");
    const QString config = root + QStringLiteral("/config/config.vdf");
    const QByteArray localBefore = readFile(localConfig);
    const QByteArray configBefore = readFile(config);
    const QString pizza = QStringLiteral("steam-2231450");
    const auto steam = startFakeSteam(root);
    auto library = openLibrary();

    // Saved while Steam runs: queued, and Steam's files untouched.
    QCOMPARE(library->setGameValue(pizza, QStringLiteral("launch_options"), QStringLiteral("mangohud %command%")),
             QString());
    QCOMPARE(library->setGameValue(pizza, QStringLiteral("proton"), QStringLiteral("proton_9")), QString());
    QTest::qWait(100);
    QVERIFY(pending(*library, pizza));
    QCOMPARE(library->store().applySteamQueue(), QStringLiteral("Steam is running; this is applied when it closes"));
    QCOMPARE(readFile(localConfig), localBefore);
    QCOMPARE(readFile(config), configBefore);
    QVERIFY(!QFileInfo::exists(m_state + QStringLiteral("/steam-backup")));

    // Steam closes: written, backed up first, and the record follows.
    steam->kill();
    steam->waitForFinished();
    QTRY_VERIFY_WITH_TIMEOUT(!pending(*library, pizza), 5000);
    QVERIFY(readFile(localConfig).contains("\t\t\t\t\t\t\"LaunchOptions\"\t\t\"mangohud %command%\"\n"));
    QVERIFY(readFile(config).contains("\t\t\t\t\t\"2231450\"\n\t\t\t\t\t{\n\t\t\t\t\t\t\"name\"\t\t\"proton_9\"\n"));
    const QString backups = m_state + QStringLiteral("/steam-backup");
    QCOMPARE(readFile(backups + QStringLiteral("/userdata/39734273/config/localconfig.vdf")), localBefore);
    QCOMPARE(readFile(backups + QStringLiteral("/config/config.vdf")), configBefore);
    const QString record = SteamSync::recordPath(m_state, AppPaths::dataDirKey(m_data));
    QVERIFY(readFile(record).contains("2231450 = { launch_options = \"mangohud %command%\", proton = \"proton_9\" }\n"));

    // The scan that follows finds them in sync.
    library->store().refresh();
    library->store().waitForIdle();
    QCoreApplication::processEvents();
    QVERIFY(!pending(*library, pizza));
    QCOMPARE(launchOptionsOf(*library, pizza), QStringLiteral("mangohud %command%"));

    // With Steam closed, a save is applied right away; cleared, the key goes again.
    QCOMPARE(library->setGameValue(pizza, QStringLiteral("launch_options"), QString()), QString());
    QTRY_COMPARE_WITH_TIMEOUT(readFile(localConfig), localBefore, 5000);
    QVERIFY(!pending(*library, pizza));
    // The backups are still the files as they first were.
    QCOMPARE(readFile(backups + QStringLiteral("/userdata/39734273/config/localconfig.vdf")), localBefore);
}

void LibrarySteamSyncTest::steamApplyRestore()
{
    // A data dir restored on a machine whose Steam doesn't have its values, with a new state
    // dir and Steam closed: the game files' values are written into Steam at the first scan.
    const QString root = installFakeSteam();
    writeFile(gamePath(QStringLiteral("steam-2231450")), "launch_options = \"gamemoderun %command%\"\nproton = \"proton_9\"\n");
    auto library = openLibrary();
    QTRY_VERIFY_WITH_TIMEOUT(!pending(*library, QStringLiteral("steam-2231450")), 5000);
    QVERIFY(readFile(root + QStringLiteral("/userdata/39734273/config/localconfig.vdf"))
                .contains("\"LaunchOptions\"\t\t\"gamemoderun %command%\""));
    QVERIFY(readFile(root + QStringLiteral("/config/config.vdf")).contains("\"name\"\t\t\"proton_9\""));
    // Tomb Raider's values were Steam's: copied into its game file, nothing written to Steam.
    QVERIFY(readFile(gamePath(QStringLiteral("steam-8000"))).contains("proton = \"GE-Proton10-4\"\n"));
    QVERIFY(readFile(SteamSync::recordPath(m_state, AppPaths::dataDirKey(m_data)))
                .contains("2231450 = { launch_options = \"gamemoderun %command%\", proton = \"proton_9\" }\n"));
}

void LibrarySteamSyncTest::steamPresetForAllAndQueue()
{
    const QString root = installFakeSteam();
    // Running throughout, so what's queued stays queued.
    const auto steam = startFakeSteam(root);
    const QString tomb = QStringLiteral("steam-8000");
    const QString pizza = QStringLiteral("steam-2231450");
    const QByteArray diablo = readFile(gamePath(QStringLiteral("proton-diablo-iv")));
    const QByteArray kart = readFile(gamePath(QStringLiteral("switch-mario-kart-8-deluxe")));
    auto library = openLibrary();
    const QString tombBefore = launchOptionsOf(*library, tomb);
    const auto preset = [&](const QString &id) {
        for (const QVariant &p : library->steam().value("presets").toList()) {
            if (p.toMap().value("id") == id)
                return p.toMap();
        }
        return QVariantMap();
    };
    const auto queued = [&](const QString &id) {
        for (const QVariant &g : library->steam().value("queue").toList()) {
            if (g.toMap().value("id") == id)
                return g.toMap();
        }
        return QVariantMap();
    };
    const int games = library->steam().value("games").toInt();
    QVERIFY(games >= 2); // the demo's steam.toml and the fake Steam's
    QCOMPARE(preset(QStringLiteral("mangohud")).value("games").toInt(), 0);
    QVERIFY(queued(pizza).isEmpty());

    // On in every Steam game, keeping what else they have, and queued for Steam.
    QCOMPARE(library->setSteamPresetForAll(QStringLiteral("mangohud"), true), QString());
    QCOMPARE(preset(QStringLiteral("mangohud")).value("games").toInt(), games);
    QCOMPARE(launchOptionsOf(*library, pizza), QStringLiteral("mangohud %command%"));
    QVERIFY(library->steamPresetOn(launchOptionsOf(*library, tomb), QStringLiteral("mangohud")));
    QVERIFY(launchOptionsOf(*library, tomb).contains(QStringLiteral("-window")));
    QVERIFY(launchOptionsOf(*library, QStringLiteral("steam-1145360")).contains(QStringLiteral("mangohud")));
    QVERIFY(pending(*library, pizza));
    // Other platforms' games aren't touched, not even Proton ones.
    QCOMPARE(readFile(gamePath(QStringLiteral("proton-diablo-iv"))), diablo);
    QCOMPARE(readFile(gamePath(QStringLiteral("switch-mario-kart-8-deluxe"))), kart);

    // The queue on the settings page: the game and what changes, from Steam's value.
    const QVariantList changes = queued(pizza).value("changes").toList();
    QCOMPARE(queued(pizza).value("title").toString(), QStringLiteral("Pizza Tower"));
    QCOMPARE(changes.size(), 1);
    QCOMPARE(changes.first().toMap().value("name").toString(), QStringLiteral("Launch options"));
    QCOMPARE(changes.first().toMap().value("from").toString(), QString());
    QCOMPARE(changes.first().toMap().value("to").toString(), QStringLiteral("mangohud %command%"));
    // A Proton version shows by its name; Steam's default when there's none.
    QCOMPARE(library->setGameValue(pizza, QStringLiteral("proton"), QStringLiteral("GE-Proton10-4")), QString());
    const QVariantMap proton = queued(pizza).value("changes").toList().value(1).toMap();
    QCOMPARE(proton.value("name").toString(), QStringLiteral("Proton"));
    QCOMPARE(proton.value("from").toString(), QStringLiteral("Steam's default"));
    QCOMPARE(proton.value("to").toString(), QStringLiteral("GE-Proton10-4"));
    QCOMPARE(library->setGameValue(pizza, QStringLiteral("proton"), QString()), QString());

    // Off in every one again: back to what they had, and nothing queued for Pizza Tower.
    QCOMPARE(library->setSteamPresetForAll(QStringLiteral("mangohud"), false), QString());
    QCOMPARE(preset(QStringLiteral("mangohud")).value("games").toInt(), 0);
    QCOMPARE(launchOptionsOf(*library, tomb), tombBefore);
    QCOMPARE(launchOptionsOf(*library, pizza), QString());
    QVERIFY(!pending(*library, pizza));
    QVERIFY(queued(pizza).isEmpty());

    QVERIFY(!library->setSteamPresetForAll(QStringLiteral("nope"), true).isEmpty());
    steam->kill();
    steam->waitForFinished();
}

void LibrarySteamSyncTest::steamUndo()
{
    const QString root = installFakeSteam();
    const QString vdf = root + kShortcutsVdf;
    const QString localConfig = root + QStringLiteral("/userdata/39734273/config/localconfig.vdf");
    auto library = openLibrary();
    QTRY_VERIFY_WITH_TIMEOUT(library->store().snapshot().steamQueued.isEmpty(), 5000); // FFXIV's shortcut added
    const QString pizza = QStringLiteral("steam-2231450");
    const QString ffxiv = QStringLiteral("proton-final-fantasy-xiv-online");
    const QString pizzaBefore = launchOptionsOf(*library, pizza);
    const QString ffxivBefore = launchOptionsOf(*library, ffxiv);
    const QByteArray vdfBefore = readFile(vdf);
    const QByteArray localBefore = readFile(localConfig);
    const auto steam = startFakeSteam(root);

    // Edits and a new game, all waiting for Steam.
    QCOMPARE(library->setGameValue(pizza, QStringLiteral("launch_options"), QStringLiteral("gamemoderun %command%")),
             QString());
    QCOMPARE(library->setGameValue(ffxiv, QStringLiteral("launch_options"), QStringLiteral("PROTON_LOG=1 %command%")),
             QString());
    quint32 appId = 0;
    const QString hollow = addUninstallGame(*library, &appId);
    QTRY_COMPARE_WITH_TIMEOUT(library->steamWaiting().size(), 3, 5000);
    for (const QVariant &w : library->steamWaiting())
        QCOMPARE(w.toMap().value("adds").toBool(), w.toMap().value("id") == hollow);

    // One game: Steam's value comes back into its game file.
    QCOMPARE(library->undoSteamEdits(pizza), QString());
    QTRY_COMPARE_WITH_TIMEOUT(launchOptionsOf(*library, pizza), pizzaBefore, 5000);
    QVERIFY(!pending(*library, pizza));
    QCOMPARE(library->steamWaiting().size(), 2);

    // The rest: FFXIV's too, and the game Steam doesn't have yet leaves the library.
    QCOMPARE(library->undoAllSteamEdits(), QString());
    QTRY_COMPARE_WITH_TIMEOUT(launchOptionsOf(*library, ffxiv), ffxivBefore, 5000);
    QVERIFY(!QFileInfo::exists(gamePath(hollow)));
    QTRY_VERIFY_WITH_TIMEOUT(library->steamWaiting().isEmpty(), 5000);

    // Steam closes: nothing to write.
    steam->kill();
    steam->waitForFinished();
    QTest::qWait(300);
    QCOMPARE(readFile(vdf), vdfBefore);
    QCOMPARE(readFile(localConfig), localBefore);
    QVERIFY(library->store().snapshot().steamQueued.isEmpty());
}

QTEST_GUILESS_MAIN(LibrarySteamSyncTest)

#include "library_steamsync_test.moc"
