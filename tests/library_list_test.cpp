#include "librarytestbase.h"

// The library tests on the list model: loading, search, sorting, filters, groups, [list].
class LibraryListTest : public LibraryTestBase
{
    Q_OBJECT

private slots:
    void firstLoadParsesSecondUsesCache();
    void handEditUpdatesModel();
    void addedAndDeletedFiles();
    void brokenFileKeepsLastGoodVersion();
    void platformsAndGroups();
    void searchFolding();
    void searchMatchesEveryWord();
    void sortOrders();
    void sortTiesAndGroups();
    void installedOnlyFilter();
    void favoritesAndTagFilters();
    void filtersAndGroups();
    void collapseSendsOneRange();
    void renameSendsAMove();
    void twoChangesReset();
    void clearFilters();
    void showFiltered();
    void showHiddenGames();
    void sortValue();
    void listSettingsParsing();
    void listSettingsWriting();
    void listSettingsRestoredAtStart();
    void listSettingsHandEdit();
    void groupBySetting();
    void statusGroupsAndFilter();
    void listSettingsFollowTagChanges();
    void emptyDataDir();
    void newFilterSettings();
    void newFiltersRestoredAtStartAndByHandEdit();
    void yearAndRatingFilters();
    void playTimeAndLastPlayedFilters();
    void achievementFilterWaitsForThePass();
    void achievementCollectionWaitsForThePass();
    void achievementFilterRestoredAtStart();
    void filterChoicesAndHistograms();
    void newSortOrders();
    void sortByPlatformFollowsTheRail();
    void sortBySize();
    void sortByAchievements();
    void sortByAchievementsRestoredAtStart();
    void newGroupings();
    void newSortValues();
    void newSortsAndGroupsInSettings();
};

void LibraryListTest::firstLoadParsesSecondUsesCache()
{
    {
        auto library = std::make_unique<Library>(m_data, m_cache);
        library->store().setInstallLocations(m_installs); // else the real machine's emulators count
        library->store().setSteamLocations({m_installs.home, {}});
        library->store().setHeroicLocations(fakeHeroic());
        QVERIFY(library->loading());
        QCOMPARE(library->gameCount(), 0);
        library->startRefresh();
        QTRY_VERIFY(!library->loading());
        QCOMPARE(library->gameCount(), 26);
        QCOMPARE(library->store().filesParsed(), 31); // 4 library files + 2 ROM indexes + 25 game files
        QVERIFY(library->problems().isEmpty());
    } // the destructor saves the cache

    auto library = std::make_unique<Library>(m_data, m_cache);
    QVERIFY(!library->loading());
    QCOMPARE(library->gameCount(), 26); // straight from the cache, before any scan
    library->store().setInstallLocations(m_installs);
    library->store().setSteamLocations({m_installs.home, {}});
    library->store().setHeroicLocations(fakeHeroic());
    library->startRefresh();
    library->store().waitForIdle();
    QCoreApplication::processEvents();
    QCOMPARE(library->store().filesParsed(), 0);
    QCOMPARE(library->gameCount(), 26);
}

void LibraryListTest::handEditUpdatesModel()
{
    auto library = openLibrary();
    const int row = library->rowOfGame(QStringLiteral("switch-pikmin-4"));
    QVERIFY(row >= 0);
    QCOMPARE(library->gameAt(row).value("title").toString(), QStringLiteral("Pikmin 4"));

    QSignalSpy reset(library.get(), &QAbstractItemModel::modelReset);
    QSignalSpy dataChanged(library.get(), &QAbstractItemModel::dataChanged);
    const QByteArray text = readFile(gamePath("switch-pikmin-4"));
    writeFile(gamePath("switch-pikmin-4"), text + "rating = 4.0\n");

    QTRY_COMPARE_WITH_TIMEOUT(library->gameAt(row).value("rating").toDouble(), 4.0, 5000);
    // Same rows: updated in place, without a reset that would move the list.
    QCOMPARE(reset.count(), 0);
    QVERIFY(dataChanged.count() > 0);
}

void LibraryListTest::addedAndDeletedFiles()
{
    auto library = openLibrary();
    // A game coming or going is a row inserted or removed, not a reset, so the list can animate it.
    QSignalSpy reset(library.get(), &QAbstractItemModel::modelReset);
    QSignalSpy inserted(library.get(), &QAbstractItemModel::rowsInserted);
    QSignalSpy removed(library.get(), &QAbstractItemModel::rowsRemoved);
    writeFile(gamePath("switch-tetris-99"), "title = \"Tetris 99\"\nplatform = \"switch\"\n");
    QTRY_VERIFY_WITH_TIMEOUT(library->rowOfGame(QStringLiteral("switch-tetris-99")) >= 0, 5000);
    QCOMPARE(library->gameCount(), 27);
    QCOMPARE(inserted.count(), 1);
    QCOMPARE(removed.count(), 0);
    QCOMPARE(reset.count(), 0);
    const int tetris = library->rowOfGame(QStringLiteral("switch-tetris-99"));
    QCOMPARE(library->gameAt(tetris).value("title").toString(), QStringLiteral("Tetris 99"));
    QCOMPARE(library->gameAt(library->rowOfGame(QStringLiteral("switch-pikmin-4"))).value("title").toString(),
             QStringLiteral("Pikmin 4"));

    QVERIFY(QFile::remove(gamePath("switch-tetris-99")));
    QTRY_COMPARE_WITH_TIMEOUT(library->rowOfGame(QStringLiteral("switch-tetris-99")), -1, 5000);
    QCOMPARE(removed.count(), 1);
    QCOMPARE(reset.count(), 0);

    // Deleting a Steam game's file only forgets the user's changes: the index brings it back.
    QVERIFY(library->gameAt(library->rowOfGame(QStringLiteral("steam-1145360"))).value("favorite").toBool());
    QVERIFY(QFile::remove(gamePath("steam-1145360")));
    QTRY_VERIFY_WITH_TIMEOUT(
        !library->gameAt(library->rowOfGame(QStringLiteral("steam-1145360"))).value("favorite").toBool(), 5000);
    QCOMPARE(library->gameCount(), 26);

    // hidden = true hides it.
    writeFile(gamePath("steam-1145360"), "hidden = true\n");
    QTRY_COMPARE_WITH_TIMEOUT(library->rowOfGame(QStringLiteral("steam-1145360")), -1, 5000);
    QCOMPARE(library->gameCount(), 25);
}

void LibraryListTest::brokenFileKeepsLastGoodVersion()
{
    auto library = openLibrary();
    writeFile(gamePath("switch-pikmin-4"), "title = \"Pikmin 4 (broken)\"\nplatform = \n");
    QTRY_VERIFY_WITH_TIMEOUT(anyContains(library->problems(), QStringLiteral("library/games/switch-pikmin-4.toml:2:")),
                             5000);
    const int row = library->rowOfGame(QStringLiteral("switch-pikmin-4"));
    QVERIFY(row >= 0);
    QCOMPARE(library->gameAt(row).value("title").toString(), QStringLiteral("Pikmin 4"));

    // Fixing the file clears the problem.
    writeFile(gamePath("switch-pikmin-4"), "title = \"Pikmin 4 (fixed)\"\nplatform = \"switch\"\n");
    QTRY_COMPARE_WITH_TIMEOUT(
        library->gameAt(library->rowOfGame(QStringLiteral("switch-pikmin-4"))).value("title").toString(),
        QStringLiteral("Pikmin 4 (fixed)"), 5000);
    QVERIFY(library->problems().isEmpty());
}

void LibraryListTest::platformsAndGroups()
{
    auto library = openLibrary();
    const QVariantList platforms = library->platforms();
    QCOMPARE(platforms.size(), 5);
    QCOMPARE(platforms.at(0).toMap().value("id").toString(), QStringLiteral("switch"));
    QCOMPARE(platforms.at(0).toMap().value("name").toString(), QStringLiteral("Switch"));
    QCOMPARE(platforms.at(3).toMap().value("icon").toString(), QStringLiteral("proton"));
    QCOMPARE(platforms.at(4).toMap().value("icon").toString(), QStringLiteral("linux"));

    // What the setup's Done step shows: the games per platform, in the same order, hidden ones aside.
    const QVariantList counts = library->gameCounts();
    QCOMPARE(counts.size(), platforms.size());
    int total = 0;
    for (qsizetype i = 0; i < counts.size(); ++i) {
        QCOMPARE(counts.at(i).toMap().value("id"), platforms.at(i).toMap().value("id"));
        QCOMPARE(counts.at(i).toMap().value("name"), platforms.at(i).toMap().value("name"));
        total += counts.at(i).toMap().value("count").toInt();
    }
    QCOMPARE(total, library->gameCount());

    // The first row is the Switch header, showing the platform's name.
    const QModelIndex header = library->index(0);
    QVERIFY(library->data(header, Library::IsHeaderRole).toBool());
    QCOMPARE(library->data(header, Library::TitleRole).toString(), QStringLiteral("Switch"));
    QCOMPARE(library->data(header, Library::GroupRole).toString(), QStringLiteral("switch"));

    library->setPlatformFilter(QStringLiteral("3ds"));
    QCOMPARE(library->rowCount(), 4);
    const QVariantMap pokemon = library->gameAt(library->rowOfGame(QStringLiteral("3ds-pokemon-ultra-sun")));
    QCOMPARE(pokemon.value("platform").toString(), QStringLiteral("3ds"));
    QCOMPARE(pokemon.value("platformName").toString(), QStringLiteral("3DS"));
}

void LibraryListTest::searchFolding()
{
    using SearchText::fold;
    QCOMPARE(fold(QStringLiteral("Pokémon Ultra Sun")), QStringLiteral("pokemon ultra sun"));
    QCOMPARE(fold(QStringLiteral("ÉLDEN RING")), QStringLiteral("elden ring"));
    QCOMPARE(fold(QStringLiteral("The Legend of Zelda: Tears of the Kingdom")),
             QStringLiteral("the legend of zelda tears of the kingdom"));
    QCOMPARE(fold(QStringLiteral("Super Mario Bros. Wonder")), QStringLiteral("super mario bros wonder"));
    QCOMPARE(fold(QStringLiteral("  Ys — Origin!! ")), QStringLiteral("ys origin"));
    // Apostrophes join, so "baldurs" finds "Baldur's".
    QCOMPARE(fold(QStringLiteral("Baldur's Gate 3")), QStringLiteral("baldurs gate 3"));
    QCOMPARE(fold(QStringLiteral("Baldur’s Gate 3")), QStringLiteral("baldurs gate 3"));
    QCOMPARE(fold(QStringLiteral("ＺＥＬＤＡ")), QStringLiteral("zelda")); // full-width
    // Other scripts are kept, not dropped like in ids.
    QVERIFY(!fold(QStringLiteral("ゼルダの伝説")).isEmpty());
    QCOMPARE(fold(QStringLiteral("?!")), QString());

    QCOMPARE(SearchText::words(QStringLiteral("  Zelda: BREATH ")), (QStringList{"zelda", "breath"}));
    QVERIFY(SearchText::words(QStringLiteral(" - ")).isEmpty());

    const QString botw = fold(QStringLiteral("The Legend of Zelda: Breath of the Wild"));
    const auto matches = [&](const char *search) { return SearchText::matches(botw, SearchText::words(search)); };
    QVERIFY(matches("zelda breath"));
    QVERIFY(matches("breath zelda")); // any order
    QVERIFY(matches("zel")); // a word typed so far
    QVERIFY(matches("Legend, of zelda"));
    QVERIFY(!matches("zelda odyssey")); // every word must be there
    QVERIFY(SearchText::matches(fold(QStringLiteral("ゼルダの伝説")), SearchText::words("ゼルダ")));
}

void LibraryListTest::searchMatchesEveryWord()
{
    auto library = openLibrary();
    library->setSearch(QStringLiteral("pokemon"));
    QCOMPARE(gameIds(*library), QStringList{"3ds-pokemon-ultra-sun"});
    library->setSearch(QStringLiteral("KINGDOM zelda"));
    QCOMPARE(gameIds(*library), QStringList{"switch-the-legend-of-zelda-tears-of-the-kingdom"});
    library->setSearch(QStringLiteral("legend zelda"));
    QCOMPARE(gameIds(*library).size(), 2);
    library->setSearch(QStringLiteral("baldurs"));
    QCOMPARE(gameIds(*library), QStringList{"steam-1086940"});
    library->setSearch(QStringLiteral("mario kart odyssey"));
    QCOMPARE(library->rowCount(), 0);

    // A space or punctuation after a word updates the text but not the rows.
    library->setSearch(QStringLiteral("zelda"));
    QSignalSpy reset(library.get(), &QAbstractItemModel::modelReset);
    library->setSearch(QStringLiteral("zelda:"));
    QCOMPARE(library->search(), QStringLiteral("zelda:"));
    QCOMPARE(reset.count(), 0);
    library->setSearch(QStringLiteral("zelda: t"));
    QCOMPARE(reset.count(), 1);

    // Searching expands a collapsed group, even for a search with no words yet.
    library->setSearch({});
    library->toggleGroup(QStringLiteral("switch"));
    QVERIFY(!gameIds(*library).contains(QStringLiteral("switch-pikmin-4")));
    library->setSearch(QStringLiteral("-"));
    QCOMPARE(gameIds(*library).size(), library->gameCount() - library->notInstalledHidden());
    library->setSearch({});
    QVERIFY(!gameIds(*library).contains(QStringLiteral("switch-pikmin-4")));
}

void LibraryListTest::sortOrders()
{
    auto library = openLibrary();
    library->setInstalledOnly(false); // Mario Tennis Aces, whose ROM is missing, is in the lists below
    library->setGroupBy(QStringLiteral("none"));
    QCOMPARE(library->sort(), QStringLiteral("title"));
    QVERIFY(!library->sortDescending());
    QStringList ids = gameIds(*library);
    QCOMPARE(ids.first(), QStringLiteral("switch-animal-crossing-new-horizons"));
    QCOMPARE(ids.last(), QStringLiteral("switch-the-legend-of-zelda-tears-of-the-kingdom"));
    library->setSortDescending(true);
    std::reverse(ids.begin(), ids.end());
    QCOMPARE(gameIds(*library), ids);

    // Switching orders sets the order's usual direction; never-played games go last, by title:
    // Celeste (no game file), Pikmin 4, Pokémon Ultra Sun and the homebrew Tetris the scan adds.
    const QStringList neverPlayed = {"steam-504230", "switch-pikmin-4", "3ds-pokemon-ultra-sun", "switch-tetris"};
    QSignalSpy sortChanged(library.get(), &Library::sortChanged);
    library->setSort(QStringLiteral("last_played"));
    QCOMPARE(sortChanged.count(), 1);
    QVERIFY(library->sortDescending());
    ids = gameIds(*library);
    QCOMPARE(ids.mid(0, 4), (QStringList{"steam-1086940", "switch-the-legend-of-zelda-tears-of-the-kingdom",
                                         "switch-mario-tennis-aces", "steam-1462040"}));
    QCOMPARE(ids.mid(ids.size() - neverPlayed.size()), neverPlayed);
    library->setSortDescending(false);
    ids = gameIds(*library);
    QCOMPARE(ids.mid(0, 2), (QStringList{"3ds-kid-icarus-uprising", "3ds-the-legend-of-zelda-a-link-between-worlds"}));
    QCOMPARE(ids.mid(ids.size() - neverPlayed.size()), neverPlayed);

    library->setSort(QStringLiteral("play_time"));
    QVERIFY(library->sortDescending());
    ids = gameIds(*library);
    QCOMPARE(ids.mid(0, 2), (QStringList{"proton-final-fantasy-xiv-online", "switch-animal-crossing-new-horizons"}));
    QCOMPARE(ids.mid(ids.size() - neverPlayed.size()), neverPlayed);
    library->setSortDescending(false);
    ids = gameIds(*library);
    QCOMPARE(ids.mid(0, 2), (QStringList{"3ds-kid-icarus-uprising", "switch-kirby-and-the-forgotten-land"}));
    QCOMPARE(ids.mid(ids.size() - neverPlayed.size()), neverPlayed);

    // Back to titles: A to Z again. An unknown order changes nothing.
    library->setSort(QStringLiteral("title"));
    QVERIFY(!library->sortDescending());
    QCOMPARE(gameIds(*library).first(), QStringLiteral("switch-animal-crossing-new-horizons"));
    sortChanged.clear();
    library->setSort(QStringLiteral("popularity"));
    QCOMPARE(library->sort(), QStringLiteral("title"));
    QCOMPARE(sortChanged.count(), 0);
}

void LibraryListTest::sortTiesAndGroups()
{
    // Numbered titles, and two games with Kid Icarus's 300 minutes.
    writeFile(gamePath("3ds-game-10"), "title = \"Game 10\"\nplatform = \"3ds\"\n");
    writeFile(gamePath("linux-factorio"), "title = \"Game 2\"\nplatform = \"3ds\"\n");
    writeFile(gamePath("3ds-zeta"), "title = \"Zeta\"\nplatform = \"3ds\"\nplay_minutes = 300\n");
    writeFile(gamePath("3ds-alpha"), "title = \"alpha\"\nplatform = \"3ds\"\nplay_minutes = 300\n");
    auto library = openLibrary();
    library->setInstalledOnly(false); // Mario Tennis Aces, whose ROM is missing, is in the lists below
    library->setGroupBy(QStringLiteral("none"));

    QStringList ids = gameIds(*library);
    QVERIFY(ids.indexOf(QStringLiteral("linux-factorio")) < ids.indexOf(QStringLiteral("3ds-game-10")));
    QCOMPARE(ids.first(), QStringLiteral("3ds-alpha")); // case doesn't matter

    // Equal values go by title, A to Z, in both directions.
    const QStringList ties = {"3ds-alpha", "3ds-kid-icarus-uprising", "3ds-zeta"};
    library->setSort(QStringLiteral("play_time"));
    ids = gameIds(*library);
    QCOMPARE(ids.mid(ids.indexOf(ties.first()), 3), ties);
    library->setSortDescending(false);
    ids = gameIds(*library);
    QCOMPARE(ids.mid(0, 3), ties);
    // Never played: last, by title, whatever the direction.
    QCOMPARE(ids.mid(ids.size() - 6), (QStringList{"steam-504230", "linux-factorio", "3ds-game-10",
                                                    "switch-pikmin-4", "3ds-pokemon-ultra-sun", "switch-tetris"}));

    // Grouped: groups stay in rail order, sorted inside.
    library->setGroupBy(QStringLiteral("platform"));
    library->setSort(QStringLiteral("last_played"));
    QVERIFY(library->data(library->index(0), Library::IsHeaderRole).toBool());
    QCOMPARE(library->data(library->index(0), Library::GroupRole).toString(), QStringLiteral("switch"));
    QCOMPARE(library->gameIdAt(1), QStringLiteral("switch-the-legend-of-zelda-tears-of-the-kingdom"));
    QCOMPARE(library->gameIdAt(2), QStringLiteral("switch-mario-tennis-aces"));
    const int threeDs = library->rowOfGame(QStringLiteral("3ds-fire-emblem-awakening"));
    QVERIFY(library->data(library->index(threeDs - 1), Library::IsHeaderRole).toBool());

    // The selected game is found again after a sort change.
    const QString selected = library->gameIdAt(5);
    library->setSortDescending(false);
    QCOMPARE(library->gameIdAt(library->rowOfGame(selected)), selected);
}

void LibraryListTest::installedOnlyFilter()
{
    auto library = openLibrary();
    // On by default, and not a filter the user turned on: Mario Tennis Aces' ROM is missing,
    // Steam doesn't have Elden Ring installed, and Diablo IV's program isn't found.
    QVERIFY(library->installedOnly());
    QVERIFY(!library->filtersActive());
    QCOMPARE(library->rowOfGame(QStringLiteral("switch-mario-tennis-aces")), -1);
    QCOMPARE(library->rowOfGame(QStringLiteral("steam-1245620")), -1);
    QCOMPARE(library->rowOfGame(QStringLiteral("proton-diablo-iv")), -1);
    QVERIFY(library->rowOfGame(QStringLiteral("proton-final-fantasy-xiv-online")) >= 0);
    QCOMPARE(library->notInstalledHidden(), 4);
    QCOMPARE(gameIds(*library).size(), library->gameCount() - 4);

    // Counted only when it matches the rest: the platform, the search and the other filters.
    library->setPlatformFilter(QStringLiteral("3ds"));
    QCOMPARE(library->notInstalledHidden(), 0);
    library->setPlatformFilter({});
    library->setSearch(QStringLiteral("tennis"));
    QCOMPARE(library->rowCount(), 0);
    QCOMPARE(library->notInstalledHidden(), 1);
    library->setPlatformFilter(QStringLiteral("steam"));
    QCOMPARE(library->notInstalledHidden(), 0);
    library->setPlatformFilter({});
    library->setSearch(QStringLiteral("kart"));
    QCOMPARE(library->notInstalledHidden(), 0);
    library->setSearch({});

    QSignalSpy filtersChanged(library.get(), &Library::filtersChanged);
    QSignalSpy countChanged(library.get(), &Library::notInstalledHiddenChanged);
    library->setInstalledOnly(false);
    QCOMPARE(filtersChanged.count(), 1);
    QCOMPARE(countChanged.count(), 1);
    QVERIFY(library->filtersActive());
    QVERIFY(library->rowOfGame(QStringLiteral("switch-mario-tennis-aces")) >= 0);
    QVERIFY(library->rowOfGame(QStringLiteral("steam-1245620")) >= 0);
    QCOMPARE(library->notInstalledHidden(), 0);
    QCOMPARE(gameIds(*library).size(), library->gameCount());
}

void LibraryListTest::favoritesAndTagFilters()
{
    auto library = openLibrary();
    library->setGroupBy(QStringLiteral("none"));

    // Favorites: Hades and Mario Tennis Aces, whose ROM is missing.
    library->setFavoritesOnly(true);
    QVERIFY(library->filtersActive());
    QCOMPARE(gameIds(*library), QStringList{"steam-1145360"});
    QCOMPARE(library->notInstalledHidden(), 1);
    library->setFavoritesOnly(false);

    // Any of the tags, or all of them.
    library->setTagFilter({QStringLiteral("Co-op"), QStringLiteral("RPG")});
    QVERIFY(library->filtersActive());
    // Elden Ring (RPG) and Diablo IV (RPG, Co-op) aren't installed.
    QCOMPARE(gameIds(*library), (QStringList{"steam-1086940", "steam-1091500", "steam-413150",
                                             "switch-super-mario-bros-wonder"}));
    library->setTagMatchAll(true);
    QCOMPARE(gameIds(*library), (QStringList{"steam-1086940", "steam-413150"}));
    library->setTagMatchAll(false);

    // Combined with favorites: every filter must pass.
    library->setTagFilter({QStringLiteral("Roguelike"), QStringLiteral("Party")});
    library->setFavoritesOnly(true);
    QCOMPARE(gameIds(*library), QStringList{"steam-1145360"});
    library->setTagFilter({QStringLiteral("Party")});
    QCOMPARE(library->rowCount(), 0);
    QCOMPARE(library->notInstalledHidden(), 1); // Mario Tennis Aces
    library->setFavoritesOnly(false);

    // A tag that doesn't exist is ignored, and alone isn't an active filter.
    library->setTagFilter({QStringLiteral("Horror")});
    QVERIFY(!library->filtersActive());
    // Not installed: Mario Tennis Aces, Elden Ring, Diablo IV, Factorio.
    QCOMPARE(gameIds(*library).size(), library->gameCount() - 4);
    library->setTagFilter({QStringLiteral("Horror"), QStringLiteral("Party"), QStringLiteral("Party")});
    QCOMPARE(library->tagFilter(), (QStringList{"Horror", "Party"}));
    QCOMPARE(gameIds(*library), (QStringList{"switch-mario-kart-8-deluxe", "switch-super-smash-bros-ultimate"}));

    // Renamed behind the app's back, the old name stops filtering. (Renaming it in Settings
    // renames it in the filter too: see listSettingsFollowTagChanges.)
    QSignalSpy filtersChanged(library.get(), &Library::filtersChanged);
    QCOMPARE(library->store().renameTag(QStringLiteral("Party"), QStringLiteral("Fiesta")), QString());
    QTRY_VERIFY_WITH_TIMEOUT(!library->filtersActive(), 5000);
    QVERIFY(filtersChanged.count() > 0);
    QCOMPARE(gameIds(*library).size(), library->gameCount() - 4);
}

void LibraryListTest::filtersAndGroups()
{
    auto library = openLibrary();
    library->toggleGroup(QStringLiteral("switch"));
    QVERIFY(!gameIds(*library).contains(QStringLiteral("switch-mario-kart-8-deluxe")));

    // Headers count the filtered games; empty groups are left out; a filter expands groups.
    library->setTagFilter({QStringLiteral("Party")});
    QCOMPARE(library->rowCount(), 3);
    QCOMPARE(library->data(library->index(0), Library::GroupRole).toString(), QStringLiteral("switch"));
    QCOMPARE(library->data(library->index(0), Library::CountRole).toInt(), 2);
    QCOMPARE(gameIds(*library), (QStringList{"switch-mario-kart-8-deluxe", "switch-super-smash-bros-ultimate"}));
    // Collapsing does nothing while filtering, like while searching.
    QCOMPARE(library->collapseGroupAt(1), 1);

    // Off again, the group is collapsed as it was.
    library->setTagFilter({});
    QVERIFY(!gameIds(*library).contains(QStringLiteral("switch-mario-kart-8-deluxe")));
}

void LibraryListTest::collapseSendsOneRange()
{
    auto library = openLibrary();
    const int rows = library->rowCount();
    const int header = 0;
    QCOMPARE(library->data(library->index(header), Library::GroupRole).toString(), QStringLiteral("switch"));
    const int games = library->data(library->index(header), Library::CountRole).toInt();
    QVERIFY(games > 1);

    QSignalSpy reset(library.get(), &QAbstractItemModel::modelReset);
    QSignalSpy inserted(library.get(), &QAbstractItemModel::rowsInserted);
    QSignalSpy removed(library.get(), &QAbstractItemModel::rowsRemoved);

    library->toggleGroup(QStringLiteral("switch"));
    QCOMPARE(reset.count(), 0);
    QCOMPARE(inserted.count(), 0);
    QCOMPARE(removed.count(), 1);
    QCOMPARE(removed.constFirst().at(1).toInt(), header + 1);
    QCOMPARE(removed.constFirst().at(2).toInt(), header + games);
    QCOMPARE(library->rowCount(), rows - games);
    QVERIFY(library->data(library->index(header), Library::CollapsedRole).toBool());

    library->toggleGroup(QStringLiteral("switch"));
    QCOMPARE(reset.count(), 0);
    QCOMPARE(removed.count(), 1);
    QCOMPARE(inserted.count(), 1);
    QCOMPARE(inserted.constFirst().at(1).toInt(), header + 1);
    QCOMPARE(inserted.constFirst().at(2).toInt(), header + games);
    QCOMPARE(library->rowCount(), rows);

    // The keyboard's collapse and expand do the same.
    QCOMPARE(library->collapseGroupAt(header + 1), header + 2);
    QCOMPARE(removed.count(), 2);
    library->expandGroupNear(header);
    QCOMPARE(inserted.count(), 2);
    QCOMPARE(reset.count(), 0);

    // While searching, groups are expanded already: nothing to insert or remove.
    library->setSearch(QStringLiteral("-"));
    QSignalSpy searchedRemoved(library.get(), &QAbstractItemModel::rowsRemoved);
    QSignalSpy searchedReset(library.get(), &QAbstractItemModel::modelReset);
    library->toggleGroup(QStringLiteral("switch"));
    QCOMPARE(searchedRemoved.count(), 0);
    QCOMPARE(searchedReset.count(), 0);
}

void LibraryListTest::renameSendsAMove()
{
    auto library = openLibrary();
    const int from = library->rowOfGame(QStringLiteral("switch-pikmin-4"));
    QVERIFY(from >= 0);
    QSignalSpy reset(library.get(), &QAbstractItemModel::modelReset);
    QSignalSpy inserted(library.get(), &QAbstractItemModel::rowsInserted);
    QSignalSpy removed(library.get(), &QAbstractItemModel::rowsRemoved);
    QSignalSpy moved(library.get(), &QAbstractItemModel::rowsMoved);

    writeFile(gamePath("switch-pikmin-4"), "title = \"Zzz Pikmin\"\nplatform = \"switch\"\n");
    QTRY_VERIFY_WITH_TIMEOUT(library->rowOfGame(QStringLiteral("switch-pikmin-4")) > from, 5000);
    QCOMPARE(moved.count(), 1);
    QCOMPARE(moved.constFirst().at(1).toInt(), from);
    QCOMPARE(moved.constFirst().at(2).toInt(), from);
    QCOMPARE(reset.count(), 0);
    QCOMPARE(inserted.count(), 0);
    QCOMPARE(removed.count(), 0);
    const int to = library->rowOfGame(QStringLiteral("switch-pikmin-4"));
    QCOMPARE(library->gameAt(to).value("title").toString(), QStringLiteral("Zzz Pikmin"));

    // And back up.
    writeFile(gamePath("switch-pikmin-4"), "title = \"Aaa Pikmin\"\nplatform = \"switch\"\n");
    QTRY_COMPARE_WITH_TIMEOUT(library->rowOfGame(QStringLiteral("switch-pikmin-4")), 1, 5000);
    QCOMPARE(moved.count(), 2);
    QCOMPARE(reset.count(), 0);
}

void LibraryListTest::twoChangesReset()
{
    auto library = openLibrary();
    QSignalSpy reset(library.get(), &QAbstractItemModel::modelReset);
    QSignalSpy moved(library.get(), &QAbstractItemModel::rowsMoved);
    const QHash<QString, QString> errors = library->saveGames({
        {QStringLiteral("switch-pikmin-4"), {{"title", QStringLiteral("Zzz Pikmin")}}},
        {QStringLiteral("switch-mario-kart-8-deluxe"), {{"title", QStringLiteral("Yyy Kart")}}},
    });
    QVERIFY(errors.isEmpty());
    QCOMPARE(moved.count(), 0);
    QCOMPARE(reset.count(), 1);
}

void LibraryListTest::clearFilters()
{
    auto library = openLibrary();
    library->setInstalledOnly(false);
    library->setFavoritesOnly(true);
    library->setTagFilter({QStringLiteral("Party")});
    library->setTagMatchAll(true);
    library->setShowHidden(true);
    library->setSearch(QStringLiteral("mario"));
    library->setSort(QStringLiteral("play_time"));
    library->setPlatformFilter(QStringLiteral("switch"));

    QSignalSpy filtersChanged(library.get(), &Library::filtersChanged);
    QSignalSpy reset(library.get(), &QAbstractItemModel::modelReset);
    library->clearFilters();
    QCOMPARE(filtersChanged.count(), 1);
    QCOMPARE(reset.count(), 1);
    QVERIFY(library->installedOnly());
    QVERIFY(!library->favoritesOnly());
    QVERIFY(library->tagFilter().isEmpty());
    QVERIFY(!library->tagMatchAll());
    QVERIFY(!library->showHidden());
    QVERIFY(!library->filtersActive());
    // The search, the platform and the sort stay.
    QCOMPARE(library->search(), QStringLiteral("mario"));
    QCOMPARE(library->platformFilter(), QStringLiteral("switch"));
    QCOMPARE(library->sort(), QStringLiteral("play_time"));

    library->clearFilters(); // nothing to clear
    QCOMPARE(filtersChanged.count(), 1);
    QCOMPARE(reset.count(), 1);
}

void LibraryListTest::showFiltered()
{
    auto library = openLibrary();
    library->setFavoritesOnly(true);
    library->setShowHidden(true);
    library->setSearch(QStringLiteral("mario"));
    library->setSort(QStringLiteral("play_time"));
    library->setPlatformFilter(QStringLiteral("3ds"));

    // The overview's click replaces every filter, counts uninstalled games too, and leaves the sort.
    library->showFiltered(QStringLiteral("switch"), {QStringLiteral("playing")}, {QStringLiteral("Party")});
    QVERIFY(!library->installedOnly());
    QVERIFY(!library->favoritesOnly());
    QVERIFY(!library->showHidden());
    QVERIFY(library->search().isEmpty());
    QCOMPARE(library->platformFilter(), QStringLiteral("switch"));
    QCOMPARE(library->statusFilter(), QStringList{QStringLiteral("playing")});
    QCOMPARE(library->tagFilter(), QStringList{QStringLiteral("Party")});
    QCOMPARE(library->sort(), QStringLiteral("play_time"));

    library->showFiltered({}, {}, {});
    QVERIFY(!library->installedOnly());
    QVERIFY(library->platformFilter().isEmpty());
    QVERIFY(library->statusFilter().isEmpty());
    QVERIFY(library->tagFilter().isEmpty());
}

void LibraryListTest::showHiddenGames()
{
    auto library = openLibrary();
    const QString cs2 = QStringLiteral("steam-730");
    QCOMPARE(library->rowOfGame(cs2), -1);
    QCOMPARE(library->game(cs2), nullptr);
    QCOMPARE(library->gameCount(), 26);
    const int steamHeader = library->rowOfGame(QStringLiteral("steam-1086940")) - 1;
    const int steamGames = library->data(library->index(steamHeader), Library::CountRole).toInt();

    library->setShowHidden(true);
    QVERIFY(library->filtersActive());
    const int row = library->rowOfGame(cs2);
    QVERIFY(row >= 0);
    QVERIFY(library->data(library->index(row), Library::HiddenRole).toBool());
    QVERIFY(!library->data(library->index(row - 1), Library::HiddenRole).toBool());
    QCOMPARE(library->data(library->index(steamHeader), Library::CountRole).toInt(), steamGames + 1);
    QCOMPARE(library->gameCount(), 26); // still not counted as a library game
    QVERIFY(library->game(cs2) != nullptr); // it can launch
    // Built like any game: its play time and art, not only a title.
    const QVariantMap game = library->gameAt(row);
    QVERIFY(game.value("hidden").toBool());
    QCOMPARE(game.value("title").toString(), QStringLiteral("Counter-Strike 2"));
    QCOMPARE(game.value("year").toString(), QStringLiteral("2023"));
    QVERIFY(!game.value("playTimeText").toString().isEmpty());
    QVERIFY(!library->gameForEditing(cs2).isEmpty());

    // Unhidden from the list, it stays where it is, no longer dimmed.
    QCOMPARE(library->unhideGame(cs2), QString());
    QTRY_VERIFY_WITH_TIMEOUT(library->hiddenGames().isEmpty(), 5000);
    QVERIFY(!library->data(library->index(library->rowOfGame(cs2)), Library::HiddenRole).toBool());
    QCOMPARE(library->gameCount(), 27);

    // A hidden game's problems aren't reported.
    writeFile(gamePath("proton-hidden"), "title = \"Hidden\"\nplatform = \"nowhere\"\nhidden = true\n");
    QTRY_VERIFY_WITH_TIMEOUT(library->rowOfGame(QStringLiteral("proton-hidden")) >= 0, 5000);
    QVERIFY(library->problems().isEmpty());
    // Its platform is a group in the list, but not a platform in Settings.
    for (const QVariant &p : library->platformSettings())
        QVERIFY(p.toMap().value("id").toString() != QStringLiteral("nowhere"));
}

void LibraryListTest::sortValue()
{
    // Times relative to now, written with this machine's offset.
    const QDateTime now = QDateTime::currentDateTime();
    const auto at = [&](int daysAgo) {
        return now.addDays(-daysAgo).toOffsetFromUtc(now.offsetFromUtc()).toString(Qt::ISODate).toUtf8();
    };
    writeFile(gamePath("3ds-today"), "title = \"Today\"\nplatform = \"3ds\"\nplay_minutes = 45\nlast_played = "
                                            + at(0) + "\n");
    writeFile(gamePath("3ds-days"), "title = \"Days\"\nplatform = \"3ds\"\nplay_minutes = 150\nlast_played = "
                                           + at(3) + "\n");
    writeFile(gamePath("3ds-long"), "title = \"Long\"\nplatform = \"3ds\"\nplay_minutes = 725\nlast_played = "
                                           + at(40) + "\n");
    auto library = openLibrary();
    const auto value = [&](const char *id) {
        return library->data(library->index(library->rowOfGame(QString::fromLatin1(id))), Library::SortValueRole).toString();
    };

    QCOMPARE(value("3ds-today"), QString()); // sorted by title: nothing to show
    library->setSort(QStringLiteral("play_time"));
    QCOMPARE(value("3ds-today"), Library::tr("%1 min").arg(45));
    QCOMPARE(value("3ds-days"), Library::tr("%1 h").arg(QLocale().toString(2.5, 'f', 1)));
    QCOMPARE(value("3ds-long"), Library::tr("%1 h").arg(QLocale().toString(12.0, 'f', 0)));
    QCOMPARE(value("switch-pikmin-4"), QString()); // never played

    library->setSort(QStringLiteral("last_played"));
    QCOMPARE(value("3ds-today"), Library::tr("Today"));
    QCOMPARE(value("3ds-days"), Library::tr("%1 days ago").arg(3));
    QCOMPARE(value("3ds-long"), QLocale().toString(now.addDays(-40).date(), QLocale::ShortFormat));
    QCOMPARE(value("switch-pikmin-4"), QString());
    // Headers have none.
    QCOMPARE(library->data(library->index(0), Library::SortValueRole).toString(), QString());
}

void LibraryListTest::listSettingsParsing()
{
    const auto parse = [](const char *text) { return parseListSettings(text, QStringLiteral("settings.toml")); };

    // Nothing: the defaults.
    Parsed<ListSettings> parsed = parse("[launch]\nwhile_running = \"stay\"\n");
    QVERIFY(parsed.ok);
    QVERIFY(parsed.warnings.isEmpty());
    QCOMPARE(parsed.value, ListSettings());

    parsed = parse("[list]\nsort = \"last_played\"\ninstalled_only = false\nfavorites_only = true\n"
                   "tags = [\"co-op\", \"backlog\", \"co-op\"]\nmatch_all_tags = true\nshow_hidden = true\n"
                   "show_sort_value = false\n");
    QVERIFY(parsed.warnings.isEmpty());
    QCOMPARE(parsed.value.sort, QStringLiteral("last_played"));
    QVERIFY(parsed.value.descending); // the order's usual direction
    QVERIFY(!parsed.value.installedOnly);
    QVERIFY(parsed.value.favoritesOnly);
    QCOMPARE(parsed.value.tags, (QStringList{"co-op", "backlog"}));
    QVERIFY(parsed.value.matchAllTags);
    QVERIFY(parsed.value.showHidden);
    QVERIFY(!parsed.value.showSortValue);
    QVERIFY(!parse("[list]\nsort = \"play_time\"\ndescending = false\n").value.descending);
    QVERIFY(parse("[list]\ndescending = true\n").value.descending);

    // Bad values warn at their line and keep the default.
    parsed = parse("[list]\nsort = \"last-played\"\ndescending = \"yes\"\ntags = \"rpg\"\nshow_hiden = true\n");
    QVERIFY(parsed.ok);
    QCOMPARE(parsed.value, ListSettings());
    QCOMPARE(parsed.warnings.size(), 4);
    QVERIFY(parsed.warnings.at(0).startsWith("settings.toml:2: sort must be"));
    QVERIFY(parsed.warnings.at(0).contains("last_played")); // the suggestion
    QVERIFY(anyContains(parsed.warnings, "settings.toml:3: descending must be true or false"));
    QVERIFY(anyContains(parsed.warnings, "settings.toml:4: tags must be a list"));
    QVERIFY(anyContains(parsed.warnings, "settings.toml:5: unknown key show_hiden"));
    QVERIFY(!parse("[list]\ntags = [1, 2]\n").warnings.isEmpty());
    QVERIFY(parse("list = 3\n").warnings.first().contains("must be a [list] section"));
    QVERIFY(!parse("[list\n").ok);
}

void LibraryListTest::listSettingsWriting()
{
    const QString file = m_data + QStringLiteral("/settings.toml");
    auto library = openLibrary();
    QCOMPARE(readFile(file), demoSettings()); // nothing changed, nothing written

    library->setSort(QStringLiteral("last_played"));
    QVERIFY(readFile(file).contains("[list]\nsort = \"last_played\"\n"));
    QVERIFY(!readFile(file).contains("descending")); // the order's usual direction

    // Only the keys that change are written; the rest of the file stays.
    writeFile(file, "# Mine.\n[shortcuts]\nquit = \"Ctrl+Q\"\n\n[list]\nsort = \"last_played\" # newest first\n");
    QTRY_VERIFY(library->sort() == QStringLiteral("last_played"));
    library->setSortDescending(false);
    library->setInstalledOnly(false);
    library->setTagFilter({QStringLiteral("Party"), QStringLiteral("Co-op")});
    QCOMPARE(readFile(file), QByteArray("# Mine.\n[shortcuts]\nquit = \"Ctrl+Q\"\n\n[list]\n"
                                        "sort = \"last_played\" # newest first\ndescending = false\n"
                                        "installed_only = false\ntags = [\"Party\", \"Co-op\"]\n"));

    // Back to its default, a key is removed.
    library->clearFilters();
    library->setSort(QStringLiteral("title"));
    QCOMPARE(readFile(file), QByteArray("# Mine.\n[shortcuts]\nquit = \"Ctrl+Q\"\n\n[list]\n"));

    // A read-only library still saves them: settings.toml isn't part of it.
    writeFile(m_data + QStringLiteral("/library/library.toml"), "version = 99\n");
    auto readOnly = openLibrary();
    QVERIFY(readOnly->readOnly());
    readOnly->setFavoritesOnly(true);
    QVERIFY(readFile(file).contains("favorites_only = true\n"));
}

void LibraryListTest::listSettingsRestoredAtStart()
{
    writeFile(m_data + QStringLiteral("/settings.toml"),
              "[list]\nsort = \"play_time\"\ntags = [\"Party\"]\nshow_hidden = true\n");
    {
        auto library = openLibrary(); // writes the cache
    }
    // From the cache, before any file is checked: already sorted and filtered.
    Library library(m_data, m_cache);
    QCOMPARE(library.sort(), QStringLiteral("play_time"));
    QVERIFY(library.sortDescending());
    QCOMPARE(library.tagFilter(), QStringList{"Party"});
    QVERIFY(library.showHidden());
    QCOMPARE(gameIds(library), (QStringList{"switch-super-smash-bros-ultimate", "switch-mario-kart-8-deluxe"}));
}

void LibraryListTest::listSettingsHandEdit()
{
    const QString file = m_data + QStringLiteral("/settings.toml");
    writeFile(file, demoSettings() + "[list]\n");
    auto library = openLibrary();

    // Applies right away.
    writeFile(file, demoSettings() + "[list]\nfavorites_only = true\n");
    QTRY_VERIFY_WITH_TIMEOUT(library->favoritesOnly(), 5000);
    QCOMPARE(gameIds(*library), QStringList{"steam-1145360"});

    // The app's own write is read back without a second rebuild.
    QSignalSpy reset(library.get(), &QAbstractItemModel::modelReset);
    library->setFavoritesOnly(false);
    QCOMPARE(reset.count(), 1);
    QTest::qWait(400); // the watcher's delay and then some
    QCOMPARE(reset.count(), 1);
    QVERIFY(!library->favoritesOnly());

    // A bad value is a problem, and keeps the default.
    writeFile(file, "[list]\nsort = \"popularity\"\n");
    QTRY_VERIFY_WITH_TIMEOUT(anyContains(library->problems(), QStringLiteral("settings.toml:2: sort must be")), 5000);
    QCOMPARE(library->sort(), QStringLiteral("title"));
    // A syntax error keeps the list as it is.
    library->setSort(QStringLiteral("play_time"));
    writeFile(file, "[list\n");
    QTRY_VERIFY_WITH_TIMEOUT(anyContains(library->problems(), QStringLiteral("settings.toml:1")), 5000);
    QCOMPARE(library->sort(), QStringLiteral("play_time"));
    writeFile(file, "");
    QTRY_VERIFY_WITH_TIMEOUT(!anyContains(library->problems(), QStringLiteral("settings.toml")), 5000);
    QCOMPARE(library->sort(), QStringLiteral("title"));
}

void LibraryListTest::groupBySetting()
{
    const QString file = m_data + QStringLiteral("/settings.toml");
    auto library = openLibrary();
    QCOMPARE(library->groupBy(), QStringLiteral("platform"));
    library->setSort(QStringLiteral("last_played"));
    // Grouped: Switch comes first in platforms.toml, so its newest game leads.
    QCOMPARE(gameIds(*library).first(), QStringLiteral("switch-the-legend-of-zelda-tears-of-the-kingdom"));

    // None: one list, the most recently played game first whatever its platform.
    QSignalSpy groupByChanged(library.get(), &Library::groupByChanged);
    library->setGroupBy(QStringLiteral("none"));
    QCOMPARE(groupByChanged.count(), 1);
    QCOMPARE(gameIds(*library).first(), QStringLiteral("steam-1086940"));
    QVERIFY(readFile(file).contains("group_by = \"none\"\n"));
    // Not a filter: clearing the filters leaves it.
    QVERIFY(!library->filtersActive());
    library->setFavoritesOnly(true);
    library->clearFilters();
    QCOMPARE(library->groupBy(), QStringLiteral("none"));
    // An unknown grouping changes nothing.
    library->setGroupBy(QStringLiteral("genre"));
    QCOMPARE(library->groupBy(), QStringLiteral("none"));
    QCOMPARE(groupByChanged.count(), 1);

    // Back to the default, the key goes; a hand edit applies right away.
    library->setGroupBy(QStringLiteral("platform"));
    QVERIFY(!readFile(file).contains("group_by"));
    writeFile(file, readFile(file) + "group_by = \"status\"\n");
    QTRY_COMPARE_WITH_TIMEOUT(library->groupBy(), QStringLiteral("status"), 5000);
    QCOMPARE(groupByChanged.count(), 3);

    // The older group_by_platform = false reads as none, and goes when the grouping changes.
    writeFile(file, demoSettings() + "\n[list]\ngroup_by_platform = false\n");
    QTRY_COMPARE_WITH_TIMEOUT(library->groupBy(), QStringLiteral("none"), 5000);
    QVERIFY(library->problems().isEmpty());
    library->setGroupBy(QStringLiteral("status"));
    QVERIFY(!readFile(file).contains("group_by_platform"));
    QVERIFY(readFile(file).contains("group_by = \"status\"\n"));

    // Read before the first rows, so a start shows the list as it was left.
    library.reset();
    Library again(m_data, m_cache);
    QCOMPARE(again.groupBy(), QStringLiteral("status"));

    const auto parse = [](const char *text) { return parseListSettings(text, QStringLiteral("settings.toml")); };
    QVERIFY(!parse("[list]\ngroup_by_platform = 1\n").warnings.isEmpty());
    // group_by wins over the older key.
    QCOMPARE(parse("[list]\ngroup_by_platform = false\ngroup_by = \"status\"\n").value.groupBy, QStringLiteral("status"));
    const Parsed<ListSettings> bad = parse("[list]\ngroup_by = \"stats\"\n");
    QCOMPARE(bad.value.groupBy, QStringLiteral("platform"));
    QCOMPARE(bad.warnings.size(), 1);
    QVERIFY(bad.warnings.first().contains("did you mean \"status\""));
}

void LibraryListTest::statusGroupsAndFilter()
{
    const QString file = m_data + QStringLiteral("/settings.toml");
    const QString zelda = QStringLiteral("switch-the-legend-of-zelda-tears-of-the-kingdom");
    const QString marioKart = QStringLiteral("switch-mario-kart-8-deluxe");
    auto library = openLibrary();
    const auto headers = [&] {
        QStringList out;
        for (int row = 0; row < library->rowCount(); ++row) {
            if (library->data(library->index(row), Library::IsHeaderRole).toBool())
                out.append(library->data(library->index(row), Library::GroupRole).toString());
        }
        return out;
    };

    // A header per status, playing first and no status last, sorted inside.
    library->setGroupBy(QStringLiteral("status"));
    QCOMPARE(headers(), (QStringList{"playing", "not_started", "finished", "completed", "dropped", "none"}));
    QCOMPARE(library->data(library->index(0), Library::TitleRole).toString(), QStringLiteral("Playing"));
    QCOMPARE(library->data(library->index(0), Library::CountRole).toInt(), 3);
    QCOMPARE(library->gameIdAt(1), QStringLiteral("steam-1086940")); // Baldur's Gate 3
    QCOMPARE(library->gameIdAt(2), QStringLiteral("linux-supertuxkart"));
    QCOMPARE(library->gameIdAt(3), zelda);
    QCOMPARE(library->data(library->index(1), Library::PlatformRole).toString(), QStringLiteral("steam"));

    // With a platform selected, still grouped, with only its games.
    library->setPlatformFilter(QStringLiteral("switch"));
    QCOMPARE(headers(), (QStringList{"playing", "not_started", "finished", "completed", "none"}));
    QCOMPARE(library->gameIdAt(1), zelda);
    QCOMPARE(library->nextGroupRow(1, 1), 3); // Pikmin 4, backlog
    library->setPlatformFilter({});

    // Collapsed groups are kept per grouping.
    library->toggleGroup(QStringLiteral("none"));
    QVERIFY(!gameIds(*library).contains(marioKart));
    library->setGroupBy(QStringLiteral("platform"));
    QVERIFY(gameIds(*library).contains(marioKart));
    library->setGroupBy(QStringLiteral("status"));
    QVERIFY(!gameIds(*library).contains(marioKart));
    QVERIFY(library->data(library->index(library->rowCount() - 1), Library::CollapsedRole).toBool());

    // The filter: any of the selected statuses. Unknown ones and repeats are dropped.
    QVERIFY(!library->filtersActive());
    library->setStatusFilter({"completed", "dropped", "bogus", "completed"});
    QCOMPARE(library->statusFilter(), (QStringList{"completed", "dropped"}));
    QVERIFY(library->filtersActive());
    QCOMPARE(gameIds(*library), (QStringList{"switch-metroid-dread", "proton-final-fantasy-xiv-online"}));
    QVERIFY(readFile(file).contains("statuses = [\"completed\", \"dropped\"]\n"));
    library->setGroupBy(QStringLiteral("none"));
    QCOMPARE(gameIds(*library), (QStringList{"proton-final-fantasy-xiv-online", "switch-metroid-dread"}));
    // "none" is the games without a status; a filter expands the collapsed group.
    library->setGroupBy(QStringLiteral("status"));
    library->setStatusFilter({"none"});
    QVERIFY(gameIds(*library).contains(marioKart));
    QVERIFY(!gameIds(*library).contains(zelda));

    // Cleared with the other filters.
    library->clearFilters();
    QVERIFY(library->statusFilter().isEmpty());
    QVERIFY(!readFile(file).contains("statuses"));

    // Each status with its name and how many games have it, hidden ones aside.
    const QVariantList statuses = library->statuses();
    QCOMPARE(statuses.size(), 6);
    QCOMPARE(statuses.first().toMap().value("id").toString(), QStringLiteral("playing"));
    QCOMPARE(statuses.first().toMap().value("name").toString(), QStringLiteral("Playing"));
    QCOMPARE(statuses.first().toMap().value("games").toInt(), 3);
    int total = 0;
    for (const QVariant &s : statuses)
        total += s.toMap().value("games").toInt();
    QCOMPARE(total, library->gameCount());

    // [list] statuses: bad values warn and are left out.
    const Parsed<ListSettings> parsed =
        parseListSettings("[list]\nstatuses = [\"playing\", \"finshed\", \"none\"]\n", QStringLiteral("settings.toml"));
    QCOMPARE(parsed.value.statuses, (QStringList{"playing", "none"}));
    QCOMPARE(parsed.warnings.size(), 1);
    QVERIFY(parsed.warnings.first().startsWith("settings.toml:2: unknown status \"finshed\""));
    QVERIFY(!parseListSettings("[list]\nstatuses = \"playing\"\n", QStringLiteral("settings.toml")).warnings.isEmpty());
}

void LibraryListTest::listSettingsFollowTagChanges()
{
    const QString file = m_data + QStringLiteral("/settings.toml");
    auto library = openLibrary();
    library->setTagFilter({QStringLiteral("Party"), QStringLiteral("Typo")});
    QVERIFY(readFile(file).contains("tags = [\"Party\", \"Typo\"]\n"));

    // Renamed in Settings: renamed in the filter, which keeps filtering.
    QCOMPARE(library->renameTag(QStringLiteral("Party"), QStringLiteral("Fiesta")), QString());
    QCOMPARE(library->tagFilter(), (QStringList{"Fiesta", "Typo"}));
    QVERIFY(readFile(file).contains("tags = [\"Fiesta\", \"Typo\"]\n"));
    QTRY_VERIFY_WITH_TIMEOUT(library->tags().first().toMap().value("name") != QStringLiteral("Party")
                                 && gameIds(*library).size() == 2, 5000);
    QVERIFY(library->filtersActive());

    // Deleted: removed from it. The unknown tag stays, ignored.
    QCOMPARE(library->removeTag(QStringLiteral("Fiesta")), QString());
    QCOMPARE(library->tagFilter(), QStringList{"Typo"});
    QVERIFY(readFile(file).contains("tags = [\"Typo\"]\n"));
    QTRY_VERIFY_WITH_TIMEOUT(!library->filtersActive(), 5000);
}

void LibraryListTest::emptyDataDir()
{
    QTemporaryDir empty;
    Library library(empty.path(), m_cache);
    library.store().setSteamLocations({m_installs.home, {}});
    library.store().setHeroicLocations(fakeHeroic());
    library.startRefresh();
    QTRY_VERIFY(!library.loading());
    QCOMPARE(library.gameCount(), 0);
    QVERIFY(library.problems().isEmpty());
    QVERIFY(QFileInfo(empty.filePath("library/games")).isDir());
    // A new library starts with the default tags, each in its own color.
    QCOMPARE(library.store().snapshot().content.tags, LibraryStore::defaultTags());
    QCOMPARE(LibraryStore::defaultTags().size(), 24);
    QSet<int> colors;
    for (const TagDef &t : LibraryStore::defaultTags())
        colors.insert(t.color);
    QCOMPARE(colors.size(), 24);
    QCOMPARE(*std::max_element(colors.cbegin(), colors.cend()), 23);
    QVERIFY(library.missingDefaultTags().isEmpty());
}

namespace {
QByteArray kvText(const char *key, const QByteArray &value)
{
    return QByteArray("\x01") + key + QByteArray(1, '\0') + value + QByteArray(1, '\0');
}

QByteArray kvInt(const QByteArray &key, qint32 value)
{
    QByteArray out = QByteArray("\x02") + key + QByteArray(1, '\0');
    for (int i = 0; i < 4; ++i)
        out.append(char((value >> (8 * i)) & 0xff));
    return out;
}

QByteArray kvMap(const QByteArray &key, const QList<QByteArray> &children)
{
    QByteArray out = QByteArray(1, '\0') + key + QByteArray(1, '\0');
    for (const QByteArray &child : children)
        out += child;
    return out + QByteArray("\x08");
}

// The fixture builder's files (tests/data/steam/appcache/stats/make-fixtures.py) for a game of `count` achievements.
void writeAchievements(const QString &steam, qint64 appId, int count, int unlocked)
{
    QList<QByteArray> bits;
    for (int i = 0; i < count; ++i) {
        const QByteArray name = "A" + QByteArray::number(i);
        bits.append(kvMap(QByteArray::number(i),
                          {kvText("name", name),
                           kvMap("display", {kvMap("name", {kvText("token", name + "_NAME"), kvText("english", name)}),
                                             kvMap("desc", {kvText("token", name + "_DESC"), kvText("english", "Do it.")}),
                                             kvInt("hidden", 0), kvText("icon", "a.jpg"), kvText("icon_gray", "a_gray.jpg")})}));
    }
    const QByteArray id = QByteArray::number(appId);
    const QString stats = steam + QStringLiteral("/appcache/stats/");
    writeFile(stats + QStringLiteral("UserGameStatsSchema_%1.bin").arg(appId),
              kvMap(id, {kvText("gamename", "Game"), kvInt("version", 3),
                         kvMap("stats", {kvMap("0", {kvText("type", "ACHIEVEMENTS"), kvMap("bits", bits)})})})
                  + QByteArray("\x08"));
    if (unlocked < 0)
        return;
    QList<QByteArray> times;
    for (int i = 0; i < unlocked; ++i)
        times.append(kvInt(QByteArray::number(i), 1700000000 + i));
    writeFile(stats + QStringLiteral("UserGameStats_39734273_%1.bin").arg(appId),
              kvMap("cache", {kvInt("crc", 123456789), kvInt("PendingChanges", 0),
                              kvMap("0", {kvInt("data", (1 << unlocked) - 1), kvMap("AchievementTimes", times)})})
                  + QByteArray("\x08"));
}

QStringList sorted(QStringList ids)
{
    ids.sort();
    return ids;
}
}

void LibraryListTest::newFilterSettings()
{
    const auto parse = [](const char *text) { return parseListSettings(text, QStringLiteral("settings.toml")); };
    Parsed<ListSettings> parsed =
        parse("[list]\nyear_from = 2015\nyear_to = 2023\nmin_rating = 3.5\nplay_times = [\"never\", \"2_10h\", \"never\"]\n"
              "last_played = \"month\"\nachievements = [\"completed\", \"none\"]\nmore_open = true\n");
    QVERIFY(parsed.warnings.isEmpty());
    QCOMPARE(parsed.value.yearFrom, 2015);
    QCOMPARE(parsed.value.yearTo, 2023);
    QCOMPARE(parsed.value.minRating, 3.5);
    QCOMPARE(parsed.value.playTimes, (QStringList{"never", "2_10h"}));
    QCOMPARE(parsed.value.lastPlayed, QStringLiteral("month"));
    QCOMPARE(parsed.value.achievements, (QStringList{"completed", "none"}));
    QVERIFY(parsed.value.moreOpen);
    QCOMPARE(parse("[list]\nmin_rating = 4\n").value.minRating, 4.0); // a whole number
    QCOMPARE(parse("[list]\nmin_rating = 0\n").value.minRating, 0.0); // 0 is a rating, not off
    QCOMPARE(parse("[list]\nmin_rating = 3.7\n").value.minRating, 3.5); // half steps

    parsed = parse("[list]\nyear_from = 0\nyear_to = \"soon\"\nmin_rating = 6\nplay_times = [\"weeks\"]\n"
                   "last_played = \"today\"\nachievements = \"all\"\nmore_open = 1\n");
    QCOMPARE(parsed.value, ListSettings());
    QCOMPARE(parsed.warnings.size(), 7);
    QVERIFY(anyContains(parsed.warnings, "settings.toml:2: year_from must be a year"));
    QVERIFY(anyContains(parsed.warnings, "settings.toml:3: year_to must be a year"));
    QVERIFY(anyContains(parsed.warnings, "settings.toml:4: min_rating must be a number from 0 to 5"));
    QVERIFY(anyContains(parsed.warnings, "settings.toml:5: unknown value \"weeks\" in play_times"));
    QVERIFY(anyContains(parsed.warnings, "settings.toml:6: last_played must be"));
    QVERIFY(anyContains(parsed.warnings, "settings.toml:7: achievements must be a list"));
    QVERIFY(anyContains(parsed.warnings, "settings.toml:8: more_open must be true or false"));

    // Written when set, removed at the default; Clear filters leaves More filters as it is.
    const QString file = m_data + QStringLiteral("/settings.toml");
    auto library = openLibrary();
    library->setYearFrom(2015);
    library->setYearTo(2023);
    library->setMinRating(3.5);
    library->setPlayTimeFilter({"never", "2_10h"});
    library->setLastPlayedFilter(QStringLiteral("month"));
    library->setAchievementFilter({"completed"});
    library->setMoreOpen(true);
    const QByteArray written = readFile(file);
    QVERIFY2(written.contains("year_from = 2015\n"), written.constData());
    QVERIFY(written.contains("year_to = 2023\n"));
    QVERIFY(written.contains("min_rating = 3.5\n"));
    QVERIFY(written.contains("play_times = [\"never\", \"2_10h\"]\n"));
    QVERIFY(written.contains("last_played = \"month\"\n"));
    QVERIFY(written.contains("achievements = [\"completed\"]\n"));
    QVERIFY(written.contains("more_open = true\n"));
    library->clearFilters();
    const QByteArray cleared = readFile(file);
    for (const char *key : {"year_from", "year_to", "min_rating", "play_times", "last_played", "achievements"})
        QVERIFY2(!cleared.contains(key), key);
    QVERIFY(cleared.contains("more_open = true\n"));
    library->setMoreOpen(false);
    QVERIFY(!readFile(file).contains("more_open"));
}

void LibraryListTest::newFiltersRestoredAtStartAndByHandEdit()
{
    const QByteArray settings = demoSettings();
    writeFile(m_data + QStringLiteral("/settings.toml"),
              settings + "[list]\ninstalled_only = false\nyear_from = 2022\nmin_rating = 4\nmore_open = true\n");
    auto library = openLibrary();
    QCOMPARE(library->yearFrom(), 2022);
    QCOMPARE(library->minRating(), 4.0);
    QVERIFY(library->moreOpen());
    QVERIFY(library->filtersActive());
    library->setGroupBy(QStringLiteral("none"));
    QCOMPARE(gameIds(*library), QStringList{"steam-1245620"});

    writeFile(m_data + QStringLiteral("/settings.toml"),
              settings + "[list]\ninstalled_only = false\nyear_from = 2022\nlast_played = \"yesterday\"\n");
    QTRY_COMPARE(library->minRating(), -1.0);
    QVERIFY(!library->moreOpen());
    QVERIFY(library->lastPlayedFilter().isEmpty()); // an unknown period is left out with a warning
    QVERIFY(gameIds(*library).size() > 1);
}

void LibraryListTest::yearAndRatingFilters()
{
    auto library = openLibrary();
    library->setInstalledOnly(false);
    library->setGroupBy(QStringLiteral("none"));
    const int all = library->rowCount();

    // Games with no year are left out while any bound is set.
    library->setYearFrom(2022);
    QCOMPARE(sorted(gameIds(*library)),
             (QStringList{"proton-diablo-iv", "steam-1086940", "steam-1245620", "switch-kirby-and-the-forgotten-land",
                          "switch-pikmin-4", "switch-super-mario-bros-wonder",
                          "switch-the-legend-of-zelda-tears-of-the-kingdom"}));
    QVERIFY(!gameIds(*library).contains(QStringLiteral("linux-supertuxkart"))); // year 0
    QVERIFY(!gameIds(*library).contains(QStringLiteral("steam-730"))); // hidden
    library->setYearFrom(0);
    QCOMPARE(library->rowCount(), all);

    library->setYearTo(2013);
    QCOMPARE(sorted(gameIds(*library)),
             (QStringList{"3ds-fire-emblem-awakening", "3ds-kid-icarus-uprising",
                          "3ds-the-legend-of-zelda-a-link-between-worlds", "proton-final-fantasy-xiv-online"}));
    library->setYearFrom(2013);
    QCOMPARE(sorted(gameIds(*library)),
             (QStringList{"3ds-the-legend-of-zelda-a-link-between-worlds", "proton-final-fantasy-xiv-online"}));
    library->setYearTo(2012); // a range that ends before it starts matches nothing
    QVERIFY(gameIds(*library).isEmpty());
    library->setYearFrom(2022);
    library->setYearTo(2022);
    QCOMPARE(sorted(gameIds(*library)), (QStringList{"steam-1245620", "switch-kirby-and-the-forgotten-land"}));
    library->setYearFrom(-5); // out of range: no bound
    library->setYearTo(12000); // clamped to the last year
    QCOMPARE(library->yearFrom(), 0);
    QCOMPARE(library->yearTo(), 9999);
    library->clearFilters();
    QCOMPARE(library->yearTo(), 0);
    library->setInstalledOnly(false);

    // A rating of -1 is "unrated" and never reaches a minimum, even 0; 3.7 (IGDB's) counts as 3.5.
    QCOMPARE(library->setGameValue(QStringLiteral("switch-pikmin-4"), QStringLiteral("rating"), 3.7), QString());
    library->setMinRating(0);
    QCOMPARE(sorted(gameIds(*library)), (QStringList{"steam-1245620", "switch-mario-tennis-aces", "switch-pikmin-4"}));
    library->setMinRating(3.5);
    QCOMPARE(sorted(gameIds(*library)), (QStringList{"steam-1245620", "switch-mario-tennis-aces", "switch-pikmin-4"}));
    library->setMinRating(3.6); // rounds to the half step, 3.5
    QCOMPARE(library->minRating(), 3.5);
    library->setMinRating(4);
    QCOMPARE(gameIds(*library), QStringList{"steam-1245620"});
    library->setMinRating(9);
    QCOMPARE(library->minRating(), 5.0);
    QVERIFY(gameIds(*library).isEmpty());
    library->setMinRating(-3);
    QCOMPARE(library->minRating(), -1.0);
    QCOMPARE(library->rowCount(), all);
    library->setInstalledOnly(true);
    QVERIFY(!library->filtersActive());
}

void LibraryListTest::playTimeAndLastPlayedFilters()
{
    auto library = openLibrary();
    library->setInstalledOnly(false);
    library->setGroupBy(QStringLiteral("none"));
    const QString pikmin = QStringLiteral("switch-pikmin-4");
    const QString tetris = QStringLiteral("switch-tetris");
    const QString pokemon = QStringLiteral("3ds-pokemon-ultra-sun");
    const QString kirby = QStringLiteral("switch-kirby-and-the-forgotten-land"); // 540 minutes
    const QString key = QStringLiteral("play_minutes");
    QCOMPARE(library->setGameValue(pikmin, key, 119), QString());
    QCOMPARE(library->setGameValue(tetris, key, 120), QString());
    QCOMPARE(library->setGameValue(pokemon, key, 5999), QString());
    QCOMPARE(library->setGameValue(QStringLiteral("linux-factorio"), key, 6000), QString());

    const auto matching = [&](const QStringList &times) {
        library->setPlayTimeFilter(times);
        return gameIds(*library);
    };
    QVERIFY(matching({"under_2h"}).contains(pikmin));
    QVERIFY(!gameIds(*library).contains(tetris)); // 120 minutes is 2 h, in the next band
    QVERIFY(matching({"2_10h"}).contains(tetris));
    QVERIFY(gameIds(*library).contains(kirby));
    QVERIFY(matching({"50_100h"}).contains(pokemon));
    QVERIFY(!gameIds(*library).contains(QStringLiteral("linux-factorio")));
    QCOMPARE(matching({"over_100h"}).contains(QStringLiteral("linux-factorio")), true);
    const QStringList never = matching({"never"});
    QVERIFY(never.contains(QStringLiteral("steam-504230")));
    QVERIFY(!never.contains(pikmin) && !never.contains(kirby));
    const QStringList either = matching({"under_2h", "2_10h", "bogus", "under_2h"});
    QCOMPARE(library->playTimeFilter(), (QStringList{"under_2h", "2_10h"}));
    QVERIFY(either.contains(pikmin) && either.contains(tetris) && !either.contains(pokemon));
    library->setPlayTimeFilter({});

    // A linked copy has the group's total: Pokémon at 5999 minutes, linked to Kirby, has both.
    QCOMPARE(library->linkCopy(kirby, pokemon), QString());
    QVERIFY(matching({"over_100h"}).contains(pokemon));
    QVERIFY(gameIds(*library).contains(kirby));
    QVERIFY(!matching({"2_10h"}).contains(kirby));
    library->setPlayTimeFilter({});
    QCOMPARE(library->unlinkCopy(pokemon), QString());

    // Last played: the periods nest; a game never played matches only "never".
    const QDateTime now = QDateTime::currentDateTime();
    const QString older = QStringLiteral("linux-factorio");
    QCOMPARE(library->recordPlaySession(pikmin, 1, now.addDays(-6)), QString());
    QCOMPARE(library->recordPlaySession(tetris, 1, now.addDays(-7)), QString());
    QCOMPARE(library->recordPlaySession(pokemon, 1, now.addDays(-30)), QString());
    QCOMPARE(library->recordPlaySession(older, 1, now.addDays(-365)), QString());
    const auto played = [&](const QString &period) {
        library->setLastPlayedFilter(period);
        return gameIds(*library);
    };
    QStringList ids = played(QStringLiteral("week"));
    QVERIFY(ids.contains(pikmin) && !ids.contains(tetris));
    ids = played(QStringLiteral("month"));
    QVERIFY(ids.contains(pikmin) && ids.contains(tetris) && !ids.contains(pokemon));
    ids = played(QStringLiteral("year"));
    QVERIFY(ids.contains(pikmin) && ids.contains(tetris) && ids.contains(pokemon) && !ids.contains(older));
    ids = played(QStringLiteral("older"));
    QVERIFY(ids.contains(older) && !ids.contains(pikmin) && !ids.contains(pokemon));
    for (const QString &period : {"week", "month", "year", "older"})
        QVERIFY(!played(period).contains(QStringLiteral("steam-504230"))); // never played
    ids = played(QStringLiteral("never"));
    QVERIFY(ids.contains(QStringLiteral("steam-504230")) && !ids.contains(pikmin) && !ids.contains(older));
    library->setLastPlayedFilter(QStringLiteral("yesterday")); // not a period: ignored
    QCOMPARE(library->lastPlayedFilter(), QStringLiteral("never"));
    library->setLastPlayedFilter({});
    QVERIFY(gameIds(*library).contains(QStringLiteral("steam-504230")));
}

void LibraryListTest::achievementFilterWaitsForThePass()
{
    const QString steam = m_data + QStringLiteral("/steam");
    writeAchievements(steam, 1145360, 5, 4); // Hades: in progress
    writeAchievements(steam, 367520, 10, -1); // Hollow Knight: a schema, no progress: not started
    writeAchievements(steam, 413150, 4, 4); // Stardew Valley: completed
    auto library = openLibrary();
    library->setInstalledOnly(false);
    library->setGroupBy(QStringLiteral("none"));
    const int all = library->rowCount();
    const auto counts = [&] {
        QMap<QString, int> out;
        for (const QVariant &choice : library->achievementChoices())
            out.insert(choice.toMap().value("id").toString(), choice.toMap().value("games").toInt());
        return out;
    };

    // Nothing reads them until a sort, group or filter needs them.
    QTest::qWait(300);
    QCOMPARE(counts().value("none"), all);

    // The pass hasn't ended: every game has none to earn, and the list recomputes when it does.
    library->setAchievementFilter({"completed", "bogus"});
    QCOMPARE(library->achievementFilter(), QStringList{"completed"});
    QVERIFY(gameIds(*library).isEmpty());
    library->setAchievementFilter({"none"});
    QCOMPARE(library->rowCount(), all);
    library->setAchievementFilter({"completed"});
    QTRY_COMPARE_WITH_TIMEOUT(gameIds(*library), QStringList{"steam-413150"}, 5000);
    QCOMPARE(counts().value("completed"), 1);
    QCOMPARE(counts().value("in_progress"), 1);
    QCOMPARE(counts().value("not_started"), 1);
    QCOMPARE(counts().value("none"), all - 3);

    library->setAchievementFilter({"in_progress", "not_started"});
    QCOMPARE(sorted(gameIds(*library)), (QStringList{"steam-1145360", "steam-367520"}));
    library->setAchievementFilter({"none"});
    QCOMPARE(library->rowCount(), all - 3);

    // A game with a schema and no achievements, and one that isn't Steam's, have none to earn.
    QVERIFY(gameIds(*library).contains(QStringLiteral("steam-1086940")));
    QVERIFY(gameIds(*library).contains(QStringLiteral("switch-pikmin-4")));

    // A linked copy has its group's best count.
    library->setAchievementFilter({"in_progress"});
    QCOMPARE(library->linkCopy(QStringLiteral("switch-tetris"), QStringLiteral("steam-1145360")), QString());
    QCOMPARE(sorted(gameIds(*library)), (QStringList{"steam-1145360", "switch-tetris"}));
}

void LibraryListTest::achievementCollectionWaitsForThePass()
{
    const QString steam = m_data + QStringLiteral("/steam");
    writeAchievements(steam, 413150, 4, 4); // Stardew Valley: completed
    writeAchievements(steam, 1145360, 5, 4); // Hades: in progress
    writeFile(m_data + QStringLiteral("/library/collections.toml"),
              "[[collection]]\nid = \"done\"\nname = \"Done\"\nrules = true\nachievements = [\"completed\"]\n");

    // A collection that needs them starts the pass at startup, with no sort or filter on; until it ends no game
    // has an achievement, and the count follows when it does.
    auto library = openLibrary();
    QTRY_COMPARE_WITH_TIMEOUT(library->collections().first().toMap().value("count").toInt(), 1, 5000);
    library->openCollection(QStringLiteral("done"));
    QCOMPARE(gameIds(*library), QStringList{"steam-413150"});
}

void LibraryListTest::achievementFilterRestoredAtStart()
{
    const QString steam = m_data + QStringLiteral("/steam");
    writeAchievements(steam, 413150, 4, 4);
    writeFile(m_data + QStringLiteral("/settings.toml"),
              demoSettings() + "[list]\ninstalled_only = false\ngroup_by = \"none\"\nachievements = [\"completed\"]\n");
    auto library = openLibrary();
    QTRY_COMPARE_WITH_TIMEOUT(gameIds(*library), QStringList{"steam-413150"}, 5000);
}

void LibraryListTest::filterChoicesAndHistograms()
{
    auto library = openLibrary();
    QCOMPARE(library->setGameValue(QStringLiteral("switch-pikmin-4"), QStringLiteral("rating"), 3.7), QString());
    const int games = library->gameCount();

    int total = 0;
    QMap<QString, int> times;
    for (const QVariant &choice : library->playTimeChoices()) {
        times.insert(choice.toMap().value("id").toString(), choice.toMap().value("games").toInt());
        total += choice.toMap().value("games").toInt();
    }
    QCOMPARE(total, games); // every shown game is in one band; hidden games aren't counted
    QCOMPARE(library->playTimeChoices().size(), 6);
    QVERIFY(times.value("never") > 0);

    // Years: every year from the first to the last, 2014 with none; the rest have no year.
    const QVariantList years = library->yearHistogram();
    QCOMPARE(years.first().toMap().value("year").toInt(), 2012);
    QCOMPARE(years.first().toMap().value("games").toInt(), 2);
    QCOMPARE(years.last().toMap().value("year").toInt(), 2023);
    QCOMPARE(years.size(), 12);
    QCOMPARE(years.at(2).toMap().value("games").toInt(), 0); // 2014
    int dated = 0;
    for (const QVariant &year : years)
        dated += year.toMap().value("games").toInt();
    QCOMPARE(dated + library->gamesWithoutYear(), games);
    QVERIFY(library->gamesWithoutYear() >= 1);

    // Ratings: eleven half steps; 3.7 is in 3.5.
    const QVariantList ratings = library->ratingHistogram();
    QCOMPARE(ratings.size(), 11);
    QCOMPARE(ratings.at(7).toMap().value("rating").toDouble(), 3.5);
    QCOMPARE(ratings.at(7).toMap().value("games").toInt(), 2);
    QCOMPARE(ratings.at(9).toMap().value("games").toInt(), 1);
    QCOMPARE(library->gamesWithoutRating(), games - 3);

    // A hidden game is left out of every count.
    const int withoutHidden = library->gamesWithoutYear();
    library->setShowHidden(true);
    QCOMPARE(library->gamesWithoutYear(), withoutHidden);
}

namespace {
// Five games on 3ds with the values the new sorts and groups read; "Sort E" has a year and no rating or date.
void writeSortGames(const std::function<QString(const QString &)> &gamePath, const QDateTime &now)
{
    const auto at = [&](int daysAgo) {
        return now.addDays(-daysAgo).toOffsetFromUtc(now.offsetFromUtc()).toString(Qt::ISODate).toUtf8();
    };
    const auto game = [&](const char *id, const char *title, const QByteArray &rest) {
        writeFile(gamePath(QString::fromLatin1(id)),
                        QByteArray("title = \"") + title + "\"\nplatform = \"3ds\"\n" + rest);
    };
    game("3ds-sort-a", "Sort A", "year = 2001\nrating = 5\nadded = 2026-03-01\nlast_played = " + at(2) + "\n");
    game("3ds-sort-b", "Sort B", "year = 2010\nrating = 4.5\nadded = 2026-03-05\nlast_played = " + at(20) + "\n");
    game("3ds-sort-c", "Sort C", "rating = 3\nlast_played = " + at(200) + "\n");
    game("3ds-sort-d", "Sort D", "year = 2005\nrating = 2.5\nadded = 2026-02-01\nlast_played = " + at(400) + "\n");
    game("3ds-sort-e", "Sort E", "year = 2010\n");
}

QStringList only(const QStringList &ids)
{
    QStringList out;
    for (const QString &id : ids) {
        if (id.startsWith(QLatin1String("3ds-sort-")))
            out.append(id.mid(9));
    }
    return out;
}

QStringList headerGroups(const Library &library)
{
    QStringList out;
    for (int i = 0; i < library.rowCount(); ++i) {
        if (library.data(library.index(i), Library::IsHeaderRole).toBool())
            out.append(library.data(library.index(i), Library::GroupRole).toString());
    }
    return out;
}
}

void LibraryListTest::newSortOrders()
{
    writeSortGames([this](const QString &id) { return gamePath(id); }, QDateTime::currentDateTime());
    auto library = openLibrary();
    library->setInstalledOnly(false);
    library->setGroupBy(QStringLiteral("none"));
    const auto order = [&](const char *sort, bool descending) {
        library->setSort(QString::fromLatin1(sort));
        library->setSortDescending(descending);
        return only(gameIds(*library));
    };

    // The usual direction is the high end first (title and platform: A to Z).
    library->setSort(QStringLiteral("rating"));
    QVERIFY(library->sortDescending());
    library->setSort(QStringLiteral("platform"));
    QVERIFY(!library->sortDescending());
    for (const char *sort : {"year", "achievements", "added", "size"}) {
        library->setSort(QString::fromLatin1(sort));
        QVERIFY2(library->sortDescending(), sort);
    }

    // Games with no value go last by title, either way; equal values go by title.
    QCOMPARE(order("rating", true), (QStringList{"a", "b", "c", "d", "e"}));
    QCOMPARE(order("rating", false), (QStringList{"d", "c", "b", "a", "e"}));
    QCOMPARE(order("year", true), (QStringList{"b", "e", "d", "a", "c"}));
    QCOMPARE(order("year", false), (QStringList{"a", "d", "b", "e", "c"}));
    QCOMPARE(order("added", true), (QStringList{"b", "a", "d", "c", "e"}));
    QCOMPARE(order("added", false), (QStringList{"d", "a", "b", "c", "e"}));

    // Every game on one platform: platform changes nothing, whatever the direction.
    QCOMPARE(order("platform", false), (QStringList{"a", "b", "c", "d", "e"}));
    QCOMPARE(order("platform", true), (QStringList{"a", "b", "c", "d", "e"}));
}

void LibraryListTest::sortByPlatformFollowsTheRail()
{
    auto library = openLibrary();
    library->setInstalledOnly(false);
    library->setGroupBy(QStringLiteral("none"));
    QStringList rail;
    for (const QVariant &p : library->platforms())
        rail.append(p.toMap().value("id").toString());
    const auto places = [&] {
        QList<int> out;
        for (const QString &id : gameIds(*library))
            out.append(int(rail.indexOf(library->gameAt(library->rowOfGame(id)).value("platform").toString())));
        return out;
    };

    library->setSort(QStringLiteral("platform"));
    QList<int> places1 = places();
    QVERIFY(std::is_sorted(places1.cbegin(), places1.cend()));
    QCOMPARE(places1.first(), 0);
    library->setSortDescending(true);
    places1 = places();
    QVERIFY(std::is_sorted(places1.crbegin(), places1.crend()));
    // Within a platform, by title.
    library->setSortDescending(false);
    QCOMPARE(library->gameAt(0).value("title").toString().toLower()
                 < library->gameAt(1).value("title").toString().toLower(),
             library->gameAt(0).value("platform") == library->gameAt(1).value("platform"));
}

void LibraryListTest::sortBySize()
{
    const auto program = [&](const char *id, const char *title, int bytes) {
        const QString dir = m_data + QStringLiteral("/sizes/") + QString::fromLatin1(id);
        writeFile(dir + QStringLiteral("/data.bin"), QByteArray(bytes, 'x'));
        writeFile(gamePath(QString::fromLatin1(id)),
                  QByteArray("title = \"") + title + "\"\nplatform = \"linux\"\ninstall_dir = \"" + dir.toUtf8()
                      + "\"\n");
    };
    program("linux-size-big", "Size Big", 9000);
    program("linux-size-small", "Size Small", 100);
    program("linux-size-none", "Size None", 0);
    auto library = openLibrary();
    library->setInstalledOnly(false);
    library->setGroupBy(QStringLiteral("none"));
    library->setSort(QStringLiteral("size"));
    const auto sizes = [&] {
        QStringList out;
        for (const QString &id : gameIds(*library)) {
            if (id.startsWith(QLatin1String("linux-size-")))
                out.append(id.mid(11));
        }
        return out;
    };
    QTRY_COMPARE_WITH_TIMEOUT(sizes(), (QStringList{"big", "small", "none"}), 5000);
    library->setSortDescending(false);
    QCOMPARE(sizes(), (QStringList{"small", "big", "none"}));
    const auto value = [&](const char *id) {
        return library->data(library->index(library->rowOfGame(QString::fromLatin1(id))), Library::SortValueRole)
            .toString();
    };
    QCOMPARE(value("linux-size-big"), library->sizeText(9000));
    QCOMPARE(value("linux-size-none"), QString());
}

void LibraryListTest::sortByAchievements()
{
    const QString steam = m_data + QStringLiteral("/steam");
    writeAchievements(steam, 1145360, 4, 1); // Hades: 1/4
    writeAchievements(steam, 367520, 10, -1); // Hollow Knight: 0/10, not started
    writeAchievements(steam, 413150, 4, 4); // Stardew Valley: 4/4
    auto library = openLibrary();
    library->setInstalledOnly(false);
    library->setGroupBy(QStringLiteral("none"));
    const QStringList steamGames = {"steam-413150", "steam-1145360", "steam-367520"};
    const auto firstThree = [&] { return gameIds(*library).mid(0, 3); };

    // Nothing reads them until the sort asks; until the pass ends, games sort by title.
    QTest::qWait(300);
    library->setSort(QStringLiteral("achievements"));
    QTRY_COMPARE_WITH_TIMEOUT(firstThree(), steamGames, 5000);
    const auto value = [&](const char *id) {
        return library->data(library->index(library->rowOfGame(QString::fromLatin1(id))), Library::SortValueRole)
            .toString();
    };
    QCOMPARE(value("steam-413150"), QStringLiteral("4/4"));
    QCOMPARE(value("steam-1145360"), QStringLiteral("1/4"));
    QCOMPARE(value("steam-367520"), QStringLiteral("0/10"));
    QCOMPARE(value("switch-pikmin-4"), QString()); // none to earn: last

    library->setSortDescending(false);
    QCOMPARE(firstThree(), (QStringList{"steam-367520", "steam-1145360", "steam-413150"}));
    QCOMPARE(gameIds(*library).mid(3).contains(QStringLiteral("steam-413150")), false);
}

void LibraryListTest::sortByAchievementsRestoredAtStart()
{
    const QString steam = m_data + QStringLiteral("/steam");
    writeAchievements(steam, 413150, 4, 4);
    writeAchievements(steam, 1145360, 4, 1);
    writeFile(m_data + QStringLiteral("/settings.toml"),
              demoSettings() + "[list]\ninstalled_only = false\ngroup_by = \"none\"\nsort = \"achievements\"\n");
    auto library = openLibrary();
    QTRY_COMPARE_WITH_TIMEOUT(gameIds(*library).mid(0, 2), (QStringList{"steam-413150", "steam-1145360"}), 5000);
}

void LibraryListTest::newGroupings()
{
    writeSortGames([this](const QString &id) { return gamePath(id); }, QDateTime::currentDateTime());
    auto library = openLibrary();
    library->setInstalledOnly(false);
    library->setSearch(QStringLiteral("Sort"));
    const auto title = [&](int row) { return library->data(library->index(row), Library::TitleRole).toString(); };

    library->setGroupBy(QStringLiteral("year"));
    QCOMPARE(headerGroups(*library), (QStringList{"2010", "2005", "2001", "none"}));
    QCOMPARE(title(0), QStringLiteral("2010"));
    QCOMPARE(title(library->rowCount() - 2), Library::tr("No year"));
    QCOMPARE(library->data(library->index(0), Library::CountRole).toInt(), 2);
    QCOMPARE(only(gameIds(*library)), (QStringList{"b", "e", "d", "a", "c"}));
    // The grouped field's direction flips the groups; games with no year stay last.
    library->setSort(QStringLiteral("year"));
    library->setSortDescending(false);
    QCOMPARE(headerGroups(*library), (QStringList{"2001", "2005", "2010", "none"}));
    library->setSort(QStringLiteral("title"));
    QCOMPARE(headerGroups(*library), (QStringList{"2010", "2005", "2001", "none"}));

    library->setGroupBy(QStringLiteral("rating"));
    QCOMPARE(headerGroups(*library), (QStringList{"5", "4", "3", "under_3", "unrated"}));
    QCOMPARE(title(0), QStringLiteral("5"));
    QCOMPARE(title(2), Library::tr("4 – 4.5"));
    QCOMPARE(title(4), Library::tr("3 – 3.5"));
    QCOMPARE(title(6), Library::tr("Under 3"));
    QCOMPARE(title(8), Library::tr("Unrated"));
    library->setSort(QStringLiteral("rating"));
    library->setSortDescending(false);
    QCOMPARE(headerGroups(*library), (QStringList{"under_3", "3", "4", "5", "unrated"}));

    library->setSort(QStringLiteral("title"));
    library->setGroupBy(QStringLiteral("last_played"));
    QCOMPARE(headerGroups(*library), (QStringList{"week", "month", "year", "older", "never"}));
    QCOMPARE(title(0), Library::tr("This week"));
    QCOMPARE(title(2), Library::tr("This month"));
    QCOMPARE(title(4), Library::tr("This year"));
    QCOMPARE(title(6), Library::tr("Older"));
    QCOMPARE(title(8), Library::tr("Never"));
    library->setSort(QStringLiteral("last_played"));
    library->setSortDescending(false);
    QCOMPARE(headerGroups(*library), (QStringList{"older", "year", "month", "week", "never"}));

    // With a platform selected the groups still show; a collapsed group hides its games, per grouping.
    library->setSearch({});
    library->setInstalledOnly(true);
    library->setPlatformFilter(QStringLiteral("switch"));
    library->setGroupBy(QStringLiteral("year"));
    const QString first = headerGroups(*library).first();
    const int shown = library->rowCount();
    library->toggleGroup(first);
    QVERIFY(library->rowCount() < shown - 1 || headerGroups(*library).size() == 1);
    library->setGroupBy(QStringLiteral("rating"));
    QCOMPARE(library->rowCount() > 0, true);
}

void LibraryListTest::newSortValues()
{
    const QDateTime now = QDateTime::currentDateTime();
    writeSortGames([this](const QString &id) { return gamePath(id); }, now);
    auto library = openLibrary();
    library->setInstalledOnly(false);
    const auto value = [&](const char *id) {
        return library->data(library->index(library->rowOfGame(QString::fromLatin1(id))), Library::SortValueRole)
            .toString();
    };
    library->setSort(QStringLiteral("rating"));
    QCOMPARE(value("3ds-sort-b"), QLocale().toString(4.5, 'f', 1));
    QCOMPARE(value("3ds-sort-e"), QString());
    library->setSort(QStringLiteral("year"));
    QCOMPARE(value("3ds-sort-b"), QStringLiteral("2010"));
    QCOMPARE(value("3ds-sort-c"), QString());
    library->setSort(QStringLiteral("added"));
    QCOMPARE(value("3ds-sort-a"), QLocale().toString(QDate(2026, 3, 1), QLocale::ShortFormat));
    QCOMPARE(value("3ds-sort-c"), QString());
    library->setSort(QStringLiteral("platform"));
    QCOMPARE(value("3ds-sort-a"), QString());
    // Headers have none.
    QVERIFY(library->data(library->index(0), Library::IsHeaderRole).toBool());
    QCOMPARE(library->data(library->index(0), Library::SortValueRole).toString(), QString());
}

void LibraryListTest::newSortsAndGroupsInSettings()
{
    Parsed<ListSettings> parsed = parseListSettings("[list]\nsort = \"platform\"\ngroup_by = \"last_played\"\n", "settings.toml");
    QVERIFY(parsed.warnings.isEmpty());
    QCOMPARE(parsed.value.sort, QStringLiteral("platform"));
    QVERIFY(!parsed.value.descending);
    QCOMPARE(parsed.value.groupBy, QStringLiteral("last_played"));
    for (const char *sort : {"rating", "year", "achievements", "added", "size"}) {
        parsed = parseListSettings(QByteArray("[list]\nsort = \"") + sort + "\"\n", "settings.toml");
        QVERIFY2(parsed.warnings.isEmpty(), sort);
        QVERIFY2(parsed.value.descending, sort);
    }
    for (const char *group : {"year", "rating"}) {
        parsed = parseListSettings(QByteArray("[list]\ngroup_by = \"") + group + "\"\n", "settings.toml");
        QVERIFY2(parsed.warnings.isEmpty(), group);
    }
    parsed = parseListSettings("[list]\nsort = \"popularity\"\n", "settings.toml");
    QVERIFY(parsed.warnings.at(0).contains(QStringLiteral("\"added\"")));

    // Written, and read back at the next start.
    auto library = openLibrary();
    library->setSort(QStringLiteral("added"));
    library->setGroupBy(QStringLiteral("year"));
    const QString file = m_data + QStringLiteral("/settings.toml");
    QTRY_VERIFY(readFile(file).contains("sort = \"added\"\n"));
    QVERIFY(readFile(file).contains("group_by = \"year\"\n"));
    QVERIFY(!readFile(file).contains("descending"));
    library.reset();
    library = openLibrary();
    QCOMPARE(library->sort(), QStringLiteral("added"));
    QVERIFY(library->sortDescending());
    QCOMPARE(library->groupBy(), QStringLiteral("year"));
}

QTEST_GUILESS_MAIN(LibraryListTest)

#include "library_list_test.moc"
