#include "collection.h"

#include <QTest>

#include <functional>

namespace {
Game game(const QString &id, const QString &platform = QStringLiteral("switch"))
{
    Game g;
    g.id = id;
    g.platform = platform;
    return g;
}

Game linkedCopy(const QString &id, const QString &main, const QStringList &others)
{
    Game g = game(id);
    g.groupId = main;
    g.linked = others;
    return g;
}

CollectionRules rulesOf(const std::function<void(CollectionRules &)> &fill)
{
    CollectionRules r;
    fill(r);
    return r;
}
}

class CollectionTest : public QObject
{
    Q_OBJECT

private slots:
    void parseAll();
    void parseProblems();
    void valuesRoundTrip();
    void handPicked();
    void rulesAndExceptions();
    void filterRulesRoundTrip();
    void filterRulesMatch();
    void deletedTagsIgnored();
    void linkedCopies();
    void missingIds();
};

void CollectionTest::parseAll()
{
    const auto r = parseCollections(
        "[[collection]]\nid = \"co-op\"\nname = \"Co-op\"\nadded = [\"a\", \"b\", \"a\"]\n\n"
        "[[collection]]\nid = \"uc-1\"\nname = \"Cozy\"\nsteam_id = \"uc-1\"\nrules = true\nplatform = \"switch\"\n"
        "installed_only = true\nfavorites_only = true\nstatuses = [\"playing\"]\ntags = [\"RPG\"]\n"
        "match_all_tags = true\nsteam = [\"owned\"]\nadded = [\"x\"]\nremoved = [\"y\"]\n",
        QStringLiteral("collections.toml"));
    QVERIFY(r.ok);
    QVERIFY2(r.warnings.isEmpty(), qPrintable(r.warnings.join(u'\n')));
    QCOMPARE(r.value.size(), 2);

    QCOMPARE(r.value.at(0).id, QStringLiteral("co-op"));
    QCOMPARE(r.value.at(0).added, (QStringList{"a", "b"})); // a repeat is dropped
    QVERIFY(!r.value.at(0).rules);

    const Collection &cozy = r.value.at(1);
    QCOMPARE(cozy.steamId, QStringLiteral("uc-1"));
    QCOMPARE(cozy.removed, QStringList{"y"});
    QVERIFY(cozy.rules);
    QCOMPARE(cozy.rules->platform, QStringLiteral("switch"));
    QVERIFY(cozy.rules->installedOnly && cozy.rules->favoritesOnly && cozy.rules->matchAllTags);
    QCOMPARE(cozy.rules->statuses, QStringList{"playing"});
    QCOMPARE(cozy.rules->tags, QStringList{"RPG"});
    QCOMPARE(cozy.rules->steamOwnership, QStringList{"owned"});
}

void CollectionTest::parseProblems()
{
    // Nothing here stops the file: each problem is a warning and the rest loads.
    const auto r = parseCollections(
        "[[collection]]\nname = \"No id\"\n\n"
        "[[collection]]\nid = \"a\"\nname = \"First\"\n\n"
        "[[collection]]\nid = \"a\"\nname = \"Same id\"\n\n"
        "[[collection]]\nid = \"b\"\nnmae = \"Typo\"\ntags = [\"RPG\"]\nadded = \"x\"\n\n"
        "[[collection]]\nid = \"c\"\nname = 5\nadded = [\"x\", 3]\n",
        QStringLiteral("collections.toml"));
    QVERIFY(r.ok);
    QCOMPARE(r.value.size(), 3);
    QCOMPARE(r.value.at(0).name, QStringLiteral("First"));
    QCOMPARE(r.value.at(1).name, QStringLiteral("b")); // no name: the id
    QVERIFY(!r.value.at(1).rules);                     // rule keys without rules = true
    QVERIFY(r.value.at(1).added.isEmpty());
    QCOMPARE(r.value.at(2).added, QStringList{"x"});
    const QString all = r.warnings.join(u'\n');
    QVERIFY(all.contains(QStringLiteral("needs an id")));
    QVERIFY(all.contains(QStringLiteral("already a collection with id a")));
    QVERIFY(all.contains(QStringLiteral("unknown key nmae")));
    QVERIFY(all.contains(QStringLiteral("without rules = true")));
    QVERIFY(all.contains(QStringLiteral("added must be a list")));
    QVERIFY(all.contains(QStringLiteral("name must be text")));

    // A broken file isn't ok, so the store keeps the last version that loaded.
    QVERIFY(!parseCollections("[[collection]\n", QStringLiteral("collections.toml")).ok);
    const auto stray = parseCollections("collection = 3\nother = 1\n", QStringLiteral("collections.toml"));
    QVERIFY(stray.ok && stray.value.isEmpty() && stray.warnings.size() == 2);
}

void CollectionTest::valuesRoundTrip()
{
    Collection c;
    c.id = QStringLiteral("uc-9");
    c.name = QStringLiteral("Quote \" and \\ and ü");
    c.steamId = QStringLiteral("uc-9");
    c.added = {"a", "b"};
    c.removed = {"c"};
    c.rules = rulesOf([](CollectionRules &r) {
        r.platform = QStringLiteral("switch");
        r.tags = {"RPG", "Co-op"};
        r.matchAllTags = true;
        r.statuses = {"playing", "none"};
    });

    QByteArray text = "[[collection]]\n";
    const CollectionValues values = collectionValues(c);
    for (const auto &[key, value] : values.set)
        text += key.toUtf8() + " = " + value.toUtf8() + "\n";
    const auto back = parseCollections(text, QStringLiteral("collections.toml"));
    QVERIFY2(back.warnings.isEmpty(), qPrintable(back.warnings.join(u'\n')));
    QCOMPARE(back.value, QList<Collection>{c});

    // A plain list leaves every rule key to remove, so an older block loses them.
    Collection plain;
    plain.id = QStringLiteral("p");
    plain.name = QStringLiteral("Plain");
    const CollectionValues plainValues = collectionValues(plain);
    QCOMPARE(plainValues.set.size(), 2);
    QVERIFY(plainValues.remove.contains(QStringLiteral("rules")));
    QVERIFY(plainValues.remove.contains(QStringLiteral("tags")));
    QVERIFY(plainValues.remove.contains(QStringLiteral("added")));
}

void CollectionTest::handPicked()
{
    Collection c;
    c.added = {"a", "b"};
    QVERIFY(isMember(game("a"), c));
    QVERIFY(isMember(game("b"), c));
    QVERIFY(!isMember(game("c"), c));

    // No rules: removed means nothing, only added counts.
    c.removed = {"b"};
    QVERIFY(isMember(game("b"), c));
    QVERIFY(!isMember(game("a"), Collection{})); // empty: no games, rules or not
}

void CollectionTest::rulesAndExceptions()
{
    Collection c;
    c.rules = rulesOf([](CollectionRules &r) {
        r.platform = QStringLiteral("switch");
        r.favoritesOnly = true;
        r.tags = {"RPG", "Co-op"};
    });
    Game fav = game("fav");
    fav.favorite = true;
    fav.tags = {{"RPG", 0}};
    QVERIFY(isMember(fav, c));

    Game notFav = fav;
    notFav.favorite = false;
    QVERIFY(!isMember(notFav, c));
    Game otherPlatform = fav;
    otherPlatform.platform = QStringLiteral("ps4");
    QVERIFY(!isMember(otherPlatform, c));
    Game noTag = fav;
    noTag.tags = {{"Racing", 0}};
    QVERIFY(!isMember(noTag, c));

    c.rules->matchAllTags = true;
    QVERIFY(!isMember(fav, c));
    fav.tags = {{"Co-op", 1}, {"RPG", 0}};
    QVERIFY(isMember(fav, c));

    // Exceptions: removed keeps a matching game out, added brings a non-matching one in, and added wins.
    c.removed = {"fav"};
    QVERIFY(!isMember(fav, c));
    c.added = {"notFav", "fav"};
    QVERIFY(isMember(game("notFav"), c));
    QVERIFY(isMember(fav, c));

    // Status "none" is a game without one; installed and Steam ownership are rules too.
    Collection other;
    other.rules = rulesOf([](CollectionRules &r) {
        r.statuses = {"none"};
        r.installedOnly = true;
        r.steamOwnership = {"owned"};
    });
    Game g = game("g");
    g.installed = true;
    g.steamOwnership = QStringLiteral("owned");
    QVERIFY(isMember(g, other));
    g.status = QStringLiteral("playing");
    QVERIFY(!isMember(g, other));
    g.status.clear();
    g.installed = false;
    QVERIFY(!isMember(g, other));
    g.installed = true;
    g.steamOwnership = QStringLiteral("family");
    QVERIFY(!isMember(g, other));

    // Rules with nothing set match every game.
    Collection all;
    all.rules = CollectionRules{};
    QVERIFY(isMember(game("anything"), all));
}

void CollectionTest::filterRulesRoundTrip()
{
    Collection c;
    c.id = QStringLiteral("f");
    c.name = QStringLiteral("Filters");
    c.rules = rulesOf([](CollectionRules &r) {
        r.yearFrom = 1990;
        r.yearTo = 1999;
        r.minRating = 3.5;
        r.playTimes = {"never", "2_10h"};
        r.lastPlayed = QStringLiteral("month");
        r.achievements = {"in_progress", "none"};
    });
    QByteArray text = "[[collection]]\n";
    for (const auto &[key, value] : collectionValues(c).set)
        text += key.toUtf8() + " = " + value.toUtf8() + "\n";
    const auto back = parseCollections(text, QStringLiteral("collections.toml"));
    QVERIFY2(back.warnings.isEmpty(), qPrintable(back.warnings.join(u'\n')));
    QCOMPARE(back.value, QList<Collection>{c});

    // Rating 0 is a rule ("rated, however low"); the default -1 leaves the key out.
    c.rules->minRating = 0;
    QByteArray zero;
    for (const auto &[key, value] : collectionValues(c).set)
        zero += key.toUtf8() + " = " + value.toUtf8() + "\n";
    QVERIFY(zero.contains("min_rating = 0.0"));
    c.rules = CollectionRules{};
    c.rules->favoritesOnly = true;
    const CollectionValues plain = collectionValues(c);
    for (const QString &key : {"year_from", "year_to", "min_rating", "play_times", "last_played", "achievements"})
        QVERIFY2(plain.remove.contains(key), qPrintable(key));

    // A file from before these keys has none of them; bad values are warnings and the rest loads.
    const auto old = parseCollections("[[collection]]\nid = \"o\"\nrules = true\nfavorites_only = true\n",
                                      QStringLiteral("collections.toml"));
    QVERIFY(old.warnings.isEmpty());
    QCOMPARE(old.value.first().rules->yearFrom, 0);
    QCOMPARE(old.value.first().rules->minRating, -1.0);
    const auto bad = parseCollections("[[collection]]\nid = \"b\"\nrules = true\nyear_from = 0\nmin_rating = 6\n"
                                      "play_times = [\"never\", \"forever\"]\nlast_played = \"lately\"\n"
                                      "achievements = [\"completed\", 3]\n",
                                      QStringLiteral("collections.toml"));
    const QString warnings = bad.warnings.join(u'\n');
    QVERIFY(warnings.contains(QStringLiteral("year_from must be a year")));
    QVERIFY(warnings.contains(QStringLiteral("min_rating must be a number from 0 to 5")));
    QVERIFY(warnings.contains(QStringLiteral("unknown value \"forever\"")));
    QVERIFY(warnings.contains(QStringLiteral("last_played must be one of")));
    QCOMPARE(bad.value.first().rules->playTimes, QStringList{"never"});
    QCOMPARE(bad.value.first().rules->achievements, QStringList{"completed"});
    QVERIFY(bad.value.first().rules->lastPlayed.isEmpty());
}

void CollectionTest::filterRulesMatch()
{
    const auto member = [](const Game &g, const std::function<void(CollectionRules &)> &fill,
                           const CollectionMatcher::AchievementState &state = {}) {
        Collection c;
        c.rules = rulesOf(fill);
        return CollectionMatcher(c, nullptr, state).isMember(g);
    };

    // Year 0 is unknown: left out while any bound is set, and bounds are inclusive.
    Game g = game("g");
    const auto years = [&](int from, int to) { return member(g, [&](CollectionRules &r) { r.yearFrom = from; r.yearTo = to; }); };
    QVERIFY(years(0, 0));
    QVERIFY(!years(2000, 0));
    QVERIFY(!years(0, 2020));
    g.year = 2010;
    QVERIFY(years(2010, 2010));
    QVERIFY(years(2000, 0));
    QVERIFY(years(0, 2020));
    QVERIFY(!years(2011, 0));
    QVERIFY(!years(0, 2009));

    // Rating -1 is unrated: left out by any minimum, even 0.
    g.rating = -1;
    QVERIFY(member(g, [](CollectionRules &) {}));
    QVERIFY(!member(g, [](CollectionRules &r) { r.minRating = 0; }));
    g.rating = 0;
    QVERIFY(member(g, [](CollectionRules &r) { r.minRating = 0; }));
    g.rating = 3.5;
    QVERIFY(member(g, [](CollectionRules &r) { r.minRating = 3.5; }));
    QVERIFY(!member(g, [](CollectionRules &r) { r.minRating = 4; }));

    // A linked group counts its copies' total play time and latest play.
    g.playMinutes = 10;
    g.groupPlayMinutes = 700;
    g.lastPlayed = QDateTime::currentDateTimeUtc().addYears(-3);
    g.groupLastPlayed = QDateTime::currentDateTimeUtc().addDays(-2);
    QVERIFY(member(g, [](CollectionRules &r) { r.playTimes = {"10_50h"}; }));
    QVERIFY(!member(g, [](CollectionRules &r) { r.playTimes = {"under_2h"}; }));
    QVERIFY(member(g, [](CollectionRules &r) { r.lastPlayed = QStringLiteral("week"); }));
    QVERIFY(!member(g, [](CollectionRules &r) { r.lastPlayed = QStringLiteral("older"); }));
    Game never = game("never");
    QVERIFY(member(never, [](CollectionRules &r) { r.playTimes = {"never"}; }));
    QVERIFY(!member(never, [](CollectionRules &r) { r.lastPlayed = QStringLiteral("older"); }));

    // Without the shared achievements pass (no lookup) every game is "none to earn", as in the filter.
    const auto completed = [](CollectionRules &r) { r.achievements = {"completed"}; };
    const auto none = [](CollectionRules &r) { r.achievements = {"none"}; };
    QVERIFY(!member(g, completed));
    QVERIFY(member(g, none));
    const CollectionMatcher::AchievementState read = [](const Game &game) {
        return game.id == QLatin1String("g") ? QStringLiteral("completed") : QStringLiteral("none");
    };
    QVERIFY(member(g, completed, read));
    QVERIFY(!member(g, none, read));
    QVERIFY(member(never, none, read));

    // Exceptions still win over the new rules.
    Collection c;
    c.rules = rulesOf([](CollectionRules &r) { r.yearFrom = 2050; });
    c.added = {"g"};
    QVERIFY(isMember(g, c));
}

void CollectionTest::deletedTagsIgnored()
{
    Collection c;
    c.rules = rulesOf([](CollectionRules &r) { r.tags = {"RPG", "Gone"}; });
    Game rpg = game("rpg");
    rpg.tags = {{"RPG", 0}};
    Game other = game("other");
    other.tags = {{"Racing", 0}};
    const QSet<QString> known{"RPG", "Racing"};

    // Without the list the tags are taken as written; with it a deleted one drops out.
    QVERIFY(CollectionMatcher(c).isMember(rpg));
    QVERIFY(CollectionMatcher(c, &known).isMember(rpg));
    QVERIFY(!CollectionMatcher(c, &known).isMember(other));

    // All the rule's tags deleted: the rule matches no game, not every game.
    c.rules->tags = {"Gone"};
    QVERIFY(!CollectionMatcher(c).isMember(rpg));
    QVERIFY(!CollectionMatcher(c, &known).isMember(rpg));
    QVERIFY(!CollectionMatcher(c, &known).isMember(other));

    // Match all counts only the tags that exist.
    c.rules->tags = {"RPG", "Racing", "Gone"};
    c.rules->matchAllTags = true;
    QVERIFY(!CollectionMatcher(c, &known).isMember(rpg));
    rpg.tags.append({"Racing", 1});
    QVERIFY(CollectionMatcher(c, &known).isMember(rpg));
}

void CollectionTest::linkedCopies()
{
    // Copies of one game (same_as) share the main copy's id as groupId and list the others.
    const Game main = linkedCopy("zelda-switch", "zelda-switch", {"zelda-wiiu"});
    const Game copy = linkedCopy("zelda-wiiu", "zelda-switch", {"zelda-switch"});
    const Game alone = game("other");

    Collection c;
    c.added = {"zelda-wiiu"};
    QVERIFY(isMember(copy, c));
    QVERIFY(isMember(main, c)); // the copy named, the main copy comes along
    QVERIFY(!isMember(alone, c));

    c.added = {"zelda-switch"};
    QVERIFY(isMember(copy, c));
    QVERIFY(isMember(main, c));

    // A removal of one copy takes the group out of a rule-based collection.
    Collection rules;
    rules.rules = CollectionRules{};
    rules.removed = {"zelda-switch"};
    QVERIFY(!isMember(copy, rules));
    QVERIFY(!isMember(main, rules));
    QVERIFY(isMember(alone, rules));
}

void CollectionTest::missingIds()
{
    // Ids of games that aren't in the library are only ignored, never dropped by the model.
    Collection c;
    c.added = {"gone", "a"};
    c.removed = {"also-gone"};
    QVERIFY(isMember(game("a"), c));
    QVERIFY(!isMember(game("b"), c));
    QCOMPARE(c.added, (QStringList{"gone", "a"}));
    const CollectionValues values = collectionValues(c);
    bool saved = false;
    for (const auto &[key, value] : values.set)
        saved = saved || (key == QLatin1String("added") && value.contains(QLatin1String("gone")));
    QVERIFY(saved);
}

QTEST_APPLESS_MAIN(CollectionTest)
#include "collection_test.moc"
