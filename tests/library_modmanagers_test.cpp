#include "librarytestbase.h"

#include "binaryvdf.h"

class LibraryModManagersTest : public LibraryTestBase
{
    Q_OBJECT

private slots:
    void parsing();
    void parseProblems();
    void linkedGamesAreMarkedMissing();
    void loadsFromFiles();
    void badFilesAreProblems();
    void addWritesUniqueFiles();
    void editingKeepsTheRest();
    void removedGameDropsOut();
    void hiddenGameStaysLinked();
    void survivesTheCache();
    void prefixOptions();
    void shortcutInSharedPrefix();
    void shortcutInOwnPrefix();
    void deletedInSteam();
    void adoptedShortcut();
    void settingsPage();
    void removeOptions();
    void removeDeletesWhatIsAsked();
    void removeLeavesShortcut();

private:
    QString shortcutsPath(const QString &root) const
    {
        return root + QStringLiteral("/userdata/39734273/config/shortcuts.vdf");
    }
    QList<SteamShortcut> shortcutsIn(const QString &root) const
    {
        return SteamShortcuts::read(BinaryVdf::parse(readFile(shortcutsPath(root))).root);
    }
    static std::optional<SteamShortcut> shortcutNamed(const QList<SteamShortcut> &all, const QString &name)
    {
        for (const SteamShortcut &s : all) {
            if (s.name == name)
                return s;
        }
        return std::nullopt;
    }
    QString managerPath(const QString &id) const
    {
        return m_data + QStringLiteral("/library/mod-managers/") + id + QStringLiteral(".toml");
    }
    void writeMo2() const
    {
        writeFile(managerPath("mo2"), "name = \"Mod Organizer 2\"\nexe = \"/mods/MO2/ModOrganizer.exe\"\n"
                                      "start_dir = \"/mods/MO2\"\ngames = [\"linux-supertuxkart\", \"steam-730\"]\n"
                                      "prefix = \"game\"\nshortcut = 3193880877\n");
    }
};

void LibraryModManagersTest::parsing()
{
    const auto r = ModManagers::parse("name = \"MO2\"\nexe = \"/m/MO2.exe\"\nlaunch_options = \"A=1 %command%\"\n"
                                      "games = [\"a\", \"b\"]\nprefix = \"own\"\nproton = \"GE-Proton10-4\"\n"
                                      "shortcut = 3193880877\n",
                                      "mo2.toml");
    QVERIFY(r.ok);
    QVERIFY2(r.warnings.isEmpty(), qPrintable(r.warnings.join(u'\n')));
    QCOMPARE(r.value.name, QStringLiteral("MO2"));
    QCOMPARE(r.value.games, (QStringList{"a", "b"}));
    QCOMPARE(r.value.prefix, QStringLiteral("own"));
    QCOMPARE(r.value.shortcut, 3193880877u);
    QVERIFY(r.value.isProton());

    ModManager native;
    native.exe = QStringLiteral("/opt/tool/tool.AppImage");
    QVERIFY(!native.isProton());
}

void LibraryModManagersTest::parseProblems()
{
    const auto r = ModManagers::parse("name = 3\nexe = \"/m/x.exe\"\ngame = []\nprefix = \"mine\"\n"
                                      "shortcut = 0\ngames = \"a\"\n",
                                      "mm.toml");
    QVERIFY(r.ok);
    const QString all = r.warnings.join(u'\n');
    QVERIFY2(all.contains("mm.toml:1: name must be text in quotes"), qPrintable(all));
    QVERIFY2(all.contains("unknown key game — did you mean \"name\"?"), qPrintable(all));
    QVERIFY2(all.contains("prefix must be one of"), qPrintable(all));
    QVERIFY2(all.contains("shortcut must be a whole number"), qPrintable(all));
    QVERIFY2(all.contains("games must be a list"), qPrintable(all));
    QVERIFY(!ModManagers::parse("name = ", "mm.toml").ok);
}

void LibraryModManagersTest::linkedGamesAreMarkedMissing()
{
    Game fine;
    fine.id = QStringLiteral("linux-a");
    fine.title = QStringLiteral("A");
    fine.platform = kLinuxPlatform;
    Game hidden = fine;
    hidden.id = QStringLiteral("linux-b");
    hidden.hidden = true;
    Game gone = fine;
    gone.id = QStringLiteral("linux-c");
    gone.installed = false;
    Game rom = fine;
    rom.id = QStringLiteral("switch-d");
    rom.platform = QStringLiteral("switch");
    const QList<Game> all = {fine, hidden, gone, rom};

    ModManager m;
    m.games = {"linux-a", "linux-b", "linux-c", "switch-d", "linux-e"};
    const auto linked = ModManagers::linkedGames(m, [&](const QString &id) -> const Game * {
        for (const Game &g : all) {
            if (g.id == id)
                return &g;
        }
        return nullptr;
    });
    QCOMPARE(linked.size(), 5);
    QVERIFY(!linked.at(0).missing);
    QCOMPARE(linked.at(0).title, QStringLiteral("A"));
    for (int i = 1; i < 5; ++i)
        QVERIFY2(linked.at(i).missing, qPrintable(linked.at(i).id));
    QCOMPARE(linked.at(4).title, QStringLiteral("linux-e"));
}

void LibraryModManagersTest::loadsFromFiles()
{
    writeMo2();
    auto library = openLibrary();
    QVERIFY2(library->problems().isEmpty(), qPrintable(library->problems().join(u'\n')));

    QCOMPARE(library->modManagers().size(), 1);
    const QVariantMap mo2 = library->modManagers().first().toMap();
    QCOMPARE(mo2.value("id").toString(), QStringLiteral("mo2"));
    QCOMPARE(mo2.value("name").toString(), QStringLiteral("Mod Organizer 2"));
    QVERIFY(mo2.value("proton").toBool());
    QCOMPARE(mo2.value("shortcut").toUInt(), 3193880877u);
    QCOMPARE(mo2.value("games").toList().size(), 2);

    QCOMPARE(library->modManagersFor(QStringLiteral("linux-supertuxkart")).size(), 1);
    QVERIFY(library->modManagersFor(QStringLiteral("linux-factorio")).isEmpty());
    // A manager never shows as a game.
    QCOMPARE(library->rowOfGame(QStringLiteral("mo2")), -1);
}

void LibraryModManagersTest::badFilesAreProblems()
{
    writeFile(managerPath("broken"), "name = \n");
    writeFile(managerPath("odd name"), "name = \"Odd\"\nexe = \"/x\"\n");
    writeFile(managerPath("typo"), "nane = \"Typo\"\nexe = \"/x\"\n");
    auto library = openLibrary();
    const QString problems = library->problems().join(u'\n');
    QVERIFY2(problems.contains("library/mod-managers/broken.toml:1"), qPrintable(problems));
    QVERIFY2(problems.contains("odd name.toml: skipped"), qPrintable(problems));
    QVERIFY2(problems.contains("unknown key nane — did you mean \"name\"?"), qPrintable(problems));
    // The typo'd file still loads, named after its file.
    QCOMPARE(library->modManagers().size(), 1);
    QCOMPARE(library->modManagers().first().toMap().value("name").toString(), QStringLiteral("typo"));
}

void LibraryModManagersTest::addWritesUniqueFiles()
{
    auto library = openLibrary();
    const QVariantMap values = {{"name", "Vortex"}, {"exe", "/mods/Vortex/Vortex.exe"},
                                {"games", QStringList{"linux-supertuxkart"}}, {"prefix", "game"}};
    const QVariantMap first = library->addModManager(values);
    QCOMPARE(first.value("id").toString(), QStringLiteral("vortex"));
    QCOMPARE(library->addModManager(values).value("id").toString(), QStringLiteral("vortex-2"));

    // With Steam found, the launcher also writes the shortcut's app ID.
    QString text = QString::fromUtf8(readFile(managerPath("vortex")));
    text.remove(QRegularExpression(QStringLiteral("shortcut = \\d+\n")));
    QCOMPARE(text,
             QStringLiteral("# A mod manager. It's opened from the pages of the games in `games` and from Settings → "
                            "Mod managers.\nname = \"Vortex\"\nexe = \"/mods/Vortex/Vortex.exe\"\n"
                            "games = [\"linux-supertuxkart\"]\nprefix = \"game\"\n"));
    QVERIFY(library->modManagers().size() == 2);

    QVERIFY(library->addModManager({{"exe", "/x"}}).contains("error"));
    QVERIFY(library->addModManager({{"name", "No program"}}).contains("error"));
    QVERIFY(library->addModManager({{"name", "N"}, {"exe", "/x"}, {"prefix", "mine"}}).contains("error"));
    QVERIFY(library->addModManager({{"name", "N"}, {"exe", "/x"}, {"nonsense", "1"}}).contains("error"));

    // A native manager has no Proton keys.
    const QVariantMap native = library->addModManager({{"name", "Tool"}, {"exe", "/opt/tool.AppImage"},
                                                       {"prefix", "own"}, {"shortcut", 5}});
    QVERIFY(!QString::fromUtf8(readFile(managerPath(native.value("id").toString()))).contains("prefix"));
}

void LibraryModManagersTest::editingKeepsTheRest()
{
    writeFile(managerPath("mo2"), "# my notes\nname = \"MO2\"   # the short name\nexe = \"/mods/MO2/ModOrganizer.exe\"\n"
                                  "games = [\"linux-supertuxkart\"]\nlaunch_options = \"A=1 %command%\"\n");
    auto library = openLibrary();

    QVERIFY(library->setModManagerValue("mo2", "name", "Mod Organizer 2").isEmpty());
    QVERIFY(library->setModManagerValue("mo2", "launch_options", "").isEmpty());
    QVERIFY(library->setModManagerValue("mo2", "games", QStringList{"linux-supertuxkart", "linux-factorio"}).isEmpty());
    QString edited = QString::fromUtf8(readFile(managerPath("mo2")));
    edited.remove(QRegularExpression(QStringLiteral("shortcut = \\d+\n")));
    QCOMPARE(edited,
             QStringLiteral("# my notes\nname = \"Mod Organizer 2\"   # the short name\n"
                            "exe = \"/mods/MO2/ModOrganizer.exe\"\n"
                            "games = [\"linux-supertuxkart\", \"linux-factorio\"]\n"));
    QCOMPARE(library->modManagers().first().toMap().value("games").toList().size(), 2);

    // An emptied list stays, so the manager stays listed with no games.
    QVERIFY(library->setModManagerValue("mo2", "games", QStringList()).isEmpty());
    QVERIFY(readFile(managerPath("mo2")).contains("games = []"));
    QVERIFY(library->modManagers().first().toMap().value("games").toList().isEmpty());

    QVERIFY(!library->setModManagerValue("mo2", "prefix", "mine").isEmpty());
    QVERIFY(!library->setModManagerValue("nope", "name", "x").isEmpty());

    // A file with a syntax error isn't written to.
    writeFile(managerPath("mo2"), "name = \n");
    QTRY_VERIFY(!library->problems().isEmpty());
    QVERIFY(!library->setModManagerValue("mo2", "name", "x").isEmpty());
    QCOMPARE(readFile(managerPath("mo2")), QByteArray("name = \n"));
}

void LibraryModManagersTest::removedGameDropsOut()
{
    writeFile(gamePath("linux-removable"), "title = \"Removable\"\nplatform = \"linux\"\nexe = \"/x/removable\"\n");
    writeFile(managerPath("mo2"), "name = \"MO2\"\nexe = \"/m/MO2.exe\"\n"
                                  "games = [\"linux-removable\", \"linux-supertuxkart\"]\n");
    writeFile(managerPath("vortex"), "name = \"Vortex\"\nexe = \"/m/Vortex.exe\"\ngames = [\"linux-removable\"]\n");
    writeFile(managerPath("other"), "name = \"Other\"\nexe = \"/m/Other.exe\"\ngames = [\"linux-supertuxkart\"]\n");
    auto library = openLibrary();

    QVERIFY(library->removeGame(QStringLiteral("linux-removable"), true).isEmpty());
    QVERIFY(readFile(managerPath("mo2")).contains("games = [\"linux-supertuxkart\"]"));
    // Left with no games, it stays listed.
    QVERIFY(readFile(managerPath("vortex")).contains("games = []"));
    QVERIFY(readFile(managerPath("other")).contains("games = [\"linux-supertuxkart\"]"));
    QCOMPARE(library->modManagers().size(), 3);
    QVERIFY(library->modManagersFor(QStringLiteral("linux-removable")).isEmpty());
}

void LibraryModManagersTest::hiddenGameStaysLinked()
{
    // The demo's Counter-Strike 2 is hidden.
    writeFile(managerPath("mo2"), "name = \"MO2\"\nexe = \"/m/MO2.exe\"\ngames = [\"steam-730\", \"linux-gone\"]\n");
    auto library = openLibrary();
    const QVariantList games = library->modManagers().first().toMap().value("games").toList();
    QCOMPARE(games.size(), 2);
    QVERIFY(games.at(0).toMap().value("missing").toBool());
    QVERIFY(games.at(1).toMap().value("missing").toBool());
    QCOMPARE(games.at(1).toMap().value("title").toString(), QStringLiteral("linux-gone"));
    // Nothing is rewritten for a missing game.
    QVERIFY(readFile(managerPath("mo2")).contains("\"steam-730\", \"linux-gone\""));
}

void LibraryModManagersTest::survivesTheCache()
{
    writeMo2();
    {
        auto library = openLibrary();
        library->store().waitForIdle();
    }
    auto again = openLibrary();
    QCOMPARE(again->modManagers().size(), 1);
    QCOMPARE(again->modManagers().first().toMap().value("shortcut").toUInt(), 3193880877u);
}

void LibraryModManagersTest::prefixOptions()
{
    const QString path = QStringLiteral("/steam/compatdata/8000");
    const QString env = QStringLiteral("STEAM_COMPAT_DATA_PATH=\"/steam/compatdata/8000\"");
    QCOMPARE(ModManagers::withPrefix({}, path), env + QStringLiteral(" %command%"));
    QCOMPARE(ModManagers::withPrefix(QStringLiteral("-x"), path), env + QStringLiteral(" %command% -x"));
    QCOMPARE(ModManagers::withPrefix(QStringLiteral("A=1 %command%"), path), env + QStringLiteral(" A=1 %command%"));
    QCOMPARE(ModManagers::withPrefix(QStringLiteral("-x"), {}), QStringLiteral("-x"));
    for (const QString &user : {QString(), QStringLiteral("-x -y"), QStringLiteral("A=1 %command% -z")})
        QCOMPARE(ModManagers::withoutPrefix(ModManagers::withPrefix(user, path)), user);
    // "%command% -q" and "-q" mean the same to Steam, so they come back as the shorter one.
    QCOMPARE(ModManagers::withoutPrefix(ModManagers::withPrefix(QStringLiteral("%command% -q"), path)),
             QStringLiteral("-q"));
    // A path with spaces and quotes survives.
    QCOMPARE(ModManagers::withoutPrefix(ModManagers::withPrefix(QStringLiteral("-x"), QStringLiteral("/a \"b\"/c"))),
             QStringLiteral("-x"));
    QCOMPARE(ModManagers::withoutPrefix(QStringLiteral("STEAM_COMPAT_DATA_PATH=/p %command%")), QString());
    QCOMPARE(ModManagers::withoutPrefix(QStringLiteral("mangohud %command%")), QStringLiteral("mangohud %command%"));
}

void LibraryModManagersTest::shortcutInSharedPrefix()
{
    const QString root = installFakeSteam();
    // Tomb Raider's prefix, and its Proton version from Steam.
    writeFile(managerPath("mo2"), "name = \"Mod Organizer 2\"\nexe = \"/mods/MO2/ModOrganizer.exe\"\n"
                                  "launch_options = \"-x\"\ngames = [\"steam-8000\"]\n");
    auto library = openLibrary();

    QTRY_VERIFY_WITH_TIMEOUT(shortcutNamed(shortcutsIn(root), QStringLiteral("Mod Organizer 2")).has_value(), 5000);
    const SteamShortcut s = *shortcutNamed(shortcutsIn(root), QStringLiteral("Mod Organizer 2"));
    // The app ID was made by the launcher and written to the file; the prefix option is Steam's only.
    QVERIFY2(readFile(managerPath("mo2")).contains(QByteArray("shortcut = ") + QByteArray::number(s.appId)),
             qPrintable(readFile(managerPath("mo2"))));
    QVERIFY(!readFile(managerPath("mo2")).contains("STEAM_COMPAT_DATA_PATH"));
    QCOMPARE(s.launchOptions,
             QStringLiteral("STEAM_COMPAT_DATA_PATH=\"%1/steamapps/compatdata/8000\" %command% -x").arg(root));
    QCOMPARE(s.exe, QStringLiteral("/mods/MO2/ModOrganizer.exe"));
    QVERIFY(readFile(root + QStringLiteral("/config/config.vdf")).contains("GE-Proton10-4"));
    QTRY_VERIFY_WITH_TIMEOUT(!library->store().snapshot().steamQueued.contains(QStringLiteral("mod-manager:mo2")), 5000);

    // Not a game: nothing in the game list.
    QCOMPARE(library->rowOfGame(QStringLiteral("mo2")), -1);
    QVERIFY(!library->store().snapshot().content.games.contains(QStringLiteral("mod-manager:mo2")));
}

void LibraryModManagersTest::shortcutInOwnPrefix()
{
    const QString root = installFakeSteam();
    writeFile(managerPath("vortex"), "name = \"Vortex\"\nexe = \"/mods/Vortex/Vortex.exe\"\n"
                                     "games = [\"steam-8000\", \"steam-2231450\"]\nproton = \"GE-Proton10-4\"\n");
    auto library = openLibrary();
    QTRY_VERIFY_WITH_TIMEOUT(shortcutNamed(shortcutsIn(root), QStringLiteral("Vortex")).has_value(), 5000);
    const SteamShortcut s = *shortcutNamed(shortcutsIn(root), QStringLiteral("Vortex"));
    QVERIFY(s.launchOptions.isEmpty());

    // Linking one more game never changes an own prefix; an explicit "game" shares the first.
    QCOMPARE(library->setModManagerValue(QStringLiteral("vortex"), QStringLiteral("prefix"), QStringLiteral("game")),
             QString());
    QTRY_VERIFY_WITH_TIMEOUT(
        shortcutNamed(shortcutsIn(root), QStringLiteral("Vortex"))->launchOptions.contains("compatdata/8000"), 5000);
}

void LibraryModManagersTest::deletedInSteam()
{
    const QString root = installFakeSteam();
    writeFile(managerPath("mo2"), "name = \"Mod Organizer 2\"\nexe = \"/mods/MO2/ModOrganizer.exe\"\n"
                                  "games = []\n");
    auto library = openLibrary();
    QTRY_VERIFY_WITH_TIMEOUT(shortcutNamed(shortcutsIn(root), QStringLiteral("Mod Organizer 2")).has_value(), 5000);
    const quint32 appId = shortcutNamed(shortcutsIn(root), QStringLiteral("Mod Organizer 2"))->appId;

    BinaryVdf::Document doc = BinaryVdf::parse(readFile(shortcutsPath(root)));
    QVERIFY(SteamShortcuts::remove(&doc.root, appId));
    writeFile(shortcutsPath(root), BinaryVdf::write(doc.root));
    library->store().refresh();
    library->store().waitForIdle();
    QCoreApplication::processEvents();

    // Marked, not added back.
    QTRY_VERIFY_WITH_TIMEOUT(readFile(managerPath("mo2")).contains("in_steam = false"), 5000);
    library->store().refresh();
    library->store().waitForIdle();
    QVERIFY(!shortcutNamed(shortcutsIn(root), QStringLiteral("Mod Organizer 2")).has_value());
    QVERIFY(library->store().snapshot().steamQueued.value(QStringLiteral("mod-manager:mo2")).isEmpty());
}

void LibraryModManagersTest::adoptedShortcut()
{
    const QString root = installFakeSteam();
    SteamShortcut mo2;
    mo2.appId = 3193880877u;
    mo2.name = QStringLiteral("MO2");
    mo2.exe = QStringLiteral("/mods/MO2/ModOrganizer.exe");
    mo2.startDir = QStringLiteral("/mods/MO2/");
    BinaryVdf::Document doc = BinaryVdf::parse(readFile(shortcutsPath(root)));
    QVERIFY(SteamShortcuts::add(&doc.root, mo2));
    writeFile(shortcutsPath(root), BinaryVdf::write(doc.root));
    auto library = openLibrary();

    // Offered as a shortcut to add, a Windows program being what a manager can be.
    bool offered = false;
    for (const QVariant &v : library->steamShortcuts()) {
        if (v.toMap().value("appId").toLongLong() == 3193880877LL) {
            offered = true;
            QVERIFY(v.toMap().value("canManage").toBool());
        }
    }
    QVERIFY(offered);

    QCOMPARE(library->addSteamShortcutAsModManager(3193880877LL), QString());
    QCOMPARE(library->modManagers().size(), 1);
    const QString file = QString::fromUtf8(readFile(managerPath("mo2")));
    QVERIFY2(file.contains("shortcut = 3193880877") && file.contains("prefix = \"own\""), qPrintable(file));
    for (const QVariant &v : library->steamShortcuts())
        QVERIFY(v.toMap().value("appId").toLongLong() != 3193880877LL);
    // In step with Steam from the start: nothing queued, nothing rewritten.
    // The fixture's own Proton games get their shortcuts first; wait for those to settle.
    QTRY_VERIFY_WITH_TIMEOUT(library->store().snapshot().steamQueued.isEmpty(), 5000);
    const QByteArray before = readFile(shortcutsPath(root));
    const QList<SteamShortcut> listBefore = shortcutsIn(root);
    library->store().refresh();
    library->store().waitForIdle();
    QCoreApplication::processEvents();
    QVERIFY(library->store().snapshot().steamQueued.value(QStringLiteral("mod-manager:mo2")).isEmpty());
    const QList<SteamShortcut> listAfter = shortcutsIn(root);
    QCOMPARE(listAfter.size(), listBefore.size());
    for (int i = 0; i < listAfter.size(); ++i) {
        QCOMPARE(listAfter.at(i).name, listBefore.at(i).name);
        QCOMPARE(listAfter.at(i).exe, listBefore.at(i).exe);
        QCOMPARE(listAfter.at(i).startDir, listBefore.at(i).startDir);
        QCOMPARE(listAfter.at(i).icon, listBefore.at(i).icon);
        QCOMPARE(listAfter.at(i).launchOptions, listBefore.at(i).launchOptions);
    }
    QCOMPARE(readFile(shortcutsPath(root)), before);
}

void LibraryModManagersTest::removeOptions()
{
    const QString root = installFakeSteam();
    const QString mods = m_installs.home + QStringLiteral("/Mods");
    writeFile(mods + QStringLiteral("/MO2/ModOrganizer.exe"), "MZ");
    writeFile(mods + QStringLiteral("/Vortex/Vortex.exe"), "MZ");
    writeFile(managerPath("mo2"), "name = \"Mod Organizer 2\"\nexe = \"" + mods.toUtf8() +
                                      "/MO2/ModOrganizer.exe\"\ngames = [\"steam-8000\"]\n");
    writeFile(managerPath("vortex"), "name = \"Vortex\"\nexe = \"" + mods.toUtf8() +
                                         "/Vortex/Vortex.exe\"\nprefix = \"own\"\ngames = [\"steam-8000\"]\n");
    writeFile(managerPath("flatpak"), "name = \"Flat\"\nexe = \"com.example.Mods\"\ngames = []\n");
    auto library = openLibrary();
    QTRY_VERIFY_WITH_TIMEOUT(shortcutNamed(shortcutsIn(root), QStringLiteral("Vortex")).has_value(), 5000);
    QTRY_VERIFY_WITH_TIMEOUT(shortcutNamed(shortcutsIn(root), QStringLiteral("Mod Organizer 2")).has_value(), 5000);
    const quint32 vortexId = shortcutNamed(shortcutsIn(root), QStringLiteral("Vortex"))->appId;
    const quint32 mo2Id = shortcutNamed(shortcutsIn(root), QStringLiteral("Mod Organizer 2"))->appId;
    QDir().mkpath(root + QStringLiteral("/steamapps/compatdata/%1").arg(vortexId));
    QDir().mkpath(root + QStringLiteral("/steamapps/compatdata/%1").arg(mo2Id));

    // In the game's prefix: nothing of it is offered.
    QVariantMap shared = library->modManagerUninstallChoices(QStringLiteral("mo2"));
    QVERIFY(shared.value("shortcut").toBool());
    QCOMPARE(shared.value("installDir").toString(), QStringLiteral("~/Mods/MO2"));
    QCOMPARE(shared.value("prefix").toString(), QString());

    // Its own prefix is.
    const QVariantMap own = library->modManagerUninstallChoices(QStringLiteral("vortex"));
    QCOMPARE(own.value("prefix").toString(),
             QStringLiteral("~/.local/share/Steam/steamapps/compatdata/%1").arg(vortexId));

    // A Flatpak has no shortcut, folder or prefix.
    const QVariantMap flat = library->modManagerUninstallChoices(QStringLiteral("flatpak"));
    QVERIFY(!flat.value("shortcut").toBool());
    QCOMPARE(flat.value("installDir").toString(), QString());
    QCOMPARE(flat.value("prefix").toString(), QString());

    // Asking for the game's prefix anyway is refused, nothing deleted.
    QStringList trashed;
    library->setTrash(fakeTrash(&trashed));
    const QVariantMap refused = library->uninstallModManager(QStringLiteral("mo2"), false, true, false, false);
    QVERIFY(!refused.value("error").toString().isEmpty());
    QVERIFY(trashed.isEmpty());
    QVERIFY(QFileInfo::exists(managerPath("mo2")));
}

void LibraryModManagersTest::removeDeletesWhatIsAsked()
{
    const QString root = installFakeSteam();
    const QString folder = m_installs.home + QStringLiteral("/Mods/Vortex");
    writeFile(folder + QStringLiteral("/Vortex.exe"), "MZ");
    writeFile(managerPath("vortex"), "name = \"Vortex\"\nexe = \"" + folder.toUtf8() +
                                         "/Vortex.exe\"\nprefix = \"own\"\ngames = []\n");
    auto library = openLibrary();
    QTRY_VERIFY_WITH_TIMEOUT(shortcutNamed(shortcutsIn(root), QStringLiteral("Vortex")).has_value(), 5000);
    const quint32 appId = shortcutNamed(shortcutsIn(root), QStringLiteral("Vortex"))->appId;
    const QString prefix = root + QStringLiteral("/steamapps/compatdata/%1").arg(appId);
    QDir().mkpath(prefix);
    QTRY_VERIFY_WITH_TIMEOUT(library->store().snapshot().steamQueued.isEmpty(), 5000);

    QStringList trashed;
    library->setTrash(fakeTrash(&trashed));
    const QVariantMap r = library->uninstallModManager(QStringLiteral("vortex"), true, true, false, false);
    QCOMPARE(r.value("error").toString(), QString());
    QCOMPARE(trashed, (QStringList{folder, prefix}));
    QVERIFY(!QFileInfo::exists(managerPath("vortex")));
    // The folder takes the shortcut with it.
    QTRY_VERIFY_WITH_TIMEOUT(!shortcutNamed(shortcutsIn(root), QStringLiteral("Vortex")).has_value(), 5000);
    QVERIFY(library->modManagers().isEmpty());
}

void LibraryModManagersTest::removeLeavesShortcut()
{
    const QString root = installFakeSteam();
    writeFile(managerPath("mo2"), "name = \"Mod Organizer 2\"\nexe = \"/mods/MO2/ModOrganizer.exe\"\ngames = []\n");
    auto library = openLibrary();
    QTRY_VERIFY_WITH_TIMEOUT(shortcutNamed(shortcutsIn(root), QStringLiteral("Mod Organizer 2")).has_value(), 5000);
    QTRY_VERIFY_WITH_TIMEOUT(library->store().snapshot().steamQueued.isEmpty(), 5000);

    QCOMPARE(library->uninstallModManager(QStringLiteral("mo2"), false, false, false, false).value("error").toString(),
             QString());
    QVERIFY(!QFileInfo::exists(managerPath("mo2")));
    library->store().refresh();
    library->store().waitForIdle();
    QCoreApplication::processEvents();
    // Forgotten, not removed: it stays in Steam as it is, and nothing is queued for it.
    QVERIFY(shortcutNamed(shortcutsIn(root), QStringLiteral("Mod Organizer 2")).has_value());
    QVERIFY(library->store().snapshot().steamQueued.isEmpty());
}

void LibraryModManagersTest::settingsPage()
{
    writeFile(managerPath("mo2"), "name = \"Mod Organizer 2\"\nexe = \"/mods/MO2/ModOrganizer.exe\"\n"
                                  "games = [\"linux-supertuxkart\"]\nin_steam = false\n");
    auto library = openLibrary();

    QVariantMap row = library->modManagers().first().toMap();
    QVERIFY(row.value("proton").toBool());
    QVERIFY(!row.value("inSteam").toBool());

    // Locate: clearing in_steam removes the key.
    QVERIFY(library->setModManagerValue("mo2", "in_steam", "").isEmpty());
    QVERIFY(!readFile(managerPath("mo2")).contains("in_steam"));
    QVERIFY(library->modManagers().first().toMap().value("inSteam").toBool());

    // The dialog sends QML lists and several keys at once.
    QVERIFY(library->setModManagerValues("mo2", {{"games", QVariantList{"linux-supertuxkart", "steam-730"}},
                                                  {"prefix", "own"}, {"start_dir", ""}}).isEmpty());
    QCOMPARE(library->modManagers().first().toMap().value("games").toList().size(), 2);
    QVERIFY(readFile(managerPath("mo2")).contains("prefix = \"own\""));
    const QVariantMap added = library->addModManager({{"name", "Tool"}, {"exe", "/opt/tool.sh"},
                                                      {"games", QVariantList{"linux-supertuxkart"}}});
    QVERIFY(added.value("error").toString().isEmpty());

    // Only Steam, Proton and Linux games can be linked.
    const QVariantList linkable = library->linkableGames();
    QVERIFY(!linkable.isEmpty());
    for (const QVariant &v : linkable) {
        const QString platform = v.toMap().value("platform").toString();
        QVERIFY2(platform == "steam" || platform == "proton" || platform == "linux", qPrintable(platform));
    }
}

QTEST_GUILESS_MAIN(LibraryModManagersTest)

#include "library_modmanagers_test.moc"
