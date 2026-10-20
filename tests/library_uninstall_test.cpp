#include "librarytestbase.h"

// The library tests on Uninstall of Proton and Linux games.
class LibraryUninstallTest : public LibraryTestBase
{
    Q_OBJECT

private slots:
    void uninstallKeptThenRemoved();
    void uninstallHidesOrShows();
    void uninstallForgets();
    void uninstallWhileSteamRuns();
    void steamUndoUninstalls();
    void uninstallWithoutTrash();
    void uninstallStopsAtAFolder();
    void uninstallRefused();
    void uninstallReadOnly();
    void uninstallRefusals();
    void uninstallLinuxGame();
    void uninstallAppImage();
};

void LibraryUninstallTest::uninstallKeptThenRemoved()
{
    const QString root = installFakeSteam();
    const QString vdf = root + kShortcutsVdf;
    const QString config = root + QStringLiteral("/config/config.vdf");
    const QString grid = root + QStringLiteral("/userdata/39734273/config/grid/");
    auto library = openLibrary();
    QTRY_VERIFY_WITH_TIMEOUT(library->store().snapshot().steamQueued.isEmpty(), 5000);
    QStringList trashed;
    library->setTrash(fakeTrash(&trashed));
    quint32 appId = 0;
    const QString game = addUninstallGame(*library, &appId);
    const QString id = QString::number(appId);
    const QString folder = m_installs.home + QStringLiteral("/Games/Hollow Knight");
    const QString prefix = root + QStringLiteral("/steamapps/compatdata/") + id;
    QTRY_COMPARE_WITH_TIMEOUT(shortcutIn(vdf, appId).appId, appId, 5000);
    QTRY_VERIFY_WITH_TIMEOUT(QFileInfo::exists(grid + id + QStringLiteral("_hero.png")), 5000);
    QTRY_VERIFY_WITH_TIMEOUT(library->store().snapshot().steamQueued.isEmpty(), 5000);
    const QList<SteamShortcut> others = shortcutsIn(vdf).mid(0, 5);

    // What the dialog shows, the sizes added up in the background.
    QSignalSpy sizes(library.get(), &Library::uninstallSizes);
    const QVariantMap choices = library->uninstallChoices(game);
    QCOMPARE(choices.value("installDir").toString(), QStringLiteral("~/Games/Hollow Knight"));
    QCOMPARE(choices.value("installDirRefused").toString(), QString());
    QCOMPARE(choices.value("prefix").toString(), QStringLiteral("~/.local/share/Steam/steamapps/compatdata/") + id);
    QTRY_COMPARE_WITH_TIMEOUT(sizes.count(), 1, 5000);
    QCOMPARE(sizes.first().at(0).toString(), game);
    QVERIFY(!sizes.first().at(1).toString().isEmpty());
    QVERIFY(!sizes.first().at(2).toString().isEmpty());

    // Only its prefix deleted: a reset. To the trash; the game stays installed, its shortcut in
    // Steam, nothing queued.
    const QByteArray before = readFile(gamePath(game));
    QVariantMap r = library->uninstallProtonGame(game, false, true, false, QStringLiteral("keep"), false);
    QCOMPARE(r.value("error").toString(), QString());
    QVERIFY(!r.contains("trashless"));
    QCOMPARE(trashed, QStringList{prefix});
    QCOMPARE(readFile(gamePath(game)), before);
    QTest::qWait(300);
    QVERIFY(library->store().snapshot().steamQueued.isEmpty());
    QCOMPARE(shortcutIn(vdf, appId).appId, appId);
    // Nothing left to delete there: the dialog doesn't offer it.
    QCOMPARE(library->uninstallChoices(game).value("prefix").toString(), QString());

    // Kept, with its files deleted (Steam made a new prefix meanwhile): not installed, and its
    // shortcut leaves Steam. Its Proton version stays for when it's located again.
    writeFile(prefix + QStringLiteral("/pfx/user.reg"), "settings");
    r = library->uninstallProtonGame(game, true, false, false, QStringLiteral("keep"), false);
    QCOMPARE(r.value("error").toString(), QString());
    QCOMPARE(trashed, (QStringList{prefix, folder}));
    QVERIFY(readFile(gamePath(game)).contains("installed = false\n"));
    QVERIFY(!readFile(gamePath(game)).contains("hidden"));
    QTRY_COMPARE_WITH_TIMEOUT(shortcutIn(vdf, appId).appId, 0u, 5000);
    QVERIFY(readFile(config).contains("\"" + id.toUtf8() + "\""));
    QVERIFY(QFileInfo::exists(m_data + QStringLiteral("/art/") + game));
    QTRY_VERIFY_WITH_TIMEOUT(library->game(game) && library->game(game)->uninstalled, 5000);

    // Uninstalled and kept, its prefix left behind: still offered, and deleted; its files
    // aren't there to delete.
    const QVariantMap left = library->uninstallChoices(game);
    QCOMPARE(left.value("installDir").toString(), QString());
    QCOMPARE(left.value("prefix").toString(), QStringLiteral("~/.local/share/Steam/steamapps/compatdata/") + id);
    QCOMPARE(library->uninstallProtonGame(game, true, false, false, QStringLiteral("keep"), false).value("error").toString(),
             QStringLiteral("Hollow Knight's files are gone already"));
    r = library->uninstallProtonGame(game, false, true, false, QStringLiteral("keep"), false);
    QCOMPARE(r.value("error").toString(), QString());
    QCOMPARE(trashed, (QStringList{prefix, folder, prefix}));
    QVERIFY(readFile(gamePath(game)).contains("installed = false\n"));

    // Installed again and located, then removed with its files: its file, art, shortcut,
    // CompatToolMapping entry and the launcher's grid files go. The other shortcuts stay as
    // they were.
    writeFile(folder + QStringLiteral("/hollow_knight.exe"), "MZ");
    QCOMPARE(library->locateProgram(game, folder + QStringLiteral("/hollow_knight.exe")), QString());
    QTRY_COMPARE_WITH_TIMEOUT(shortcutIn(vdf, appId).appId, appId, 5000);
    r = library->uninstallProtonGame(game, true, false, true, QStringLiteral("remove"), false);
    QCOMPARE(r.value("error").toString(), QString());
    QCOMPARE(trashed, (QStringList{prefix, folder, prefix, folder}));
    QVERIFY(!QFileInfo::exists(folder));
    QVERIFY(!QFileInfo::exists(gamePath(game)));
    QVERIFY(!QFileInfo::exists(m_data + QStringLiteral("/art/") + game));
    QTRY_COMPARE_WITH_TIMEOUT(shortcutIn(vdf, appId).appId, 0u, 5000);
    QTRY_VERIFY_WITH_TIMEOUT(!readFile(config).contains("\"" + id.toUtf8() + "\""), 5000);
    QTRY_VERIFY_WITH_TIMEOUT(!QFileInfo::exists(grid + id + QStringLiteral("_hero.png")), 5000);
    QCOMPARE(shortcutsIn(vdf), others);
    QTRY_VERIFY_WITH_TIMEOUT(library->store().snapshot().steamQueued.isEmpty(), 5000);
    const auto record = [&] {
        QStringList warnings;
        return SteamSync::readRecord(SteamSync::recordPath(m_state, AppPaths::dataDirKey(m_data)), libraryId(), &warnings);
    };
    QTRY_VERIFY_WITH_TIMEOUT(!record().grid.contains(appId), 5000);
    QVERIFY(record().removed.isEmpty());
    QVERIFY(!record().shortcuts.contains(appId));
}

void LibraryUninstallTest::uninstallHidesOrShows()
{
    const QString root = installFakeSteam();
    const QString vdf = root + kShortcutsVdf;
    auto library = openLibrary();
    QStringList trashed;
    library->setTrash(fakeTrash(&trashed));
    quint32 appId = 0;
    const QString game = addUninstallGame(*library, &appId);
    QTRY_COMPARE_WITH_TIMEOUT(shortcutIn(vdf, appId).appId, appId, 5000);
    QTRY_VERIFY_WITH_TIMEOUT(library->store().snapshot().steamQueued.isEmpty(), 5000);
    const QByteArray before = readFile(gamePath(game));
    const SteamShortcut shortcut = shortcutIn(vdf, appId);
    const auto settled = [&] {
        library->store().waitForIdle();
        QCoreApplication::processEvents();
        QTest::qWait(300);
    };

    // Hide: in the launcher only. Its shortcut stays in Steam as it was, nothing queued.
    QCOMPARE(library->removeGame(game), QString());
    settled();
    QCOMPARE(readFile(gamePath(game)), before + "hidden = true\n");
    QVERIFY(library->store().snapshot().steamQueued.isEmpty());
    QCOMPARE(shortcutIn(vdf, appId), shortcut);

    // Uninstall, "Keep it": unhidden, still installed. "Keep it, hidden" with nothing deleted:
    // hidden again. The same again changes nothing.
    QCOMPARE(library->uninstallProtonGame(game, false, false, false, QStringLiteral("keep"), false).value("error").toString(),
             QString());
    QCOMPARE(readFile(gamePath(game)), before);
    QCOMPARE(library->uninstallProtonGame(game, false, false, false, QStringLiteral("hide"), false).value("error").toString(),
             QString());
    QCOMPARE(readFile(gamePath(game)), before + "hidden = true\n");
    QCOMPARE(library->uninstallProtonGame(game, false, false, false, QStringLiteral("hide"), false).value("error").toString(),
             QString());
    QCOMPARE(readFile(gamePath(game)), before + "hidden = true\n");
    settled();
    QVERIFY(library->store().snapshot().steamQueued.isEmpty());
    QCOMPARE(shortcutIn(vdf, appId), shortcut);
    QVERIFY(trashed.isEmpty());

    // Its shortcut removed from Steam, nothing deleted, kept hidden: hidden and not installed;
    // the shortcut leaves Steam.
    const QVariantMap r = library->uninstallProtonGame(game, false, false, true, QStringLiteral("hide"), false);
    QCOMPARE(r.value("error").toString(), QString());
    QVERIFY(trashed.isEmpty());
    QVERIFY(QFileInfo::exists(m_installs.home + QStringLiteral("/Games/Hollow Knight/hollow_knight.exe")));
    QVERIFY(readFile(gamePath(game)).contains("hidden = true\n"));
    QVERIFY(readFile(gamePath(game)).contains("installed = false\n"));
    QTRY_COMPARE_WITH_TIMEOUT(shortcutIn(vdf, appId).appId, 0u, 5000);

    // Not an action.
    QCOMPARE(library->uninstallProtonGame(game, false, false, false, QStringLiteral("delete"), false).value("error").toString(),
             QStringLiteral("Unknown action: delete"));
}

void LibraryUninstallTest::uninstallForgets()
{
    const QString root = installFakeSteam();
    const QString vdf = root + kShortcutsVdf;
    const QString config = root + QStringLiteral("/config/config.vdf");
    const QString hero = root + QStringLiteral("/userdata/39734273/config/grid/%1_hero.png");
    auto library = openLibrary();
    QStringList trashed;
    library->setTrash(fakeTrash(&trashed));
    quint32 appId = 0;
    const QString game = addUninstallGame(*library, &appId);
    QTRY_COMPARE_WITH_TIMEOUT(shortcutIn(vdf, appId).appId, appId, 5000);
    QTRY_VERIFY_WITH_TIMEOUT(QFileInfo::exists(hero.arg(appId)), 5000);
    QTRY_VERIFY_WITH_TIMEOUT(library->store().snapshot().steamQueued.isEmpty(), 5000);
    const QByteArray vdfBefore = readFile(vdf);
    const QByteArray configBefore = readFile(config);
    const QByteArray heroBefore = readFile(hero.arg(appId));
    const auto record = [&] {
        QStringList warnings;
        return SteamSync::readRecord(SteamSync::recordPath(m_state, AppPaths::dataDirKey(m_data)), libraryId(), &warnings);
    };
    QTRY_VERIFY_WITH_TIMEOUT(record().grid.contains(appId), 5000);

    // Removed from the launcher only: its file and art go; Steam keeps the shortcut, its
    // Proton version and the art the launcher wrote, which is Steam's now. Nothing queued.
    QCOMPARE(library->uninstallProtonGame(game, false, false, false, QStringLiteral("remove"), false).value("error").toString(),
             QString());
    QVERIFY(!QFileInfo::exists(gamePath(game)));
    QVERIFY(!QFileInfo::exists(m_data + QStringLiteral("/art/") + game));
    QVERIFY(trashed.isEmpty());
    library->store().waitForIdle();
    QCoreApplication::processEvents();
    QTest::qWait(300);
    QVERIFY(library->store().snapshot().steamQueued.isEmpty());
    QVERIFY(library->steamWaiting().isEmpty());
    QCOMPARE(readFile(vdf), vdfBefore);
    QCOMPARE(readFile(config), configBefore);
    QCOMPARE(readFile(hero.arg(appId)), heroBefore);
    QVERIFY(!record().shortcuts.contains(appId));
    QVERIFY(!record().grid.contains(appId));
    QVERIFY(record().removed.isEmpty());

    // "Add existing shortcuts…" offers it again, and adding it leaves Steam as it is.
    const QVariantList shortcuts = library->steamShortcuts();
    QVERIFY(std::any_of(shortcuts.cbegin(), shortcuts.cend(),
                        [&](const QVariant &s) { return s.toMap().value("appId").toLongLong() == appId; }));
    QCOMPARE(library->addSteamShortcuts({qint64(appId)}), QString());
    library->store().waitForIdle();
    QCoreApplication::processEvents();
    QTest::qWait(300);
    QVERIFY(library->store().snapshot().steamQueued.isEmpty());
    QCOMPARE(readFile(vdf), vdfBefore);
    QVERIFY(record().shortcuts.contains(appId));
}

void LibraryUninstallTest::uninstallWhileSteamRuns()
{
    const QString root = installFakeSteam();
    const QString vdf = root + kShortcutsVdf;
    auto library = openLibrary();
    QTRY_VERIFY_WITH_TIMEOUT(library->store().snapshot().steamQueued.isEmpty(), 5000);
    QStringList trashed;
    library->setTrash(fakeTrash(&trashed));
    quint32 appId = 0;
    const QString game = addUninstallGame(*library, &appId);
    QTRY_COMPARE_WITH_TIMEOUT(shortcutIn(vdf, appId).appId, appId, 5000);
    QTRY_VERIFY_WITH_TIMEOUT(library->store().snapshot().steamQueued.isEmpty(), 5000);

    // Removed while Steam runs: the game goes now, its shortcut waits, and the queue says so.
    const auto steam = startFakeSteam(root);
    QCOMPARE(library->uninstallProtonGame(game, false, false, true, QStringLiteral("remove"), false).value("error").toString(),
             QString());
    QVERIFY(!QFileInfo::exists(gamePath(game)));
    QVERIFY(trashed.isEmpty());
    QVariantMap entry;
    for (const QVariant &g : library->steam().value("queue").toList()) {
        if (g.toMap().value("id") == SteamSync::removedKey(appId))
            entry = g.toMap();
    }
    QCOMPARE(entry.value("title").toString(), QStringLiteral("Hollow Knight (removed)"));
    const QVariantList changes = entry.value("changes").toList();
    QCOMPARE(changes.size(), 2);
    QCOMPARE(changes.at(0).toMap().value("to").toString(), QStringLiteral("Removed"));
    QCOMPARE(changes.at(1).toMap().value("name").toString(), QStringLiteral("Proton"));
    // The game page's Steam chip counts it though no game is left to select.
    QTRY_COMPARE_WITH_TIMEOUT(library->steamWaiting().value(0).toMap().value("title").toString(),
                              QStringLiteral("Hollow Knight (removed)"), 5000);
    QTest::qWait(300);
    QCOMPARE(shortcutIn(vdf, appId).appId, appId);
    // Still waiting after a restart of the launcher: it's in the record.
    library.reset();
    library = openLibrary();
    QVERIFY(library->store().snapshot().steamQueued.contains(SteamSync::removedKey(appId)));

    // Steam closes: gone, and forgotten.
    steam->kill();
    steam->waitForFinished();
    QTRY_COMPARE_WITH_TIMEOUT(shortcutIn(vdf, appId).appId, 0u, 5000);
    QTRY_VERIFY_WITH_TIMEOUT(library->store().snapshot().steamQueued.isEmpty(), 5000);
    QCOMPARE(library->store().removedShortcutTitle(appId), QString());
    QTRY_VERIFY_WITH_TIMEOUT(library->steamWaiting().isEmpty(), 5000);
}

void LibraryUninstallTest::steamUndoUninstalls()
{
    const QString root = installFakeSteam();
    const QString vdf = root + kShortcutsVdf;
    auto library = openLibrary();
    QStringList trashed;
    library->setTrash(fakeTrash(&trashed));
    quint32 appId = 0;
    const QString game = addUninstallGame(*library, &appId);
    QTRY_COMPARE_WITH_TIMEOUT(shortcutIn(vdf, appId).appId, appId, 5000);
    QTRY_VERIFY_WITH_TIMEOUT(library->store().snapshot().steamQueued.isEmpty(), 5000);
    const auto steam = startFakeSteam(root);

    // Uninstalled but kept: undone, it's installed again (its files gone: "Game not found")
    // and its shortcut stays.
    QCOMPARE(library->uninstallProtonGame(game, true, false, false, QStringLiteral("keep"), false).value("error").toString(),
             QString());
    QTRY_VERIFY_WITH_TIMEOUT(library->store().snapshot().steamQueued.contains(game), 5000);
    QCOMPARE(library->undoSteamEdits(game), QString());
    QTRY_VERIFY_WITH_TIMEOUT(!library->store().snapshot().steamQueued.contains(game), 5000);
    QVERIFY(!readFile(gamePath(game)).contains("installed"));

    // Removed: undone, its shortcut stays in Steam, out of the library.
    QCOMPARE(library->uninstallProtonGame(game, false, false, true, QStringLiteral("remove"), false).value("error").toString(),
             QString());
    const QString key = SteamSync::removedKey(appId);
    QTRY_VERIFY_WITH_TIMEOUT(library->store().snapshot().steamQueued.contains(key), 5000);
    QCOMPARE(library->undoSteamEdits(key), QString());
    QVERIFY(library->steamWaiting().isEmpty());
    QCOMPARE(library->store().removedShortcutTitle(appId), QString());
    steam->kill();
    steam->waitForFinished();
    QTest::qWait(300);
    QCOMPARE(shortcutIn(vdf, appId).appId, appId);
    QVERIFY(library->store().snapshot().steamQueued.isEmpty());
}

void LibraryUninstallTest::uninstallWithoutTrash()
{
    const QString root = installFakeSteam();
    auto library = openLibrary();
    QStringList trashed;
    library->setTrash(fakeTrash(&trashed, false));
    quint32 appId = 0;
    const QString game = addUninstallGame(*library, &appId);
    const QString folder = m_installs.home + QStringLiteral("/Games/Hollow Knight");
    const QByteArray before = readFile(gamePath(game));

    // No trash for it: asked again, nothing done yet.
    QVariantMap r = library->uninstallProtonGame(game, true, false, false, QStringLiteral("keep"), false);
    QCOMPARE(r.value("trashless").toString(), QStringLiteral("~/Games/Hollow Knight"));
    QCOMPARE(r.value("error").toString(), QString());
    QVERIFY(QFileInfo::exists(folder + QStringLiteral("/save.dat")));
    QCOMPARE(readFile(gamePath(game)), before);

    // Confirmed: deleted for good.
    r = library->uninstallProtonGame(game, true, false, false, QStringLiteral("keep"), true);
    QCOMPARE(r.value("error").toString(), QString());
    QVERIFY(!r.contains("trashless"));
    QVERIFY(!QFileInfo::exists(folder));
    QVERIFY(readFile(gamePath(game)).contains("installed = false\n"));
    QVERIFY(QFileInfo::exists(root + QStringLiteral("/steamapps/compatdata/%1").arg(appId)));
}

void LibraryUninstallTest::uninstallStopsAtAFolder()
{
    const QString root = installFakeSteam();
    auto library = openLibrary();
    QStringList trashed;
    library->setTrash(fakeTrash(&trashed));
    quint32 appId = 0;
    const QString game = addUninstallGame(*library, &appId);
    const QString games = m_installs.home + QStringLiteral("/Games");
    const QByteArray before = readFile(gamePath(game));

    // The game's folder can't be moved: the prefix after it isn't touched, nor the game.
    QFile::setPermissions(games, QFile::ReadOwner | QFile::ExeOwner);
    const QVariantMap r = library->uninstallProtonGame(game, true, true, true, QStringLiteral("remove"), true);
    QFile::setPermissions(games, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner);
    QVERIFY(r.value("error").toString().contains(QStringLiteral("read-only")));
    QVERIFY(trashed.isEmpty());
    QVERIFY(QFileInfo::exists(games + QStringLiteral("/Hollow Knight/save.dat")));
    QVERIFY(QFileInfo::exists(root + QStringLiteral("/steamapps/compatdata/%1/pfx/user.reg").arg(appId)));
    QCOMPARE(readFile(gamePath(game)), before);
}

void LibraryUninstallTest::uninstallRefused()
{
    installFakeSteam();
    auto library = openLibrary();
    QStringList trashed;
    library->setTrash(fakeTrash(&trashed));
    quint32 appId = 0;
    const QString game = addUninstallGame(*library, &appId);

    // Its install folder the home folder: not offered, and refused if asked.
    QCOMPARE(library->setGameValue(game, QStringLiteral("install_dir"), m_installs.home), QString());
    library->store().waitForIdle();
    QCoreApplication::processEvents();
    const QVariantMap choices = library->uninstallChoices(game);
    QCOMPARE(choices.value("installDirRefused").toString(), QStringLiteral("That's your home folder"));
    QVERIFY(!choices.value("prefix").toString().isEmpty());
    const QVariantMap r = library->uninstallProtonGame(game, true, true, false, QStringLiteral("keep"), false);
    QCOMPARE(r.value("error").toString(), QStringLiteral("The game's files can't be deleted: That's your home folder"));
    QVERIFY(trashed.isEmpty());
    QVERIFY(!readFile(gamePath(game)).contains("installed"));

    // Only Proton games.
    QCOMPARE(library->uninstallProtonGame(QStringLiteral("switch-pikmin-4"), false, false, true, QStringLiteral("remove"),
                                          false)
                 .value("error").toString(),
             QStringLiteral("Only Proton and Linux games are uninstalled here"));
    QVERIFY(QFileInfo::exists(gamePath(QStringLiteral("switch-pikmin-4"))));
}

void LibraryUninstallTest::uninstallReadOnly()
{
    const QString root = installFakeSteam();
    const QString folder = m_installs.home + QStringLiteral("/Games/Diablo IV");
    writeFile(folder + QStringLiteral("/Diablo IV Launcher.exe"), "MZ");
    writeFile(root + QStringLiteral("/steamapps/compatdata/3110441846/pfx/user.reg"), "settings");
    writeFile(m_data + QStringLiteral("/library/library.toml"), "version = 99\n");
    auto library = openLibrary();
    QVERIFY(library->readOnly());
    QStringList trashed;
    library->setTrash(fakeTrash(&trashed));
    const QString diablo = QStringLiteral("proton-diablo-iv");
    const QByteArray before = readFile(gamePath(diablo));
    QCOMPARE(library->uninstallProtonGame(diablo, true, true, true, QStringLiteral("remove"), true).value("error").toString(),
             QStringLiteral("The library is read-only; nothing was deleted"));
    QVERIFY(trashed.isEmpty());
    QVERIFY(QFileInfo::exists(folder + QStringLiteral("/Diablo IV Launcher.exe")));
    QVERIFY(QFileInfo::exists(root + QStringLiteral("/steamapps/compatdata/3110441846/pfx/user.reg")));
    QCOMPARE(readFile(gamePath(diablo)), before);
}

void LibraryUninstallTest::uninstallRefusals()
{
    const QString home = m_installs.home;
    const QString steam = home + QStringLiteral("/.local/share/Steam");
    const QString library = m_dir->filePath(QStringLiteral("disk/SteamLibrary"));
    for (const QString &dir : {steam + QStringLiteral("/steamapps/common/Game"), steam + QStringLiteral("/userdata"),
                               library + QStringLiteral("/steamapps/common/Game"), library + QStringLiteral("/Mine"),
                               home + QStringLiteral("/Games/Hollow Knight/bin"), home + QStringLiteral("/Documents"),
                               home + QStringLiteral("/Games/Other")})
        QDir().mkpath(dir);
    QFile::link(home + QStringLiteral("/Games/Other"), home + QStringLiteral("/Games/Link"));
    Uninstall::Places places;
    places.home = home;
    places.dataDir = home + QStringLiteral("/.local/share/lattis");
    places.steamRoot = steam;
    places.steamLibraries = {steam, library};
    places.userDirs = {home + QStringLiteral("/Documents")};
    places.otherGames = {{QStringLiteral("Other"), home + QStringLiteral("/Games/Other/other.exe")}};

    const auto refusal = [&](const QString &folder) { return Uninstall::refusal(folder, places); };
    // Allowed: a game's folder below the home folder or in a library, a prefix.
    QCOMPARE(refusal(home + QStringLiteral("/Games/Hollow Knight")), QString());
    QCOMPARE(refusal(steam + QStringLiteral("/steamapps/common/Game")), QString());
    QCOMPARE(refusal(steam + QStringLiteral("/steamapps/compatdata/123")), QString());
    QCOMPARE(refusal(library + QStringLiteral("/steamapps/common/Game")), QString());
    QCOMPARE(refusal(library + QStringLiteral("/Mine")), QString());
    // Refused.
    QCOMPARE(refusal(QString()), QStringLiteral("It has no folder"));
    QCOMPARE(refusal(QStringLiteral("/")), QStringLiteral("That's the root folder"));
    QCOMPARE(refusal(home), QStringLiteral("That's your home folder"));
    QCOMPARE(refusal(home + QStringLiteral("/")), QStringLiteral("That's your home folder"));
    QCOMPARE(refusal(home + QStringLiteral("/Documents")), QStringLiteral("That's your Documents folder"));
    QCOMPARE(refusal(home + QStringLiteral("/.local")), QStringLiteral("It holds the launcher's library"));
    QCOMPARE(refusal(places.dataDir), QStringLiteral("It holds the launcher's library"));
    QCOMPARE(refusal(steam), QStringLiteral("It holds Steam"));
    QCOMPARE(refusal(library), QStringLiteral("It holds a Steam library"));
    QCOMPARE(refusal(m_dir->filePath(QStringLiteral("disk"))), QStringLiteral("It holds a Steam library"));
    QCOMPARE(refusal(steam + QStringLiteral("/steamapps")), QStringLiteral("It's one of Steam's own folders"));
    QCOMPARE(refusal(steam + QStringLiteral("/steamapps/common")), QStringLiteral("It's one of Steam's own folders"));
    QCOMPARE(refusal(library + QStringLiteral("/steamapps/compatdata")), QStringLiteral("It's one of Steam's own folders"));
    QCOMPARE(refusal(steam + QStringLiteral("/userdata")), QStringLiteral("It's one of Steam's own folders"));
    QCOMPARE(refusal(home + QStringLiteral("/Games")), QStringLiteral("It holds Other's files too"));
    QCOMPARE(refusal(home + QStringLiteral("/Games/Link")), QStringLiteral("It's a link to another folder"));
    QCOMPARE(refusal(m_dir->filePath(QStringLiteral("elsewhere"))),
             QStringLiteral("It's outside your home folder and Steam's libraries"));
    QCOMPARE(refusal(QStringLiteral("/usr")), QStringLiteral("It's outside your home folder and Steam's libraries"));
}

void LibraryUninstallTest::uninstallLinuxGame()
{
    auto library = openLibrary();
    QStringList trashed;
    library->setTrash(fakeTrash(&trashed));
    const QString folder = m_installs.home + QStringLiteral("/Games/SuperTux");
    const QString script = folder + QStringLiteral("/bin/run.sh");
    writeFile(script, "#!/bin/sh\n");
    writeFile(folder + QStringLiteral("/data/levels.dat"), QByteArray(4096, 'x'));
    const QVariantMap added = library->addGame(script, QStringLiteral("linux"), QStringLiteral("SuperTux"),
                                               {{"makeExecutable", true}}, {});
    const QString game = added.value("id").toString();
    QCOMPARE(added.value("error").toString(), QString());
    QTRY_VERIFY_WITH_TIMEOUT(library->game(game) && library->game(game)->installed, 5000);

    // Only its files: no prefix, and its size added up.
    QSignalSpy sizes(library.get(), &Library::uninstallSizes);
    const QVariantMap choices = library->uninstallChoices(game);
    QCOMPARE(choices.value("installDir").toString(), QStringLiteral("~/Games/SuperTux"));
    QCOMPARE(choices.value("installDirRefused").toString(), QString());
    QCOMPARE(choices.value("prefix").toString(), QString());
    QTRY_COMPARE_WITH_TIMEOUT(sizes.count(), 1, 5000);
    QVERIFY(!sizes.first().at(1).toString().isEmpty());
    QCOMPARE(sizes.first().at(2).toString(), QString());
    // A prefix or a shortcut: not a Linux game's.
    QVERIFY(!library->uninstallProtonGame(game, false, true, false, QStringLiteral("keep"), false)
                 .value("error").toString().isEmpty());
    QVERIFY(!library->uninstallProtonGame(game, true, false, true, QStringLiteral("keep"), false)
                 .value("error").toString().isEmpty());
    QVERIFY(trashed.isEmpty());

    // Kept, hidden, its files deleted: not installed.
    QVariantMap r = library->uninstallProtonGame(game, true, false, false, QStringLiteral("hide"), false);
    QCOMPARE(r.value("error").toString(), QString());
    QCOMPARE(trashed, QStringList{folder});
    QVERIFY(readFile(gamePath(game)).contains("installed = false\n"));
    QVERIFY(readFile(gamePath(game)).contains("hidden = true\n"));
    library->setShowHidden(true);
    QTRY_VERIFY_WITH_TIMEOUT(library->game(game) && library->game(game)->uninstalled, 5000);
    QVERIFY(!library->gameAt(library->rowOfGame(game)).value("gameNotFound").toBool());
    // Its files are gone: nothing left to offer, nor to delete.
    QCOMPARE(library->uninstallChoices(game).value("installDir").toString(), QString());
    QVERIFY(!library->uninstallProtonGame(game, true, false, false, QStringLiteral("keep"), false)
                 .value("error").toString().isEmpty());

    // Located again: installed comes back.
    const QString moved = m_installs.home + QStringLiteral("/Games/SuperTux 2/run.sh");
    writeFile(moved, "#!/bin/sh\n");
    QCOMPARE(library->locateProgram(game, moved), QString());
    QVERIFY(!readFile(gamePath(game)).contains("installed"));
    QTRY_VERIFY_WITH_TIMEOUT(library->game(game) && library->game(game)->installed, 5000);

    // Removed, files kept: its game file goes, and nothing is deleted.
    r = library->uninstallProtonGame(game, false, false, false, QStringLiteral("remove"), false);
    QCOMPARE(r.value("error").toString(), QString());
    QCOMPARE(trashed, QStringList{folder});
    QVERIFY(!QFileInfo::exists(gamePath(game)));
    QVERIFY(QFileInfo::exists(moved));
    QTRY_VERIFY_WITH_TIMEOUT(!library->game(game), 5000);
    QVERIFY(!library->store().snapshot().programFiles.contains(game));

    // A game whose install folder is the home folder: refused, nothing moved.
    const QString loose = m_installs.home + QStringLiteral("/tool.sh");
    writeFile(loose, "#!/bin/sh\n");
    const QString looseId = library->addGame(loose, QStringLiteral("linux"), QStringLiteral("Tool"),
                                             {{"makeExecutable", true}}, {}).value("id").toString();
    QVERIFY(!looseId.isEmpty());
    QVERIFY(!library->uninstallChoices(looseId).value("installDirRefused").toString().isEmpty());
    QVERIFY(library->uninstallProtonGame(looseId, true, false, false, QStringLiteral("remove"), false)
                .value("error").toString().contains(QStringLiteral("can't be deleted")));
    QVERIFY(QFileInfo::exists(gamePath(looseId)));
    QCOMPARE(trashed, QStringList{folder});
}

void LibraryUninstallTest::uninstallAppImage()
{
    QByteArray appImage = elf(2, 62);
    appImage.replace(8, 3, QByteArray("AI\x02", 3));
    const QString folder = m_installs.home + QStringLiteral("/AppImages");
    const QString dusklight = folder + QStringLiteral("/dusklight.appimage");
    const QString other = folder + QStringLiteral("/helium.appimage");
    for (const QString &path : {dusklight, other}) {
        writeFile(path, appImage);
        QFile(path).setPermissions(QFile(path).permissions() | QFileDevice::ExeOwner);
    }
    // As "+" wrote it before: the AppImage's folder as the install folder.
    writeFile(gamePath(QStringLiteral("linux-dusklight")),
              "title = \"Dusklight\"\nplatform = \"linux\"\nexe = \"~/AppImages/dusklight.appimage\"\n"
              "install_dir = \"~/AppImages\"\n");
    auto library = openLibrary();
    QStringList trashed;
    library->setTrash(fakeTrash(&trashed));
    QTRY_VERIFY_WITH_TIMEOUT(library->game("linux-dusklight") && library->game("linux-dusklight")->installed, 5000);

    // Its files are the AppImage alone, with the AppImage's size.
    QVERIFY(library->store().snapshot().programFiles.value("linux-dusklight").singleFile);
    QSignalSpy sizes(library.get(), &Library::uninstallSizes);
    const QVariantMap choices = library->uninstallChoices("linux-dusklight");
    QCOMPARE(choices.value("installDir").toString(), QStringLiteral("~/AppImages/dusklight.appimage"));
    QCOMPARE(choices.value("installDirRefused").toString(), QString());
    QTRY_COMPARE_WITH_TIMEOUT(sizes.count(), 1, 5000);
    QCOMPARE(sizes.first().at(1).toString(),
             QLocale().formattedDataSize(appImage.size(), 1, QLocale::DataSizeTraditionalFormat));
    QVariantMap r = library->uninstallProtonGame("linux-dusklight", true, false, false, QStringLiteral("remove"), false);
    QCOMPARE(r.value("error").toString(), QString());
    QCOMPARE(trashed, QStringList{dusklight});
    QVERIFY(QFileInfo::exists(other));

    // "+" gives an AppImage no install folder, and its title from its own name.
    const QVariantMap pick = library->inspectRom(other);
    QCOMPARE(pick.value("installDir").toString(), QString());
    QCOMPARE(pick.value("title").toString(), QStringLiteral("Helium"));

    // A folder holding an emulator's program isn't deleted with a game in it.
    QCOMPARE(library->setEmulatorValue("ryubing", "path", "~/AppImages/helium.appimage"), QString());
    QTRY_COMPARE_WITH_TIMEOUT(library->store().snapshot().installs.value("ryubing").program, other, 5000);
    const QString script = folder + QStringLiteral("/start.sh");
    writeFile(script, "#!/bin/sh\n");
    const QString scriptId = library->addGame(script, QStringLiteral("linux"), QStringLiteral("Start"),
                                              {{"makeExecutable", true}}, {}).value("id").toString();
    QTRY_VERIFY_WITH_TIMEOUT(library->game(scriptId) && library->game(scriptId)->installed, 5000);
    QVERIFY(library->uninstallChoices(scriptId).value("installDirRefused").toString().contains(QStringLiteral("Ryubing")));
    QVERIFY(library->uninstallProtonGame(scriptId, true, false, false, QStringLiteral("remove"), false)
                .value("error").toString().contains(QStringLiteral("can't be deleted")));
    QCOMPARE(trashed, QStringList{dusklight});
}

QTEST_GUILESS_MAIN(LibraryUninstallTest)

#include "library_uninstall_test.moc"
