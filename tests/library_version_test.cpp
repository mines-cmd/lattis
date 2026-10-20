#include "librarytestbase.h"

// The library tests on the library format's version: read-only, upgrades; the setup key.
class LibraryVersionTest : public LibraryTestBase
{
    Q_OBJECT

private slots:
    void newerVersionIsReadOnly();
    void missingLibraryTomlIsCreated();
    void upgradeFrameBacksUpAndUpgrades();
    void failedUpgradeOpensReadOnly();
    void brokenLibraryTomlIsReadOnly();
    void setupKeyLife();
    void setupHoldsGamesBack();
};

void LibraryVersionTest::newerVersionIsReadOnly()
{
    const QString info = m_data + QStringLiteral("/library/library.toml");
    writeFile(info, "version = 99\n");
    const QByteArray pikmin = readFile(gamePath("switch-pikmin-4"));
    const QByteArray index = readFile(romIndexPath("switch"));
    {
        auto library = openLibrary();
        QVERIFY(library->readOnly());
        QVERIFY(library->rowOfGame(QStringLiteral("switch-tetris")) >= 0); // found, shown, not written
        QVERIFY(library->readOnlyNotice().contains(QStringLiteral("format 99")));
        QCOMPARE(library->gameCount(), 26); // still shows everything it understands

        const QString error = library->setGameValue(QStringLiteral("switch-pikmin-4"), QStringLiteral("favorite"), true);
        QVERIFY(error.startsWith(QStringLiteral("Not saved")));
        QVERIFY(!library->gameAt(library->rowOfGame(QStringLiteral("switch-pikmin-4"))).value("favorite").toBool());
        QVERIFY(!library->setGameValue(QStringLiteral("steam-504230"), QStringLiteral("favorite"), true).isEmpty());
        QVERIFY(library->setEmulatorValue("ryubing", "path", "/x").startsWith(QStringLiteral("Not saved")));
        QVERIFY(library->store()
                    .addGame(m_data + QStringLiteral("/roms/switch/Homebrew/Tetris.nro"), QStringLiteral("switch"), "T")
                    .error.startsWith(QStringLiteral("Not added")));
        QVERIFY(library->store().addTag(QStringLiteral("Horror"), 1).startsWith(QStringLiteral("Not saved")));
        QVERIFY(library->store().removeTag(QStringLiteral("Party")).startsWith(QStringLiteral("Not saved")));
        library->emulatorsFile();
    }
    QVERIFY(!QFile::exists(m_data + QStringLiteral("/library/emulators.toml")));
    QCOMPARE(readFile(gamePath("switch-pikmin-4")), pikmin);
    QVERIFY(!QFile::exists(gamePath("steam-504230")));
    QCOMPARE(readFile(info), QByteArray("version = 99\n"));
    QCOMPARE(readFile(romIndexPath("switch")), index);
    QVERIFY(!QFile::exists(m_data + QStringLiteral("/library/emulators.reference.toml")));
    QVERIFY(QDir(m_cache).entryList(QDir::Files).isEmpty()); // no cache either
}

void LibraryVersionTest::missingLibraryTomlIsCreated()
{
    const QString info = m_data + QStringLiteral("/library/library.toml");
    QVERIFY(QFile::remove(info));
    auto library = openLibrary();
    QVERIFY(!library->readOnly());
    QVERIFY(readFile(info).contains("version = 1\n"));
    const QString id = libraryId();
    QVERIFY(!id.isEmpty());
    QCOMPARE(library->gameCount(), 26);

    // A brand new data dir gets one too, with its own id.
    QTemporaryDir fresh;
    Library empty(fresh.path(), m_cache);
    const QByteArray created = readFile(fresh.filePath("library/library.toml"));
    QVERIFY(created.contains("version = 1\n"));
    QVERIFY(created.contains("id = \""));
    QVERIFY(!created.contains(id.toUtf8()));

    // One from before ids gets one added, and keeps it.
    library.reset();
    writeFile(info, "# Library format version. Don't edit: the app upgrades it.\nversion = 1\n");
    library = openLibrary();
    QVERIFY(!libraryId().isEmpty());
    QVERIFY(readFile(info).startsWith("# Library format version. Don't edit: the app upgrades it.\nversion = 1\n"));
    const QString added = libraryId();
    library.reset();
    library = openLibrary();
    QCOMPARE(libraryId(), added);
}

void LibraryVersionTest::upgradeFrameBacksUpAndUpgrades()
{
    const QString libraryDir = m_data + QStringLiteral("/library");
    const QByteArray pikmin = readFile(gamePath("switch-pikmin-4"));
    QStringList ran;
    const QList<LibraryVersion::Upgrade> steps = {
        {1, [&](const QString &dir) {
             ran.append(QStringLiteral("1→2"));
             writeFile(dir + QStringLiteral("/upgraded.txt"), "yes");
             return QString();
         }},
    };

    const LibraryVersion::Status status = LibraryVersion::prepare(m_data, 2, steps);
    QVERIFY2(status.problems.isEmpty(), qPrintable(status.problems.join(u'\n')));
    QVERIFY(!status.readOnly);
    QCOMPARE(status.version, 2);
    QCOMPARE(ran, QStringList{"1→2"});
    QVERIFY(readFile(libraryDir + QStringLiteral("/library.toml")).contains("version = 2\n"));
    QVERIFY(readFile(libraryDir + QStringLiteral("/library.toml")).startsWith("# Library format version"));
    QVERIFY(QFile::exists(libraryDir + QStringLiteral("/upgraded.txt")));

    // The backup holds the library as it was.
    const QString backup = libraryDir + QStringLiteral("/backup-v1");
    QVERIFY(readFile(backup + QStringLiteral("/library.toml")).contains("version = 1\n"));
    QCOMPARE(readFile(backup + QStringLiteral("/games/switch-pikmin-4.toml")), pikmin);
    QVERIFY(QFile::exists(backup + QStringLiteral("/steam.toml")));
    QVERIFY(!QFile::exists(backup + QStringLiteral("/upgraded.txt")));

    // Already current: nothing runs.
    QCOMPARE(LibraryVersion::prepare(m_data, 2, steps).version, 2);
    QCOMPARE(ran.size(), 1);
}

void LibraryVersionTest::failedUpgradeOpensReadOnly()
{
    const QList<LibraryVersion::Upgrade> steps = {
        {1, [](const QString &) { return QStringLiteral("disk on fire"); }},
    };
    const LibraryVersion::Status status = LibraryVersion::prepare(m_data, 2, steps);
    QVERIFY(status.readOnly);
    QVERIFY(anyContains(status.problems, QStringLiteral("disk on fire")));
    QVERIFY(status.notice.contains(QStringLiteral("backup-v1")));
    QVERIFY(QFile::exists(m_data + QStringLiteral("/library/backup-v1/library.toml")));
    QVERIFY(readFile(m_data + QStringLiteral("/library/library.toml")).contains("version = 1\n"));

    // A missing step fails the same way.
    QVERIFY(LibraryVersion::prepare(m_data, 3, {}).readOnly);
}

void LibraryVersionTest::brokenLibraryTomlIsReadOnly()
{
    writeFile(m_data + QStringLiteral("/library/library.toml"), "version = \n");
    auto library = openLibrary();
    QVERIFY(library->readOnly());
    QVERIFY(anyContains(library->problems(), QStringLiteral("library/library.toml:1:")));
}

void LibraryVersionTest::setupKeyLife()
{
    // A new library waits for the setup only when the app asks for it.
    const QString fresh = m_dir->filePath(QStringLiteral("fresh"));
    const QString freshInfo = fresh + QStringLiteral("/library/library.toml");
    LibraryVersion::Status status = LibraryVersion::prepare(fresh);
    QVERIFY(status.created);
    QVERIFY(!status.setupPending);
    QVERIFY(!readFile(freshInfo).contains("setup"));
    QVERIFY(QFile::remove(freshInfo));
    status = LibraryVersion::prepare(fresh, LibraryVersion::kCurrent, {}, true);
    QVERIFY(status.setupPending);
    QVERIFY(readFile(freshInfo).contains("setup = \"pending\"\n"));

    // Closed halfway: still pending on the next start.
    status = LibraryVersion::prepare(fresh, LibraryVersion::kCurrent, {}, true);
    QVERIFY(!status.created);
    QVERIFY(status.setupPending);

    // An existing library (and a restored backup, which has no key) never waits for it.
    const QString info = m_data + QStringLiteral("/library/library.toml");
    {
        Library library(m_data, m_cache, NewLibrary::WithSetup);
        QVERIFY(!library.setupPending());
        QVERIFY(!readFile(info).contains("setup"));
    }

    // Nor does a read-only one.
    writeFile(info, "version = 99\nsetup = \"pending\"\n");
    QVERIFY(!LibraryVersion::prepare(m_data).setupPending);

    // Any other value is reported and ignored.
    const Parsed<LibraryInfo> parsed = parseLibraryInfo("version = 1\nsetup = \"done\"\n", QStringLiteral("library.toml"));
    QVERIFY(!parsed.value.setupPending);
    QVERIFY(anyContains(parsed.warnings, QStringLiteral("setup can only be \"pending\"")));
}

void LibraryVersionTest::setupHoldsGamesBack()
{
    installFakeSteam();
    const QString data = m_dir->filePath(QStringLiteral("new"));
    const QString roms = m_installs.home + QStringLiteral("/roms/switch");
    writeFile(roms + QStringLiteral("/Pikmin 4 [0100B7C00933A000].nsp"), "rom");
    const auto open = [&] {
        auto library = std::make_unique<Library>(data, m_cache, NewLibrary::WithSetup);
        library->store().setInstallLocations(m_installs);
        library->store().setSteamLocations({m_installs.home, {}});
        library->store().setStateDir(m_state);
        library->startRefresh();
        library->store().waitForIdle();
        QCoreApplication::processEvents();
        return library;
    };
    const auto scan = [](Library &library) {
        library.store().waitForIdle();
        QCoreApplication::processEvents();
    };

    auto library = open();
    QVERIFY(library->setupPending());
    QSignalSpy added(&library->store(), &LibraryStore::gamesAdded);
    // Steam and the emulators are found, for the setup's pages; no game comes in.
    QVERIFY(library->store().snapshot().steam.install.found());
    QVERIFY(library->store().snapshot().installsDetected);
    QVERIFY(library->store().snapshot().installs.value(QStringLiteral("ryubing")).found());
    QVERIFY(!library->store().snapshot().romsScanned);
    QCOMPARE(library->gameCount(), 0);
    QVERIFY(!readFile(data + QStringLiteral("/library/steam.toml")).contains("Pizza Tower"));

    // The Steam step's counts: what would come in, read without importing. Its presets are
    // offered only for the programs on PATH: here mangohud, not gamemoderun.
    const QString bin = m_dir->filePath(QStringLiteral("bin"));
    writeFile(bin + QStringLiteral("/mangohud"), "#!/bin/sh\n");
    QFile::setPermissions(bin + QStringLiteral("/mangohud"), QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner);
    const QByteArray path = qgetenv("PATH");
    qputenv("PATH", bin.toUtf8());
    const QVariantMap steam = library->steam();
    qputenv("PATH", path);
    const auto preset = [&](const QString &id) {
        for (const QVariant &p : steam.value("presets").toList()) {
            if (p.toMap().value("id").toString() == id)
                return p.toMap();
        }
        return QVariantMap();
    };
    QCOMPARE(preset(QStringLiteral("mangohud")).value("programFound").toBool(), true);
    QCOMPARE(preset(QStringLiteral("gamemode")).value("program").toString(), QStringLiteral("gamemoderun"));
    QCOMPARE(preset(QStringLiteral("gamemode")).value("programFound").toBool(), false);
    QCOMPARE(preset(QStringLiteral("wayland")).value("programFound").toBool(), false);
    QVERIFY(steam.value("found").toBool());
    QCOMPARE(steam.value("userId").toString(), QStringLiteral("39734273"));
    const QList<SteamApp> apps = library->store().snapshot().steam.games();
    const int installed = int(std::count_if(apps.cbegin(), apps.cend(), [](const SteamApp &a) { return a.installed; }));
    QVERIFY(installed > 0);
    QCOMPARE(steam.value("installed").toInt(), installed);
    QCOMPARE(steam.value("games").toInt(), 0);
    QCOMPARE(steam.value("shortcuts").toInt(), 3);
    QCOMPARE(steam.value("shortcutsInLibrary").toInt(), 0);
    QCOMPARE(library->steamShortcuts().size(), 3);
    QCOMPARE(library->gameCount(), 0);

    // A ROM folder added meanwhile isn't scanned either.
    QCOMPARE(library->setPlatformValue(QStringLiteral("switch"), QStringLiteral("rom_folders"), QStringList{roms}),
             QString());
    library->scanNow();
    scan(*library);
    QTRY_VERIFY_WITH_TIMEOUT(!library->store().scanning(), 5000);
    QCOMPARE(library->gameCount(), 0);
    QVERIFY(!QFileInfo::exists(data + QStringLiteral("/library/roms/switch.toml")));
    QCOMPARE(added.size(), 0);

    // Closed halfway: the next start still waits.
    library.reset();
    library = open();
    QVERIFY(library->setupPending());
    QCOMPARE(library->gameCount(), 0);

    // Finishing brings everything in with one scan, reported as new games (so they're looked up).
    QSignalSpy added2(&library->store(), &LibraryStore::gamesAdded);
    QSignalSpy pending(library.get(), &Library::setupPendingChanged);
    QCOMPARE(library->finishSetup(), QString());
    QVERIFY(!library->setupPending());
    QCOMPARE(pending.size(), 1);
    QVERIFY(!readFile(data + QStringLiteral("/library/library.toml")).contains("setup"));
    scan(*library);
    QTRY_VERIFY_WITH_TIMEOUT(!library->store().scanning(), 5000);
    QCOMPARE(added2.size(), 1);
    const QStringList ids = added2.first().first().toStringList();
    QVERIFY(ids.contains(QStringLiteral("switch-pikmin-4")));
    QVERIFY(ids.contains(QStringLiteral("steam-2231450")));
    QVERIFY(library->rowOfGame(QStringLiteral("switch-pikmin-4")) >= 0);
    QVERIFY(library->rowOfGame(QStringLiteral("steam-2231450")) >= 0);
    const QVariantList counts = library->gameCounts();
    QCOMPARE(counts.first().toMap().value("id").toString(), QStringLiteral("switch"));
    QCOMPARE(counts.first().toMap().value("count").toInt(), 1);

    // And it stays done.
    library.reset();
    library = open();
    QVERIFY(!library->setupPending());
    QVERIFY(library->rowOfGame(QStringLiteral("switch-pikmin-4")) >= 0);
}

QTEST_GUILESS_MAIN(LibraryVersionTest)

#include "library_version_test.moc"
