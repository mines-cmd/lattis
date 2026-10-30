#include "librarytestbase.h"

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

private:
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

    const QString text = QString::fromUtf8(readFile(managerPath("vortex")));
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
    QCOMPARE(readFile(managerPath("mo2")),
             QByteArray("# my notes\nname = \"Mod Organizer 2\"   # the short name\n"
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

QTEST_GUILESS_MAIN(LibraryModManagersTest)

#include "library_modmanagers_test.moc"
