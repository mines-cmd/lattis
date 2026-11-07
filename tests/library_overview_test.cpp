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
    void gamesOnTwoDrives();
    void unknownSizesAreNotCounted();
    void otherFilesNeverBelowZero();
    void storageLeavesOutHiddenAndUninstalled();
    void linkedCopiesEachTakeSpace();
    void largestAndDormant();
    void storageVariantMap();
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

QTEST_APPLESS_MAIN(LibraryOverviewTest)
#include "library_overview_test.moc"
