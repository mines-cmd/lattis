#include "libraryoverview.h"

#include <QTest>

using LibraryOverview::Game;

namespace {
Game game(const QString &id, const QString &platform = QStringLiteral("switch"))
{
    Game g;
    g.id = id;
    g.title = id;
    g.platform = platform;
    return g;
}

QDateTime day(int d)
{
    return QDateTime(QDate(2026, 10, d), QTime(12, 0), QTimeZone::UTC);
}
} // namespace

class LibraryOverviewTest : public QObject
{
    Q_OBJECT

private slots:
    void emptyLibrary();
    void totals();
    void hiddenGamesAreLeftOut();
    void linkedCopiesCountOnce();
    void aHiddenMainCopyDoesNotHideTheGroup();
    void continuePlayingOrder();
    void continuePlayingLimitAndGroups();
    void variantMap();
};

void LibraryOverviewTest::emptyLibrary()
{
    const LibraryOverview::Overview o = LibraryOverview::overview({});
    QCOMPARE(o.totals.games, 0);
    QCOMPARE(o.totals.installed, 0);
    QCOMPARE(o.totals.platforms, 0);
    QCOMPARE(o.totals.minutes, 0);
    QCOMPARE(o.totals.backlog, 0);
    QVERIFY(o.continuePlaying.isEmpty());

    const QVariantMap map = LibraryOverview::toVariantMap(o);
    QCOMPARE(map.value("games").toInt(), 0);
    QVERIFY(map.value("continuePlaying").toList().isEmpty());
}

void LibraryOverviewTest::totals()
{
    Game a = game("a");
    a.playMinutes = 90;
    a.status = "not_started";
    Game b = game("b", "steam");
    b.playMinutes = 30;
    b.installed = false;
    b.status = "not_started";
    Game c = game("c", "steam");
    c.status = "finished";
    Game d = game("d");

    const LibraryOverview::Overview o = LibraryOverview::overview({a, b, c, d});
    QCOMPARE(o.totals.games, 4);
    QCOMPARE(o.totals.installed, 3);
    QCOMPARE(o.totals.platforms, 2);
    QCOMPARE(o.totals.minutes, qint64(120));
    QCOMPARE(o.totals.backlog, 2);
}

void LibraryOverviewTest::hiddenGamesAreLeftOut()
{
    Game shown = game("shown");
    shown.playMinutes = 10;
    Game hidden = game("hidden", "epic");
    hidden.hidden = true;
    hidden.playMinutes = 500;
    hidden.status = "playing";
    hidden.lastPlayed = day(5);
    Game alsoHidden = game("also", "epic");
    alsoHidden.hidden = true;
    alsoHidden.status = "not_started";

    const LibraryOverview::Overview o = LibraryOverview::overview({shown, hidden, alsoHidden});
    QCOMPARE(o.totals.games, 1);
    QCOMPARE(o.totals.installed, 1);
    QCOMPARE(o.totals.platforms, 1); // epic has only hidden games
    QCOMPARE(o.totals.minutes, qint64(10));
    QCOMPARE(o.totals.backlog, 0);
    QVERIFY(o.continuePlaying.isEmpty());
}

void LibraryOverviewTest::linkedCopiesCountOnce()
{
    // One game on Steam and Epic: its group total counts once, and each copy's platform still counts.
    Game steam = game("steam-1", "steam");
    steam.groupId = "steam-1";
    steam.playMinutes = 100;
    steam.groupPlayMinutes = 160;
    steam.status = "playing";
    steam.groupLastPlayed = day(9);
    steam.lastPlayed = day(2);
    steam.installed = false;
    Game epic = game("epic-1", "epic");
    epic.groupId = "steam-1";
    epic.playMinutes = 60;
    epic.groupPlayMinutes = 160;
    epic.status = "playing";
    epic.groupLastPlayed = day(9);
    epic.lastPlayed = day(9);
    Game other = game("other");
    other.playMinutes = 5;

    const LibraryOverview::Overview o = LibraryOverview::overview({steam, epic, other});
    QCOMPARE(o.totals.games, 2);
    QCOMPARE(o.totals.installed, 2); // the Epic copy is installed
    QCOMPARE(o.totals.platforms, 3);
    QCOMPARE(o.totals.minutes, qint64(165));
    QCOMPARE(o.continuePlaying.size(), 1);
    QCOMPARE(o.continuePlaying.at(0).id, QStringLiteral("steam-1"));
    QCOMPARE(o.continuePlaying.at(0).lastPlayed, day(9));
}

void LibraryOverviewTest::aHiddenMainCopyDoesNotHideTheGroup()
{
    Game main = game("main", "steam");
    main.groupId = "main";
    main.hidden = true;
    main.groupPlayMinutes = 40;
    Game copy = game("copy", "epic");
    copy.groupId = "main";
    copy.groupPlayMinutes = 40;
    copy.status = "not_started";

    // Listed in either order, the group shows under the copy that's shown.
    for (const QList<Game> &games : {QList<Game>{main, copy}, QList<Game>{copy, main}}) {
        const LibraryOverview::Overview o = LibraryOverview::overview(games);
        QCOMPARE(o.totals.games, 1);
        QCOMPARE(o.totals.platforms, 1);
        QCOMPARE(o.totals.minutes, qint64(40));
        QCOMPARE(o.totals.backlog, 1);
    }

    Game hiddenCopy = copy;
    hiddenCopy.hidden = true;
    QCOMPARE(LibraryOverview::overview({main, hiddenCopy}).totals.games, 0);

    // The main copy is the one the group goes under when both are shown, whatever the order.
    Game shownMain = main;
    shownMain.hidden = false;
    shownMain.title = "Main";
    shownMain.status = "playing";
    copy.status = "playing";
    const LibraryOverview::Overview o = LibraryOverview::overview({copy, shownMain});
    QCOMPARE(o.continuePlaying.size(), 1);
    QCOMPARE(o.continuePlaying.at(0).id, QStringLiteral("main"));
}

void LibraryOverviewTest::continuePlayingOrder()
{
    Game old = game("old");
    old.status = "playing";
    old.lastPlayed = day(1);
    Game recent = game("recent");
    recent.status = "playing";
    recent.lastPlayed = day(20);
    Game never = game("never");
    never.status = "playing";
    Game neverToo = game("A never");
    neverToo.status = "playing";
    Game finished = game("finished");
    finished.status = "finished";
    finished.lastPlayed = day(25);

    const LibraryOverview::Overview o = LibraryOverview::overview({old, never, finished, recent, neverToo});
    QStringList ids;
    for (const LibraryOverview::Counted &c : o.continuePlaying)
        ids.append(c.id);
    QCOMPARE(ids, (QStringList{"recent", "old", "A never", "never"}));
}

void LibraryOverviewTest::continuePlayingLimitAndGroups()
{
    QList<Game> games;
    for (int i = 1; i <= 10; ++i) {
        Game g = game(QStringLiteral("g%1").arg(i));
        g.status = "playing";
        g.lastPlayed = day(i);
        games.append(g);
    }
    const LibraryOverview::Overview o = LibraryOverview::overview(games);
    QCOMPARE(o.continuePlaying.size(), LibraryOverview::kContinueLimit);
    QCOMPARE(o.continuePlaying.first().id, QStringLiteral("g10"));
    QCOMPARE(o.continuePlaying.last().id, QStringLiteral("g4"));
    QCOMPARE(o.totals.games, 10);
}

void LibraryOverviewTest::variantMap()
{
    Game g = game("g", "steam");
    g.title = "Hades";
    g.status = "playing";
    g.lastPlayed = day(3);
    g.coverUrl = "file:///cover.png";
    g.playMinutes = 61;

    const QVariantMap map = LibraryOverview::toVariantMap(LibraryOverview::overview({g}));
    QCOMPARE(map.value("games").toInt(), 1);
    QCOMPARE(map.value("minutes").toLongLong(), 61);
    const QVariantList playing = map.value("continuePlaying").toList();
    QCOMPARE(playing.size(), 1);
    const QVariantMap row = playing.at(0).toMap();
    QCOMPARE(row.value("id").toString(), QStringLiteral("g"));
    QCOMPARE(row.value("title").toString(), QStringLiteral("Hades"));
    QCOMPARE(row.value("coverUrl").toString(), QStringLiteral("file:///cover.png"));
    QCOMPARE(row.value("lastPlayed").toDateTime(), day(3));
}

QTEST_APPLESS_MAIN(LibraryOverviewTest)
#include "library_overview_test.moc"
