#include "librarytestbase.h"

// The library tests on writing games: values, adding and removing, art, the editor's API.
class LibraryEditTest : public LibraryTestBase
{
    Q_OBJECT

private slots:
    void favoriteKeepsTheRestOfTheFile();
    void favoriteOnIndexOnlyGameCreatesFile();
    void logoPlacementIsOneWrite();
    void resetScrimIsOneWrite();
    void clearingRemovesKeys();
    void unchangedValuesWriteNothing();
    void lookupGames();
    void batchWritesRebuildOnce();
    void emptiedFileIsDeleted();
    void addGameOutsideRomFolders();
    void addGameInsideRomFolder();
    void addGameRefused();
    void removeGameFile();
    void setGamesValueReloadsOnceAndUndoes();
    void hideAndUnhide();
    void setAndRemoveArt();
    void editorApi();
    void removeAndUnhideFromQml();
    void recordPlaySession();
    void invalidValueWritesNothing();
    void brokenFileIsNotWritten();
    void savedValueSurvivesRestart();
    void artCopyErrors();
};

void LibraryEditTest::favoriteKeepsTheRestOfTheFile()
{
    const QByteArray before = "# My notes\n"
                              "title = \"Pikmin 4\"\n"
                              "platform = \"switch\" # the Switch\n"
                              "\n"
                              "# trailing comment\n";
    writeFile(gamePath("switch-pikmin-4"), before);
    auto library = openLibrary();
    const int parsed = library->store().filesParsed();

    QCOMPARE(library->setGameValue(QStringLiteral("switch-pikmin-4"), QStringLiteral("favorite"), true), QString());
    // The model has it right away, before any rescan.
    QVERIFY(library->gameAt(library->rowOfGame(QStringLiteral("switch-pikmin-4"))).value("favorite").toBool());
    QCOMPARE(readFile(gamePath("switch-pikmin-4")),
             QByteArray("# My notes\n"
                        "title = \"Pikmin 4\"\n"
                        "platform = \"switch\" # the Switch\n"
                        "favorite = true\n"
                        "\n"
                        "# trailing comment\n"));

    // Unfavoriting removes the line.
    QCOMPARE(library->setGameValue(QStringLiteral("switch-pikmin-4"), QStringLiteral("favorite"), false), QString());
    QCOMPARE(readFile(gamePath("switch-pikmin-4")), before);

    // The watcher sees the app's own writes, but the recorded stamps say nothing changed.
    QTest::qWait(700);
    library->store().waitForIdle();
    QCoreApplication::processEvents();
    QCOMPARE(library->store().filesParsed(), parsed);
}

void LibraryEditTest::favoriteOnIndexOnlyGameCreatesFile()
{
    auto library = openLibrary();
    QVERIFY(!QFile::exists(gamePath("steam-504230")));
    QCOMPARE(library->setGameValue(QStringLiteral("steam-504230"), QStringLiteral("favorite"), true), QString());
    QCOMPARE(readFile(gamePath("steam-504230")), QByteArray("favorite = true\n"));
    const QVariantMap celeste = library->gameAt(library->rowOfGame(QStringLiteral("steam-504230")));
    QCOMPARE(celeste.value("title").toString(), QStringLiteral("Celeste")); // still from the index
    QVERIFY(celeste.value("favorite").toBool());

    QVERIFY(!library->setGameValue(QStringLiteral("steam-1"), QStringLiteral("favorite"), true).isEmpty());
    QVERIFY(!QFile::exists(gamePath("steam-1")));
}

void LibraryEditTest::logoPlacementIsOneWrite()
{
    auto library = openLibrary();
    const QString id = QStringLiteral("switch-pikmin-4");
    const auto logo = [&] { return library->store().snapshot().content.games.value(id); };
    QSignalSpy changed(&library->store(), &LibraryStore::changed);

    QCOMPARE(library->saveGame(id, {{"logo_position", "center_right"}, {"logo_offset_x", -0.0625},
                                    {"logo_offset_y", 0.25}, {"logo_scale", 1.5}}),
             QString());
    QCOMPARE(changed.size(), 1);
    QCOMPARE(logo().logoPosition, QStringLiteral("center_right"));
    QCOMPARE(logo().logoOffsetX, -0.0625);
    QCOMPARE(logo().logoOffsetY, 0.25);
    QCOMPARE(logo().logoScale, 1.5);

    // What a preset or Reset position saves.
    QCOMPARE(library->saveGame(id, {{"logo_position", QVariant()}, {"logo_offset_x", QVariant()},
                                    {"logo_offset_y", QVariant()}}),
             QString());
    QVERIFY(logo().logoPosition.isEmpty());
    QCOMPARE(logo().logoOffsetX, 0.0);
    QCOMPARE(logo().logoOffsetY, 0.0);
    QCOMPARE(logo().logoScale, 1.5);
    const QByteArray text = readFile(gamePath(id));
    QVERIFY(!text.contains("logo_offset"));
    QVERIFY(!text.contains("logo_position"));
}

void LibraryEditTest::resetScrimIsOneWrite()
{
    auto library = openLibrary();
    const QString id = QStringLiteral("switch-pikmin-4");
    const auto logo = [&] { return library->store().snapshot().content.games.value(id); };
    QSignalSpy changed(&library->store(), &LibraryStore::changed);

    QCOMPARE(library->saveGame(id, {{"logo_scrim", "shadow"}, {"logo_scrim_side", "left"}, {"logo_scrim_strength", 1.5}}),
             QString());
    QCOMPARE(logo().logoScrim, QStringLiteral("shadow"));
    QCOMPARE(logo().logoScrimSide, QStringLiteral("left"));
    QCOMPARE(logo().logoScrimStrength, 1.5);

    // What Reset to theme saves.
    changed.clear();
    QCOMPARE(library->saveGame(id, {{"logo_scrim", QVariant()}, {"logo_scrim_side", QVariant()},
                                    {"logo_scrim_strength", QVariant()}}),
             QString());
    QCOMPARE(changed.size(), 1);
    QVERIFY(logo().logoScrim.isEmpty());
    QVERIFY(logo().logoScrimSide.isEmpty());
    QCOMPARE(logo().logoScrimStrength, 1.0);
    QVERIFY(!readFile(gamePath(id)).contains("logo_scrim"));
}

void LibraryEditTest::clearingRemovesKeys()
{
    const QByteArray before = "# Notes\n"
                              "title = \"Pikmin 4\"\n"
                              "platform = \"switch\"\n"
                              "description = \"Plants.\" # short\n"
                              "rating = 4.5\n"
                              "tags = [\"Co-op\"]\n"
                              "args = [\"--mod\"]\n";
    writeFile(gamePath("switch-pikmin-4"), before);
    auto library = openLibrary();
    const QString id = QStringLiteral("switch-pikmin-4");
    QCOMPARE(library->store().setGameValues(id, {{"description", QString()},
                                                 {"rating", QVariant()},
                                                 {"tags", QStringList()},
                                                 {"args", QVariantList()},
                                                 {"year", 2023}}),
             QString());
    QCOMPARE(readFile(gamePath("switch-pikmin-4")),
             QByteArray("# Notes\n"
                        "title = \"Pikmin 4\"\n"
                        "platform = \"switch\"\n"
                        "year = 2023\n"));
    const QVariantMap pikmin = library->gameAt(library->rowOfGame(id));
    QCOMPARE(pikmin.value("description").toString(), QString());
    QCOMPARE(pikmin.value("year").toInt(), 2023);
    const GameFile &file = library->store().snapshot().content.games.value(id);
    QCOMPARE(file.rating, -1.0);
    QVERIFY(file.tags.isEmpty());
    QVERIFY(file.args.isEmpty());

    // Values that are still checked: a wrong type isn't taken for "cleared".
    QVERIFY(!library->store().setGameValues(id, {{"year", QString()}}).isEmpty());
}

void LibraryEditTest::unchangedValuesWriteNothing()
{
    const QByteArray before = "title = \"Pikmin 4\" # mine\nplatform = \"switch\"\nfavorite = true\n";
    writeFile(gamePath("switch-pikmin-4"), before);
    const QDateTime old = QDateTime::currentDateTime().addDays(-1);
    {
        QFile file(gamePath("switch-pikmin-4"));
        QVERIFY(file.open(QIODevice::ReadWrite));
        QVERIFY(file.setFileTime(old, QFileDevice::FileModificationTime));
    }
    auto library = openLibrary();
    QSignalSpy changed(&library->store(), &LibraryStore::changed);
    QCOMPARE(library->store().setGameValues(QStringLiteral("switch-pikmin-4"),
                                            {{"title", QStringLiteral("Pikmin 4")}, {"favorite", true}, {"hidden", false}}),
             QString());
    QCOMPARE(readFile(gamePath("switch-pikmin-4")), before);
    QCOMPARE(QFileInfo(gamePath("switch-pikmin-4")).lastModified(), old);
    QVERIFY(changed.isEmpty());

    // Clearing keys a game without a file doesn't have creates no file.
    QCOMPARE(library->store().setGameValues(QStringLiteral("steam-504230"),
                                            {{"favorite", false}, {"tags", QStringList()}, {"description", QString()}}),
             QString());
    QVERIFY(!QFile::exists(gamePath("steam-504230")));
    QVERIFY(!library->store().snapshot().content.games.contains(QStringLiteral("steam-504230")));
}

void LibraryEditTest::lookupGames()
{
    writeFile(gamePath("switch-pikmin-4"), "title = \"Pikmin 4\"\nplatform = \"switch\"\nigdb = 0\n");
    auto library = openLibrary();
    const QList<LookupGame> games = library->lookupGames();
    const auto find = [&](const QString &id) {
        return *std::find_if(games.cbegin(), games.cend(), [&](const LookupGame &g) { return g.id == id; });
    };
    QVERIFY(std::none_of(games.cbegin(), games.cend(), [](const LookupGame &g) { return g.id == "steam-730"; })); // hidden
    const LookupGame hades = find(QStringLiteral("steam-1145360"));
    QCOMPARE(hades.steamAppId, 1145360);
    QCOMPARE(hades.igdbPlatforms, QList<int>{6});
    const LookupGame pikmin = find(QStringLiteral("switch-pikmin-4"));
    QCOMPARE(pikmin.igdb, 0);
    QCOMPARE(pikmin.igdbPlatforms, QList<int>{130});
    QVERIFY(!pikmin.hasDescription);
    QCOMPARE(pikmin.missing().first(), QStringLiteral("description"));
    QCOMPARE(pikmin.steamAppId, 0);
}

void LibraryEditTest::batchWritesRebuildOnce()
{
    auto library = openLibrary();
    QSignalSpy changed(&library->store(), &LibraryStore::changed);
    const QHash<QString, QString> errors = library->saveGames({
        {QStringLiteral("switch-pikmin-4"), {{"igdb", 1942}, {"year", 2023}}},
        {QStringLiteral("steam-504230"), {{"igdb", 0}}},
        {QStringLiteral("no-such-game"), {{"igdb", 1}}},
        {QStringLiteral("switch-metroid-dread"), {{"steamgriddb", 5}}},
    });
    QCOMPARE(errors.size(), 1);
    QVERIFY(errors.contains(QStringLiteral("no-such-game")));
    QCOMPARE(changed.size(), 1);
    QCOMPARE(library->store().snapshot().content.games.value(QStringLiteral("switch-pikmin-4")).igdb, 1942);
    QCOMPARE(library->store().snapshot().content.games.value(QStringLiteral("steam-504230")).igdb, 0);

    const QString image = m_dir->filePath(QStringLiteral("cover.png"));
    QImage(60, 90, QImage::Format_RGB32).save(image);
    changed.clear();
    const QHash<QString, QString> artErrors = library->setArts({
        {QStringLiteral("switch-pikmin-4"), QStringLiteral("cover"), QUrl::fromLocalFile(image).toString()},
        {QStringLiteral("steam-504230"), QStringLiteral("cover"), image},
        {QStringLiteral("steam-504230"), QStringLiteral("hero"), m_dir->filePath(QStringLiteral("missing.png"))},
    });
    QCOMPARE(artErrors.keys(), QStringList{"steam-504230"});
    QCOMPARE(changed.size(), 1);
    QVERIFY(QFile::exists(m_data + QStringLiteral("/art/switch-pikmin-4/cover.png")));
    QVERIFY(QFile::exists(m_data + QStringLiteral("/art/steam-504230/cover.png")));
}

void LibraryEditTest::emptiedFileIsDeleted()
{
    auto library = openLibrary();
    const QString id = QStringLiteral("steam-504230");
    QCOMPARE(library->setGameValue(id, QStringLiteral("favorite"), true), QString());
    QVERIFY(QFile::exists(gamePath("steam-504230")));
    QCOMPARE(library->setGameValue(id, QStringLiteral("favorite"), false), QString());
    QVERIFY(!QFile::exists(gamePath("steam-504230")));
    QVERIFY(!library->store().snapshot().content.games.contains(id));
    QVERIFY(!library->gameAt(library->rowOfGame(id)).value("favorite").toBool());

    // A file with a comment left in it stays.
    writeFile(gamePath("steam-504230"), "# keep me\nfavorite = true\n");
    library->store().refresh();
    library->store().waitForIdle();
    QCoreApplication::processEvents();
    QCOMPARE(library->setGameValue(id, QStringLiteral("favorite"), false), QString());
    QCOMPARE(readFile(gamePath("steam-504230")), QByteArray("# keep me\n"));
}

void LibraryEditTest::addGameOutsideRomFolders()
{
    const QString rom = m_installs.home + QStringLiteral("/Downloads/SUPER MARIO ODYSSEY.nsp");
    writeFile(rom, {});
    auto library = openLibrary();
    LibraryStore &store = library->store();

    const LibraryStore::AddedGame added =
        store.addGame(rom, QStringLiteral("switch"), QStringLiteral("Super Mario Odyssey"),
                      {{QStringLiteral("favorite"), true}, {QStringLiteral("tags"), QStringList()}});
    QCOMPARE(added.error, QString());
    QVERIFY(!added.existing);
    QCOMPARE(added.id, QStringLiteral("switch-super-mario-odyssey"));
    // title, platform, rom, then the rest in GameFields' order; ~ for the home folder.
    QCOMPARE(readFile(gamePath(added.id)),
             QByteArray("title = \"Super Mario Odyssey\"\n"
                        "platform = \"switch\"\n"
                        "rom = \"~/Downloads/SUPER MARIO ODYSSEY.nsp\"\n"
                        "favorite = true\n"
                        "added = ")
                 + QDate::currentDate().toString(Qt::ISODate).toUtf8() + "\n");
    QVERIFY(!QFile::exists(romIndexPath("switch-super-mario-odyssey")));
    const QVariantMap odyssey = library->gameAt(library->rowOfGame(added.id));
    QVERIFY(odyssey.value("installed").toBool());
    QCOMPARE(odyssey.value("romPath").toString(), rom);
    QVERIFY(odyssey.value("favorite").toBool());

    // The same file again: that game, nothing added.
    const LibraryStore::AddedGame again = store.addGame(rom, QStringLiteral("switch"), QStringLiteral("Other"));
    QVERIFY(again.existing);
    QCOMPARE(again.id, added.id);
    QCOMPARE(QDir(m_data + "/library/games").count(), 28u); // 25 + 1, with . and ..

    // Another file with the same title gets its own id.
    const QString copy = m_dir->filePath(QStringLiteral("elsewhere/Odyssey.xci"));
    writeFile(copy, {});
    const LibraryStore::AddedGame second =
        store.addGame(copy, QStringLiteral("switch"), QStringLiteral("Super Mario Odyssey"));
    QCOMPARE(second.id, QStringLiteral("switch-super-mario-odyssey-2"));
    QVERIFY(readFile(gamePath(second.id)).contains("rom = \"" + QFile::encodeName(copy) + "\"\n"));

    // Nothing to reparse after the app's own writes; after a restart it's still there.
    const int parsed = store.filesParsed();
    QTest::qWait(700);
    store.waitForIdle();
    QCoreApplication::processEvents();
    QCOMPARE(store.filesParsed(), parsed);
    library.reset();
    library = openLibrary();
    QVERIFY(library->gameAt(library->rowOfGame(added.id)).value("installed").toBool());
    QVERIFY(library->problems().isEmpty());
}

void LibraryEditTest::addGameInsideRomFolder()
{
    const QString rom = m_data + QStringLiteral("/roms/switch/New/Celeste.nsp");
    writeFile(rom, {});
    auto library = openLibrary(); // the startup scan already indexed it; start over without it
    QVERIFY(QFile::remove(rom));
    library->store().scanRoms();
    library->store().waitForIdle();
    QCoreApplication::processEvents();

    const QString hades = m_data + QStringLiteral("/roms/switch/Hades [0100535012974000][v0].nsp");
    writeFile(hades, {});
    const LibraryStore::AddedGame added =
        library->store().addGame(hades, QStringLiteral("switch"), QStringLiteral("Hades"));
    QCOMPARE(added.error, QString());
    QCOMPARE(added.id, QStringLiteral("switch-hades"));
    QVERIFY(readFile(romIndexPath("switch"))
                .contains("\"Hades [0100535012974000][v0].nsp\" = { id = \"switch-hades\", title = \"Hades\", "
                          "title_id = \"0100535012974000\" }\n"));
    QVERIFY(!QFile::exists(gamePath(added.id))); // nothing else to say
    QVERIFY(library->gameAt(library->rowOfGame(added.id)).value("installed").toBool());

    // Other values go into a game file.
    const QString dread = m_data + QStringLiteral("/roms/switch/Sub/Dread.nsp");
    writeFile(dread, {});
    const LibraryStore::AddedGame withValues = library->store().addGame(
        dread, QStringLiteral("switch"), QStringLiteral("Metroid Dread"), {{QStringLiteral("year"), 2021}});
    QCOMPARE(withValues.error, QString());
    QCOMPARE(withValues.id, QStringLiteral("switch-metroid-dread-2"));
    QCOMPARE(readFile(gamePath(withValues.id)), QByteArray("year = 2021\n"));

    // A ROM the scan already has is that game.
    const LibraryStore::AddedGame existing = library->store().addGame(
        m_data + QStringLiteral("/roms/switch/Metroid Dread [010093801237C000][v0].nsp"), QStringLiteral("switch"),
        QStringLiteral("Dread"));
    QVERIFY(existing.existing);
    QCOMPARE(existing.id, QStringLiteral("switch-metroid-dread"));

    // The next scan finds nothing new.
    const QByteArray index = readFile(romIndexPath("switch"));
    library->store().scanRoms();
    library->store().waitForIdle();
    QCoreApplication::processEvents();
    QCOMPARE(readFile(romIndexPath("switch")), index);
}

void LibraryEditTest::addGameRefused()
{
    auto library = openLibrary();
    LibraryStore &store = library->store();
    const QString rom = m_installs.home + QStringLiteral("/Celeste.nsp");
    writeFile(rom, {});
    QVERIFY(!store.addGame(rom, QStringLiteral("nope"), QStringLiteral("Celeste")).error.isEmpty());
    QVERIFY(!store.addGame(rom, QStringLiteral("switch"), QStringLiteral("  ")).error.isEmpty());
    QVERIFY(!store.addGame(m_installs.home, QStringLiteral("switch"), QStringLiteral("Home")).error.isEmpty());
    QVERIFY(!store.addGame(rom, QStringLiteral("switch"), QStringLiteral("Celeste"), {{QStringLiteral("year"), -3}})
                 .error.isEmpty());
    QVERIFY(!store.addGame(rom, QStringLiteral("switch"), QStringLiteral("Celeste"), {{QStringLiteral("yaer"), 2018}})
                 .error.isEmpty());
    QVERIFY(!QFile::exists(gamePath("switch-celeste")));
}

void LibraryEditTest::removeGameFile()
{
    const QString rom = m_installs.home + QStringLiteral("/Celeste.nsp");
    writeFile(rom, {});
    auto library = openLibrary();
    const QString id = library->store().addGame(rom, QStringLiteral("switch"), QStringLiteral("Celeste")).id;
    writeFile(m_data + QStringLiteral("/art/") + id + QStringLiteral("/cover.png"), "png");
    library->store().refresh();
    library->store().waitForIdle();
    QCoreApplication::processEvents();

    QCOMPARE(library->store().removeGameFile(id, true), QString());
    QVERIFY(!QFile::exists(gamePath(id)));
    QVERIFY(!QFileInfo::exists(m_data + QStringLiteral("/art/") + id));
    QCOMPARE(library->rowOfGame(id), -1);
    QVERIFY(QFile::exists(rom)); // the ROM itself stays
    QVERIFY(library->store().gameWithRom(rom).isEmpty());
}

void LibraryEditTest::setGamesValueReloadsOnceAndUndoes()
{
    auto library = openLibrary();
    const QStringList ids{"switch-metroid-dread", "switch-pikmin-4", "steam-504230"};
    QMap<QString, QByteArray> before;
    for (const QString &id : ids)
        before.insert(id, QFile::exists(gamePath(id)) ? readFile(gamePath(id)) : QByteArray());
    QSignalSpy changed(&library->store(), &LibraryStore::changed);
    QSignalSpy offered(library.get(), &Library::undoOffered);

    QCOMPARE(library->setGamesValue(ids, QStringLiteral("favorite"), true), QString());
    QCOMPARE(changed.size(), 1);
    for (const QString &id : ids)
        QVERIFY(library->store().snapshot().content.games.value(id).favorite);
    QCOMPARE(offered.size(), 1);
    QCOMPARE(offered.last().at(1).toString(), QStringLiteral("3 games added to favorites"));

    changed.clear();
    QCOMPARE(library->undo(offered.last().at(0).value<quint64>()), QString());
    for (const QString &id : ids)
        QCOMPARE(QFile::exists(gamePath(id)) ? readFile(gamePath(id)) : QByteArray(), before.value(id));

    // An emulator pinned to many games, then back to the platform's default.
    QCOMPARE(library->setGamesValue({"switch-pikmin-4", "switch-metroid-dread"}, QStringLiteral("emulator"),
                                    QStringLiteral("ryubing")),
             QString());
    QCOMPARE(library->store().snapshot().content.games.value(QStringLiteral("switch-pikmin-4")).emulator,
             QStringLiteral("ryubing"));
    QCOMPARE(library->setGamesValue({"switch-pikmin-4", "switch-metroid-dread"}, QStringLiteral("emulator"), QString()),
             QString());
    QVERIFY(library->store().snapshot().content.games.value(QStringLiteral("switch-pikmin-4")).emulator.isEmpty());
    QCOMPARE(library->gameAt(library->rowOfGame(QStringLiteral("switch-pikmin-4"))).value("emulator").toString(), QString());
}

void LibraryEditTest::hideAndUnhide()
{
    auto library = openLibrary();
    const QString id = QStringLiteral("switch-metroid-dread");
    const QByteArray before = readFile(gamePath(id));
    QCOMPARE(library->setGameValue(id, QStringLiteral("hidden"), true), QString());
    QCOMPARE(library->rowOfGame(id), -1);
    QVERIFY(readFile(gamePath(id)).endsWith("hidden = true\n"));
    QCOMPARE(library->setGameValue(id, QStringLiteral("hidden"), false), QString());
    QVERIFY(library->rowOfGame(id) >= 0);
    QCOMPARE(readFile(gamePath(id)), before);
}

void LibraryEditTest::setAndRemoveArt()
{
    const auto image = [&](const QString &name) {
        const QString path = m_dir->filePath(QStringLiteral("pictures/") + name);
        QDir().mkpath(QFileInfo(path).absolutePath());
        QImage picture(8, 4, QImage::Format_RGB32);
        picture.fill(Qt::darkCyan);
        return picture.save(path) ? path : QString();
    };
    const QString png = image(QStringLiteral("Hero.PNG"));
    const QString jpg = image(QStringLiteral("hero shot.jpg"));
    QVERIFY(!png.isEmpty() && !jpg.isEmpty());
    auto library = openLibrary();
    LibraryStore &store = library->store();
    const QString id = QStringLiteral("switch-metroid-dread");
    const QString folder = m_data + QStringLiteral("/art/") + id;
    const auto heroUrl = [&] { return library->gameAt(library->rowOfGame(id)).value("heroUrl").toUrl(); };

    QCOMPARE(store.setArt(id, QStringLiteral("hero"), png), QString());
    QVERIFY(QFile::exists(folder + "/hero.png"));
    QCOMPARE(heroUrl().toLocalFile(), folder + "/hero.png");
    QVERIFY(heroUrl().query().startsWith(QStringLiteral("v=")));

    // Another extension replaces it; the same name again still changes the URL, so it reloads.
    QCOMPARE(store.setArt(id, QStringLiteral("hero"), jpg), QString());
    QVERIFY(!QFile::exists(folder + "/hero.png"));
    QCOMPARE(heroUrl().toLocalFile(), folder + "/hero.jpg");
    const QUrl before = heroUrl();
    QTest::qWait(20);
    QCOMPARE(store.setArt(id, QStringLiteral("hero"), jpg), QString());
    QVERIFY(heroUrl() != before);
    QCOMPARE(store.setArt(id, QStringLiteral("hero"), folder + "/hero.jpg"), QString()); // itself: nothing to do
    QCOMPARE(QDir(folder).entryList(QDir::Files | QDir::Hidden), QStringList{"hero.jpg"});

    // Refused: not an image, an unknown kind, an unknown game. The old art stays.
    const QString text = m_dir->filePath(QStringLiteral("pictures/notes.png"));
    writeFile(text, "not a picture");
    QVERIFY(!store.setArt(id, QStringLiteral("hero"), text).isEmpty());
    QVERIFY(!store.setArt(id, QStringLiteral("banner"), png).isEmpty());
    QVERIFY(!store.setArt(QStringLiteral("switch-nope"), QStringLiteral("hero"), png).isEmpty());
    QCOMPARE(QDir(folder).entryList(QDir::Files | QDir::Hidden), QStringList{"hero.jpg"});

    // Nothing to reparse after the app's own writes.
    QTest::qWait(700);
    store.waitForIdle();
    QCoreApplication::processEvents();
    QCOMPARE(heroUrl().toLocalFile(), folder + "/hero.jpg");

    // A logo loses its transparent margins, faint specks included; other kinds keep theirs.
    QImage padded(40, 20, QImage::Format_ARGB32);
    padded.fill(Qt::transparent);
    padded.setPixel(0, 0, qRgba(0, 0, 0, 3));
    for (int y = 6; y < 9; ++y)
        for (int x = 10; x < 15; ++x)
            padded.setPixel(x, y, qRgba(255, 255, 255, 255));
    const QString paddedPath = m_dir->filePath(QStringLiteral("pictures/padded.png"));
    QVERIFY(padded.save(paddedPath));
    QCOMPARE(store.setArt(id, QStringLiteral("logo"), paddedPath), QString());
    QCOMPARE(QImageReader(folder + "/logo.png").size(), QSize(5, 3));
    QCOMPARE(store.setArt(id, QStringLiteral("cover"), paddedPath), QString());
    QCOMPARE(QImageReader(folder + "/cover.png").size(), QSize(40, 20));
    QCOMPARE(store.removeArt(id, QStringLiteral("logo")), QString());

    QCOMPARE(store.setArt(id, QStringLiteral("cover"), png), QString());
    QCOMPARE(store.removeArt(id, QStringLiteral("hero")), QString());
    QVERIFY(heroUrl().isEmpty());
    QVERIFY(QFile::exists(folder + "/cover.png"));
    QCOMPARE(store.removeArt(id, QStringLiteral("cover")), QString());
    QVERIFY(!QFileInfo::exists(folder)); // the empty folder goes too
}

void LibraryEditTest::editorApi()
{
    auto library = openLibrary();

    const QVariantMap dread = library->gameForEditing(QStringLiteral("switch-metroid-dread"));
    QCOMPARE(dread.value("title").toString(), QStringLiteral("Metroid Dread"));
    QCOMPARE(dread.value("tags").toList().size(), 2);
    QVERIFY(dread.value("rom").toBool());
    QVERIFY(!dread.value("romKey").toBool());
    QVERIFY(!dread.value("removeDeletes").toBool()); // in the ROM index: removing hides it
    QCOMPARE(dread.value("defaultEmulator").toString(), QStringLiteral("Ryubing"));
    const QVariantMap ryubing = dread.value("emulators").toList().first().toMap();
    QCOMPARE(ryubing.value("id").toString(), QStringLiteral("ryubing"));
    QVERIFY(ryubing.value("installed").toBool());
    QVERIFY(library->gameForEditing(QStringLiteral("proton-diablo-iv")).value("removeDeletes").toBool());
    QVERIFY(library->gameForEditing(QStringLiteral("nope")).isEmpty());

    // "+": inspect the picked file (as a FileDialog URL), then add it with values and art.
    const QString rom = m_installs.home + QStringLiteral("/Downloads/celeste.nsp");
    writeFile(rom, {});
    const QVariantMap pick = library->inspectRom(QUrl::fromLocalFile(rom).toString());
    QCOMPARE(pick.value("path").toString(), rom);
    QCOMPARE(pick.value("title").toString(), QStringLiteral("Celeste"));
    QCOMPARE(pick.value("platforms").toStringList(), QStringList{"switch"});
    QCOMPARE(pick.value("existing").toString(), QString());
    QStringList choices;
    for (const QVariant &c : pick.value("choices").toList())
        choices.append(c.toMap().value("id").toString());
    QCOMPARE(choices, (QStringList{"switch", "3ds"})); // the platforms with emulators, in rail order
    QCOMPARE(library->inspectRom(m_data + QStringLiteral("/roms/switch/Homebrew/Tetris.nro")).value("existing").toString(),
             QStringLiteral("switch-tetris"));
    const QStringList filters = library->romNameFilters();
    QCOMPARE(filters.size(), 4);
    QVERIFY(filters.first().contains(QStringLiteral("*.nsp *.NSP")));
    QVERIFY(filters.first().contains(QStringLiteral("*.3ds")));
    // The demo's Steam is found: Windows programs too.
    QCOMPARE(filters.at(1), QStringLiteral("Windows programs (*.exe *.EXE)"));
    QVERIFY(filters.at(2).startsWith(QStringLiteral("Linux programs (")));
    QCOMPARE(filters.last(), QStringLiteral("All files (*)"));

    QImage picture(4, 4, QImage::Format_RGB32);
    picture.fill(Qt::red);
    const QString cover = m_dir->filePath(QStringLiteral("cover.png"));
    QVERIFY(picture.save(cover));
    // As values come from JavaScript: numbers are doubles, lists are variant lists.
    const QVariantMap added = library->addGame(
        QUrl::fromLocalFile(rom).toString(), QStringLiteral("switch"), QStringLiteral("Celeste"),
        {{QStringLiteral("year"), 2018.0}, {QStringLiteral("tags"), QVariantList{"Platformer"}},
         {QStringLiteral("description"), QVariant::fromValue(nullptr)}},
        {{QStringLiteral("cover"), QUrl::fromLocalFile(cover).toString()}});
    QCOMPARE(added.value("error").toString(), QString());
    const QString id = added.value("id").toString();
    QCOMPARE(id, QStringLiteral("switch-celeste"));
    QVERIFY(QFile::exists(m_data + QStringLiteral("/art/switch-celeste/cover.png")));
    const QVariantMap celeste = library->gameForEditing(id);
    QVERIFY(celeste.value("romKey").toBool());
    QCOMPARE(celeste.value("romWritten").toString(), QStringLiteral("~/Downloads/celeste.nsp"));
    QCOMPARE(celeste.value("year").toInt(), 2018);
    QVERIFY(!celeste.value("art").toMap().value("cover").toString().isEmpty());

    // Save: only what's given; null and "" clear.
    QCOMPARE(library->saveGame(id, {{QStringLiteral("year"), QVariant::fromValue(nullptr)},
                                    {QStringLiteral("args"), QVariantList{"--mod"}},
                                    {QStringLiteral("rating"), 3.5}}),
             QString());
    const QByteArray text = readFile(gamePath(id));
    QVERIFY(!text.contains("year"));
    QVERIFY(text.contains("args = [\"--mod\"]\n"));
    QVERIFY(text.contains("rating = 3.5\n"));
    QVERIFY(!library->saveGame(id, {{QStringLiteral("rating"), 7}}).isEmpty());

    // A rating from a lookup says so; setting the rating without saying so makes it yours.
    QCOMPARE(library->saveGame(id, {{QStringLiteral("rating"), 4.1}, {QStringLiteral("rating_source"), "igdb"}}), QString());
    QVERIFY(readFile(gamePath(id)).contains("rating_source = \"igdb\"\n"));
    QTRY_COMPARE(library->gameForEditing(id).value("ratingSource").toString(), QStringLiteral("igdb"));
    QCOMPARE(library->saveGame(id, {{QStringLiteral("rating"), 4.5}}), QString());
    QVERIFY(!readFile(gamePath(id)).contains("rating_source"));
    QTRY_COMPARE(library->gameForEditing(id).value("ratingSource").toString(), QString());

    // Change… on a "+" game's ROM: written with ~, found right away.
    const QString moved = m_installs.home + QStringLiteral("/Games/Celeste.nsp");
    writeFile(moved, {});
    QCOMPARE(library->saveGame(id, {{QStringLiteral("rom"), QUrl::fromLocalFile(moved).toString()}}), QString());
    QVERIFY(readFile(gamePath(id)).contains("rom = \"~/Games/Celeste.nsp\"\n"));
    QCOMPARE(library->gameForEditing(id).value("romPath").toString(), moved);

    // Tags as the Library page lists them; the next free color.
    const QVariantMap first = library->tags().first().toMap();
    QCOMPARE(first.value("name").toString(), QStringLiteral("Adventure"));
    QCOMPARE(first.value("games").toInt(), 7);
    QVERIFY(first.value("listed").toBool());
    QCOMPARE(library->nextTagColor(8), 0); // the demo uses all eight
    QCOMPARE(library->nextTagColor(10), 8);
}

void LibraryEditTest::removeAndUnhideFromQml()
{
    auto library = openLibrary();
    const auto hiddenTitles = [&] {
        QStringList titles;
        for (const QVariant &game : library->hiddenGames())
            titles << game.toMap().value("title").toString();
        return titles;
    };
    // The demo hides Counter-Strike 2.
    QCOMPARE(hiddenTitles(), QStringList{QStringLiteral("Counter-Strike 2")});
    QCOMPARE(library->rowOfGame(QStringLiteral("steam-730")), -1);

    QCOMPARE(library->removeGame(QStringLiteral("switch-metroid-dread")), QString());
    QCOMPARE(library->rowOfGame(QStringLiteral("switch-metroid-dread")), -1);
    QCOMPARE(library->hiddenGames().size(), 2);
    QVERIFY(hiddenTitles().contains(QStringLiteral("Metroid Dread")));
    for (const QVariant &game : library->hiddenGames()) {
        if (game.toMap().value("title").toString() == QStringLiteral("Metroid Dread"))
            QCOMPARE(game.toMap().value("platformName").toString(), QStringLiteral("Switch"));
    }
    QCOMPARE(library->unhideGame(QStringLiteral("switch-metroid-dread")), QString());
    QVERIFY(library->rowOfGame(QStringLiteral("switch-metroid-dread")) >= 0);
    QCOMPARE(hiddenTitles(), QStringList{QStringLiteral("Counter-Strike 2")});

    // Wiping hides it and forgets the rest: its keys but hidden and rom, and its art.
    const QString hades = QStringLiteral("steam-1145360");
    const QString icon = m_dir->filePath(QStringLiteral("icon.png"));
    QVERIFY(QImage(4, 4, QImage::Format_RGB32).save(icon));
    QCOMPARE(library->setArt(hades, QStringLiteral("icon"), icon), QString());
    QVERIFY(readFile(gamePath(hades)).contains("favorite"));
    QCOMPARE(library->removeGame(hades, true), QString());
    QCOMPARE(readFile(gamePath(hades)).trimmed(), QByteArray("hidden = true"));
    QVERIFY(!QDir(m_data + QStringLiteral("/art/") + hades).exists());
    QVERIFY(hiddenTitles().contains(QStringLiteral("Hades")));

    // A game only its file makes is hidden like any other, and wiping it deletes the file.
    const QString rom = m_installs.home + QStringLiteral("/Celeste.nsp");
    writeFile(rom, {});
    const QString celeste = library->store().addGame(rom, QStringLiteral("switch"), QStringLiteral("Celeste")).id;
    library->store().waitForIdle();
    QCoreApplication::processEvents();
    QCOMPARE(library->removeGame(celeste), QString());
    QVERIFY(readFile(gamePath(celeste)).contains("hidden = true"));
    QCOMPARE(library->rowOfGame(celeste), -1);
    QCOMPARE(hiddenTitles().size(), 3);
    QCOMPARE(library->unhideGame(celeste), QString());
    QVERIFY(library->rowOfGame(celeste) >= 0);
    QCOMPARE(library->removeGame(celeste, true), QString());
    QVERIFY(!QFile::exists(gamePath(celeste)));
    QCOMPARE(hiddenTitles().size(), 2); // Counter-Strike 2 and Hades

    // A Proton game is hidden in the launcher only; wiping it is Uninstall's: refused, nothing
    // written.
    const QString ffxiv = QStringLiteral("proton-final-fantasy-xiv-online");
    QCOMPARE(library->removeGame(ffxiv), QString());
    const QByteArray hidden = readFile(gamePath(ffxiv));
    QVERIFY(hidden.contains("hidden = true"));
    QCOMPARE(library->removeGame(ffxiv, true), QStringLiteral("A Proton game is removed with Uninstall"));
    QCOMPARE(readFile(gamePath(ffxiv)), hidden);
}

void LibraryEditTest::recordPlaySession()
{
    auto library = openLibrary();
    const QString id = QStringLiteral("switch-kirby-and-the-forgotten-land"); // 540 minutes
    const QDateTime ended(QDate(2026, 9, 24), QTime(22, 30), QTimeZone::fromSecondsAheadOfUtc(-3 * 3600));
    QCOMPARE(library->recordPlaySession(id, 45, ended), QString());

    const QByteArray text = readFile(gamePath(id));
    QVERIFY2(text.contains("play_minutes = 585\n"), text.constData());
    QVERIFY2(text.contains("last_played = 2026-09-24T22:30:00-03:00\n"), text.constData());
    QVERIFY(text.contains("tags = [\"Platformer\"]\n")); // the rest is untouched

    // The current local time is written with this machine's offset.
    QCOMPARE(library->recordPlaySession(id, 5, QDateTime::currentDateTime()), QString());
    static const QRegularExpression withOffset(
        QStringLiteral("last_played = \\d{4}-\\d\\d-\\d\\dT\\d\\d:\\d\\d:\\d\\d([+-]\\d\\d:\\d\\d|Z)\n"));
    QVERIFY(withOffset.match(QString::fromUtf8(readFile(gamePath(id)))).hasMatch());
    QVERIFY(readFile(gamePath(id)).contains("play_minutes = 590\n"));
}

void LibraryEditTest::invalidValueWritesNothing()
{
    auto library = openLibrary();
    const QByteArray before = readFile(gamePath("switch-pikmin-4"));
    QVERIFY(!library->setGameValue(QStringLiteral("switch-pikmin-4"), QStringLiteral("favorite"), "yes").isEmpty());
    QVERIFY(!library->setGameValue(QStringLiteral("switch-pikmin-4"), QStringLiteral("favourite"), true).isEmpty());
    QCOMPARE(readFile(gamePath("switch-pikmin-4")), before);
}

void LibraryEditTest::brokenFileIsNotWritten()
{
    auto library = openLibrary();
    const QByteArray broken = "title = \"Pikmin 4\"\nplatform = \n";
    writeFile(gamePath("switch-pikmin-4"), broken);
    QTRY_VERIFY_WITH_TIMEOUT(anyContains(library->problems(), QStringLiteral("switch-pikmin-4.toml:2:")), 5000);

    const QString error = library->setGameValue(QStringLiteral("switch-pikmin-4"), QStringLiteral("favorite"), true);
    QVERIFY(error.contains(QStringLiteral("library/games/switch-pikmin-4.toml")));
    QCOMPARE(readFile(gamePath("switch-pikmin-4")), broken);
}

void LibraryEditTest::savedValueSurvivesRestart()
{
    {
        auto library = openLibrary();
        QCOMPARE(library->setGameValue(QStringLiteral("switch-pikmin-4"), QStringLiteral("favorite"), true), QString());
    }
    {
        // From the cache, before any scan.
        Library library(m_data, m_cache);
        QVERIFY(library.gameAt(library.rowOfGame(QStringLiteral("switch-pikmin-4"))).value("favorite").toBool());
    }
    // And from the files alone.
    QVERIFY(QDir(m_cache).removeRecursively());
    auto library = openLibrary();
    QCOMPARE(library->store().filesParsed(), 31);
    QVERIFY(library->gameAt(library->rowOfGame(QStringLiteral("switch-pikmin-4"))).value("favorite").toBool());
}

void LibraryEditTest::artCopyErrors()
{
    const QString art = m_dir->filePath(QStringLiteral("art"));
    QDir().mkpath(art);
    const QString nowhere = m_dir->filePath(QStringLiteral("missing/icon.png"));

    // Small enough to copy as it is, into a folder that isn't there.
    const QString small = art + QStringLiteral("/small.png");
    QImage(64, 64, QImage::Format_RGB32).save(small);
    QCOMPARE(ArtImage::copy(small, nowhere, QStringLiteral("icon")), QStringLiteral("Could not copy small.png"));

    // Too big for an icon, so it's read to be shrunk, but only its header is there.
    const QString big = art + QStringLiteral("/big.png");
    QImage image(1024, 1024, QImage::Format_RGB32);
    image.fill(Qt::red);
    QVERIFY(image.save(big));
    const QString cut = art + QStringLiteral("/cut.png");
    writeFile(cut, readFile(big).left(64));
    QCOMPARE(QImageReader(cut).size(), QSize(1024, 1024));
    const QString target = art + QStringLiteral("/icon.png");
    QVERIFY(ArtImage::copy(cut, target, QStringLiteral("icon")).startsWith(QStringLiteral("cut.png can't be shown: ")));
    QVERIFY(!QFile::exists(target));

    // Shrunk, but the smaller copy can't be written.
    QVERIFY(ArtImage::copy(big, nowhere, QStringLiteral("icon"))
                .startsWith(QStringLiteral("Could not save a smaller copy of big.png: ")));
    QVERIFY(!QFile::exists(nowhere));

    // And where it can, the copy is shrunk to the icon's size.
    QCOMPARE(ArtImage::copy(big, target, QStringLiteral("icon")), QString());
    QCOMPARE(QImageReader(target).size(), QSize(512, 512));
}

QTEST_GUILESS_MAIN(LibraryEditTest)

#include "library_edit_test.moc"
