#include "librarytestbase.h"

#include "librarycache.h"
#include "libraryundo.h"
#include "steamcollections.h"

#include <QTemporaryDir>

// The library tests on collections.toml: loading, saving through the store, the cache, Undo's reach.
class LibraryCollectionsTest : public LibraryTestBase
{
    Q_OBJECT

private slots:
    void noFile();
    void saveUpdateRemove();
    void editedByHand();
    void persists();
    void undoCoversTheFile();
    void missingGamesStay();
    void listedAtoZWithCounts();
    void openShowsOnlyItsGames();
    void filtersNarrowWithinAndClearReturns();
    void leavingBringsTheOldFiltersBack();
    void openingNeverTouchesSavedFilters();
    void emptyCollection();
    void openingAnotherReplaces();
    void overviewFilterLeaves();
    void deletedWhileOpen();
    void restoredFromViewState();
    void tagSearchMatching();
    void makeAddRemove();
    void statesOfASelection();
    void removeFromARuleBasedCollection();
    void linkedCopiesTogether();
    void namesAndIds();
    void renameAndDelete();
    void removeFromTheOpenCollection();
    void savedFromTheFilters();
    void savedFiltersAreAFixedScope();
    void rulesIgnoreADeletedTag();
    void renamingATagUpdatesTheRules();
    void editRulesAndExceptions();
    void countsMatchesOfUnsavedRules();
    void countIgnoresUnknownTags();
    void savedFromTheMoreFilters();
    void editTurnsRulesOnAndOff();
    void steamAccounts();
    void steamImportMakesCollections();
    void steamImportAgainReplacesGames();
    void steamImportNamesAndFailures();
};

namespace {
Collection coop()
{
    Collection c;
    c.id = QStringLiteral("co-op");
    c.name = QStringLiteral("Co-op");
    c.added = {"steam-1145360"};
    return c;
}
}

void LibraryCollectionsTest::noFile()
{
    auto library = openLibrary();
    QVERIFY(library->store().collections().isEmpty());
    QVERIFY(!QFileInfo::exists(m_data + QStringLiteral("/library/collections.toml")));
}

void LibraryCollectionsTest::saveUpdateRemove()
{
    auto library = openLibrary();
    LibraryStore &store = library->store();
    const QString path = m_data + QStringLiteral("/library/collections.toml");

    QVERIFY(!store.setCollection(Collection{}).isEmpty()); // no id
    QCOMPARE(store.setCollection(coop()), QString());
    const QByteArray first = readFile(path);
    QVERIFY(first.contains("[[collection]]\nid = \"co-op\"\nname = \"Co-op\"\nadded = [\"steam-1145360\"]\n"));
    QCOMPARE(store.collections(), QList<Collection>{coop()});

    Collection saved;
    saved.id = QStringLiteral("saved");
    saved.name = QStringLiteral("Cozy Switch");
    saved.rules = CollectionRules{QStringLiteral("switch"), false, true, {}, {"Adventure"}, false, {}};
    saved.removed = {"switch-metroid-dread"};
    QCOMPARE(store.setCollection(saved), QString());
    QCOMPARE(store.collections(), (QList<Collection>{coop(), saved}));
    QVERIFY(readFile(path).startsWith(first)); // the first block is as it was

    // Updating one block leaves the other, and a comment above it, alone; keys it no longer uses go.
    writeFile(path, "# my note\n" + readFile(path));
    Collection edited = coop();
    edited.name = QStringLiteral("Couch co-op");
    edited.added.clear();
    edited.removed = {"x"};
    QCOMPARE(store.setCollection(edited), QString());
    QVERIFY(readFile(path).startsWith("# my note\n"));
    QVERIFY(readFile(path).contains("name = \"Couch co-op\"\nremoved = [\"x\"]\n"));
    QVERIFY(!readFile(path).contains("steam-1145360"));
    QCOMPARE(store.collections().at(1), saved);

    // Rules added to a hand-picked collection, then dropped again.
    edited.rules = CollectionRules{};
    edited.rules->tags = {"RPG"};
    QCOMPARE(store.setCollection(edited), QString());
    QCOMPARE(store.collections().at(0), edited);
    edited.rules.reset();
    QCOMPARE(store.setCollection(edited), QString());
    QCOMPARE(store.collections().at(0), edited);
    QVERIFY(!readFile(path).contains("rules = true\ntags"));

    QCOMPARE(store.removeCollection(QStringLiteral("co-op")), QString());
    QCOMPARE(store.collections(), QList<Collection>{saved});
    QCOMPARE(store.removeCollection(QStringLiteral("nothing")), QString());
    QCOMPARE(store.removeCollection(QStringLiteral("saved")), QString());
    QVERIFY(store.collections().isEmpty());
}

void LibraryCollectionsTest::editedByHand()
{
    auto library = openLibrary();
    LibraryStore &store = library->store();
    const QString path = m_data + QStringLiteral("/library/collections.toml");

    writeFile(path, "[[collection]]\nid = \"a\"\nname = \"A\"\nadded = [\"steam-367520\"]\n");
    store.reloadFiles({QStringLiteral("library/collections.toml")});
    QCOMPARE(store.collections().size(), 1);
    QCOMPARE(store.collections().first().name, QStringLiteral("A"));

    // A file that doesn't parse leaves the last good version in the store, and is not overwritten.
    writeFile(path, "[[collection]\n");
    store.reloadFiles({QStringLiteral("library/collections.toml")});
    QCOMPARE(store.collections().size(), 1);
    QVERIFY(!store.setCollection(coop()).isEmpty());
    QCOMPARE(readFile(path), QByteArray("[[collection]\n"));

    // Deleted: gone.
    QFile::remove(path);
    store.reloadFiles({QStringLiteral("library/collections.toml")});
    QVERIFY(store.collections().isEmpty());
}

void LibraryCollectionsTest::persists()
{
    Collection saved;
    saved.id = QStringLiteral("saved");
    saved.name = QStringLiteral("Saved");
    saved.steamId = QStringLiteral("uc-1");
    saved.rules = CollectionRules{QString(), true, false, {"playing"}, {"RPG"}, true, {"owned"}};
    saved.added = {"gone-game"};
    {
        auto library = openLibrary();
        QCOMPARE(library->store().setCollection(coop()), QString());
        QCOMPARE(library->store().setCollection(saved), QString());
    }
    // From the cache the first library wrote, then from the file alone.
    for (int pass = 0; pass < 2; ++pass) {
        auto library = openLibrary();
        QCOMPARE(library->store().collections(), (QList<Collection>{coop(), saved}));
        if (pass == 0) {
            QDir(m_cache).removeRecursively();
            QVERIFY(!QFileInfo::exists(LibraryCache::pathFor(m_cache, m_data)));
        }
    }
}

void LibraryCollectionsTest::undoCoversTheFile()
{
    auto library = openLibrary();
    LibraryStore &store = library->store();
    const QString path = m_data + QStringLiteral("/library/collections.toml");
    QTemporaryDir undoDir;
    LibraryUndo undo;
    undo.setFolders(m_data, undoDir.path());

    // The file is new: undoing the first change removes it.
    undo.start(QStringLiteral("add"));
    undo.keepCopy(QStringLiteral("library/collections.toml"));
    QCOMPARE(store.setCollection(coop()), QString());
    const quint64 added = undo.commit();
    QVERIFY(added != 0);
    QString error;
    QCOMPARE(undo.restore(added, &error), QStringList{"library/collections.toml"});
    QVERIFY(error.isEmpty());
    QVERIFY(!QFileInfo::exists(path));
    store.reloadFiles({QStringLiteral("library/collections.toml")});
    QVERIFY(store.collections().isEmpty());

    // The file exists: undoing puts the old text back.
    QCOMPARE(store.setCollection(coop()), QString());
    const QByteArray before = readFile(path);
    undo.start(QStringLiteral("edit"));
    undo.keepCopy(QStringLiteral("library/collections.toml"));
    Collection edited = coop();
    edited.name = QStringLiteral("Renamed");
    QCOMPARE(store.setCollection(edited), QString());
    const quint64 token = undo.commit();
    QVERIFY(token != 0);
    QCOMPARE(undo.restore(token, &error), QStringList{"library/collections.toml"});
    QCOMPARE(readFile(path), before);
    store.reloadFiles({QStringLiteral("library/collections.toml")});
    QCOMPARE(store.collections(), QList<Collection>{coop()});
}

void LibraryCollectionsTest::missingGamesStay()
{
    // A game that left the library (here: never was) keeps its id through other changes to the file.
    Collection c = coop();
    c.added = {"left-the-library", "steam-1145360"};
    auto library = openLibrary();
    LibraryStore &store = library->store();
    QCOMPARE(store.setCollection(c), QString());
    Collection other;
    other.id = QStringLiteral("other");
    other.name = QStringLiteral("Other");
    QCOMPARE(store.setCollection(other), QString());
    QCOMPARE(store.collections().first().added, (QStringList{"left-the-library", "steam-1145360"}));
    QVERIFY(readFile(m_data + QStringLiteral("/library/collections.toml")).contains("left-the-library"));
}

namespace {
Collection mix()
{
    Collection c;
    c.id = QStringLiteral("mix");
    c.name = QStringLiteral("Mix");
    // Hades is a favorite and installed; Mario Tennis Aces has no ROM, so Installed would drop it.
    c.added = {"steam-1145360", "switch-mario-tennis-aces", "switch-pikmin-4"};
    return c;
}

Collection named(const QString &id, const QString &name)
{
    Collection c;
    c.id = id;
    c.name = name;
    return c;
}

QStringList sorted(QStringList ids)
{
    ids.sort();
    return ids;
}
}

void LibraryCollectionsTest::listedAtoZWithCounts()
{
    auto library = openLibrary();
    QVERIFY(library->collections().isEmpty());
    QCOMPARE(library->store().setCollection(named(QStringLiteral("z"), QStringLiteral("zebra"))), QString());
    QCOMPARE(library->store().setCollection(mix()), QString());
    QCOMPARE(library->store().setCollection(named(QStringLiteral("a"), QStringLiteral("Apple"))), QString());

    const QVariantList list = library->collections();
    QCOMPARE(list.size(), 3);
    QCOMPARE(list.at(0).toMap().value("name").toString(), QStringLiteral("Apple"));
    QCOMPARE(list.at(1).toMap().value("name").toString(), QStringLiteral("Mix"));
    QCOMPARE(list.at(2).toMap().value("name").toString(), QStringLiteral("zebra"));
    QCOMPARE(list.at(1).toMap().value("id").toString(), QStringLiteral("mix"));
    QCOMPARE(list.at(1).toMap().value("count").toInt(), 3);
    QCOMPARE(list.at(0).toMap().value("count").toInt(), 0);
}

void LibraryCollectionsTest::openShowsOnlyItsGames()
{
    auto library = openLibrary();
    QCOMPARE(library->store().setCollection(mix()), QString());
    QVERIFY(library->collectionId().isEmpty());

    QSignalSpy changed(library.get(), &Library::collectionChanged);
    library->openCollection(QStringLiteral("mix"));
    QCOMPARE(changed.count(), 1);
    QCOMPARE(library->collectionId(), QStringLiteral("mix"));
    QCOMPARE(library->collection().value("name").toString(), QStringLiteral("Mix"));
    // Installed is off, so the uninstalled game is listed too.
    QCOMPARE(sorted(gameIds(*library)),
             (QStringList{"steam-1145360", "switch-mario-tennis-aces", "switch-pikmin-4"}));
    QVERIFY(!library->installedOnly());
    QVERIFY(!library->filtersActive());
    QVERIFY(library->platformFilter().isEmpty());

    library->openCollection(QStringLiteral("nope")); // unknown: nothing changes
    QCOMPARE(library->collectionId(), QStringLiteral("mix"));
    library->openCollection(QStringLiteral("mix")); // already open
    QCOMPARE(changed.count(), 1);

    // Search, sort and group by work as usual; a hidden game of the collection stays out.
    library->setSearch(QStringLiteral("pikmin"));
    QCOMPARE(gameIds(*library), QStringList{"switch-pikmin-4"});
    library->setSearch({});
    library->setGroupBy(QStringLiteral("none"));
    QCOMPARE(gameIds(*library).size(), 3);
}

void LibraryCollectionsTest::filtersNarrowWithinAndClearReturns()
{
    auto library = openLibrary();
    QCOMPARE(library->store().setCollection(mix()), QString());
    library->openCollection(QStringLiteral("mix"));

    library->setInstalledOnly(true);
    QVERIFY(library->filtersActive()); // Installed on is the filter here
    QVERIFY(!gameIds(*library).contains(QStringLiteral("switch-mario-tennis-aces")));
    QCOMPARE(library->notInstalledHidden(), 1);

    library->setFavoritesOnly(true);
    QCOMPARE(gameIds(*library), QStringList{"steam-1145360"});
    library->setFavoritesOnly(false);
    library->setPlatformFilter(QStringLiteral("switch"));
    QVERIFY(!gameIds(*library).contains(QStringLiteral("steam-1145360")));
    QCOMPARE(library->collectionId(), QStringLiteral("mix"));

    library->clearFilters();
    QVERIFY(!library->installedOnly());
    QVERIFY(!library->filtersActive());
    QCOMPARE(library->collectionId(), QStringLiteral("mix")); // only the x leaves
    library->setPlatformFilter({});
    QCOMPARE(gameIds(*library).size(), 3);
    QSignalSpy filtersChanged(library.get(), &Library::filtersChanged);
    library->clearFilters(); // nothing to clear
    QCOMPARE(filtersChanged.count(), 0);
}

void LibraryCollectionsTest::leavingBringsTheOldFiltersBack()
{
    auto library = openLibrary();
    QCOMPARE(library->store().setCollection(mix()), QString());
    library->setFavoritesOnly(true);
    library->setStatusFilter({QStringLiteral("none")});
    library->setShowHidden(true);
    library->setPlatformFilter(QStringLiteral("steam"));
    library->setSearch(QStringLiteral("hades"));
    const QStringList before = gameIds(*library);

    library->openCollection(QStringLiteral("mix"));
    QVERIFY(!library->favoritesOnly());
    QVERIFY(library->statusFilter().isEmpty());
    QVERIFY(!library->showHidden());
    QVERIFY(library->platformFilter().isEmpty());
    library->setTagFilter({QStringLiteral("Party")});
    library->setSearch({});

    library->closeCollection();
    QVERIFY(library->collectionId().isEmpty());
    QVERIFY(library->favoritesOnly());
    QCOMPARE(library->statusFilter(), QStringList{"none"});
    QVERIFY(library->showHidden());
    QVERIFY(library->tagFilter().isEmpty());
    QCOMPARE(library->platformFilter(), QStringLiteral("steam"));
    library->setSearch(QStringLiteral("hades"));
    QCOMPARE(gameIds(*library), before);
}

void LibraryCollectionsTest::openingNeverTouchesSavedFilters()
{
    auto library = openLibrary();
    QCOMPARE(library->store().setCollection(mix()), QString());
    library->setFavoritesOnly(true);
    const QString file = m_data + QStringLiteral("/settings.toml");
    const QByteArray normal = readFile(file);
    QVERIFY(normal.contains("favorites_only = true"));

    library->openCollection(QStringLiteral("mix"));
    library->setInstalledOnly(true);
    library->setTagFilter({QStringLiteral("Party")});
    library->setStatusFilter({QStringLiteral("playing")});
    library->setSort(QStringLiteral("play_time")); // not a filter: it saves as usual
    QByteArray now = readFile(file);
    QVERIFY(now.contains("favorites_only = true"));
    QVERIFY(!now.contains("Party"));
    QVERIFY(!now.contains("playing"));
    QVERIFY(!now.contains("installed_only"));
    QVERIFY(now.contains("sort = \"play_time\""));

    // The file edited by hand while open lands in the normal view, not in the collection.
    library->clearFilters();
    QVERIFY(!library->installedOnly());
    library->closeCollection();
    QVERIFY(library->favoritesOnly());
    QVERIFY(library->installedOnly());
    now = readFile(file);
    QVERIFY(now.contains("favorites_only = true"));
}

void LibraryCollectionsTest::emptyCollection()
{
    auto library = openLibrary();
    QCOMPARE(library->store().setCollection(named(QStringLiteral("none"), QStringLiteral("Nothing yet"))), QString());
    library->openCollection(QStringLiteral("none"));
    QCOMPARE(library->collection().value("count").toInt(), 0);
    QVERIFY(gameIds(*library).isEmpty());
    QCOMPARE(library->rowCount(), 0);
    library->closeCollection();
    QVERIFY(!gameIds(*library).isEmpty());
}

void LibraryCollectionsTest::openingAnotherReplaces()
{
    auto library = openLibrary();
    Collection other = named(QStringLiteral("other"), QStringLiteral("Other"));
    other.added = {"linux-supertuxkart"};
    QCOMPARE(library->store().setCollection(mix()), QString());
    QCOMPARE(library->store().setCollection(other), QString());
    library->setFavoritesOnly(true);

    library->openCollection(QStringLiteral("mix"));
    library->setInstalledOnly(true);
    library->openCollection(QStringLiteral("other"));
    QCOMPARE(library->collectionId(), QStringLiteral("other"));
    QVERIFY(!library->installedOnly()); // starts empty again
    QCOMPARE(gameIds(*library), QStringList{"linux-supertuxkart"});

    library->closeCollection(); // back to the view from before the first
    QVERIFY(library->favoritesOnly());
    QVERIFY(library->installedOnly());
}

void LibraryCollectionsTest::overviewFilterLeaves()
{
    auto library = openLibrary();
    QCOMPARE(library->store().setCollection(mix()), QString());
    library->openCollection(QStringLiteral("mix"));
    library->showFiltered(QStringLiteral("switch"), {}, {});
    QVERIFY(library->collectionId().isEmpty());
    QCOMPARE(library->platformFilter(), QStringLiteral("switch"));
    QVERIFY(gameIds(*library).size() > 3);
}

void LibraryCollectionsTest::deletedWhileOpen()
{
    auto library = openLibrary();
    QCOMPARE(library->store().setCollection(mix()), QString());
    library->setFavoritesOnly(true);
    library->openCollection(QStringLiteral("mix"));
    QCOMPARE(library->store().removeCollection(QStringLiteral("mix")), QString());
    QTRY_VERIFY(library->collectionId().isEmpty());
    QVERIFY(library->collections().isEmpty());
    QVERIFY(library->favoritesOnly());
}

void LibraryCollectionsTest::restoredFromViewState()
{
    {
        auto library = openLibrary();
        QCOMPARE(library->store().setCollection(mix()), QString());
    }
    const QString state = m_state + QStringLiteral("/view.json");
    QDir().mkpath(m_state);
    writeFile(state, R"({"platform": "switch", "collection": "mix", "tagsOpen": true})");

    auto library = openLibrary();
    library->setViewStateFile(state);
    QCOMPARE(library->collectionId(), QStringLiteral("mix"));
    QVERIFY(library->tagsOpen());
    QVERIFY(library->platformFilter().isEmpty()); // the normal view's platform is held
    library->closeCollection();
    QCOMPARE(library->platformFilter(), QStringLiteral("switch"));

    // A collection that is gone is dropped.
    writeFile(state, R"({"collection": "gone"})");
    auto second = openLibrary();
    second->setViewStateFile(state);
    QVERIFY(second->collectionId().isEmpty());
    QVERIFY(second->tagsOpen()); // open unless the saved state says it was folded

    writeFile(state, R"({"tagsOpen": false})");
    auto folded = openLibrary();
    folded->setViewStateFile(state);
    QVERIFY(!folded->tagsOpen());
}

void LibraryCollectionsTest::tagSearchMatching()
{
    auto library = openLibrary();
    QVERIFY(library->matchesSearch(QStringLiteral("Platformer"), QStringLiteral("PLATFÖRM")));
    QVERIFY(library->matchesSearch(QStringLiteral("Co-op"), QStringLiteral("co op")));
    QVERIFY(library->matchesSearch(QStringLiteral("Rôle-playing"), QStringLiteral("role")));
    QVERIFY(library->matchesSearch(QStringLiteral("Racing"), QString()));
    QVERIFY(!library->matchesSearch(QStringLiteral("Racing"), QStringLiteral("party")));
}

void LibraryCollectionsTest::makeAddRemove()
{
    auto library = openLibrary();
    QSignalSpy offered(library.get(), &Library::undoOffered);
    const QStringList two{"steam-1145360", "switch-pikmin-4"};

    QCOMPARE(library->addCollection(QStringLiteral("  Weekend "), two), QString());
    QCOMPARE(library->collections().size(), 1);
    QCOMPARE(library->collections().first().toMap().value("name").toString(), QStringLiteral("Weekend"));
    QCOMPARE(library->collections().first().toMap().value("count").toInt(), 2);
    QCOMPARE(library->store().collections().first().id, QStringLiteral("weekend"));
    QCOMPARE(library->store().collections().first().added, two);
    QCOMPARE(offered.last().at(1).toString(), QStringLiteral("Collection Weekend made with 2 games"));

    QCOMPARE(library->setCollectionMembers({"switch-metroid-dread", "steam-1145360"}, QStringLiteral("weekend"), true),
             QString());
    QCOMPARE(library->store().collections().first().added,
             (QStringList{"steam-1145360", "switch-pikmin-4", "switch-metroid-dread"}));
    QCOMPARE(offered.last().at(1).toString(), QStringLiteral("1 game added to Weekend"));

    // Nothing to change: no change and no Undo.
    const int before = offered.size();
    QCOMPARE(library->setCollectionMembers({"steam-1145360"}, QStringLiteral("weekend"), true), QString());
    QCOMPARE(offered.size(), before);

    QCOMPARE(library->setCollectionMembers({"steam-1145360", "switch-pikmin-4"}, QStringLiteral("weekend"), false), QString());
    QCOMPARE(library->store().collections().first().added, QStringList{"switch-metroid-dread"});
    QCOMPARE(offered.last().at(1).toString(), QStringLiteral("2 games removed from Weekend"));
    QCOMPARE(library->collections().first().toMap().value("count").toInt(), 1);

    // Undo puts the two back.
    QCOMPARE(library->undo(offered.last().at(0).value<quint64>()), QString());
    QCOMPARE(library->store().collections().first().added.size(), 3);

    QVERIFY(!library->setCollectionMembers({"steam-1145360"}, QStringLiteral("gone"), true).isEmpty());
}

void LibraryCollectionsTest::statesOfASelection()
{
    auto library = openLibrary();
    QCOMPARE(library->addCollection(QStringLiteral("A"), {"steam-1145360", "switch-pikmin-4"}), QString());
    QCOMPARE(library->addCollection(QStringLiteral("B"), {"steam-1145360"}), QString());
    QCOMPARE(library->addCollection(QStringLiteral("C"), {}), QString());

    const QVariantMap states = library->collectionStates({"steam-1145360", "switch-pikmin-4"});
    QCOMPARE(states.value("a").toInt(), 1);
    QCOMPARE(states.value("b").toInt(), 2); // one of two
    QVERIFY(!states.contains("c"));
    QVERIFY(library->collectionStates({}).isEmpty());
}

void LibraryCollectionsTest::removeFromARuleBasedCollection()
{
    auto library = openLibrary();
    Collection c = named(QStringLiteral("fav"), QStringLiteral("Favorites"));
    c.rules = CollectionRules{};
    c.rules->favoritesOnly = true;
    QCOMPARE(library->store().setCollection(c), QString());
    // Hades is a favorite in the demo.
    QCOMPARE(library->collectionStates({"steam-1145360"}).value("fav").toInt(), 1);

    QCOMPARE(library->setCollectionMembers({"steam-1145360"}, QStringLiteral("fav"), false), QString());
    QCOMPARE(library->store().collections().first().removed, QStringList{"steam-1145360"});
    QVERIFY(library->store().collections().first().added.isEmpty());
    QVERIFY(library->collectionStates({"steam-1145360"}).isEmpty());

    // Adding it back drops the exception rather than listing it twice.
    QCOMPARE(library->setCollectionMembers({"steam-1145360"}, QStringLiteral("fav"), true), QString());
    QVERIFY(library->store().collections().first().removed.isEmpty());
    QVERIFY(library->store().collections().first().added.isEmpty());

    // A game the rules don't match is added by id, and removing it drops that id without an exception.
    QCOMPARE(library->setCollectionMembers({"switch-pikmin-4"}, QStringLiteral("fav"), true), QString());
    QCOMPARE(library->store().collections().first().added, QStringList{"switch-pikmin-4"});
    QCOMPARE(library->setCollectionMembers({"switch-pikmin-4"}, QStringLiteral("fav"), false), QString());
    QVERIFY(library->store().collections().first().added.isEmpty());
    QVERIFY(library->store().collections().first().removed.isEmpty());
}

void LibraryCollectionsTest::linkedCopiesTogether()
{
    auto library = openLibrary();
    const QString main = QStringLiteral("switch-mario-kart-8-deluxe");
    const QString copy = QStringLiteral("switch-metroid-dread");
    QCOMPARE(library->linkCopy(copy, main), QString());
    QCOMPARE(library->addCollection(QStringLiteral("Racing"), {main}), QString());
    QCOMPARE(library->collectionStates({copy}).value("racing").toInt(), 1);

    // Removing the copy removes the group's entry, whichever id the file holds.
    QCOMPARE(library->setCollectionMembers({copy}, QStringLiteral("racing"), false), QString());
    QVERIFY(library->store().collections().first().added.isEmpty());
    QVERIFY(library->collectionStates({main, copy}).isEmpty());
}

void LibraryCollectionsTest::namesAndIds()
{
    auto library = openLibrary();
    QVERIFY(!library->addCollection(QStringLiteral("   "), {}).isEmpty());
    QCOMPARE(library->addCollection(QStringLiteral("Co-op"), {}), QString());
    QVERIFY(!library->addCollection(QStringLiteral("co-OP"), {}).isEmpty());
    // Another name with the same id gets a number.
    QCOMPARE(library->addCollection(QStringLiteral("Co op"), {}), QString());
    QCOMPARE(library->addCollection(QStringLiteral("日本語"), {}), QString());
    QStringList ids;
    for (const Collection &c : library->store().collections())
        ids.append(c.id);
    QCOMPARE(ids, (QStringList{"co-op", "co-op-2", "collection"}));
}

void LibraryCollectionsTest::renameAndDelete()
{
    auto library = openLibrary();
    QSignalSpy offered(library.get(), &Library::undoOffered);
    QCOMPARE(library->addCollection(QStringLiteral("One"), {"steam-1145360"}), QString());
    QCOMPARE(library->addCollection(QStringLiteral("Two"), {}), QString());

    QVERIFY(!library->renameCollection(QStringLiteral("one"), QStringLiteral("two")).isEmpty());
    QVERIFY(!library->renameCollection(QStringLiteral("one"), QString()).isEmpty());
    QCOMPARE(library->renameCollection(QStringLiteral("one"), QStringLiteral("Uno")), QString());
    QCOMPARE(library->store().collections().first().name, QStringLiteral("Uno"));
    QCOMPARE(library->store().collections().first().id, QStringLiteral("one")); // the id stays
    QCOMPARE(library->store().collections().first().added, QStringList{"steam-1145360"});
    QCOMPARE(library->collections().first().toMap().value("name").toString(), QStringLiteral("Two"));

    const int gamesBefore = library->store().snapshot().content.games.size();
    QCOMPARE(library->deleteCollection(QStringLiteral("one")), QString());
    QCOMPARE(library->store().collections().size(), 1);
    QCOMPARE(library->store().snapshot().content.games.size(), gamesBefore);
    QCOMPARE(offered.last().at(1).toString(), QStringLiteral("Collection Uno deleted"));
    QCOMPARE(library->undo(offered.last().at(0).value<quint64>()), QString());
    QCOMPARE(library->store().collections().size(), 2);
    QCOMPARE(library->store().collections().first().name, QStringLiteral("Uno"));
}

void LibraryCollectionsTest::removeFromTheOpenCollection()
{
    auto library = openLibrary();
    QCOMPARE(library->store().setCollection(mix()), QString());
    library->openCollection(QStringLiteral("mix"));
    QCOMPARE(gameIds(*library).size(), 3);

    QCOMPARE(library->setCollectionMembers({"switch-pikmin-4"}, QStringLiteral("mix"), false), QString());
    QTRY_COMPARE(sorted(gameIds(*library)), (QStringList{"steam-1145360", "switch-mario-tennis-aces"}));
    QCOMPARE(library->collection().value("count").toInt(), 2);
    QCOMPARE(library->collectionId(), QStringLiteral("mix"));
}

void LibraryCollectionsTest::savedFromTheFilters()
{
    auto library = openLibrary();
    QSignalSpy offered(library.get(), &Library::undoOffered);

    library->setInstalledOnly(false);
    library->setFavoritesOnly(false);
    QVERIFY(!library->addCollectionFromFilters(QStringLiteral("Everything")).isEmpty()); // nothing to save
    QVERIFY(library->store().collections().isEmpty());

    library->setTagFilter({"Co-op", "RPG"});
    library->setTagMatchAll(true);
    library->setStatusFilter({"none"});
    library->setPlatformFilter(QStringLiteral("switch"));
    library->setFavoritesOnly(true);
    library->setInstalledOnly(true);
    QVERIFY(!library->addCollectionFromFilters(QStringLiteral("  ")).isEmpty());
    QCOMPARE(library->addCollectionFromFilters(QStringLiteral("My filters")), QString());
    QCOMPARE(offered.last().at(1).toString(), QStringLiteral("Collection My filters saved from the filters"));

    const Collection saved = library->store().collections().first();
    QCOMPARE(saved.id, QStringLiteral("my-filters"));
    QVERIFY(saved.added.isEmpty());
    QVERIFY(saved.removed.isEmpty());
    QVERIFY(saved.rules.has_value());
    CollectionRules expected;
    expected.platform = QStringLiteral("switch");
    expected.installedOnly = true;
    expected.favoritesOnly = true;
    expected.statuses = {"none"};
    expected.tags = {"Co-op", "RPG"};
    expected.matchAllTags = true;
    QCOMPARE(*saved.rules, expected);
    QVERIFY(!library->addCollectionFromFilters(QStringLiteral("my FILTERS")).isEmpty()); // name taken

    // The filters in the panel stay as they were; Undo takes the collection out.
    QCOMPARE(library->tagFilter(), (QStringList{"Co-op", "RPG"}));
    QCOMPARE(library->undo(offered.last().at(0).value<quint64>()), QString());
    QVERIFY(library->store().collections().isEmpty());

    // A tag selected but not matching all (one tag) doesn't keep "match all".
    library->setTagFilter({"Co-op"});
    library->setPlatformFilter({});
    library->setFavoritesOnly(false);
    library->setStatusFilter({});
    QCOMPARE(library->addCollectionFromFilters(QStringLiteral("Co-op games")), QString());
    QVERIFY(!library->store().collections().first().rules->matchAllTags);
}

void LibraryCollectionsTest::savedFiltersAreAFixedScope()
{
    auto library = openLibrary();
    library->setInstalledOnly(false);
    library->setFavoritesOnly(false);
    library->setTagFilter({"Co-op"});
    const QStringList coOp = gameIds(*library);
    QVERIFY(coOp.size() >= 2);
    QCOMPARE(library->addCollectionFromFilters(QStringLiteral("Co-op games")), QString());
    library->clearFilters();
    library->setInstalledOnly(false);
    const QStringList everything = gameIds(*library);
    QVERIFY(everything.size() > coOp.size());

    library->openCollection(QStringLiteral("co-op-games"));
    QCOMPARE(sorted(gameIds(*library)), sorted(coOp));
    // Its rules never show up in the filters: the tag filter and the platform are empty.
    QVERIFY(library->tagFilter().isEmpty());
    QVERIFY(library->platformFilter().isEmpty());
    QVERIFY(!library->installedOnly());
    QVERIFY(!library->filtersActive());
    QCOMPARE(library->collection().value("count").toInt(), coOp.size());

    // The panel's filters narrow within it and Clear returns to the whole collection.
    library->setFavoritesOnly(true);
    QVERIFY(gameIds(*library).size() < coOp.size());
    library->clearFilters();
    QCOMPARE(sorted(gameIds(*library)), sorted(coOp));

    library->closeCollection();
    QCOMPARE(sorted(gameIds(*library)), sorted(everything));
    QVERIFY(library->tagFilter().isEmpty());

    // The rules follow the games: a new Co-op game joins the collection.
    QCOMPARE(library->retagGames({"switch-pikmin-4"}, {"Co-op"}, {}), QString());
    QTRY_COMPARE(library->collections().first().toMap().value("count").toInt(), coOp.size() + 1);
}

void LibraryCollectionsTest::rulesIgnoreADeletedTag()
{
    auto library = openLibrary();
    Collection c = named(QStringLiteral("tagged"), QStringLiteral("Tagged"));
    c.rules = CollectionRules{};
    c.rules->tags = {"Co-op"};
    QCOMPARE(library->store().setCollection(c), QString());
    const int coOp = library->collections().first().toMap().value("count").toInt();
    const int all = int(gameIds(*library).size());
    QVERIFY(coOp > 0 && coOp < all + 10);

    QSignalSpy offered(library.get(), &Library::undoOffered);
    QCOMPARE(library->removeTag(QStringLiteral("Co-op")), QString());
    // Its only tag is gone, so the rules name no tag: everything not hidden matches, as an empty tag filter does.
    QTRY_VERIFY(library->collections().first().toMap().value("count").toInt() > coOp);
    // The rule keeps the name, so Undo brings the tag and the rule back together.
    QVERIFY(library->store().collections().first().rules->tags.contains(QStringLiteral("Co-op")));
    QCOMPARE(library->undo(offered.last().at(0).value<quint64>()), QString());
    QTRY_COMPARE(library->collections().first().toMap().value("count").toInt(), coOp);
}

void LibraryCollectionsTest::renamingATagUpdatesTheRules()
{
    auto library = openLibrary();
    Collection c = named(QStringLiteral("tagged"), QStringLiteral("Tagged"));
    c.rules = CollectionRules{};
    c.rules->tags = {"Co-op", "RPG"};
    c.rules->favoritesOnly = false;
    QCOMPARE(library->store().setCollection(c), QString());
    const int before = library->collections().first().toMap().value("count").toInt();

    QCOMPARE(library->renameTag(QStringLiteral("Co-op"), QStringLiteral("Together")), QString());
    QCOMPARE(library->store().collections().first().rules->tags, (QStringList{"Together", "RPG"}));
    QTRY_COMPARE(library->collections().first().toMap().value("count").toInt(), before);
}

void LibraryCollectionsTest::editRulesAndExceptions()
{
    auto library = openLibrary();
    QSignalSpy offered(library.get(), &Library::undoOffered);
    Collection c = named(QStringLiteral("fav"), QStringLiteral("Favorites"));
    c.rules = CollectionRules{};
    c.rules->favoritesOnly = true;
    c.added = {"switch-pikmin-4", "left-the-library"};
    c.removed = {"steam-1145360"};
    QCOMPARE(library->store().setCollection(c), QString());

    const QVariantMap details = library->collectionDetails(QStringLiteral("fav"));
    QVERIFY(details.value("enabled").toBool());
    QVERIFY(details.value("favoritesOnly").toBool());
    QVERIFY(!details.value("installedOnly").toBool());
    const QVariantList added = details.value("added").toList();
    QCOMPARE(added.size(), 2);
    QCOMPARE(added.at(0).toMap().value("id").toString(), QStringLiteral("switch-pikmin-4"));
    QCOMPARE(added.at(0).toMap().value("title").toString(), QStringLiteral("Pikmin 4"));
    QCOMPARE(added.at(1).toMap().value("title").toString(), QStringLiteral("left-the-library")); // not in the library
    QCOMPARE(details.value("removed").toList().at(0).toMap().value("title").toString(), QStringLiteral("Hades"));
    QVERIFY(library->collectionDetails(QStringLiteral("nope")).isEmpty());

    // Other rules, one added and the removed exception dropped, in one Undo step.
    QVariantMap rules = details;
    rules.insert("favoritesOnly", false);
    rules.insert("platform", QStringLiteral("switch"));
    rules.insert("statuses", QStringList{"playing"});
    rules.insert("tags", QStringList{"Co-op", "Gone"});
    rules.insert("matchAllTags", true);
    rules.insert("steam", QStringList{"owned"});
    rules.insert("installedOnly", true);
    const int steps = int(offered.size());
    QCOMPARE(library->editCollection(QStringLiteral("fav"), rules, {"left-the-library"}, {"steam-1145360"}), QString());
    QCOMPARE(int(offered.size()), steps + 1);
    QCOMPARE(offered.last().at(1).toString(), QStringLiteral("Collection Favorites changed"));

    const Collection edited = library->store().collections().first();
    QCOMPARE(edited.added, QStringList{"switch-pikmin-4"});
    QVERIFY(edited.removed.isEmpty());
    QCOMPARE(edited.rules->platform, QStringLiteral("switch"));
    QVERIFY(edited.rules->installedOnly);
    QVERIFY(!edited.rules->favoritesOnly);
    QCOMPARE(edited.rules->statuses, QStringList{"playing"});
    QCOMPARE(edited.rules->tags, (QStringList{"Co-op", "Gone"})); // the dialog sends what it shows
    QVERIFY(edited.rules->matchAllTags);
    QCOMPARE(edited.rules->steamOwnership, QStringList{"owned"});

    // Nothing changed, nothing written and no step.
    QCOMPARE(library->editCollection(QStringLiteral("fav"), rules, {}, {}), QString());
    QCOMPARE(int(offered.size()), steps + 1);
    QVERIFY(!library->editCollection(QStringLiteral("nope"), rules, {}, {}).isEmpty());

    // The details show only the tags that exist, so saving them as they are drops a deleted tag's name.
    QCOMPARE(library->collectionDetails(QStringLiteral("fav")).value("tags").toStringList(), QStringList{"Co-op"});

    QCOMPARE(library->undo(offered.last().at(0).value<quint64>()), QString());
    QCOMPARE(library->store().collections().first(), c);
}

void LibraryCollectionsTest::countsMatchesOfUnsavedRules()
{
    auto library = openLibrary();
    Collection c = named(QStringLiteral("fav"), QStringLiteral("Favorites"));
    c.rules = CollectionRules{};
    c.rules->favoritesOnly = true;
    c.added = {"switch-pikmin-4"};
    c.removed = {"steam-1145360"};
    QCOMPARE(library->store().setCollection(c), QString());
    const QString id = QStringLiteral("fav");
    const int saved = library->collections().first().toMap().value("count").toInt();

    // Unchanged rules count what the sidebar shows.
    QVariantMap rules = library->collectionDetails(id);
    QCOMPARE(library->countCollectionMatches(id, rules, {}, {}), saved);

    // Rules off: only what was added by hand is left.
    rules.insert("enabled", false);
    QCOMPARE(library->countCollectionMatches(id, rules, {}, {}), 1);
    QCOMPARE(library->countCollectionMatches(id, rules, {"switch-pikmin-4"}, {}), 0);

    // A dropped left-out exception brings the game back when the rules match it.
    rules.insert("enabled", true);
    rules.insert("favoritesOnly", false);
    const int everything = library->countCollectionMatches(id, rules, {}, {});
    QCOMPARE(library->countCollectionMatches(id, rules, {}, {"steam-1145360"}), everything + 1);

    // Narrower unsaved rules count fewer, without writing anything.
    rules.insert("platform", QStringLiteral("switch"));
    const int onSwitch = library->countCollectionMatches(id, rules, {}, {});
    QVERIFY(onSwitch > 0 && onSwitch < everything);
    QCOMPARE(library->store().collections().first(), c);

    QCOMPARE(library->countCollectionMatches(QStringLiteral("nope"), rules, {}, {}), 0);
}

void LibraryCollectionsTest::countIgnoresUnknownTags()
{
    auto library = openLibrary();
    Collection c = named(QStringLiteral("t"), QStringLiteral("T"));
    c.rules = CollectionRules{};
    QCOMPARE(library->store().setCollection(c), QString());
    QVariantMap rules = library->collectionDetails(QStringLiteral("t"));
    const int everything = library->countCollectionMatches(QStringLiteral("t"), rules, {}, {});
    QVERIFY(everything > 0);

    rules.insert("tags", QStringList{"Co-op"});
    const int coOp = library->countCollectionMatches(QStringLiteral("t"), rules, {}, {});
    QVERIFY(coOp > 0 && coOp < everything);

    // A tag that doesn't exist is ignored, as the tag filter ignores it.
    rules.insert("tags", QStringList{"No such tag"});
    QCOMPARE(library->countCollectionMatches(QStringLiteral("t"), rules, {}, {}), everything);
}

void LibraryCollectionsTest::savedFromTheMoreFilters()
{
    auto library = openLibrary();
    library->setInstalledOnly(false);
    library->setYearFrom(2015);
    library->setYearTo(2023);
    library->setMinRating(3);
    library->setPlayTimeFilter({"never", "2_10h"});
    library->setLastPlayedFilter(QStringLiteral("year"));
    library->setAchievementFilter({"none"});
    QCOMPARE(library->addCollectionFromFilters(QStringLiteral("Recent")), QString());

    const Collection saved = library->store().collections().first();
    QCOMPARE(saved.rules->yearFrom, 2015);
    QCOMPARE(saved.rules->yearTo, 2023);
    QCOMPARE(saved.rules->minRating, 3.0);
    QCOMPARE(saved.rules->playTimes, (QStringList{"never", "2_10h"}));
    QCOMPARE(saved.rules->lastPlayed, QStringLiteral("year"));
    QCOMPARE(saved.rules->achievements, QStringList{"none"});

    // Each filter alone is enough to save one.
    library->clearFilters();
    library->setInstalledOnly(false);
    library->setYearTo(2000);
    QCOMPARE(library->addCollectionFromFilters(QStringLiteral("Old")), QString());
    library->clearFilters();
    library->setInstalledOnly(false);

    // The same games open from a collection as the filters showed, and the file loads again with the same rules.
    library->setYearFrom(2017);
    library->setYearTo(2020);
    const QStringList expectedIds = gameIds(*library);
    QVERIFY(!expectedIds.isEmpty());
    QCOMPARE(library->addCollectionFromFilters(QStringLiteral("Years")), QString());
    library->clearFilters();
    library->setInstalledOnly(false);
    library->openCollection(QStringLiteral("years"));
    QCOMPARE(sorted(gameIds(*library)), sorted(expectedIds));
    library->closeCollection();
    library->openCollection(QStringLiteral("old"));
    QVERIFY(gameIds(*library).isEmpty()); // no game before 2001 in the demo; year 0 is left out
    library->closeCollection();
    auto again = openLibrary();
    QCOMPARE(again->store().collections().first(), saved);

    // Edit shows them and takes changes back.
    const QVariantMap details = library->collectionDetails(QStringLiteral("recent"));
    QCOMPARE(details.value("yearFrom").toInt(), 2015);
    QCOMPARE(details.value("yearTo").toInt(), 2023);
    QCOMPARE(details.value("minRating").toDouble(), 3.0);
    QCOMPARE(details.value("playTimes").toStringList(), (QStringList{"never", "2_10h"}));
    QCOMPARE(details.value("lastPlayed").toString(), QStringLiteral("year"));
    QCOMPARE(details.value("achievements").toStringList(), QStringList{"none"});
    QVariantMap rules = details;
    rules.insert("yearFrom", 0);
    rules.insert("yearTo", 0);
    rules.insert("minRating", -1);
    rules.insert("playTimes", QStringList{});
    rules.insert("lastPlayed", QString());
    rules.insert("achievements", QStringList{});
    rules.insert("favoritesOnly", true);
    QCOMPARE(library->editCollection(QStringLiteral("recent"), rules, {}, {}), QString());
    const CollectionRules edited = *library->store().collections().first().rules;
    QCOMPARE(edited.yearFrom, 0);
    QCOMPARE(edited.minRating, -1.0);
    QVERIFY(edited.playTimes.isEmpty() && edited.lastPlayed.isEmpty() && edited.achievements.isEmpty());
    QVERIFY(edited.favoritesOnly);
}

void LibraryCollectionsTest::editTurnsRulesOnAndOff()
{
    auto library = openLibrary();
    QCOMPARE(library->store().setCollection(mix()), QString());
    QVERIFY(!library->collectionDetails(QStringLiteral("mix")).value("enabled").toBool());

    QVariantMap rules{{"enabled", true}, {"favoritesOnly", true}};
    QCOMPARE(library->editCollection(QStringLiteral("mix"), rules, {}, {}), QString());
    QVERIFY(library->store().collections().first().rules.has_value());
    // Hades matches the rule and is listed too; Pikmin and Tennis Aces are the added ones.
    QCOMPARE(library->collections().first().toMap().value("count").toInt(), 3);

    rules.insert("enabled", false);
    QCOMPARE(library->editCollection(QStringLiteral("mix"), rules, {"steam-1145360"}, {}), QString());
    QVERIFY(!library->store().collections().first().rules.has_value());
    QCOMPARE(library->store().collections().first().added, (QStringList{"switch-mario-tennis-aces", "switch-pikmin-4"}));
    QCOMPARE(library->collections().first().toMap().value("count").toInt(), 2);
}

namespace {
QString steamFile(const QString &data, const QString &account)
{
    return steamCollectionsPath(data + QStringLiteral("/steam"), account);
}

QVariantMap rowNamed(const QVariantList &rows, const QString &name)
{
    for (const QVariant &row : rows) {
        if (row.toMap().value("name").toString() == name)
            return row.toMap();
    }
    return {};
}
}

void LibraryCollectionsTest::steamAccounts()
{
    // One account with a file: the demo's.
    {
        auto library = openLibrary();
        const QVariantList accounts = library->steamCollectionAccounts();
        QCOMPARE(accounts.size(), 1);
        QCOMPARE(accounts.first().toMap().value("id").toString(), QStringLiteral("39734273"));
        QCOMPARE(accounts.first().toMap().value("name").toString(), QStringLiteral("Player One"));
    }

    // Several: the current one first, and one with no file isn't offered.
    const QString second = steamFile(m_data, QStringLiteral("39734274"));
    QDir().mkpath(QFileInfo(second).absolutePath());
    writeFile(second, "[]");
    writeFile(m_data + QStringLiteral("/steam/config/loginusers.vdf"),
              "\"users\"\n{\n\t\"76561198000000001\"\n\t{\n\t\t\"PersonaName\"\t\"Player One\"\n\t\t\"MostRecent\"\t\"1\"\n\t\t\"Timestamp\"\t\"2\"\n\t}\n"
              "\t\"76561198000000002\"\n\t{\n\t\t\"PersonaName\"\t\"Player Two\"\n\t\t\"MostRecent\"\t\"0\"\n\t\t\"Timestamp\"\t\"1\"\n\t}\n"
              "\t\"76561198000000003\"\n\t{\n\t\t\"PersonaName\"\t\"Player Three\"\n\t\t\"MostRecent\"\t\"0\"\n\t\t\"Timestamp\"\t\"1\"\n\t}\n}\n");
    auto library = openLibrary();
    const QVariantList accounts = library->steamCollectionAccounts();
    QCOMPARE(accounts.size(), 2);
    QCOMPARE(accounts.at(0).toMap().value("id").toString(), QStringLiteral("39734273"));
    QCOMPARE(accounts.at(1).toMap().value("name").toString(), QStringLiteral("Player Two"));

    // A file with nothing readable is an error to show, not a crash; an account that isn't a number is refused.
    writeFile(second, "garbage");
    QVERIFY(!library->readSteamCollections(QStringLiteral("39734274")).value("error").toString().isEmpty());
    QVERIFY(!library->readSteamCollections(QStringLiteral("39734275")).value("error").toString().isEmpty());
    QVERIFY(!library->readSteamCollections(QStringLiteral("../x")).value("error").toString().isEmpty());
    QVERIFY(!library->importSteamCollections(QStringLiteral("../x"), {"uc-AbCdEf1"}).value("error").toString().isEmpty());
}

void LibraryCollectionsTest::steamImportMakesCollections()
{
    auto library = openLibrary();
    QSignalSpy offered(library.get(), &Library::undoOffered);
    const QString account = QStringLiteral("39734273");
    const QByteArray steamBefore = readFile(steamFile(m_data, account));

    // What the dialog lists: hand-picked and from-tag ones; no dynamic, favorite, hidden or deleted one.
    const QVariantMap read = library->readSteamCollections(account);
    QCOMPARE(read.value("error").toString(), QString());
    const QVariantList rows = read.value("collections").toList();
    QCOMPARE(rows.size(), 3);
    QCOMPARE(read.value("dynamic").toInt(), 1);
    QCOMPARE(read.value("unreadable").toInt(), 0);
    QCOMPARE(rowNamed(rows, "Weekend Games").value("games").toInt(), 3);
    QCOMPARE(rowNamed(rows, "Weekend Games").value("missing").toInt(), 1);
    QVERIFY(!rowNamed(rows, "Weekend Games").value("imported").toBool());
    QCOMPARE(rowNamed(rows, "Roguelike").value("games").toInt(), 1);
    QVERIFY(library->store().collections().isEmpty()); // reading changes nothing

    const QVariantMap done = library->importSteamCollections(account, {"uc-AbCdEf1", "from-tag-Roguelike", "uc-nothing"});
    QCOMPARE(done.value("error").toString(), QString());
    QCOMPARE(done.value("added").toList().size(), 2);
    QVERIFY(done.value("updated").toList().isEmpty());
    QCOMPARE(done.value("dynamic").toInt(), 1);
    QCOMPARE(done.value("missingApps").toStringList(), QStringList{"999001"});
    QCOMPARE(library->store().collections().size(), 2);

    const Collection weekend = library->store().collections().at(0);
    QCOMPARE(weekend.name, QStringLiteral("Weekend Games"));
    QCOMPARE(weekend.id, QStringLiteral("weekend-games"));
    QCOMPARE(weekend.steamId, QStringLiteral("uc-AbCdEf1"));
    QVERIFY(!weekend.rules.has_value());
    // The games not in the library stay in the list, so one that comes back returns to it.
    QCOMPARE(weekend.added, (QStringList{"steam-1145360", "steam-367520", "steam-999001"}));
    QCOMPARE(library->store().collections().at(1).steamId, QStringLiteral("from-tag-Roguelike"));
    QCOMPARE(library->collections().size(), 2);
    QCOMPARE(rowNamed(library->collections(), "Weekend Games").value("count").toInt(), 2);

    // One Undo step for the whole import, and the Steam file as it was.
    QCOMPARE(offered.last().at(1).toString(), QStringLiteral("2 collections imported from Steam"));
    QCOMPARE(library->undo(offered.last().at(0).value<quint64>()), QString());
    QVERIFY(library->store().collections().isEmpty());
    QVERIFY(!QFileInfo::exists(m_data + QStringLiteral("/library/collections.toml")));
    QCOMPARE(readFile(steamFile(m_data, account)), steamBefore);

    // Steam's favorites and hidden games don't reach Lattis: its Favorites are its own.
    QVERIFY(!library->gameAt(library->rowOfGame(QStringLiteral("steam-730"))).value("favorite").toBool());
}

void LibraryCollectionsTest::steamImportAgainReplacesGames()
{
    auto library = openLibrary();
    QSignalSpy offered(library.get(), &Library::undoOffered);
    const QString account = QStringLiteral("39734273");
    QCOMPARE(library->importSteamCollections(account, {"uc-AbCdEf1", "uc-GhIjKl2"}).value("error").toString(), QString());

    // The user renames one and adds a game to it by hand.
    QCOMPARE(library->renameCollection(QStringLiteral("weekend-games"), QStringLiteral("Saturdays")), QString());
    QCOMPARE(library->setCollectionMembers({"switch-pikmin-4"}, QStringLiteral("weekend-games"), true), QString());
    QVERIFY(rowNamed(library->readSteamCollections(account).value("collections").toList(), "Weekend Games")
                .value("imported").toBool());

    // Steam's list changes: Hades leaves Weekend Games, Hollow Knight stays.
    QByteArray text = readFile(steamFile(m_data, account));
    text.replace("[1145360,367520,999001]", "[367520,999001]");
    writeFile(steamFile(m_data, account), text);
    const int steps = offered.size();
    const QVariantMap again = library->importSteamCollections(account, {"uc-AbCdEf1", "uc-GhIjKl2", "from-tag-Roguelike"});
    QCOMPARE(again.value("error").toString(), QString());
    QCOMPARE(again.value("added").toList().size(), 1);
    QCOMPARE(again.value("updated").toList().size(), 1);
    QCOMPARE(again.value("unchanged").toInt(), 1);
    QCOMPARE(offered.size(), steps + 1);

    const QList<Collection> all = library->store().collections();
    QCOMPARE(all.size(), 3);
    const auto byId = [&](const QString &id) {
        return *std::find_if(all.cbegin(), all.cend(), [&](const Collection &c) { return c.id == id; });
    };
    // Replaced by Steam's list (the game added in Lattis is gone), the rename kept, no second copy made.
    QCOMPARE(byId("weekend-games").name, QStringLiteral("Saturdays"));
    QCOMPARE(byId("weekend-games").added, (QStringList{"steam-367520", "steam-999001"}));

    // The same again changes nothing, and offers no Undo.
    const int quiet = offered.size();
    const QVariantMap same = library->importSteamCollections(account, {"uc-AbCdEf1", "uc-GhIjKl2", "from-tag-Roguelike"});
    QCOMPARE(same.value("unchanged").toInt(), 3);
    QVERIFY(same.value("added").toList().isEmpty() && same.value("updated").toList().isEmpty());
    QCOMPARE(offered.size(), quiet);

    // Rules the user gave an imported collection stay.
    QVariantMap rules{{"enabled", true}, {"favoritesOnly", true}};
    QCOMPARE(library->editCollection(QStringLiteral("couch-co-op"), rules, {}, {}), QString());
    text.replace("[413150,1091500]", "[413150]");
    writeFile(steamFile(m_data, account), text);
    QCOMPARE(library->importSteamCollections(account, {"uc-GhIjKl2"}).value("updated").toList().size(), 1);
    const QList<Collection> now = library->store().collections();
    const auto couch = std::find_if(now.cbegin(), now.cend(), [](const Collection &c) { return c.id == QLatin1String("couch-co-op"); });
    QVERIFY(couch != now.cend());
    QVERIFY(couch->rules.has_value());
    QCOMPARE(couch->added, QStringList{"steam-413150"});
}

void LibraryCollectionsTest::steamImportNamesAndFailures()
{
    auto library = openLibrary();
    const QString account = QStringLiteral("39734273");

    // A name the user already has is never overwritten: Steam's collection arrives under another name.
    QCOMPARE(library->addCollection(QStringLiteral("weekend games"), {"switch-pikmin-4"}), QString());
    const QVariantMap done = library->importSteamCollections(account, {"uc-AbCdEf1"});
    QCOMPARE(done.value("added").toList().first().toMap().value("name").toString(), QStringLiteral("Weekend Games (Steam)"));
    QCOMPARE(library->store().collections().size(), 2);
    QCOMPARE(library->store().collections().at(0).added, QStringList{"switch-pikmin-4"});
    QCOMPARE(library->store().collections().at(1).id, QStringLiteral("weekend-games-steam"));

    // Nothing ticked, nothing changed, no error.
    const QVariantMap none = library->importSteamCollections(account, {});
    QCOMPARE(none.value("error").toString(), QString());
    QVERIFY(none.value("added").toList().isEmpty());

    // The file gone or unreadable: an error, and the collections as they are.
    writeFile(steamFile(m_data, account), "[");
    QVERIFY(!library->importSteamCollections(account, {"uc-AbCdEf1"}).value("error").toString().isEmpty());
    QFile::remove(steamFile(m_data, account));
    QVERIFY(!library->importSteamCollections(account, {"uc-AbCdEf1"}).value("error").toString().isEmpty());
    QCOMPARE(library->store().collections().size(), 2);

    // A broken entry is skipped and counted; the others come in.
    writeFile(steamFile(m_data, account),
              "[[\"user-collections.uc-Good\",{\"value\":\"{\\\"name\\\":\\\"Good\\\",\\\"added\\\":[1145360]}\"}],"
              "[\"user-collections.uc-Bad\",{\"value\":\"{oops\"}]]");
    const QVariantMap mixed = library->importSteamCollections(account, {"uc-Good", "uc-Bad"});
    QCOMPARE(mixed.value("added").toList().size(), 1);
    QCOMPARE(mixed.value("unreadable").toInt(), 1);
}

QTEST_GUILESS_MAIN(LibraryCollectionsTest)

#include "library_collections_test.moc"
