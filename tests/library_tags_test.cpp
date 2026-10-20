#include "librarytestbase.h"

// The library tests on tags: tags.toml, the defaults, IGDB's tags.
class LibraryTagsTest : public LibraryTestBase
{
    Q_OBJECT

private slots:
    void tags();
    void retagGames();
    void defaultTags();
    void tagsIgdbMapping();
    void igdbTags();
};

void LibraryTagsTest::tags()
{
    auto library = openLibrary();
    LibraryStore &store = library->store();
    const QString tagsPath = m_data + QStringLiteral("/library/tags.toml");
    const auto tagsOf = [&](const QString &id) { return store.snapshot().content.games.value(id).tags; };
    const auto colorOf = [&](const QString &name) {
        for (const TagDef &t : store.snapshot().content.tags) {
            if (t.name == name)
                return t.color;
        }
        return -1;
    };
    QCOMPARE(store.tagUse().value(QStringLiteral("Adventure")), 7);
    const int parsed = store.filesParsed();

    // Add: a new block at the end; names that exist are refused.
    const QByteArray before = readFile(tagsPath);
    QCOMPARE(store.addTag(QStringLiteral("Horror"), 3), QString());
    QCOMPARE(readFile(tagsPath), before + "\n[[tag]]\nname = \"Horror\"\ncolor = 3\n");
    QCOMPARE(colorOf(QStringLiteral("Horror")), 3);
    QVERIFY(!store.addTag(QStringLiteral("Adventure"), 1).isEmpty());
    QVERIFY(!store.addTag(QStringLiteral("Horror"), 1).isEmpty());
    QVERIFY(!store.addTag(QStringLiteral(" "), 1).isEmpty());

    // Recolor.
    QCOMPARE(store.setTagColor(QStringLiteral("Racing"), 6), QString());
    QVERIFY(readFile(tagsPath).contains("name = \"Racing\"\ncolor = 6\n"));
    QCOMPARE(colorOf(QStringLiteral("Racing")), 6);

    // Rename: the block and every game that has it, and nothing else in those files.
    const QByteArray dread = readFile(gamePath("switch-metroid-dread"));
    QCOMPARE(store.renameTag(QStringLiteral("Metroidvania"), QStringLiteral("Search Action")), QString());
    QVERIFY(readFile(tagsPath).contains("name = \"Search Action\"\ncolor = 7\n"));
    QVERIFY(!readFile(tagsPath).contains("Metroidvania"));
    QCOMPARE(readFile(gamePath("switch-metroid-dread")),
             QByteArray(dread).replace("[\"Metroidvania\", \"Adventure\"]", "[\"Search Action\", \"Adventure\"]"));
    QCOMPARE(tagsOf(QStringLiteral("steam-367520")), QStringList{"Search Action"});
    QVERIFY(!store.renameTag(QStringLiteral("Search Action"), QStringLiteral("RPG")).isEmpty());
    QVERIFY(!store.renameTag(QStringLiteral("Search Action"), QStringLiteral("Horror")).isEmpty()); // listed, unused

    // A tag only games use: renamed in the games; recoloring lists it.
    QCOMPARE(store.setGameValues(QStringLiteral("steam-1091500"),
                                 {{QStringLiteral("tags"), QStringList{"RPG", "Stealth"}}}),
             QString());
    const QByteArray listed = readFile(tagsPath);
    QCOMPARE(store.renameTag(QStringLiteral("Stealth"), QStringLiteral("Sneaky")), QString());
    QCOMPARE(readFile(tagsPath), listed);
    QCOMPARE(tagsOf(QStringLiteral("steam-1091500")), (QStringList{"RPG", "Sneaky"}));
    QCOMPARE(store.setTagColor(QStringLiteral("Sneaky"), 2), QString());
    QCOMPARE(readFile(tagsPath), listed + "\n[[tag]]\nname = \"Sneaky\"\ncolor = 2\n");

    // Remove: the block, and the tag from every game; a list left empty goes.
    QCOMPARE(store.removeTag(QStringLiteral("Party")), QString());
    QVERIFY(!readFile(tagsPath).contains("Party"));
    QCOMPARE(tagsOf(QStringLiteral("switch-mario-kart-8-deluxe")), QStringList{"Racing"});
    QVERIFY(!readFile(gamePath("switch-mario-tennis-aces")).contains("tags"));
    QVERIFY(!store.tagUse().contains(QStringLiteral("Party")));
    const QVariantMap kart = library->gameAt(library->rowOfGame(QStringLiteral("switch-mario-kart-8-deluxe")));
    QCOMPARE(kart.value("tags").toList().size(), 1);

    // All written by the app: nothing to parse again.
    QTest::qWait(700);
    store.waitForIdle();
    QCoreApplication::processEvents();
    QCOMPARE(store.filesParsed(), parsed);
}

void LibraryTagsTest::retagGames()
{
    auto library = openLibrary();
    LibraryStore &store = library->store();
    const auto tagsOf = [&](const QString &id) { return store.snapshot().content.games.value(id).tags; };
    const QStringList ids{"switch-pikmin-4", "switch-metroid-dread", "switch-kirby-and-the-forgotten-land"};
    QSignalSpy changed(&store, &LibraryStore::changed);
    QSignalSpy offered(library.get(), &Library::undoOffered);

    // Adventure is on two of the three (mixed): adding puts it on all, once.
    QCOMPARE(library->retagGames(ids, {"Adventure"}, {}), QString());
    QCOMPARE(changed.size(), 1);
    QCOMPARE(tagsOf("switch-pikmin-4"), QStringList{"Adventure"});
    QCOMPARE(tagsOf("switch-metroid-dread"), (QStringList{"Metroidvania", "Adventure"}));
    QCOMPARE(tagsOf("switch-kirby-and-the-forgotten-land"), (QStringList{"Platformer", "Adventure"}));
    QCOMPARE(offered.last().at(1).toString(), QStringLiteral("Tag Adventure added to 3 games"));

    // Removing it takes it off all of them, and the Undo puts the first state back.
    QCOMPARE(library->retagGames(ids, {}, {"Adventure"}), QString());
    QVERIFY(!tagsOf("switch-pikmin-4").contains("Adventure"));
    QVERIFY(!tagsOf("switch-metroid-dread").contains("Adventure"));
    QCOMPARE(library->undo(offered.last().at(0).value<quint64>()), QString());
    QCOMPARE(tagsOf("switch-kirby-and-the-forgotten-land"), (QStringList{"Platformer", "Adventure"}));
    QCOMPARE(tagsOf("switch-pikmin-4"), QStringList{"Adventure"});
}

void LibraryTagsTest::defaultTags()
{
    // An existing library doesn't get them by itself.
    const QString tagsPath = m_data + QStringLiteral("/library/tags.toml");
    const QByteArray before = readFile(tagsPath);
    auto library = openLibrary();
    LibraryStore &store = library->store();
    QCOMPARE(readFile(tagsPath), before);

    // Missing: those it has no tag for, listed or only used by a game, ignoring case.
    QCOMPARE(store.missingDefaultTags().size(), 18);
    QCOMPARE(store.setGameValues(QStringLiteral("steam-1091500"), {{QStringLiteral("tags"), QStringList{"horror"}}}),
             QString());
    const QList<TagDef> missing = store.missingDefaultTags();
    QCOMPARE(missing.size(), 17);
    QCOMPARE(missing.first().name, QStringLiteral("Soulslike"));
    QCOMPARE(missing.first().color, 23);
    QTRY_COMPARE(library->missingDefaultTags().size(), 17);

    // Added in one write, after the library's own, with their own colors.
    QCOMPARE(library->addDefaultTags(), QString());
    const QByteArray after = readFile(tagsPath);
    QVERIFY(after.startsWith(before));
    QVERIFY(after.contains("\n[[tag]]\nname = \"Soulslike\"\ncolor = 23\nigdb = [\"keyword:soulslike\"]\n"));
    QVERIFY(after.contains("\n[[tag]]\nname = \"Split screen\"\ncolor = 22\n"));
    QVERIFY(!after.contains("name = \"Horror\""));
    QCOMPARE(after.count("[[tag]]"), before.count("[[tag]]") + 17);
    QVERIFY(store.missingDefaultTags().isEmpty());
    QTRY_VERIFY(library->missingDefaultTags().isEmpty());

    // Nothing left to add: no write.
    QCOMPARE(library->addDefaultTags(), QString());
    QCOMPARE(readFile(tagsPath), after);
}

void LibraryTagsTest::tagsIgdbMapping()
{
    // A default's name, in any case, without its own list: the default's.
    QCOMPARE(LibraryStore::igdbOf(TagDef{QStringLiteral("platformer"), 0}), QStringList{"genre:8"});
    QCOMPARE(LibraryStore::igdbOf(TagDef{QStringLiteral("Shoot 'em up"), 0}),
             (QStringList{"keyword:shoot-em-up", "keyword:shmup"}));
    // Its own list wins, [] included; another name has none.
    QCOMPARE(LibraryStore::igdbOf(TagDef{QStringLiteral("Platformer"), 0, {"theme:1"}, true}), QStringList{"theme:1"});
    QVERIFY(LibraryStore::igdbOf(TagDef{QStringLiteral("Platformer"), 0, {}, true}).isEmpty());
    QVERIFY(LibraryStore::igdbOf(TagDef{QStringLiteral("Party"), 0}).isEmpty());

    // The demo's Metroidvania has no list: renamed, it keeps the default's in its block.
    auto library = openLibrary();
    LibraryStore &store = library->store();
    const QString tagsPath = m_data + QStringLiteral("/library/tags.toml");
    QCOMPARE(store.renameTag(QStringLiteral("Metroidvania"), QStringLiteral("Search Action")), QString());
    QVERIFY(readFile(tagsPath).contains("name = \"Search Action\"\ncolor = 7\nigdb = [\"keyword:metroidvania\"]\n"));
    const TagDef renamed = store.snapshot().content.tags.last();
    QCOMPARE(LibraryStore::igdbOf(renamed), QStringList{"keyword:metroidvania"});
    // A tag without one keeps having none.
    QCOMPARE(store.renameTag(QStringLiteral("Party"), QStringLiteral("Fiesta")), QString());
    QVERIFY(readFile(tagsPath).contains("name = \"Fiesta\"\ncolor = 2\n\n"));
}

void LibraryTagsTest::igdbTags()
{
    // The demo's tags have no igdb lists; this one has its own, and "Platformer" is a default.
    const QString tagsPath = m_data + QStringLiteral("/library/tags.toml");
    writeFile(tagsPath, readFile(tagsPath) + "\n[[tag]]\nname = \"Horror\"\ncolor = 9\nigdb = [\"theme:19\"]\n");
    auto library = openLibrary();

    const auto mappings = library->tagMappings();
    QCOMPARE(mappings.size(), 11);
    QCOMPARE(mappings.first(), (std::pair<QString, QStringList>{"Adventure", {}}));
    QCOMPARE(mappings.at(3), (std::pair<QString, QStringList>{"Platformer", {"genre:8"}}));
    QCOMPARE(mappings.last(), (std::pair<QString, QStringList>{"Horror", {"theme:19"}}));

    // Metroid Dread's facets: in tags.toml's order, whether the list is a default's or its own.
    const QStringList dread{"genre:5", "genre:8", "genre:31", "theme:1", "theme:18", "theme:19", "mode:1",
                            "keyword:metroidvania"};
    QCOMPARE(library->igdbTags(dread, {}), (QStringList{"Platformer", "Metroidvania", "Horror"}));
    // The tags the game has are left out.
    QCOMPARE(library->igdbTags(dread, {"Metroidvania", "Adventure"}), (QStringList{"Platformer", "Horror"}));
    // "Adventure" has no list: IGDB's Adventure genre doesn't suggest it.
    QVERIFY(library->igdbTags({"genre:31"}, {}).isEmpty());
    QVERIFY(library->igdbTags({}, {}).isEmpty());
}

QTEST_GUILESS_MAIN(LibraryTagsTest)

#include "library_tags_test.moc"
