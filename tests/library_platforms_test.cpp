#include "librarytestbase.h"

namespace {

QStringList platformIds(const Library &library)
{
    QStringList ids;
    for (const QVariant &p : library.platformSettings())
        ids.append(p.toMap().value("id").toString());
    return ids;
}

// The demo's linux platform, as the app appends it (without the demo's comment).
const QByteArray kLinuxBlock = "\n[[platform]]\nid = \"linux\"\nname = \"Linux\"\nicon = \"linux\"\n";

// The demo's platforms.toml without its linux platform, like a library from before it.
QByteArray withoutLinux(const QString &file)
{
    QByteArray text = readFile(file);
    text.replace("\n# Native Linux games, run by the launcher itself." + kLinuxBlock, "");
    return text;
}

QStringList railIds(const Library &library)
{
    QStringList ids;
    for (const QVariant &p : library.platforms())
        ids.append(p.toMap().value("id").toString());
    return ids;
}

} // namespace

// The library tests on platforms: their settings, order, ROM folders, the Linux platform.
class LibraryPlatformsTest : public LibraryTestBase
{
    Q_OBJECT

private slots:
    void platformSettingsList();
    void platformValues();
    void movePlatforms();
    void movePlatformsBefore();
    void platformEmulatorsAndFolders();
    void matchRomFolders();
    void platformProblems();
    void addAndRemovePlatforms();
    void defaultPlatforms();
    void linuxPlatformAdded();
    void linuxPlatformFromCache();
    void linuxPlatformKept();
    void linuxPlatformInRail();
};

void LibraryPlatformsTest::platformSettingsList()
{
    auto library = openLibrary();
    QCOMPARE(platformIds(*library), QStringList({"switch", "3ds", "steam", "proton", "linux"}));
    const QVariantMap sw = library->platformSettings().first().toMap();
    QCOMPARE(sw.value("name"), QStringLiteral("Switch"));
    QCOMPARE(sw.value("icon"), QStringLiteral("switch"));
    QVERIFY(sw.value("defined").toBool());
    QCOMPARE(sw.value("games").toInt(), 10);
    QCOMPARE(sw.value("emulators").toStringList(), QStringList{"ryubing"});
    QCOMPARE(sw.value("emulatorNames").toStringList(), QStringList{"Ryubing"});
    QCOMPARE(sw.value("romFolders").toStringList(), QStringList{"roms/switch"});
}

void LibraryPlatformsTest::platformValues()
{
    const QString file = m_data + QStringLiteral("/library/platforms.toml");
    const QByteArray before = readFile(file);
    auto library = openLibrary();

    QCOMPARE(library->setPlatformValue("3ds", "name", " Nintendo 3DS "), QString());
    QCOMPARE(library->setPlatformValue("3ds", "icon", QString()), QString());
    QByteArray expected = before;
    expected.replace("name = \"3DS\"\nicon = \"3ds\"\n", "name = \"Nintendo 3DS\"\n");
    QCOMPARE(readFile(file), expected);
    QTRY_COMPARE(library->platformSettings().at(1).toMap().value("name"), QStringLiteral("Nintendo 3DS"));
    QCOMPARE(library->platformSettings().at(1).toMap().value("icon"), QString());

    QVERIFY(!library->setPlatformValue("3ds", "name", "  ").isEmpty());
    QVERIFY(!library->setPlatformValue("wii", "name", "Wii").isEmpty());
    QVERIFY(!library->setPlatformValue("3ds", "id", "n3ds").isEmpty());
    QCOMPARE(readFile(file), expected);

    QVERIFY(library->platformSettings().at(0).toMap().value("inRail").toBool());
    QCOMPARE(library->setPlatformValue("switch", "in_rail", false), QString());
    QVERIFY(readFile(file).contains("in_rail = false"));
    QTRY_VERIFY(!library->platformSettings().at(0).toMap().value("inRail").toBool());
    for (const QVariant &p : library->platforms())
        QVERIFY(p.toMap().value("id") != QStringLiteral("switch"));
    QCOMPARE(library->setPlatformValue("switch", "in_rail", true), QString());
    QVERIFY(!readFile(file).contains("in_rail"));
    QTRY_VERIFY(library->platformSettings().at(0).toMap().value("inRail").toBool());
}

void LibraryPlatformsTest::movePlatforms()
{
    const QString file = m_data + QStringLiteral("/library/platforms.toml");
    auto library = openLibrary();

    // Two moves in a row, before the rescan: each starts from the file.
    QCOMPARE(library->movePlatform("proton", -1), QString());
    QCOMPARE(library->movePlatform("proton", -1), QString());
    QTRY_COMPARE(platformIds(*library), QStringList({"switch", "proton", "3ds", "steam", "linux"}));
    // Its comment moved with it.
    QVERIFY(readFile(file).contains("# Non-Steam games that run through Proton.\n[[platform]]\nid = \"proton\""));
    QVERIFY(readFile(file).indexOf("id = \"proton\"") < readFile(file).indexOf("id = \"3ds\""));

    QCOMPARE(library->movePlatform("switch", 1), QString());
    QTRY_COMPARE(platformIds(*library), QStringList({"proton", "switch", "3ds", "steam", "linux"}));
    QCOMPARE(library->movePlatform("linux", 1), QString()); // already last
    QCOMPARE(library->movePlatform("proton", -1), QString()); // already first
    QCOMPARE(library->movePlatform("steam", -3), QString());
    QTRY_COMPARE(platformIds(*library), QStringList({"steam", "proton", "switch", "3ds", "linux"}));
    // Several places down at once, as typing its place does; to last, too.
    QCOMPARE(library->movePlatform("steam", 2), QString());
    QTRY_COMPARE(platformIds(*library), QStringList({"proton", "switch", "steam", "3ds", "linux"}));
    QCOMPARE(library->movePlatform("proton", 4), QString());
    QTRY_COMPARE(platformIds(*library), QStringList({"switch", "steam", "3ds", "linux", "proton"}));
}

void LibraryPlatformsTest::movePlatformsBefore()
{
    const QString file = m_data + QStringLiteral("/library/platforms.toml");
    writeFile(m_data + QStringLiteral("/library/games/xbox-halo.toml"), "title = \"Halo\"\nplatform = \"xbox\"\n");
    auto library = openLibrary();
    QSignalSpy offered(library.get(), &Library::undoOffered);

    // A platform without games isn't in the rail, and sits between two that are.
    QCOMPARE(library->addPlatform("Empty"), QString());
    QCOMPARE(library->movePlatformBefore("empty", "3ds"), QString());
    QTRY_COMPARE(platformIds(*library), QStringList({"switch", "empty", "3ds", "steam", "proton", "linux", "xbox"}));

    // The rail drop: Steam above 3DS, whose neighbour in the file is Empty.
    QCOMPARE(library->movePlatformBefore("steam", "3ds"), QString());
    QTRY_COMPARE(platformIds(*library), QStringList({"switch", "empty", "steam", "3ds", "proton", "linux", "xbox"}));
    QCOMPARE(offered.last().at(1).toString(), QStringLiteral("Steam moved"));

    // Last, as an empty id says; a target the file doesn't have means last too.
    QCOMPARE(library->movePlatformBefore("switch", QString()), QString());
    QTRY_COMPARE(platformIds(*library).first(), QStringLiteral("empty"));
    QCOMPARE(library->movePlatformBefore("empty", "nowhere"), QString());
    QTRY_COMPARE(platformIds(*library).first(), QStringLiteral("steam"));
    QCOMPARE(platformIds(*library).last(), QStringLiteral("xbox"));

    // Undo puts the order back.
    QCOMPARE(library->undo(offered.last().at(0).toULongLong()), QString());
    QTRY_COMPARE(platformIds(*library).first(), QStringLiteral("empty"));

    // A platform only its games define gets its block where it is dropped, named after its id.
    QVERIFY(!readFile(file).contains("id = \"xbox\""));
    QCOMPARE(library->movePlatformBefore("xbox", "steam"), QString());
    QVERIFY(readFile(file).contains("id = \"xbox\"\nname = \"xbox\""));
    QVERIFY(readFile(file).indexOf("id = \"xbox\"") < readFile(file).indexOf("id = \"steam\""));
    QTRY_VERIFY(library->platformSettings().first().toMap().value("id") == "empty"
                && platformIds(*library).indexOf("xbox") < platformIds(*library).indexOf("steam"));
    QVERIFY(library->platformSettings().at(platformIds(*library).indexOf("xbox")).toMap().value("defined").toBool());
}

void LibraryPlatformsTest::platformEmulatorsAndFolders()
{
    const QString file = m_data + QStringLiteral("/library/platforms.toml");
    auto library = openLibrary();
    const auto installed = [&](const QString &id) {
        return library->gameAt(library->rowOfGame(id)).value("installed").toBool();
    };

    // An unknown id is kept as written (it warns), so saving from the page doesn't lose it.
    QCOMPARE(library->setPlatformValue("3ds", "emulators", QStringList({"azahar", "citr"})), QString());
    QVERIFY(readFile(file).contains("emulators = [\"azahar\", \"citr\"]\n"));
    QTRY_COMPARE(library->platformSettings().at(1).toMap().value("emulators").toStringList(),
                 QStringList({"azahar", "citr"}));
    QCOMPARE(library->platformSettings().at(1).toMap().value("emulatorNames").toStringList(),
             QStringList({"Azahar", "citr"}));

    // Without its folder, a platform's games stay, as "ROM not found"; the folder back
    // finds them again.
    QVERIFY(installed("switch-tetris"));
    QCOMPARE(library->setPlatformValue("switch", "rom_folders", QStringList()), QString());
    QVERIFY(!readFile(file).contains("rom_folders = [\"roms/switch\"]"));
    QTRY_VERIFY(!installed("switch-tetris"));
    QCOMPARE(library->setPlatformValue("switch", "rom_folders", QStringList{"roms/switch"}), QString());
    QTRY_VERIFY(installed("switch-tetris"));
}

void LibraryPlatformsTest::matchRomFolders()
{
    auto library = openLibrary();

    // matchRomFolders() expands and reports ~/… against $HOME, not the fake home the rest of
    // the test uses for installs.
    const QByteArray realHome = qgetenv("HOME");
    const QString fakeHome = m_dir->filePath(QStringLiteral("romsHome"));
    QDir().mkpath(fakeHome);
    qputenv("HOME", fakeHome.toUtf8());

    const QString parent = fakeHome + QStringLiteral("/Games/roms");
    QDir().mkpath(parent + QStringLiteral("/switch"));
    QDir().mkpath(parent + QStringLiteral("/Nintendo 3DS"));
    QDir().mkpath(parent + QStringLiteral("/bios"));

    // Already one of the platform's.
    QCOMPARE(library->setPlatformValue("switch", "rom_folders", QStringList{"~/Games/roms/switch"}), QString());
    QTRY_COMPARE(library->platformSettings().at(0).toMap().value("romFolders").toStringList(),
                 QStringList{"~/Games/roms/switch"});

    const QVariantList matches = library->matchRomFolders(QStringLiteral("~/Games/roms"));
    qputenv("HOME", realHome);

    QCOMPARE(matches.size(), 3);
    QHash<QString, QVariantMap> byFolder;
    for (const QVariant &m : matches)
        byFolder.insert(m.toMap().value("folder").toString(), m.toMap());

    const QVariantMap sw = byFolder.value(QStringLiteral("switch"));
    QCOMPARE(sw.value("platformId").toString(), QStringLiteral("switch"));
    QCOMPARE(sw.value("path").toString(), QStringLiteral("~/Games/roms/switch"));
    QVERIFY(sw.value("already").toBool());

    // Matched by the built-in alias table (3ds: "nintendo3ds"), not the id or name directly.
    const QVariantMap n3ds = byFolder.value(QStringLiteral("Nintendo 3DS"));
    QCOMPARE(n3ds.value("platformId").toString(), QStringLiteral("3ds"));
    QCOMPARE(n3ds.value("path").toString(), QStringLiteral("~/Games/roms/Nintendo 3DS"));
    QVERIFY(!n3ds.value("already").toBool());

    const QVariantMap bios = byFolder.value(QStringLiteral("bios"));
    QCOMPARE(bios.value("platformId").toString(), QString());

    QCOMPARE(library->matchRomFolders(QStringLiteral("~/Games/nowhere")), QVariantList());
}

void LibraryPlatformsTest::platformProblems()
{
    // Ryubing's Flatpak may only read ~/Games; the 3DS folder is gone, and Azahar isn't
    // installed.
    writeFile(m_installs.flatpakDirs.first() + QStringLiteral("/io.github.ryubing.Ryujinx/current/active/metadata"),
              "[Context]\nfilesystems=~/Games;\n");
    QVERIFY(QDir(m_data + QStringLiteral("/roms/3ds")).removeRecursively());
    QVERIFY(QDir(m_installs.flatpakDirs.first() + QStringLiteral("/org.azahar_emu.Azahar")).removeRecursively());
    auto library = openLibrary();

    const auto problemsOf = [&](int row) {
        QStringList out;
        for (const QVariant &p : library->platformSettings().at(row).toMap().value("problems").toList())
            out.append(p.toMap().value("text").toString() + u'|' + p.toMap().value("command").toString());
        return out;
    };
    const QString switchFolder = m_data + QStringLiteral("/roms/switch");
    QCOMPARE(problemsOf(0), QStringList{QStringLiteral(
        "Ryubing is a Flatpak and can't read %1; to let it, run:|"
        "flatpak override --user --filesystem=%1 io.github.ryubing.Ryujinx").arg(switchFolder)});
    QCOMPARE(problemsOf(1), QStringList({
        "ROM folder roms/3ds not found; its games show as \"ROM not found\"|",
        "Azahar isn't installed, so these games can't launch|",
    }));
    QVERIFY(problemsOf(2).isEmpty()); // Steam: no emulators

    // In Library.problems with the file and line, the missing folder once (from the scan).
    const QStringList problems = library->problems();
    QVERIFY(anyContains(problems, QStringLiteral("library/platforms.toml:11: Switch: Ryubing is a Flatpak")));
    QVERIFY(anyContains(problems, QStringLiteral("library/platforms.toml:18: 3DS: Azahar isn't installed")));
    QCOMPARE(std::count_if(problems.cbegin(), problems.cend(), [](const QString &p) { return p.contains("roms/3ds not found"); }), 1);

    // Allowed by a user override, the Flatpak problem goes away with the next scan.
    writeFile(m_installs.flatpakDirs.first() + QStringLiteral("/../overrides/io.github.ryubing.Ryujinx"),
              "[Context]\nfilesystems=" + QFile::encodeName(switchFolder) + ";\n");
    library->scanNow();
    QTRY_VERIFY(problemsOf(0).isEmpty());
}

void LibraryPlatformsTest::addAndRemovePlatforms()
{
    const QString file = m_data + QStringLiteral("/library/platforms.toml");
    auto library = openLibrary();
    library->setInstalledOnly(false); // without its ROM folders, a platform's ROMs aren't found

    QCOMPARE(library->platformIdFor(" Nintendo Wii U! "), QStringLiteral("nintendo-wii-u"));
    QCOMPARE(library->addPlatform("Wii U"), QString());
    QVERIFY(readFile(file).endsWith("\n\n[[platform]]\nid = \"wii-u\"\nname = \"Wii U\"\n"));
    QVERIFY(!library->addPlatform("wii u").isEmpty()); // the same id
    QVERIFY(!library->addPlatform("ゼルダ").isEmpty());  // no id to derive
    QTRY_COMPARE(platformIds(*library), QStringList({"switch", "3ds", "steam", "proton", "linux", "wii-u"}));

    // Removed, a platform's games stay, under its id.
    QCOMPARE(library->removePlatform("switch"), QString());
    QVERIFY(!readFile(file).contains("id = \"switch\""));
    QTRY_COMPARE(platformIds(*library), QStringList({"3ds", "steam", "proton", "linux", "wii-u", "switch"}));
    QVERIFY(!library->platformSettings().last().toMap().value("defined").toBool());
    QVERIFY(library->rowOfGame(QStringLiteral("switch-pikmin-4")) >= 0);

    // Added again with that id, it takes them back.
    QCOMPARE(library->addPlatform("Nintendo Switch", "switch"), QString());
    QTRY_VERIFY(library->platformSettings().last().toMap().value("defined").toBool());
    QCOMPARE(library->platformSettings().last().toMap().value("name"), QStringLiteral("Nintendo Switch"));
    QCOMPARE(library->platformSettings().last().toMap().value("games").toInt(), 10);
}

void LibraryPlatformsTest::defaultPlatforms()
{
    const QString file = m_data + QStringLiteral("/library/platforms.toml");
    QVERIFY(QFile::remove(file)); // like a new library
    auto library = openLibrary();
    library->setInstalledOnly(false); // no ROM folders yet, so no ROM is found

    QFile resource(QStringLiteral(":/qt/qml/Launcher/resources/platforms.toml"));
    QVERIFY(resource.open(QIODevice::ReadOnly));
    // The demo's Steam is found, and it has Proton games, so the Steam and Proton platforms
    // are added after them, then Linux, which every library gets.
    const QByteArray withSteam = resource.readAll() + "\n[[platform]]\nid = \"steam\"\nname = \"Steam\"\nicon = \"steam\"\n"
                                 "\n[[platform]]\nid = \"proton\"\nname = \"Proton\"\nicon = \"proton\"\n"
                                 "\n[[platform]]\nid = \"linux\"\nname = \"Linux\"\nicon = \"linux\"\n";
    QCOMPARE(readFile(file), withSteam);
    QCOMPARE(platformIds(*library), QStringList({"switch", "3ds", "wiiu", "wii", "gamecube", "n64", "nds", "gba",
                                                 "ps4", "ps3", "ps2", "ps1", "psp", "steam", "proton", "linux"}));
    // Every default platform's emulator is a preset. Steam, Proton and Linux need none.
    for (const QVariant &p : library->platformSettings()) {
        const QVariantMap platform = p.toMap();
        const QString id = platform.value("id").toString();
        if (platform.value("defined").toBool() && id != QStringLiteral("steam") && id != QStringLiteral("proton")
            && id != QStringLiteral("linux"))
            QCOMPARE(platform.value("emulatorNames").toStringList().size(), 1);
    }
    // No ROM folders yet, so nothing to warn about, even with emulators not installed.
    QVERIFY2(library->problems().isEmpty(), qPrintable(library->problems().join(u'\n')));
    QCOMPARE(library->gameAt(library->rowOfGame("switch-pikmin-4")).value("platformName"), QStringLiteral("Switch"));
}

void LibraryPlatformsTest::linuxPlatformAdded()
{
    const QString file = m_data + QStringLiteral("/library/platforms.toml");
    const QByteArray without = withoutLinux(file);
    QVERIFY(!without.contains("\"linux\""));
    writeFile(file, without);

    // Added after the others, whether or not Steam is found or any game is a Linux game.
    {
        auto library = openLibrary();
        QCOMPARE(readFile(file), without + kLinuxBlock);
        QCOMPARE(platformIds(*library).last(), QStringLiteral("linux"));
        QCOMPARE(library->platformSettings().last().toMap().value("name"), QStringLiteral("Linux"));
    }
    // Once: the next start leaves it.
    auto library = openLibrary();
    QCOMPARE(readFile(file), without + kLinuxBlock);

    // Removed, it comes back, as steam does.
    QCOMPARE(library->removePlatform("linux"), QString());
    library->store().waitForIdle();
    QCoreApplication::processEvents();
    QTRY_COMPARE(readFile(file), without + kLinuxBlock);
}

void LibraryPlatformsTest::linuxPlatformFromCache()
{
    // A library from before the linux platform, cached, with nothing changed since: the scan
    // finds nothing new, and still adds the platform.
    { auto library = openLibrary(); } // saves the cache
    const QString file = m_data + QStringLiteral("/library/platforms.toml");
    const QByteArray without = withoutLinux(file);
    writeFile(file, without);
    const QString cachePath = LibraryCache::pathFor(m_cache, m_data);
    std::optional<LibrarySnapshot> snapshot = LibraryCache::load(cachePath, m_data);
    QVERIFY(snapshot);
    snapshot->content.platforms.removeIf([](const Platform &p) { return p.id == kLinuxPlatform; });
    const QFileInfo info(file);
    snapshot->files[QStringLiteral("library/platforms.toml")].stamp =
        FileStamp{info.lastModified(QTimeZone::UTC).toMSecsSinceEpoch(), info.size()};
    QVERIFY(LibraryCache::save(cachePath, m_data, *snapshot));

    auto library = std::make_unique<Library>(m_data, m_cache);
    QVERIFY(!library->loading());
    const auto linuxDefined = [&] {
        for (const QVariant &p : library->platformSettings())
            if (p.toMap().value("id") == QStringLiteral("linux"))
                return p.toMap().value("defined").toBool();
        return false;
    };
    QVERIFY(!linuxDefined()); // its games show, under the id
    library->store().setInstallLocations(m_installs);
    library->store().setSteamLocations({m_installs.home, {}});
    library->store().setStateDir(m_state);
    library->startRefresh();
    library->store().waitForIdle();
    QCoreApplication::processEvents();
    QCOMPARE(library->store().filesParsed(), 0);
    QCOMPARE(readFile(file), without + kLinuxBlock);
    QVERIFY(linuxDefined());
}

void LibraryPlatformsTest::linuxPlatformKept()
{
    // Renamed and moved to the top by the user, it stays as they wrote it.
    const QString file = m_data + QStringLiteral("/library/platforms.toml");
    QByteArray text = withoutLinux(file);
    text.replace("[[platform]]\nid = \"switch\"",
                 "[[platform]]\nid = \"linux\"\nname = \"Native\"\n\n[[platform]]\nid = \"switch\"");
    writeFile(file, text);
    auto library = openLibrary();
    QCOMPARE(readFile(file), text);
    QCOMPARE(platformIds(*library).first(), QStringLiteral("linux"));
    QCOMPARE(library->platformSettings().first().toMap().value("name"), QStringLiteral("Native"));
}

void LibraryPlatformsTest::linuxPlatformInRail()
{
    // Like any platform, it shows in the rail once it has a game.
    QVERIFY(QFile::remove(gamePath("linux-supertuxkart")));
    QVERIFY(QFile::remove(gamePath("linux-factorio")));
    auto library = openLibrary();
    QVERIFY(!railIds(*library).contains("linux"));
    writeFile(gamePath("linux-tux-racer"), "title = \"Tux Racer\"\nplatform = \"linux\"\n");
    library->store().refresh();
    library->store().waitForIdle();
    QCoreApplication::processEvents();
    QTRY_VERIFY(railIds(*library).contains("linux"));
    const QVariantMap rail = library->platforms().at(railIds(*library).indexOf("linux")).toMap();
    QCOMPARE(rail.value("name"), QStringLiteral("Linux"));
    QCOMPARE(rail.value("icon"), QStringLiteral("linux"));
}

QTEST_GUILESS_MAIN(LibraryPlatformsTest)

#include "library_platforms_test.moc"
