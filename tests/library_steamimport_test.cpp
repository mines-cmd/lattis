#include "librarytestbase.h"

// The library tests on the Steam import and Steam's art.
class LibrarySteamImportTest : public LibraryTestBase
{
    Q_OBJECT

private slots:
    void steamImportAddsGames();
    void steamImportFollowsSteam();
    void steamImportOff();
    void steamReadOnlyWritesNothing();
    void steamGamesInList();
    void steamOffNotInstalled();
    void steamArtCopiedOnce();
    void steamSettingsPage();
    void oversizedArtIsShrunk();
    void steamDownloadsFollowTheLog();
};

void LibrarySteamImportTest::steamImportAddsGames()
{
    installFakeSteam();
    const QByteArray before = readFile(steamIndexPath());
    auto library = openLibrary();
    // Added after the existing lines, which stay as they are; no Proton, no runtime.
    const QByteArray after = readFile(steamIndexPath());
    QCOMPARE(after, before + "8000 = \"Tomb Raider: Anniversary\"\n2231450 = \"Pizza Tower\"\n");
    QCOMPARE(library->gameCount(), 28);
    QCOMPARE(library->gameAt(library->rowOfGame(QStringLiteral("steam-2231450"))).value("title").toString(),
             QStringLiteral("Pizza Tower"));
    QVERIFY(library->problems().isEmpty());
    QVERIFY(!QFileInfo::exists(gamePath(QStringLiteral("steam-2231450"))));

    // Nothing new: steam.toml isn't written again.
    const QDateTime modified = QFileInfo(steamIndexPath()).lastModified();
    QTest::qWait(20);
    library->store().refresh();
    library->store().waitForIdle();
    QCoreApplication::processEvents();
    QCOMPARE(readFile(steamIndexPath()), after);
    QCOMPARE(QFileInfo(steamIndexPath()).lastModified(), modified);

    // A title fixed by hand is never renamed back.
    QByteArray fixed = after;
    fixed.replace("\"Pizza Tower\"", "\"Pizza Tower!\"");
    writeFile(steamIndexPath(), fixed);
    library->store().refresh();
    library->store().waitForIdle();
    QCoreApplication::processEvents();
    QCOMPARE(readFile(steamIndexPath()), fixed);
    QCOMPARE(library->gameAt(library->rowOfGame(QStringLiteral("steam-2231450"))).value("title").toString(),
             QStringLiteral("Pizza Tower!"));

    // The next start (from the cache) adds nothing twice.
    library.reset();
    library = openLibrary();
    QCOMPARE(readFile(steamIndexPath()), fixed);
    QCOMPARE(library->gameCount(), 28);
}

void LibrarySteamImportTest::steamImportFollowsSteam()
{
    const QString root = installFakeSteam();
    auto library = openLibrary();
    QCOMPARE(library->gameCount(), 28);
    // Installed in Steam while the app runs: the watcher picks it up.
    const QString manifest = root + QStringLiteral("/steamapps/appmanifest_620.acf");
    writeFile(manifest, "\"AppState\"\n{\n\t\"appid\"\t\t\"620\"\n\t\"name\"\t\t\"Portal 2\"\n\t\"StateFlags\"\t\t\"4\"\n}\n");
    QTRY_VERIFY_WITH_TIMEOUT(readFile(steamIndexPath()).contains("620 = \"Portal 2\"\n"), 5000);
    QTRY_COMPARE_WITH_TIMEOUT(library->gameCount(), 29, 5000);
    // Uninstalled: it stays in steam.toml.
    QVERIFY(QFile::remove(manifest));
    QTest::qWait(1500);
    library->store().waitForIdle();
    QCoreApplication::processEvents();
    QVERIFY(readFile(steamIndexPath()).contains("620 = \"Portal 2\"\n"));
    QCOMPARE(library->gameCount(), 29);
}

void LibrarySteamImportTest::steamImportOff()
{
    installFakeSteam();
    writeFile(m_data + QStringLiteral("/settings.toml"), "[steam]\nenabled = false\n");
    const QByteArray before = readFile(steamIndexPath());
    auto library = openLibrary();
    QCOMPARE(readFile(steamIndexPath()), before);
    QCOMPARE(library->gameCount(), 26);
    QVERIFY(!library->store().snapshot().steam.install.found());

    // A typo in [steam] is a problem, and the import runs with the defaults.
    writeFile(m_data + QStringLiteral("/settings.toml"), "[steam]\nenable = false\n");
    QTRY_VERIFY_WITH_TIMEOUT(readFile(steamIndexPath()).contains("2231450"), 5000);
    QTRY_VERIFY_WITH_TIMEOUT(anyContains(library->problems(), QStringLiteral("unknown key enable")), 5000);
}

void LibrarySteamImportTest::steamReadOnlyWritesNothing()
{
    const QString root = installFakeSteam();
    writeFile(m_data + QStringLiteral("/library/library.toml"), "version = 99\n");
    const QByteArray before = readFile(steamIndexPath());
    const QString localConfig = root + QStringLiteral("/userdata/39734273/config/localconfig.vdf");
    const QByteArray config = readFile(localConfig);
    auto library = openLibrary();
    QVERIFY(library->readOnly());
    // Shown, but not written down.
    QCOMPARE(library->gameCount(), 28);
    QCOMPARE(readFile(steamIndexPath()), before);
    QVERIFY(!QFileInfo::exists(gamePath(QStringLiteral("steam-8000"))));
    QVERIFY(!QFileInfo::exists(m_data + QStringLiteral("/art/steam-2231450"))); // no art copied either
    // Steam's files are only read. No sync either: no record, nothing copied or queued.
    QCOMPARE(readFile(localConfig), config);
    QVERIFY(!QFileInfo::exists(m_state));
    QVERIFY(library->store().snapshot().steamQueued.isEmpty());
}

void LibrarySteamImportTest::steamGamesInList()
{
    const QString root = installFakeSteam();
    const QString platforms = m_data + QStringLiteral("/library/platforms.toml");
    QByteArray withoutSteam = readFile(platforms);
    withoutSteam.replace("[[platform]]\nid = \"steam\"\nname = \"Steam\"\nicon = \"steam\"\n\n", "");
    QVERIFY(!withoutSteam.contains("\"steam\""));
    writeFile(platforms, withoutSteam);
    auto library = openLibrary();

    // Steam found: its platform is added back, once.
    QCOMPARE(readFile(platforms), withoutSteam + "\n[[platform]]\nid = \"steam\"\nname = \"Steam\"\nicon = \"steam\"\n");
    QCOMPARE(library->game(QStringLiteral("steam-2231450"))->platformName, QStringLiteral("Steam"));

    // Installed, with the size, the folder and Steam's play time.
    const Game *pizza = library->game(QStringLiteral("steam-2231450"));
    QVERIFY(pizza);
    QVERIFY(pizza->installed);
    QCOMPARE(pizza->sizeBytes, 404028818);
    QCOMPARE(pizza->installDir, root + QStringLiteral("/steamapps/common/Pizza Tower"));
    QCOMPARE(pizza->playMinutes, 4085);
    QCOMPARE(pizza->steamPlayMinutes, 4085);
    QCOMPARE(pizza->lastPlayed, QDateTime::fromSecsSinceEpoch(1788735088));
    QVERIFY(library->rowOfGame(QStringLiteral("steam-2231450")) >= 0);
    const QVariantMap row = library->gameAt(library->rowOfGame(QStringLiteral("steam-2231450")));
    QVERIFY(row.value("installed").toBool());
    QVERIFY(!row.value("sizeText").toString().isEmpty());

    // The editor's Launch card: the installed tools, Steam's default, the presets, the folder.
    const QVariantMap editing = library->gameForEditing(QStringLiteral("steam-2231450"));
    QCOMPARE(editing.value("installPath").toString(), pizza->installDir);
    QStringList tools;
    for (const QVariant &t : editing.value("protonTools").toList())
        tools.append(t.toMap().value("name").toString() + u'=' + t.toMap().value("displayName").toString());
    QCOMPARE(tools, QStringList({QStringLiteral("proton_experimental=Proton Experimental"),
                                 QStringLiteral("GE-Proton10-4=GE-Proton10-4")}));
    QCOMPARE(editing.value("protonDefault").toString(), QStringLiteral("Proton Experimental"));
    QStringList presets;
    for (const QVariant &p : editing.value("steamPresets").toList())
        presets.append(p.toMap().value("id").toString());
    QCOMPARE(presets, QStringList({QStringLiteral("mangohud"), QStringLiteral("gamemode"), QStringLiteral("wayland")}));
    QVERIFY(library->gameForEditing(QStringLiteral("steam-1145360")).value("installPath").toString().isEmpty());
    QVERIFY(library->gameForEditing(QStringLiteral("switch-metroid-dread")).value("protonTools").toList().isEmpty());

    // Its preset check boxes.
    QVERIFY(!library->steamPresetOn(QString(), QStringLiteral("mangohud")));
    QCOMPARE(library->setSteamPreset(QStringLiteral("-skipintro"), QStringLiteral("mangohud"), true),
             QStringLiteral("mangohud %command% -skipintro"));
    QVERIFY(library->steamPresetOn(QStringLiteral("mangohud %command% -skipintro"), QStringLiteral("mangohud")));
    QCOMPARE(library->setSteamPreset(QStringLiteral("mangohud %command% -skipintro"), QStringLiteral("mangohud"), false),
             QStringLiteral("-skipintro"));
    QCOMPARE(library->setSteamPreset(QStringLiteral("-skipintro"), QStringLiteral("nope"), true), QStringLiteral("-skipintro"));
    QVERIFY(!library->steamPresetOn(QStringLiteral("mangohud %command%"), QStringLiteral("nope")));

    // In steam.toml without a manifest: not installed, and left out of the list with the count.
    const Game *hades = library->game(QStringLiteral("steam-1145360"));
    QVERIFY(hades);
    QVERIFY(!hades->installed);
    QCOMPARE(hades->sizeBytes, 0);
    QVERIFY(hades->installDir.isEmpty());
    QCOMPARE(hades->steamPlayMinutes, -1);
    QCOMPARE(library->rowOfGame(QStringLiteral("steam-1145360")), -1);
    library->setPlatformFilter(QStringLiteral("steam"));
    QCOMPARE(library->notInstalledHidden(), 8); // the demo's Steam games but the hidden Counter-Strike 2
    library->setPlatformFilter({});

    // Steam's, read-only: a value in the game file is ignored, with a problem saying so.
    writeFile(gamePath(QStringLiteral("steam-2231450")), "play_minutes = 5\nfavorite = true\n");
    QTRY_VERIFY_WITH_TIMEOUT(library->game(QStringLiteral("steam-2231450"))->favorite, 5000);
    QCOMPARE(library->game(QStringLiteral("steam-2231450"))->playMinutes, 4085);
    QVERIFY(anyContains(library->problems(),
                        QStringLiteral("steam-2231450.toml: play_minutes and last_played are Steam's")));

    // Uninstalled in Steam: stays, not installed.
    QVERIFY(QFile::remove(root + QStringLiteral("/steamapps/appmanifest_8000.acf")));
    QTRY_VERIFY_WITH_TIMEOUT(!library->game(QStringLiteral("steam-8000"))->installed, 5000);
    QCOMPARE(library->game(QStringLiteral("steam-8000"))->sizeBytes, 0);
    QCOMPARE(library->game(QStringLiteral("steam-8000"))->playMinutes, 70); // Steam keeps it
    library->store().waitForIdle();
    QCoreApplication::processEvents();
    QCOMPARE(readFile(platforms).count("id = \"steam\""), 1);

    // The next start shows the same from the cache, before any scan.
    library.reset();
    Library cached(m_data, m_cache);
    QVERIFY(!cached.loading());
    QVERIFY(cached.game(QStringLiteral("steam-2231450"))->installed);
    QCOMPARE(cached.game(QStringLiteral("steam-2231450"))->sizeBytes, 404028818);
    QCOMPARE(cached.game(QStringLiteral("steam-2231450"))->steamPlayMinutes, 4085);
    QVERIFY(!cached.game(QStringLiteral("steam-8000"))->installed);
}

void LibrarySteamImportTest::steamOffNotInstalled()
{
    // Without Steam (or with the import off) no Steam game can launch.
    QFile::remove(m_data + QStringLiteral("/settings.toml"));
    auto library = openLibrary();
    QVERIFY(!library->game(QStringLiteral("steam-1145360"))->installed);
    QCOMPARE(library->rowOfGame(QStringLiteral("steam-1145360")), -1);
    QVERIFY(!library->store().snapshot().steam.install.found());
}

void LibrarySteamImportTest::steamArtCopiedOnce()
{
    const QString root = installFakeSteam();
    const QString cache = root + QStringLiteral("/appcache/librarycache/2231450/");
    const QString pizzaArt = m_data + QStringLiteral("/art/steam-2231450/");
    const QString tombArt = m_data + QStringLiteral("/art/steam-8000/");
    // The user's own art wins: Tomb Raider's hero isn't replaced.
    const QByteArray mine = readFile(QStringLiteral(STEAM_DATA_DIR "/appcache/librarycache/8000/library_hero.jpg"));
    writeFile(tombArt + QStringLiteral("hero.jpg"), mine);
    auto library = openLibrary();

    // Copied into art/<id>/, keeping each file's format.
    QCOMPARE(QDir(pizzaArt).entryList(QDir::Files), (QStringList{"cover.jpg", "hero.jpg", "icon.jpg", "logo.png"}));
    QCOMPARE(readFile(pizzaArt + QStringLiteral("hero.jpg")),
             readFile(cache + QStringLiteral("5f1c0e2a9b7d4c3e8a6f1b2d3c4e5f6a7b8c9d0e/library_hero.jpg")));
    QCOMPARE(QDir(tombArt).entryList(QDir::Files), (QStringList{"cover.jpg", "hero.jpg"}));
    QCOMPARE(readFile(tombArt + QStringLiteral("hero.jpg")), mine);
    const QVariantMap pizza = library->gameAt(library->rowOfGame(QStringLiteral("steam-2231450")));
    QVERIFY(pizza.value("heroUrl").toUrl().toLocalFile().startsWith(pizzaArt + QStringLiteral("hero.jpg")));
    QVERIFY(pizza.value("logoUrl").toUrl().toLocalFile().startsWith(pizzaArt));

    // Once: Steam's newer art doesn't replace the copy.
    const QByteArray copy = readFile(pizzaArt + QStringLiteral("logo.png"));
    writeFile(cache + QStringLiteral("logo.png"), readFile(pizzaArt + QStringLiteral("icon.jpg")));
    library->store().refresh();
    library->store().waitForIdle();
    QCoreApplication::processEvents();
    QCOMPARE(readFile(pizzaArt + QStringLiteral("logo.png")), copy);

    // Removed in the editor: remembered, so it isn't copied back.
    QCOMPARE(library->removeArt(QStringLiteral("steam-2231450"), QStringLiteral("hero")), QString());
    QCOMPARE(readFile(gamePath(QStringLiteral("steam-2231450"))), QByteArray("skip_steam_art = [\"hero\"]\n"));
    library->store().refresh();
    library->store().waitForIdle();
    QCoreApplication::processEvents();
    QVERIFY(!QFile::exists(pizzaArt + QStringLiteral("hero.jpg")));
    // A kind Steam doesn't have isn't written down.
    QCOMPARE(library->removeArt(QStringLiteral("steam-8000"), QStringLiteral("hero")), QString());
    QCOMPARE(library->removeArt(QStringLiteral("steam-8000"), QStringLiteral("logo")), QString());
    // (Its launch options and Proton version are Steam's, copied by the sync.)
    QVERIFY(readFile(gamePath(QStringLiteral("steam-8000"))).endsWith("proton = \"GE-Proton10-4\"\nskip_steam_art = [\"hero\"]\n"));

    // Taken off the list by hand: copied again.
    QVERIFY(QFile::remove(gamePath(QStringLiteral("steam-2231450"))));
    QTRY_VERIFY_WITH_TIMEOUT(QFile::exists(pizzaArt + QStringLiteral("hero.jpg")), 5000);
    QVERIFY(QDir(pizzaArt).entryList(QDir::Hidden | QDir::Files).filter(QStringLiteral(".steam-")).isEmpty());
}

void LibrarySteamImportTest::steamSettingsPage()
{
    const QString root = installFakeSteam();
    const QString file = m_data + QStringLiteral("/settings.toml");
    auto library = openLibrary();
    const auto steam = [&] { return library->steam(); };

    QVERIFY(steam().value("enabled").toBool());
    QVERIFY(steam().value("found").toBool());
    QVERIFY(!steam().value("flatpak").toBool());
    QVERIFY(!steam().value("problem").toBool());
    QCOMPARE(steam().value("path").toString(), QString());
    QCOMPARE(steam().value("userId").toString(), QStringLiteral("39734273"));
    const QVariantList users = steam().value("users").toList();
    QCOMPARE(users.size(), 2);
    QCOMPARE(users.at(0).toMap().value("name").toString(), QStringLiteral("Player One"));
    QVERIFY(users.at(0).toMap().value("mostRecent").toBool());
    QCOMPARE(users.at(1).toMap().value("id").toString(), QStringLiteral("39734274"));
    const QList<SteamApp> games = library->store().snapshot().steam.games();
    const int installed = int(std::count_if(games.cbegin(), games.cend(), [](const SteamApp &a) { return a.installed; }));
    QCOMPARE(steam().value("installed").toInt(), installed);
    QCOMPARE(steam().value("status").toString(),
             QStringLiteral("Steam found at %1, %2 game(s) installed.").arg(root).arg(installed));
    QStringList presets;
    for (const QVariant &p : steam().value("presets").toList())
        presets.append(p.toMap().value("id").toString());
    QCOMPARE(presets, (QStringList{"mangohud", "gamemode", "wayland"}));
    QCOMPARE(steam().value("defaultPresets").toStringList(), QStringList());

    // Each setting is written to [steam] and shows right away; its default removes the key.
    QCOMPARE(library->setSteamValue(QStringLiteral("default_presets"), QStringList{"mangohud", "wayland"}), QString());
    QCOMPARE(readFile(file).trimmed(), QByteArray("[steam]\ndefault_presets = [\"mangohud\", \"wayland\"]"));
    QCOMPARE(steam().value("defaultPresets").toStringList(), (QStringList{"mangohud", "wayland"}));
    QCOMPARE(library->setSteamValue(QStringLiteral("default_presets"), QStringList()), QString());
    QVERIFY(!readFile(file).contains("default_presets"));

    // The account: a number, as the userdata folder reads.
    QCOMPARE(library->setSteamValue(QStringLiteral("user"), QStringLiteral("39734274")), QString());
    QVERIFY(readFile(file).contains("user = 39734274\n"));
    QTRY_COMPARE_WITH_TIMEOUT(steam().value("userId").toString(), QStringLiteral("39734274"), 5000);
    QCOMPARE(library->setSteamValue(QStringLiteral("user"), QString()), QString());
    QTRY_COMPARE_WITH_TIMEOUT(steam().value("userId").toString(), QStringLiteral("39734273"), 5000);

    // A folder without Steam is a problem, saying where it looked.
    const QString nowhere = m_dir->filePath(QStringLiteral("nowhere"));
    QCOMPARE(library->setSteamValue(QStringLiteral("path"), nowhere), QString());
    QTRY_VERIFY_WITH_TIMEOUT(steam().value("problem").toBool(), 5000);
    QVERIFY(!steam().value("found").toBool());
    QVERIFY(steam().value("status").toString().contains(nowhere));
    QCOMPARE(library->setSteamValue(QStringLiteral("path"), QString()), QString());
    QTRY_VERIFY_WITH_TIMEOUT(steam().value("found").toBool(), 5000);

    // Off: said so right away, and nothing found.
    QCOMPARE(library->setSteamValue(QStringLiteral("enabled"), false), QString());
    QVERIFY(readFile(file).contains("enabled = false\n"));
    QVERIFY(!steam().value("enabled").toBool());
    QVERIFY(steam().value("status").toString().startsWith(QStringLiteral("Off:")));
    QTRY_VERIFY_WITH_TIMEOUT(!library->store().snapshot().steam.install.found(), 5000);
    QVERIFY(!steam().value("problem").toBool());
    QCOMPARE(library->setSteamValue(QStringLiteral("enabled"), true), QString());
    QTRY_VERIFY_WITH_TIMEOUT(steam().value("found").toBool(), 5000);
    QCOMPARE(readFile(file).trimmed(), QByteArray("[steam]"));

    QVERIFY(!library->setSteamValue(QStringLiteral("enable"), false).isEmpty());
    QCOMPARE(readFile(file).trimmed(), QByteArray("[steam]"));
}

void LibrarySteamImportTest::oversizedArtIsShrunk()
{
    auto library = openLibrary();
    const QString id = QStringLiteral("switch-pikmin-4");
    const QString folder = m_data + QStringLiteral("/art/") + id;

    // A logo far larger than the game page shows: scaled to fit 1280, still a PNG with its
    // transparency. Drawn out to two corners, so there's no margin to trim.
    QImage big(6000, 2000, QImage::Format_ARGB32);
    big.fill(Qt::transparent);
    big.setPixelColor(5999, 0, Qt::red);
    big.setPixelColor(0, 1999, Qt::red);
    const QString bigFile = m_dir->filePath(QStringLiteral("big-logo.png"));
    QVERIFY(big.save(bigFile));
    QCOMPARE(library->store().setArt(id, QStringLiteral("logo"), bigFile), QString());
    const QImage logo(folder + QStringLiteral("/logo.png"));
    QCOMPARE(logo.size(), QSize(1280, 426));
    QVERIFY(logo.hasAlphaChannel());
    QCOMPARE(logo.pixelColor(0, 0).alpha(), 0);

    // A hero that's too big too, as a JPEG.
    QImage hero(5000, 1500, QImage::Format_RGB32);
    hero.fill(Qt::darkGreen);
    const QString heroFile = m_dir->filePath(QStringLiteral("big-hero.jpg"));
    QVERIFY(hero.save(heroFile));
    QCOMPARE(library->store().setArt(id, QStringLiteral("hero"), heroFile), QString());
    QImageReader heroReader(folder + QStringLiteral("/hero.jpg"));
    QCOMPARE(heroReader.format(), QByteArray("jpeg"));
    QCOMPARE(heroReader.size(), QSize(3840, 1152));

    // Art the app shows at its size or less is copied as it is.
    QImage small(400, 600, QImage::Format_RGB32);
    small.fill(Qt::blue);
    const QString smallFile = m_dir->filePath(QStringLiteral("cover.png"));
    QVERIFY(small.save(smallFile));
    QCOMPARE(library->store().setArt(id, QStringLiteral("cover"), smallFile), QString());
    QCOMPARE(readFile(folder + QStringLiteral("/cover.png")), readFile(smallFile));
}

void LibrarySteamImportTest::steamDownloadsFollowTheLog()
{
    // The lines Steam wrote for a real update: it ran, then was paused.
    const QByteArray log =
        "[2026-10-09 08:52:31] AppID 1184370 update started : download 0/20969404624, store 0/0, reuse 0/2771391903, "
        "delta 0/0, stage 0/29292040346 \n"
        "[2026-10-09 08:52:31] AppID 1184370 state changed : Update Required,Fully Installed,Update Queued,Update Running,"
        "Update Started,\n"
        "[2026-10-09 08:52:31] AppID 99 state changed : Update Running,\n"
        "[2026-10-09 08:52:54] AppID 1184370 state changed : Update Required,Fully Installed,Update Queued,Update Started, "
        "(Suspended)\n";
    auto state = SteamDownloads::parseLog(log, {1184370, 77});
    QVERIFY(!state.value(1184370).running);
    QCOMPARE(state.value(1184370).stageTotal, 29292040346);
    QVERIFY(!state.value(77).running); // not in the log
    state = SteamDownloads::parseLog(log.left(log.indexOf("[2026-10-09 08:52:54]")), {1184370});
    QVERIFY(state.value(1184370).running);

    // Running or paused from the log's end, and the percentage from the folder against the stage total.
    const QString logPath = m_dir->filePath(QStringLiteral("content_log.txt"));
    const QString folder = m_dir->filePath(QStringLiteral("downloading/1184370"));
    QDir().mkpath(folder);
    writeFile(folder + QStringLiteral("/chunk"), QByteArray(1 << 20, 'x'));
    writeFile(logPath, log.left(log.indexOf("[2026-10-09 08:52:54]")).replace("29292040346", "10485760"));
    SteamDownloads downloads;
    QSignalSpy changed(&downloads, &SteamDownloads::changed);
    downloads.setApps(logPath, {{1184370, folder}});
    QTRY_VERIFY_WITH_TIMEOUT(downloads.progress().value(1184370).running
                                 && downloads.progress().value(1184370).percent == 10,
                             5000);
    // Steam suspends it: the log's last state has no "running", and the polling stops.
    QFile file(logPath);
    QVERIFY(file.open(QIODevice::Append));
    file.write("[x] AppID 1184370 state changed : Update Started, (Suspended)\n");
    file.close();
    QTRY_VERIFY_WITH_TIMEOUT(!downloads.progress().value(1184370).running, 5000);
    QCOMPARE(downloads.progress().value(1184370).percent, 10);
    // Nothing started: nothing kept.
    downloads.setApps({}, {});
    QVERIFY(downloads.progress().isEmpty());
}

QTEST_GUILESS_MAIN(LibrarySteamImportTest)

#include "library_steamimport_test.moc"
