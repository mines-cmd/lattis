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

Game steamGame(const QString &id, qint64 appId)
{
    Game g = game(id, QStringLiteral("steam"));
    g.steamAppId = appId;
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
    void gamesOnTwoDrives();
    void unknownSizesAreNotCounted();
    void otherFilesNeverBelowZero();
    void storageLeavesOutHiddenAndUninstalled();
    void linkedCopiesEachTakeSpace();
    void largestAndDormant();
    void storageVariantMap();
    void platformCounts();
    void statusCounts();
    void topTags();
    void releaseYears();
    void breakdownsVariantMap();
    void nothingNeedsAttention();
    void attentionRows();
    void attentionLeavesOutHiddenAndCountsGroupsOnce();
    void savesNeverBackedUp();
    void backlogPicks();
    void attentionVariantMap();
    void achievementsNotReadYet();
    void achievementShareAndPerfectGames();
    void noSteamGamesStillReadsNothing();
    void nearlyDoneOrderAndLimit();
    void nearlyDoneLeavesOutBelowEightyAndPerfect();
    void achievementsLeaveOutHiddenAndCountLinkedCopiesOnce();
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
    shownMain.groupLastPlayed = day(3);
    copy.groupLastPlayed = day(3);
    const LibraryOverview::Overview o = LibraryOverview::overview({copy, shownMain});
    QCOMPARE(o.continuePlaying.size(), 1);
    QCOMPARE(o.continuePlaying.at(0).id, QStringLiteral("main"));
}

void LibraryOverviewTest::continuePlayingOrder()
{
    Game old = game("old");
    old.lastPlayed = day(1);
    Game recent = game("recent");
    recent.status = "finished";
    recent.lastPlayed = day(20);
    Game never = game("never");
    never.status = "playing";
    Game removed = game("removed");
    removed.installed = false;
    removed.lastPlayed = day(30);
    Game tied = game("A tied");
    tied.lastPlayed = day(1);

    // Any status counts; what matters is that it is installed and has been played.
    const LibraryOverview::Overview o = LibraryOverview::overview({old, never, removed, recent, tied});
    QStringList ids;
    for (const LibraryOverview::Counted &c : o.continuePlaying)
        ids.append(c.id);
    QCOMPARE(ids, (QStringList{"recent", "A tied", "old"}));
}

void LibraryOverviewTest::continuePlayingLimitAndGroups()
{
    QList<Game> games;
    for (int i = 1; i <= 10; ++i) {
        Game g = game(QStringLiteral("g%1").arg(i));
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

namespace {
constexpr qint64 GB = 1000LL * 1000 * 1000;

const QDateTime kNow(QDate(2026, 11, 6), QTime(12, 0), QTimeZone::UTC);

LibraryOverview::Drive drive(const QString &root, qint64 total, qint64 free, const QString &device = {})
{
    return {device.isEmpty() ? root : device, root, root, total, free};
}

Game installedAt(const QString &id, const QString &platform, const QString &path, qint64 size)
{
    Game g = game(id, platform);
    g.path = path;
    g.sizeBytes = size;
    return g;
}
} // namespace

void LibraryOverviewTest::gamesOnTwoDrives()
{
    const QList<LibraryOverview::Drive> drives{drive("/", 500 * GB, 200 * GB), drive("/mnt/games", 1000 * GB, 400 * GB)};
    const LibraryOverview::Overview o = LibraryOverview::overview(
        {installedAt("a", "steam", "/home/me/.steam/common/a", 50 * GB),
         installedAt("b", "steam", "/mnt/games/steam/b", 100 * GB),
         installedAt("c", "epic", "/mnt/games/epic/c", 60 * GB),
         installedAt("d", "switch", "/mnt/gamesque/d.nsp", 5 * GB)}, // not under /mnt/games
        drives, kNow);

    QCOMPARE(o.drives.size(), 2);
    // Most games first.
    QCOMPARE(o.drives.at(0).drive.root, QStringLiteral("/mnt/games"));
    QCOMPARE(o.drives.at(0).gamesBytes, 160 * GB);
    QCOMPARE(o.drives.at(0).otherBytes, 440 * GB); // 600 used - 160
    QCOMPARE(o.drives.at(0).platforms.size(), 2);
    QCOMPARE(o.drives.at(0).platforms.at(0).platform, QStringLiteral("steam"));
    QCOMPARE(o.drives.at(0).platforms.at(0).bytes, 100 * GB);
    QCOMPARE(o.drives.at(1).drive.root, QStringLiteral("/"));
    QCOMPARE(o.drives.at(1).gamesBytes, 55 * GB); // a, and d which falls back to /
    QCOMPARE(o.drives.at(1).otherBytes, 245 * GB);

    // The legend adds a platform up over every drive.
    QCOMPARE(o.platformSizes.size(), 3);
    QCOMPARE(o.platformSizes.at(0).platform, QStringLiteral("steam"));
    QCOMPARE(o.platformSizes.at(0).bytes, 150 * GB);
    QCOMPARE(o.platformSizes.at(1).platform, QStringLiteral("epic"));
}

void LibraryOverviewTest::unknownSizesAreNotCounted()
{
    const QList<LibraryOverview::Drive> drives{drive("/", 100 * GB, 50 * GB)};
    const LibraryOverview::Overview o = LibraryOverview::overview(
        {installedAt("a", "proton", "/g/a", 0), installedAt("b", "proton", "/g/b", 10 * GB)}, drives, kNow);

    QCOMPARE(o.drives.size(), 1);
    QCOMPARE(o.drives.at(0).gamesBytes, 10 * GB);
    QCOMPARE(o.drives.at(0).unknown, 1);
    QCOMPARE(o.drives.at(0).otherBytes, 40 * GB);
    QCOMPARE(o.platformSizes.at(0).bytes, 10 * GB);
    QCOMPARE(o.platformSizes.at(0).unknown, 1);
    QCOMPARE(o.largest.size(), 1); // the game of unknown size isn't ranked

    // Only unknown sizes: the drive still holds games, with nothing split by platform.
    const LibraryOverview::Overview none = LibraryOverview::overview({installedAt("a", "proton", "/g/a", 0)}, drives, kNow);
    QCOMPARE(none.drives.size(), 1);
    QCOMPARE(none.drives.at(0).gamesBytes, qint64(0));
    QCOMPARE(none.drives.at(0).otherBytes, 50 * GB);
    QVERIFY(none.largest.isEmpty());
    QCOMPARE(none.dormant.size(), 1);
}

void LibraryOverviewTest::otherFilesNeverBelowZero()
{
    // Sizes the drive can't hold (a game folder copied while the drive filled, a stale size) leave no negative rest.
    const LibraryOverview::Overview o = LibraryOverview::overview(
        {installedAt("a", "steam", "/g/a", 80 * GB), installedAt("b", "steam", "/g/b", 80 * GB)},
        {drive("/", 100 * GB, 20 * GB)}, kNow);
    QCOMPARE(o.drives.at(0).gamesBytes, 160 * GB);
    QCOMPARE(o.drives.at(0).otherBytes, qint64(0));

    // More free than total (a drive reporting oddly) is no negative either.
    const LibraryOverview::Overview odd = LibraryOverview::overview(
        {installedAt("a", "steam", "/g/a", 1 * GB)}, {drive("/", 10 * GB, 20 * GB)}, kNow);
    QCOMPARE(odd.drives.at(0).otherBytes, qint64(0));
}

void LibraryOverviewTest::storageLeavesOutHiddenAndUninstalled()
{
    Game hidden = installedAt("h", "steam", "/g/h", 90 * GB);
    hidden.hidden = true;
    Game notInstalled = installedAt("n", "steam", "/g/n", 70 * GB);
    notInstalled.installed = false;
    Game noPath = installedAt("p", "heroic", QString(), 5 * GB);
    const Game shown = installedAt("s", "steam", "/g/s", 10 * GB);

    const LibraryOverview::Overview o = LibraryOverview::overview(
        {hidden, notInstalled, noPath, shown}, {drive("/", 200 * GB, 100 * GB)}, kNow);
    QCOMPARE(o.drives.at(0).gamesBytes, 10 * GB);
    QCOMPARE(o.drives.at(0).otherBytes, 90 * GB); // the hidden game's files are among them
    QCOMPARE(o.platformSizes.size(), 2); // steam (s) and heroic (p, on no known drive)
    QCOMPARE(o.largest.size(), 2);
    QCOMPARE(o.largest.at(0).id, QStringLiteral("s"));
    for (const LibraryOverview::Sized &row : o.dormant)
        QVERIFY(row.id != "h" && row.id != "n");

    // No drive holds a game: no bars.
    QVERIFY(LibraryOverview::overview({noPath}, {drive("/", 200 * GB, 100 * GB)}, kNow).drives.isEmpty());
    QVERIFY(LibraryOverview::overview({shown}, {}, kNow).drives.isEmpty());
}

void LibraryOverviewTest::linkedCopiesEachTakeSpace()
{
    Game steam = installedAt("steam-1", "steam", "/g/steam-1", 30 * GB);
    steam.groupId = "steam-1";
    steam.groupLastPlayed = QDateTime(QDate(2026, 11, 1), QTime(10, 0), QTimeZone::UTC);
    Game epic = installedAt("epic-1", "epic", "/g/epic-1", 32 * GB);
    epic.groupId = "steam-1";
    epic.groupLastPlayed = steam.groupLastPlayed;
    epic.lastPlayed = QDateTime(QDate(2025, 1, 1), QTime(10, 0), QTimeZone::UTC); // played here long ago

    const LibraryOverview::Overview o = LibraryOverview::overview({steam, epic}, {drive("/", 200 * GB, 100 * GB)}, kNow);
    QCOMPARE(o.totals.games, 1);
    QCOMPARE(o.drives.at(0).gamesBytes, 62 * GB);
    QCOMPARE(o.largest.size(), 2);
    QCOMPARE(o.largest.at(0).id, QStringLiteral("epic-1"));
    // The group was played this month, so neither copy is dormant.
    QVERIFY(o.dormant.isEmpty());
}

void LibraryOverviewTest::largestAndDormant()
{
    QList<Game> games;
    for (int i = 0; i < 6; ++i)
        games.append(installedAt(QString("g%1").arg(i), "steam", "/g", (i + 1) * GB));
    // g5 is the largest and was played yesterday; g0..g4 never or long ago.
    games[5].lastPlayed = kNow.addDays(-1);
    games[4].lastPlayed = kNow.addMonths(-6).addDays(1); // just inside the threshold
    games[3].lastPlayed = kNow.addMonths(-6).addDays(-1); // just outside
    games[2].lastPlayed = kNow.addYears(-2);
    Game unknown = installedAt("u", "steam", "/g", 0);
    games.append(unknown);

    const LibraryOverview::Overview o = LibraryOverview::overview(games, {drive("/", 100 * GB, 50 * GB)}, kNow);
    QCOMPARE(o.largest.size(), LibraryOverview::kStorageListLimit);
    QCOMPARE(o.largest.at(0).id, QStringLiteral("g5"));
    QCOMPARE(o.largest.at(3).id, QStringLiteral("g2"));

    // Dormant: g3, g2, g1, g0 and u; the biggest four, the unknown size last (so cut).
    QCOMPARE(o.dormant.size(), LibraryOverview::kStorageListLimit);
    QCOMPARE(o.dormant.at(0).id, QStringLiteral("g3"));
    QCOMPARE(o.dormant.at(1).id, QStringLiteral("g2"));
    QCOMPARE(o.dormant.at(2).id, QStringLiteral("g1"));
    QCOMPARE(o.dormant.at(3).id, QStringLiteral("g0"));
}

void LibraryOverviewTest::storageVariantMap()
{
    Game g = installedAt("g", "steam", "/g/g", 5 * GB);
    g.uninstallable = true;
    g.title = "Hades";
    const QVariantMap map = LibraryOverview::toVariantMap(LibraryOverview::overview({g}, {drive("/", 100 * GB, 40 * GB)}, kNow));

    const QVariantList drives = map.value("drives").toList();
    QCOMPARE(drives.size(), 1);
    const QVariantMap d = drives.at(0).toMap();
    QCOMPARE(d.value("root").toString(), QStringLiteral("/"));
    QCOMPARE(d.value("total").toLongLong(), 100 * GB);
    QCOMPARE(d.value("free").toLongLong(), 40 * GB);
    QCOMPARE(d.value("gamesBytes").toLongLong(), 5 * GB);
    QCOMPARE(d.value("otherBytes").toLongLong(), 55 * GB);
    QCOMPARE(d.value("platforms").toList().at(0).toMap().value("platform").toString(), QStringLiteral("steam"));

    const QVariantMap row = map.value("largest").toList().at(0).toMap();
    QCOMPARE(row.value("id").toString(), QStringLiteral("g"));
    QCOMPARE(row.value("bytes").toLongLong(), 5 * GB);
    QVERIFY(row.value("uninstallable").toBool());
    QCOMPARE(map.value("dormant").toList().size(), 1);
    QCOMPARE(map.value("platformSizes").toList().size(), 1);
}

void LibraryOverviewTest::platformCounts()
{
    Game a = game("a", "steam");
    Game b = game("b", "steam");
    b.installed = false;
    Game hidden = game("h", "steam");
    hidden.hidden = true;
    Game copy1 = game("c1", "steam");
    copy1.groupId = "c1";
    Game copy2 = game("c2", "epic");
    copy2.groupId = "c1";
    Game rom = game("r", "switch");
    rom.playMinutes = 5;

    const auto o = LibraryOverview::overview({a, b, hidden, copy1, copy2, rom});
    // Linked copies are one game in the totals but each counts on its own platform.
    QCOMPARE(o.totals.games, 4);
    QCOMPARE(o.platformCounts.size(), 3);
    QCOMPARE(o.platformCounts.at(0).platform, QStringLiteral("steam"));
    QCOMPARE(o.platformCounts.at(0).games, 3);
    QCOMPARE(o.platformCounts.at(0).installed, 2);
    QCOMPARE(o.platformCounts.at(1).platform, QStringLiteral("epic"));
    QCOMPARE(o.platformCounts.at(1).games, 1);
    QCOMPARE(o.platformCounts.at(2).platform, QStringLiteral("switch"));
    QCOMPARE(o.totals.played, 1);
}

void LibraryOverviewTest::statusCounts()
{
    Game a = game("a");
    a.status = "finished";
    Game b = game("b");
    b.status = "playing";
    Game c = game("c");
    c.status = "finished";
    Game d = game("d");
    Game odd = game("e");
    odd.status = "not-a-status";
    Game hidden = game("h");
    hidden.hidden = true;
    hidden.status = "dropped";
    Game linked1 = game("l1");
    linked1.groupId = "l1";
    linked1.status = "dropped";
    Game linked2 = game("l2");
    linked2.groupId = "l1";
    linked2.status = "dropped";

    const auto o = LibraryOverview::overview({a, b, c, d, odd, hidden, linked1, linked2});
    // The list's order; a linked group once; unknown and empty statuses are "none"; unused ones are left out.
    QCOMPARE(o.statusCounts.size(), 4);
    QCOMPARE(o.statusCounts.at(0).status, QStringLiteral("playing"));
    QCOMPARE(o.statusCounts.at(1).status, QStringLiteral("finished"));
    QCOMPARE(o.statusCounts.at(1).count, 2);
    QCOMPARE(o.statusCounts.at(2).status, QStringLiteral("dropped"));
    QCOMPARE(o.statusCounts.at(2).count, 1);
    QCOMPARE(o.statusCounts.at(3).status, QStringLiteral("none"));
    QCOMPARE(o.statusCounts.at(3).count, 2);
}

void LibraryOverviewTest::topTags()
{
    QList<Game> games;
    for (int i = 0; i < 15; ++i) {
        Game g = game(QString("g%1").arg(i));
        g.tags = {QString("tag%1").arg(i), "Common", "Common"};
        if (i < 3)
            g.tags.append("Often");
        games.append(g);
    }
    Game hidden = game("h");
    hidden.hidden = true;
    hidden.tags = {"Hidden"};
    games.append(hidden);

    const auto o = LibraryOverview::overview(games);
    QCOMPARE(o.topTags.size(), LibraryOverview::kTopTagLimit);
    QCOMPARE(o.topTags.at(0).tag, QStringLiteral("Common"));
    QCOMPARE(o.topTags.at(0).count, 15); // a repeated tag counts once per game
    QCOMPARE(o.topTags.at(1).tag, QStringLiteral("Often"));
    QCOMPARE(o.topTags.at(1).count, 3);
    // Equal counts by name.
    QCOMPARE(o.topTags.at(2).tag, QStringLiteral("tag0"));
    QCOMPARE(o.topTags.at(3).tag, QStringLiteral("tag1"));
    for (const auto &t : o.topTags)
        QVERIFY(t.tag != QLatin1String("Hidden"));
}

void LibraryOverviewTest::releaseYears()
{
    QVERIFY(LibraryOverview::overview({game("none")}).years.isEmpty());

    Game a = game("a");
    a.year = 2018;
    Game b = game("b");
    b.year = 2021;
    Game c = game("c");
    c.year = 2021;
    Game hidden = game("h");
    hidden.hidden = true;
    hidden.year = 1990;
    auto o = LibraryOverview::overview({a, b, c, hidden, game("unknown")});
    QCOMPARE(o.years.size(), 4); // the years between are listed with 0
    QCOMPARE(o.years.at(0).year, 2018);
    QCOMPARE(o.years.at(0).count, 1);
    QCOMPARE(o.years.at(1).count, 0);
    QCOMPARE(o.years.at(3).year, 2021);
    QCOMPARE(o.years.at(3).count, 2);

    // A mistyped year doesn't make a column for every year in between.
    Game odd = game("odd");
    odd.year = 5;
    o = LibraryOverview::overview({a, odd});
    QCOMPARE(o.years.size(), 2);
    QCOMPARE(o.years.at(0).year, 5);
    QCOMPARE(o.years.at(1).year, 2018);
}

void LibraryOverviewTest::breakdownsVariantMap()
{
    Game g = game("g", "steam");
    g.status = "playing";
    g.tags = {"RPG"};
    g.year = 2020;
    const QVariantMap map = LibraryOverview::toVariantMap(LibraryOverview::overview({g}));
    QCOMPARE(map.value("played").toInt(), 0);
    const QVariantMap p = map.value("platformCounts").toList().at(0).toMap();
    QCOMPARE(p.value("platform").toString(), QStringLiteral("steam"));
    QCOMPARE(p.value("games").toInt(), 1);
    QCOMPARE(p.value("installed").toInt(), 1);
    QCOMPARE(map.value("statusCounts").toList().at(0).toMap().value("status").toString(), QStringLiteral("playing"));
    QCOMPARE(map.value("topTags").toList().at(0).toMap().value("tag").toString(), QStringLiteral("RPG"));
    QCOMPARE(map.value("years").toList().at(0).toMap().value("year").toInt(), 2020);
}

namespace {
int attentionCount(const LibraryOverview::Overview &o, const QString &kind)
{
    for (const LibraryOverview::Attention &a : o.attention) {
        if (a.kind == kind)
            return a.count;
    }
    return 0;
}
} // namespace

void LibraryOverviewTest::nothingNeedsAttention()
{
    const LibraryOverview::Overview o = LibraryOverview::overview({game("a"), game("b")});
    QVERIFY(o.attention.isEmpty());
    QVERIFY(o.backlog.isEmpty());
    QVERIFY(LibraryOverview::toVariantMap(o).value("attention").toList().isEmpty());
}

void LibraryOverviewTest::attentionRows()
{
    Game art = game("art");
    art.missingArt = true;
    Game info = game("info");
    info.missingInfo = true;
    Game update = game("update", "steam");
    update.updateWaiting = true;
    Game gone = game("gone");
    gone.rom = true;
    gone.installed = false;
    Game noEmulator = game("noemu");
    noEmulator.rom = true;
    noEmulator.emulatorMissing = true;
    const LibraryOverview::Overview o = LibraryOverview::overview({art, info, update, gone, noEmulator, game("fine")});
    QCOMPARE(attentionCount(o, "missingArt"), 1);
    QCOMPARE(attentionCount(o, "missingInfo"), 1);
    QCOMPARE(attentionCount(o, "updates"), 1);
    QCOMPARE(attentionCount(o, "romsNotFound"), 1);
    QCOMPARE(attentionCount(o, "emulatorsMissing"), 1);
    QCOMPARE(attentionCount(o, "neverBackedUp"), 0); // not read: left out, not zero-and-shown
    QCOMPARE(o.attention.size(), 5);
    // The brief's order, whatever the games' order.
    QCOMPARE(o.attention.at(0).kind, QStringLiteral("missingArt"));
    QCOMPARE(o.attention.at(4).kind, QStringLiteral("emulatorsMissing"));
}

void LibraryOverviewTest::attentionLeavesOutHiddenAndCountsGroupsOnce()
{
    Game hidden = game("h");
    hidden.hidden = true;
    hidden.missingArt = true;
    hidden.updateWaiting = true;
    hidden.rom = true;
    hidden.installed = false;
    Game main = game("main", "steam");
    main.groupId = "main";
    main.missingArt = true;
    main.missingInfo = true;
    Game copy = game("copy", "epic");
    copy.groupId = "main";
    copy.missingArt = true;
    copy.missingInfo = true;
    const LibraryOverview::Overview o = LibraryOverview::overview({hidden, main, copy});
    QCOMPARE(attentionCount(o, "missingArt"), 1);
    QCOMPARE(attentionCount(o, "missingInfo"), 1);
    QCOMPARE(attentionCount(o, "updates"), 0);
    QCOMPARE(attentionCount(o, "romsNotFound"), 0);

    // An update on each copy is two updates to run.
    main.updateWaiting = true;
    copy.updateWaiting = true;
    QCOMPARE(attentionCount(LibraryOverview::overview({main, copy}), "updates"), 2);
}

void LibraryOverviewTest::savesNeverBackedUp()
{
    const auto played = [](const QString &id, const QString &key) {
        Game g = game(id, "steam");
        g.playMinutes = 30;
        g.backupKey = key;
        return g;
    };
    Game never = game("never", "steam"); // unplayed: nothing to back up yet
    never.backupKey = "never";
    Game rom = game("rom"); // no key: saves can't cover it
    rom.playMinutes = 10;
    Game mainCopy = played("main", "main");
    mainCopy.groupId = "main";
    mainCopy.groupPlayMinutes = 30;
    Game linked = game("linked", "linux");
    linked.groupId = "main";
    linked.groupPlayMinutes = 30;
    linked.backupKey = "main";
    Game hidden = played("hidden", "hidden");
    hidden.hidden = true;
    const QList<Game> games{played("a", "a"), played("b", "b"), never, rom, mainCopy, linked, hidden};

    LibraryOverview::Overview o = LibraryOverview::overview(games, {}, QDateTime::currentDateTimeUtc(), QSet<QString>{"b"});
    QCOMPARE(attentionCount(o, "neverBackedUp"), 2); // a, and the linked group once
    o = LibraryOverview::overview(games, {}, QDateTime::currentDateTimeUtc(), QSet<QString>{"a", "b", "main"});
    QCOMPARE(attentionCount(o, "neverBackedUp"), 0);
    o = LibraryOverview::overview(games);
    QCOMPARE(attentionCount(o, "neverBackedUp"), 0);
}

void LibraryOverviewTest::backlogPicks()
{
    Game a = game("a");
    a.status = "not_started";
    a.year = 2020;
    Game uninstalled = game("u");
    uninstalled.status = "not_started";
    uninstalled.installed = false;
    Game hidden = game("h");
    hidden.status = "not_started";
    hidden.hidden = true;
    Game playing = game("p");
    playing.status = "playing";
    Game main = game("m", "steam");
    main.status = "not_started";
    main.groupId = "m";
    Game copy = game("m2", "epic");
    copy.status = "not_started";
    copy.groupId = "m";
    const LibraryOverview::Overview o = LibraryOverview::overview({copy, main, a, uninstalled, hidden, playing});
    QCOMPARE(o.backlog.size(), 2); // installed only, a group once
    QCOMPARE(o.backlog.at(0).id, QStringLiteral("a"));
    QCOMPARE(o.backlog.at(0).year, 2020);
    QCOMPARE(o.backlog.at(1).id, QStringLiteral("m"));

    const QVariantMap map = LibraryOverview::toVariantMap(o);
    QCOMPARE(map.value("backlogPicks").toList().size(), 2);
    QCOMPARE(map.value("backlogPicks").toList().at(0).toMap().value("platform").toString(), QStringLiteral("switch"));
}

void LibraryOverviewTest::attentionVariantMap()
{
    Game art = game("art");
    art.missingArt = true;
    const QVariantMap map = LibraryOverview::toVariantMap(LibraryOverview::overview({art}));
    const QVariantMap row = map.value("attention").toList().at(0).toMap();
    QCOMPARE(row.value("kind").toString(), QStringLiteral("missingArt"));
    QCOMPARE(row.value("count").toInt(), 1);
}

void LibraryOverviewTest::achievementsNotReadYet()
{
    Game g = game("steam-1", "steam");
    g.steamAppId = 1;
    const LibraryOverview::Overview o = LibraryOverview::overview({g});
    QVERIFY(!o.achievements);
    const QVariantMap map = LibraryOverview::toVariantMap(o);
    QVERIFY(!map.contains("achievements"));
    QVERIFY(map.value("nearlyDone").toList().isEmpty());
}

void LibraryOverviewTest::achievementShareAndPerfectGames()
{
    Game a = steamGame("steam-1", 1);
    Game b = steamGame("steam-2", 2);
    Game none = steamGame("steam-3", 3);
    Game unread = steamGame("steam-4", 4);
    Game epic = game("epic-1", "epic");
    const LibraryOverview::AchievementCounts counts{{1, {10, 10}}, {2, {5, 20}}, {3, {0, 0}}};

    const LibraryOverview::Overview o = LibraryOverview::overview({a, b, none, unread, epic}, {}, day(20), std::nullopt, counts);
    QVERIFY(o.achievements);
    QCOMPARE(o.achievements->games, 2); // no achievements and not read are left out
    QCOMPARE(o.achievements->unlocked, 15);
    QCOMPARE(o.achievements->total, 30);
    QCOMPARE(o.achievements->perfect, 1);

    const QVariantMap map = LibraryOverview::toVariantMap(o).value("achievements").toMap();
    QCOMPARE(map.value("unlocked").toInt(), 15);
    QCOMPARE(map.value("total").toInt(), 30);
    QCOMPARE(map.value("perfect").toInt(), 1);
}

void LibraryOverviewTest::noSteamGamesStillReadsNothing()
{
    const LibraryOverview::Overview o = LibraryOverview::overview({game("a")}, {}, day(20), std::nullopt,
                                                                  LibraryOverview::AchievementCounts());
    QVERIFY(o.achievements);
    QCOMPARE(o.achievements->games, 0);
    QCOMPARE(o.achievements->total, 0);
    QVERIFY(o.achievements->nearlyDone.isEmpty());
}

void LibraryOverviewTest::nearlyDoneOrderAndLimit()
{
    QList<Game> games;
    LibraryOverview::AchievementCounts counts;
    const std::pair<int, int> shares[] = {{79, 100}, {80, 100}, {9, 10}, {10, 10}, {19, 20}, {99, 100}, {40, 50}};
    int appId = 10;
    for (const auto &[unlocked, total] : shares) {
        games.append(steamGame(QStringLiteral("steam-%1").arg(appId), appId));
        counts.insert(appId, {unlocked, total});
        ++appId;
    }
    games[5].title = "B"; // 99%
    games[2].title = "A"; // 90%
    games[4].title = "C"; // 95%

    const LibraryOverview::Overview o = LibraryOverview::overview(games, {}, day(20), std::nullopt, counts);
    QVERIFY(o.achievements);
    QCOMPARE(o.achievements->perfect, 1);
    // Five games are at 80% or more and not perfect; the fifth, an 80% one, is cut.
    QCOMPARE(o.achievements->nearlyDone.size(), 4);
    QCOMPARE(o.achievements->nearlyDone.at(0).title, QStringLiteral("B"));
    QCOMPARE(o.achievements->nearlyDone.at(1).title, QStringLiteral("C"));
    QCOMPARE(o.achievements->nearlyDone.at(2).title, QStringLiteral("A"));
    QCOMPARE(o.achievements->nearlyDone.at(3).id, QStringLiteral("steam-11")); // 80/100 and 40/50 tie: by title
}

void LibraryOverviewTest::nearlyDoneLeavesOutBelowEightyAndPerfect()
{
    Game below = steamGame("steam-1", 1);
    Game exact = steamGame("steam-2", 2);
    Game perfect = steamGame("steam-3", 3);
    const LibraryOverview::AchievementCounts counts{{1, {79, 100}}, {2, {4, 5}}, {3, {5, 5}}};
    const LibraryOverview::Overview o = LibraryOverview::overview({below, exact, perfect}, {}, day(20), std::nullopt, counts);
    QCOMPARE(o.achievements->nearlyDone.size(), 1);
    QCOMPARE(o.achievements->nearlyDone.at(0).id, QStringLiteral("steam-2"));
    const QVariantMap row = LibraryOverview::toVariantMap(o).value("nearlyDone").toList().at(0).toMap();
    QCOMPARE(row.value("unlocked").toInt(), 4);
    QCOMPARE(row.value("total").toInt(), 5);
}

void LibraryOverviewTest::achievementsLeaveOutHiddenAndCountLinkedCopiesOnce()
{
    Game hidden = steamGame("steam-1", 1);
    hidden.hidden = true;
    Game main = steamGame("steam-2", 2);
    main.groupId = "steam-2";
    Game copy = steamGame("steam-3", 3);
    copy.groupId = "steam-2";
    const LibraryOverview::AchievementCounts counts{{1, {9, 10}}, {2, {3, 10}}, {3, {9, 10}}};

    const LibraryOverview::Overview o = LibraryOverview::overview({hidden, main, copy}, {}, day(20), std::nullopt, counts);
    QCOMPARE(o.achievements->games, 1);
    QCOMPARE(o.achievements->unlocked, 9); // the copy that was played
    QCOMPARE(o.achievements->total, 10);
    QCOMPARE(o.achievements->nearlyDone.size(), 1);
    QCOMPARE(o.achievements->nearlyDone.at(0).id, QStringLiteral("steam-3"));
}

QTEST_APPLESS_MAIN(LibraryOverviewTest)
#include "library_overview_test.moc"
