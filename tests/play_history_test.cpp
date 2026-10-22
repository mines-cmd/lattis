#include "librarytestbase.h"
#include "dateformat.h"

namespace {

const QTimeZone kPlus2 = QTimeZone::fromSecondsAheadOfUtc(2 * 3600);

QDateTime at(int year, int month, int day, int hour, int minute, const QTimeZone &zone = kPlus2)
{
    return QDateTime(QDate(year, month, day), QTime(hour, minute), zone);
}

// The fixture in tests/data/history: October 2026 starts on a Thursday, so its weeks are clipped at
// the start with either first day of the week, and at the end with Monday.
QList<PlayHistory::StatsGame> fixtureGames()
{
    QList<PlayHistory::StatsGame> games{{QStringLiteral("switch-alpha"), QStringLiteral("Alpha"), QStringLiteral("switch"), {}},
                                        {QStringLiteral("steam-1"), QStringLiteral("Steam One"), QStringLiteral("steam"), {}},
                                        {QStringLiteral("linux-newbie"), QStringLiteral("Newbie"), QStringLiteral("linux"), {}},
                                        {QStringLiteral("switch-old"), QStringLiteral("Old"), QStringLiteral("switch"), {}}};
    PlayHistory::load(QStringLiteral(HISTORY_DATA_DIR), &games);
    return games;
}

// Steam writing a game's play time when it closes.
void setPlaytime(const QString &localConfig, qint64 appId, int minutes, qint64 lastPlayed)
{
    QByteArray text = readFile(localConfig);
    for (const auto &[key, value] : {std::pair{QStringLiteral("Playtime"), QString::number(minutes)},
                                     std::pair{QStringLiteral("LastPlayed"), QString::number(lastPlayed)}}) {
        const Vdf::EditResult r = Vdf::setValue(text,
                                                {QStringLiteral("UserLocalConfigStore"), QStringLiteral("Software"),
                                                 QStringLiteral("Valve"), QStringLiteral("Steam"), QStringLiteral("apps"),
                                                 QString::number(appId), key},
                                                value);
        QVERIFY(r.ok);
        text = r.text;
    }
    writeFile(localConfig, text);
}

} // namespace

class PlayHistoryTest : public LibraryTestBase
{
    Q_OBJECT

private slots:
    void appendAndReadBack();
    void malformedEntriesStay();
    void brokenFileIsNotTouched();
    void romSessionLandsInBothFiles();
    void shortSessionIsNotRecorded();
    void deleteRomSession();
    void deleteSteamSession();
    void beforeHistory();
    void removingAGameRemovesItsHistory();
    void mergingGamesMergesTheirHistory();
    void dayCountsWhereTheSessionStarted();
    void weeksAreClipped();
    void monthFromTheFixture();
    void currentMonthComparesTheSameDays();
    void stripIsThePickedYear();
    void historyMonthsAcrossYears();
    void yearFromTheFixture();
    void yearColumnsSplitMergedAndTopGames();
    void yearWeekdaysFollowTheFirstDay();
    void yearComparesTheSameSpan();
    void streaksCrossBoundaries();
    void yearStripSpansTheYearsWithHistory();
    void emptyMonths();
    void steamDiffRules();
    void seenTotalsRoundTrip();
    void steamPlayBecomesSessions();
    void steamUsersDontMix();
    void statsOnAWorkerThread();
    void pageStatsPerPeriod();
    void pageStatsWeekFollowsTheFirstDay();
    void pageStatsWithoutHistory();
    void pageHistoryFromTheLibrary();

private:
    QString historyFile(const QString &id) const { return PlayHistory::path(m_data, id); }
};

void PlayHistoryTest::appendAndReadBack()
{
    const QString id = QStringLiteral("switch-test");
    QVERIFY(!QFileInfo::exists(historyFile(id)));
    QCOMPARE(PlayHistory::append(m_data, id, {at(2026, 10, 12, 1, 10), 240, true}), QString());
    QCOMPARE(PlayHistory::append(m_data, id, {at(2026, 10, 10, 22, 41), 95, false}), QString());
    QCOMPARE(PlayHistory::append(m_data, id,
                                 {at(2026, 10, 11, 20, 0, QTimeZone::fromSecondsAheadOfUtc(-3 * 3600)), 30, false}),
             QString());

    QCOMPARE(readFile(historyFile(id)),
             QByteArray("[[session]]\nend = 2026-10-12T01:10:00+02:00\nminutes = 240\nmerged = true\n\n"
                        "[[session]]\nend = 2026-10-10T22:41:00+02:00\nminutes = 95\n\n"
                        "[[session]]\nend = 2026-10-11T20:00:00-03:00\nminutes = 30\n"));
    const Parsed<PlayHistory::File> file = PlayHistory::read(m_data, id);
    QVERIFY(file.ok);
    QVERIFY(file.warnings.isEmpty());
    // Oldest first, each with the offset it was written with.
    QCOMPARE(file.value.sessions.size(), 3);
    QCOMPARE(file.value.sessions.at(0).minutes, 95);
    QCOMPARE(file.value.sessions.at(0).end.offsetFromUtc(), 2 * 3600);
    QCOMPARE(file.value.sessions.at(1).end.offsetFromUtc(), -3 * 3600);
    QCOMPARE(file.value.sessions.at(2).merged, true);
    QCOMPARE(file.value.sessions.at(2).start(), at(2026, 10, 11, 21, 10));

    QVERIFY(PlayHistory::read(m_data, QStringLiteral("nothing-yet")).ok);
    QVERIFY(PlayHistory::read(m_data, QStringLiteral("nothing-yet")).value.sessions.isEmpty());
}

void PlayHistoryTest::malformedEntriesStay()
{
    const QString id = QStringLiteral("switch-test");
    writeFile(historyFile(id),
              "# mine\n[[session]]\nminutes = 5\n\n[[session]]\nend = 2026-10-10T22:41:00+02:00\nminutes = 10\n\n"
              "[[session]]\nend = \"yesterday\"\nminutes = 7\n");
    Parsed<PlayHistory::File> file = PlayHistory::read(m_data, id);
    QVERIFY(file.ok);
    QCOMPARE(file.value.sessions.size(), 1);
    QCOMPARE(file.warnings.size(), 2);
    QVERIFY(file.warnings.at(0).contains(QStringLiteral("switch-test.toml:2: skipped")));

    QCOMPARE(PlayHistory::append(m_data, id, {at(2026, 10, 11, 9, 0), 20, false}), QString());
    const QByteArray text = readFile(historyFile(id));
    QVERIFY(text.startsWith("# mine\n[[session]]\nminutes = 5\n"));
    QVERIFY(text.contains("end = \"yesterday\""));
    QVERIFY(text.endsWith("end = 2026-10-11T09:00:00+02:00\nminutes = 20\n"));

    // Deleting a session leaves the ones that can't be read.
    PlayHistory::Session removed;
    QCOMPARE(PlayHistory::remove(m_data, id, at(2026, 10, 10, 22, 41), &removed), QString());
    QCOMPARE(removed.minutes, 10);
    QVERIFY(!readFile(historyFile(id)).contains("minutes = 10"));
    QVERIFY(readFile(historyFile(id)).contains("end = \"yesterday\""));
    file = PlayHistory::read(m_data, id);
    QCOMPARE(file.value.sessions.size(), 1);
    QCOMPARE(file.value.sessions.at(0).minutes, 20);
}

void PlayHistoryTest::brokenFileIsNotTouched()
{
    const QString id = QStringLiteral("switch-test");
    writeFile(historyFile(id), "[[session\nend = ");
    QVERIFY(!PlayHistory::read(m_data, id).ok);
    QVERIFY(!PlayHistory::append(m_data, id, {at(2026, 10, 11, 9, 0), 20, false}).isEmpty());
    QVERIFY(!PlayHistory::remove(m_data, id, at(2026, 10, 11, 9, 0), nullptr).isEmpty());
    QVERIFY(!PlayHistory::setDeletedMinutes(m_data, id, 5).isEmpty());
    QCOMPARE(readFile(historyFile(id)), QByteArray("[[session\nend = "));
}

void PlayHistoryTest::romSessionLandsInBothFiles()
{
    auto library = openLibrary();
    const QString id = QStringLiteral("switch-mario-kart-8-deluxe");
    const QDateTime end = at(2026, 10, 10, 22, 41);
    QCOMPARE(library->recordPlaySession(id, 60, end), QString());

    const GameFile game = library->store().snapshot().content.games.value(id);
    QCOMPARE(game.playMinutes, 2460 + 60);
    QCOMPARE(game.lastPlayed, end);
    const Parsed<PlayHistory::File> file = PlayHistory::read(m_data, id);
    QCOMPARE(file.value.sessions.size(), 1);
    QCOMPARE(file.value.sessions.at(0).minutes, 60);
    QCOMPARE(file.value.sessions.at(0).end, end);
    QVERIFY(readFile(historyFile(id)).contains("end = 2026-10-10T22:41:00+02:00\n"));
    QVERIFY(!file.value.sessions.at(0).merged);
}

void PlayHistoryTest::shortSessionIsNotRecorded()
{
    auto library = openLibrary();
    const QString id = QStringLiteral("switch-mario-kart-8-deluxe");
    QCOMPARE(library->recordPlaySession(id, 0, at(2026, 10, 10, 22, 41)), QString());
    QVERIFY(!QFileInfo::exists(historyFile(id)));
}

void PlayHistoryTest::deleteRomSession()
{
    auto library = openLibrary();
    const QString id = QStringLiteral("switch-mario-kart-8-deluxe");
    const QDateTime first = at(2026, 10, 9, 20, 0);
    const QDateTime second = at(2026, 10, 10, 22, 41);
    QCOMPARE(library->recordPlaySession(id, 60, first), QString());
    QCOMPARE(library->recordPlaySession(id, 600, second), QString()); // left running overnight
    const auto game = [&] { return library->store().snapshot().content.games.value(id); };
    QCOMPARE(game().playMinutes, 2460 + 660);

    const QVariantMap deleted = library->deleteSession(id, second.toString(Qt::ISODate));
    QCOMPARE(deleted.value("error").toString(), QString());
    QCOMPARE(game().playMinutes, 2460 + 60);
    QCOMPARE(game().lastPlayed, first);
    QCOMPARE(PlayHistory::read(m_data, id).value.sessions.size(), 1);
    QCOMPARE(PlayHistory::read(m_data, id).value.deletedMinutes, 0);
    QCOMPARE(library->historyFor(id).value("before").toInt(), 2460);

    QCOMPARE(library->restoreSession(id, deleted.value("session").toMap()), QString());
    QCOMPARE(game().playMinutes, 2460 + 660);
    QCOMPARE(game().lastPlayed, second);
    const auto sessions = PlayHistory::read(m_data, id).value.sessions;
    QCOMPARE(sessions.size(), 2);
    QCOMPARE(sessions.at(1).end, second);
    QCOMPARE(sessions.at(1).minutes, 600);

    // Never below nothing, and the last session going leaves last_played where it was.
    QCOMPARE(library->setGameValue(id, QStringLiteral("play_minutes"), 100), QString());
    QCOMPARE(library->deleteSession(id, second.toString(Qt::ISODate)).value("error").toString(), QString());
    QCOMPARE(game().playMinutes, 0);
    QCOMPARE(library->deleteSession(id, first.toString(Qt::ISODate)).value("error").toString(), QString());
    QCOMPARE(game().playMinutes, 0);
    QCOMPARE(game().lastPlayed, first);
    QVERIFY(!QFileInfo::exists(historyFile(id)));

    QVERIFY(!library->deleteSession(id, first.toString(Qt::ISODate)).value("error").toString().isEmpty());
}

void PlayHistoryTest::deleteSteamSession()
{
    auto library = openLibrary();
    const QString id = QStringLiteral("steam-367520");
    const auto total = [&] { return library->gameForEditing(id).value("playMinutes").toInt(); };
    QCOMPARE(total(), 3300);
    QCOMPARE(PlayHistory::append(m_data, id, {at(2026, 10, 9, 20, 0), 100, false}), QString());
    QCOMPARE(PlayHistory::append(m_data, id, {at(2026, 10, 10, 22, 41), 50, false}), QString());
    QCOMPARE(library->historyFor(id).value("before").toInt(), 3150);

    const QVariantMap deleted = library->deleteSession(id, at(2026, 10, 10, 22, 41).toString(Qt::ISODate));
    QCOMPARE(deleted.value("error").toString(), QString());
    // Steam's total stays; what was deleted isn't turned into "Before history".
    QCOMPARE(total(), 3300);
    QCOMPARE(PlayHistory::read(m_data, id).value.deletedMinutes, 50);
    QCOMPARE(PlayHistory::read(m_data, id).value.sessions.size(), 1);
    QCOMPARE(library->historyFor(id).value("before").toInt(), 3150);
    QVERIFY(readFile(historyFile(id)).startsWith("deleted_minutes = 50\n"));
    QCOMPARE(library->historyFor(id).value("fromSteam").toBool(), true);

    QCOMPARE(library->restoreSession(id, deleted.value("session").toMap()), QString());
    QCOMPARE(total(), 3300);
    QCOMPARE(PlayHistory::read(m_data, id).value.deletedMinutes, 0);
    QVERIFY(!readFile(historyFile(id)).contains("deleted_minutes"));
    QCOMPARE(PlayHistory::read(m_data, id).value.sessions.size(), 2);
    QCOMPARE(library->historyFor(id).value("before").toInt(), 3150);

    // The only session going with a deleted total keeps the file, with just the total.
    QCOMPARE(library->deleteSession(id, at(2026, 10, 10, 22, 41).toString(Qt::ISODate)).value("error").toString(), QString());
    QCOMPARE(library->deleteSession(id, at(2026, 10, 9, 20, 0).toString(Qt::ISODate)).value("error").toString(), QString());
    QCOMPARE(readFile(historyFile(id)), QByteArray("deleted_minutes = 150\n"));
    QCOMPARE(library->historyFor(id).value("before").toInt(), 3150);
}

void PlayHistoryTest::beforeHistory()
{
    auto library = openLibrary();
    const QString id = QStringLiteral("switch-mario-kart-8-deluxe");
    // Play time from before the feature is all "before".
    QCOMPARE(library->historyFor(id).value("total").toInt(), 2460);
    QCOMPARE(library->historyFor(id).value("before").toInt(), 2460);
    QCOMPARE(library->recordPlaySession(id, 60, at(2026, 10, 10, 22, 41)), QString());
    QCOMPARE(library->historyFor(id).value("before").toInt(), 2460);

    // A hand edit of play_minutes moves "before" and leaves the history alone.
    QCOMPARE(library->setGameValue(id, QStringLiteral("play_minutes"), 3000), QString());
    QCOMPARE(library->historyFor(id).value("before").toInt(), 2940);
    QCOMPARE(library->historyFor(id).value("count").toInt(), 1);
    QCOMPARE(library->setGameValue(id, QStringLiteral("play_minutes"), 30), QString());
    QCOMPARE(library->historyFor(id).value("before").toInt(), 0);
    QVERIFY(library->historyFor(QStringLiteral("no-such-game")).isEmpty());
}

void PlayHistoryTest::removingAGameRemovesItsHistory()
{
    auto library = openLibrary();
    const QString id = QStringLiteral("linux-factorio");
    QCOMPARE(PlayHistory::append(m_data, id, {at(2026, 10, 9, 20, 0), 100, false}), QString());
    // Hiding keeps it.
    QCOMPARE(library->removeGame(id, false), QString());
    QVERIFY(QFileInfo::exists(historyFile(id)));
    // Removing a game that has no index entry deletes its file, and the history with it.
    QCOMPARE(library->removeGame(id, true), QString());
    QVERIFY(!QFileInfo::exists(gamePath(id)));
    QVERIFY(!QFileInfo::exists(historyFile(id)));
}

void PlayHistoryTest::mergingGamesMergesTheirHistory()
{
    auto library = openLibrary();
    const QString a = QStringLiteral("switch-mario-kart-8-deluxe");
    const QString b = QStringLiteral("switch-pikmin-4");
    const QString rom = m_dir->filePath(QStringLiteral("elsewhere/Pikmin 4.nsp"));
    writeFile(rom, "x");
    QCOMPARE(library->setGameValue(b, QStringLiteral("rom"), rom), QString());
    QCOMPARE(library->recordPlaySession(a, 60, at(2026, 10, 9, 20, 0)), QString());
    QCOMPARE(library->recordPlaySession(b, 30, at(2026, 10, 10, 20, 0)), QString());
    // Pointing a at the file b has: b merges into a.
    QCOMPARE(library->saveGame(a, {{"rom", rom}}), QString());
    QVERIFY(!QFileInfo::exists(gamePath(b)));
    QVERIFY(!QFileInfo::exists(historyFile(b)));
    QCOMPARE(PlayHistory::read(m_data, a).value.sessions.size(), 2);
}

void PlayHistoryTest::dayCountsWhereTheSessionStarted()
{
    // Ends at 01:30 on the 2nd, started at 23:30 on the 1st.
    const PlayHistory::Session overnight{at(2026, 10, 2, 1, 30), 120, false};
    QCOMPARE(PlayHistory::dayOf(overnight, kPlus2), QDate(2026, 10, 1));
    // The same session seen from ten hours further east happened on the 2nd.
    QCOMPARE(PlayHistory::dayOf(overnight, QTimeZone::fromSecondsAheadOfUtc(10 * 3600)), QDate(2026, 10, 2));
    // A merged entry's start means nothing (it may be days of play): it counts where it ended.
    const PlayHistory::Session merged{at(2026, 10, 20, 12, 0), 500, true};
    QCOMPARE(PlayHistory::dayOf(merged, kPlus2), QDate(2026, 10, 20));
}

void PlayHistoryTest::weeksAreClipped()
{
    const QList<PlayHistory::StatsGame> games = fixtureGames();
    const QDate today(2026, 11, 15);
    using Range = std::pair<int, int>;
    const auto ranges = [&](Qt::DayOfWeek first) -> QList<Range> {
        QList<Range> out;
        const PlayHistory::MonthStats m = PlayHistory::monthStats(games, 2026, 10, today, first, kPlus2);
        for (const PlayHistory::WeekStats &w : m.weeks) {
            if (w.number != int(out.size()) + 1)
                return QList<Range>();
            out.append({w.firstDay, w.lastDay});
        }
        return out;
    };
    QCOMPARE(ranges(Qt::Monday), (QList<Range>{{1, 4}, {5, 11}, {12, 18}, {19, 25}, {26, 31}}));
    QCOMPARE(ranges(Qt::Sunday), (QList<Range>{{1, 3}, {4, 10}, {11, 17}, {18, 24}, {25, 31}}));

    // February 2027 is exactly four Monday-first weeks, and five Sunday-first ones, the last a single day.
    const auto february = [&](Qt::DayOfWeek first) {
        return PlayHistory::monthStats({}, 2027, 2, today, first, kPlus2).weeks;
    };
    QCOMPARE(february(Qt::Monday).size(), 4);
    QCOMPARE(february(Qt::Sunday).size(), 5);
    QCOMPARE(february(Qt::Sunday).last().firstDay, 28);
    QCOMPARE(february(Qt::Sunday).last().lastDay, 28);
}

void PlayHistoryTest::monthFromTheFixture()
{
    const QList<PlayHistory::StatsGame> games = fixtureGames();
    const PlayHistory::MonthStats m =
        PlayHistory::monthStats(games, 2026, 10, QDate(2026, 11, 15), Qt::Monday, kPlus2);

    QVERIFY(m.hasHistory);
    QCOMPARE(m.firstYear, 2025);
    QCOMPARE(m.firstMonth, 12);
    QCOMPARE(m.minutes, 1505);
    QCOMPARE(m.sessions, 11);
    QCOMPARE(m.games, 4);

    QCOMPARE(m.days.size(), 31);
    const auto minutes = [&](int day) { return m.days.at(day - 1).minutes; };
    QCOMPARE(minutes(1), 180); // 19:00–20:00, and 23:30–01:30 that ended on the 2nd
    QCOMPARE(minutes(2), 90);
    QCOMPARE(minutes(3), 100);
    QCOMPARE(minutes(4), 0);
    QCOMPARE(minutes(10), 95);
    QCOMPARE(minutes(14), 300);
    QCOMPARE(minutes(20), 500);
    QCOMPARE(m.days.at(19).mergedMinutes, 500);
    QCOMPARE(m.days.at(19).sessions, 1);
    QCOMPARE(minutes(21), 180);
    QCOMPARE(minutes(22), 30);
    QCOMPARE(minutes(23), 30);
    QCOMPARE(m.days.at(20).games.size(), 2);
    QCOMPARE(m.days.at(20).games.at(0).id, QStringLiteral("steam-1"));
    QCOMPARE(m.days.at(20).games.at(0).minutes, 120);
    QCOMPARE(m.days.at(20).games.at(1).id, QStringLiteral("linux-newbie"));

    QCOMPARE(m.weeks.size(), 5);
    const QList<int> perWeek{370, 95, 300, 740, 0};
    for (int i = 0; i < 5; ++i)
        QCOMPARE(m.weeks.at(i).minutes, perWeek.at(i));
    QCOMPARE(m.weeks.at(3).mergedMinutes, 500);
    QCOMPARE(m.weeks.at(0).mergedMinutes, 0);

    QCOMPARE(m.platforms.size(), 3);
    QCOMPARE(m.platforms.at(0).platform, QStringLiteral("switch"));
    QCOMPARE(m.platforms.at(0).minutes, 795);
    QCOMPARE(m.platforms.at(1).platform, QStringLiteral("steam"));
    QCOMPARE(m.platforms.at(1).minutes, 650);
    QCOMPARE(m.platforms.at(2).platform, QStringLiteral("linux"));

    QCOMPARE(m.top.size(), 4);
    QCOMPARE(m.top.at(0).id, QStringLiteral("switch-alpha"));
    QCOMPARE(m.top.at(0).minutes, 765);
    QCOMPARE(m.top.at(1).id, QStringLiteral("steam-1"));
    QCOMPARE(m.top.at(1).minutes, 650);
    QCOMPARE(m.top.at(3).id, QStringLiteral("switch-old"));

    // Not the 500-minute merged entry, which may be several sessions.
    QCOMPARE(m.longest.id, QStringLiteral("switch-alpha"));
    QCOMPARE(m.longest.minutes, 300);
    QCOMPARE(m.longest.end, at(2026, 10, 14, 20, 0));

    // The 1st–3rd is three days; the 20th–23rd is four, across a merged entry's day.
    QCOMPARE(m.streak.days, 4);
    QCOMPARE(m.streak.first, QDate(2026, 10, 20));

    // The 500-minute merged entry is left out: 1005 minutes over 10 sessions.
    QCOMPARE(m.average.minutes, 101);
    QCOMPARE(m.average.sessions, 10);

    // September, whole: Steam One on the 5th, Alpha's 22:00–23:00 on the 30th.
    QVERIFY(m.previousHasHistory);
    QCOMPARE(m.previousMinutes, 160);
    QCOMPARE(m.comparedThrough, 0);

    const QVariantMap map = PlayHistory::toVariantMap(
        m, {{QStringLiteral("steam-1"), {{"title", "Steam One"}}}}, {{QStringLiteral("steam"), {{"name", "Steam"}}}});
    QCOMPARE(map.value("minutes").toInt(), 1505);
    QCOMPARE(map.value("days").toList().size(), 31);
    QCOMPARE(map.value("top").toList().at(1).toMap().value("title").toString(), QStringLiteral("Steam One"));
    QCOMPARE(map.value("platforms").toList().at(1).toMap().value("name").toString(), QStringLiteral("Steam"));
    QCOMPARE(map.value("longest").toMap().value("minutes").toInt(), 300);
    QCOMPARE(map.value("streakFirstDay").toInt(), 20);
}

void PlayHistoryTest::currentMonthComparesTheSameDays()
{
    const QList<PlayHistory::StatsGame> games = fixtureGames();
    // On the 25th: September 1–25 has only Steam One's 100 minutes; Alpha's session was on the 30th.
    PlayHistory::MonthStats m = PlayHistory::monthStats(games, 2026, 10, QDate(2026, 10, 25), Qt::Monday, kPlus2);
    QCOMPARE(m.comparedThrough, 25);
    QCOMPARE(m.previousMinutes, 100);
    QVERIFY(m.previousHasHistory);

    // The 31st of a month after a shorter one compares the whole of it.
    m = PlayHistory::monthStats(games, 2026, 10, QDate(2026, 10, 31), Qt::Monday, kPlus2);
    QCOMPARE(m.comparedThrough, 30);
    QCOMPARE(m.previousMinutes, 160);

    // A past month compares with the whole of the one before.
    m = PlayHistory::monthStats(games, 2026, 10, QDate(2026, 12, 1), Qt::Monday, kPlus2);
    QCOMPARE(m.comparedThrough, 0);
    QCOMPARE(m.previousMinutes, 160);

    // March 31st against a February of 28 days.
    m = PlayHistory::monthStats({}, 2027, 3, QDate(2027, 3, 31), Qt::Monday, kPlus2);
    QCOMPARE(m.comparedThrough, 28);
}

void PlayHistoryTest::stripIsThePickedYear()
{
    const QDate today(2026, 10, 25);
    const PlayHistory::MonthStats m = PlayHistory::monthStats(fixtureGames(), 2026, 10, today, Qt::Monday, kPlus2);
    QCOMPARE(m.strip.size(), 12);
    for (int i = 0; i < 12; ++i) {
        QCOMPARE(m.strip.at(i).year, 2026);
        QCOMPARE(m.strip.at(i).month, i + 1);
    }
    // 23:50 on New Year's Eve counts for December, so January has nothing.
    QCOMPARE(m.strip.at(0).minutes, 0);
    QVERIFY(!m.strip.at(0).hasHistory);
    QCOMPARE(m.strip.at(8).minutes, 160); // September
    QVERIFY(m.strip.at(8).hasHistory);
    QCOMPARE(m.strip.at(9).minutes, 1505);
    QVERIFY(m.strip.at(9).pickable);
    // Months after the current one are empty slots that can't be picked.
    QVERIFY(!m.strip.at(10).pickable);
    QVERIFY(!m.strip.at(11).pickable);
    QVERIFY(!m.strip.at(10).hasHistory);

    // The year before: December has history, the months before the first session have none but can be picked.
    const PlayHistory::MonthStats other = PlayHistory::monthStats(fixtureGames(), 2025, 12, today, Qt::Monday, kPlus2);
    QCOMPARE(other.strip.first().year, 2025);
    QCOMPARE(other.strip.last().month, 12);
    QCOMPARE(other.strip.last().minutes, 90);
    QVERIFY(other.strip.last().hasHistory);
    QVERIFY(!other.strip.first().hasHistory);
    QVERIFY(other.strip.first().pickable);
    QCOMPARE(other.minutes, 90);

    // A click on a bar never moves the strip: every month of a year gives the same one.
    const PlayHistory::MonthStats sameYear = PlayHistory::monthStats(fixtureGames(), 2026, 3, today, Qt::Monday, kPlus2);
    QCOMPARE(sameYear.strip.first().year, 2026);
    QCOMPARE(sameYear.strip.at(9).minutes, 1505);
}

void PlayHistoryTest::historyMonthsAcrossYears()
{
    const QDate today(2026, 10, 25);
    const PlayHistory::MonthStats m = PlayHistory::monthStats(fixtureGames(), 2026, 10, today, Qt::Monday, kPlus2);
    QCOMPARE(m.historyMonths.size(), 3);
    QCOMPARE(m.historyMonths.at(0).year, 2025);
    QCOMPARE(m.historyMonths.at(0).month, 12);
    QCOMPARE(m.historyMonths.at(0).minutes, 90);
    QCOMPARE(m.historyMonths.at(1).month, 9);
    QCOMPARE(m.historyMonths.at(1).minutes, 160);
    QCOMPARE(m.historyMonths.at(2).minutes, 1505);

    const PlayHistory::YearStats y = PlayHistory::yearStats(fixtureGames(), 2024, today, Qt::Monday, kPlus2);
    QCOMPARE(y.historyMonths.size(), 3);
    QCOMPARE(y.historyMonths.at(0).year, 2025);

    const QVariantMap map = PlayHistory::toVariantMap(m, {}, {});
    QCOMPARE(map.value("historyMonths").toList().size(), 3);
    QCOMPARE(map.value("historyMonths").toList().at(2).toMap().value("minutes").toInt(), 1505);
    QVERIFY(map.value("strip").toList().at(10).toMap().contains("pickable"));
    QVERIFY(PlayHistory::toVariantMap(y, {}, {}).contains("historyMonths"));

    QVERIFY(PlayHistory::monthStats({}, 2026, 10, today, Qt::Monday, kPlus2).historyMonths.isEmpty());
}

void PlayHistoryTest::yearFromTheFixture()
{
    const QList<PlayHistory::StatsGame> games = fixtureGames();
    const PlayHistory::YearStats y = PlayHistory::yearStats(games, 2026, QDate(2026, 10, 25), Qt::Monday, kPlus2);
    QVERIFY(y.hasHistory);
    QCOMPARE(y.firstYear, 2025);
    QCOMPARE(y.firstMonth, 12);
    // September's 160 and October's 1505; December 2025 is another year.
    QCOMPARE(y.minutes, 1665);
    QCOMPARE(y.sessions, 13);
    QCOMPARE(y.games, 4);
    QCOMPARE(y.activeDays, 11);
    QCOMPARE(y.platforms.size(), 3);
    QCOMPARE(y.platforms.at(0).platform, QStringLiteral("switch"));
    QCOMPARE(y.platforms.at(0).minutes, 855);
    QCOMPARE(y.top.at(0).id, QStringLiteral("switch-alpha"));
    QCOMPARE(y.top.at(0).minutes, 825);
    QCOMPARE(y.top.at(1).id, QStringLiteral("steam-1"));
    QCOMPARE(y.top.at(1).minutes, 750);
    QCOMPARE(y.longest.id, QStringLiteral("switch-alpha"));
    QCOMPARE(y.longest.minutes, 300);
    // 13 sessions, 1165 minutes without the 500-minute merged one: 12 plain ones.
    QCOMPARE(y.average.sessions, 12);
    QCOMPARE(y.average.minutes, 97);

    // The month stats see the same October as before.
    const PlayHistory::MonthStats m = PlayHistory::monthStats(games, 2026, 10, QDate(2026, 10, 25), Qt::Monday, kPlus2);
    QCOMPARE(m.minutes, 1505);

    const QVariantMap map = PlayHistory::toVariantMap(y, {}, {});
    QCOMPARE(map.value("minutes").toInt(), 1665);
    QCOMPARE(map.value("columns").toList().size(), 12);
    QCOMPARE(map.value("weekdays").toList().size(), 7);
    QCOMPARE(map.value("streakFirstMonth").toInt(), 9);
    QCOMPARE(map.value("streakFirstDay").toInt(), 30);
}

void PlayHistoryTest::yearColumnsSplitMergedAndTopGames()
{
    const QDate today(2026, 10, 25);
    const PlayHistory::YearStats y = PlayHistory::yearStats(fixtureGames(), 2026, today, Qt::Monday, kPlus2);
    QCOMPARE(y.columns.size(), 12);
    for (int i = 0; i < 12; ++i)
        QCOMPARE(y.columns.at(i).month, i + 1);
    QCOMPARE(y.columns.at(0).minutes, 0);
    QVERIFY(y.columns.at(0).top.isEmpty());

    const PlayHistory::MonthColumn &september = y.columns.at(8);
    QCOMPARE(september.minutes, 160);
    QCOMPARE(september.mergedMinutes, 0);
    QCOMPARE(september.games, 2);
    QCOMPARE(september.top.at(0).id, QStringLiteral("steam-1"));

    const PlayHistory::MonthColumn &october = y.columns.at(9);
    QCOMPARE(october.minutes, 1505);
    QCOMPARE(october.mergedMinutes, 500);
    QCOMPARE(october.games, 4);
    QCOMPARE(october.top.size(), 3); // the fourth game is "+1 more"
    QCOMPARE(october.top.at(0).id, QStringLiteral("switch-alpha"));
    QCOMPARE(october.top.at(0).minutes, 765);
    QCOMPARE(october.top.at(1).id, QStringLiteral("steam-1"));
    QCOMPARE(october.top.at(2).id, QStringLiteral("linux-newbie"));

    // Pickable from the first session's month (December 2025) to the current one.
    QVERIFY(y.columns.at(0).pickable);
    QVERIFY(y.columns.at(9).pickable);
    QVERIFY(!y.columns.at(10).pickable);
    const PlayHistory::YearStats before = PlayHistory::yearStats(fixtureGames(), 2025, today, Qt::Monday, kPlus2);
    QVERIFY(!before.columns.at(10).pickable);
    QVERIFY(before.columns.at(11).pickable);
    QCOMPARE(before.columns.at(11).minutes, 90);

    for (const PlayHistory::MonthColumn &c : PlayHistory::yearStats({}, 2026, today, Qt::Monday, kPlus2).columns)
        QVERIFY(!c.pickable);
}

void PlayHistoryTest::yearWeekdaysFollowTheFirstDay()
{
    const QDate today(2026, 10, 25);
    const auto weekdays = [&](Qt::DayOfWeek first) {
        return PlayHistory::yearStats(fixtureGames(), 2026, today, first, kPlus2).weekdays;
    };
    // Each session on the weekday it started; the merged entry on the Tuesday it ended.
    QCOMPARE(weekdays(Qt::Monday), (QList<int>{0, 500, 540, 210, 120, 295, 0}));
    QCOMPARE(weekdays(Qt::Sunday), (QList<int>{0, 0, 500, 540, 210, 120, 295}));
    QCOMPARE(PlayHistory::yearStats({}, 2026, today, Qt::Monday, kPlus2).weekdays, (QList<int>(7, 0)));
}

void PlayHistoryTest::yearComparesTheSameSpan()
{
    const QList<PlayHistory::StatsGame> games{
        {QStringLiteral("a"), QStringLiteral("A"), QStringLiteral("switch"),
         {{at(2025, 3, 10, 20, 0), 60, false}, {at(2025, 10, 20, 20, 0), 120, false}, {at(2025, 10, 27, 20, 0), 30, false},
          {at(2025, 11, 20, 20, 0), 500, false}, {at(2026, 2, 3, 20, 0), 90, false}}}};

    // 27 October 2026: the previous year up to 27 October (60 + 120 + 30), not its November.
    PlayHistory::YearStats y = PlayHistory::yearStats(games, 2026, QDate(2026, 10, 27), Qt::Monday, kPlus2);
    QCOMPARE(y.comparedThrough, QDate(2025, 10, 27));
    QCOMPARE(y.previousMinutes, 210);
    QVERIFY(y.previousHasHistory);

    // Earlier in the year, less of the previous one is compared.
    y = PlayHistory::yearStats(games, 2026, QDate(2026, 10, 25), Qt::Monday, kPlus2);
    QCOMPARE(y.previousMinutes, 180);

    // 29 February has no counterpart.
    y = PlayHistory::yearStats(games, 2028, QDate(2028, 2, 29), Qt::Monday, kPlus2);
    QCOMPARE(y.comparedThrough, QDate(2027, 2, 28));

    // A past year compares with the whole of the one before.
    y = PlayHistory::yearStats(games, 2026, QDate(2027, 2, 1), Qt::Monday, kPlus2);
    QVERIFY(!y.comparedThrough.isValid());
    QCOMPARE(y.previousMinutes, 710);
    QCOMPARE(y.minutes, 90);

    // The first year has no previous history.
    y = PlayHistory::yearStats(games, 2025, QDate(2026, 10, 27), Qt::Monday, kPlus2);
    QVERIFY(!y.previousHasHistory);
    QCOMPARE(y.previousMinutes, 0);
    QCOMPARE(y.minutes, 710);
    QCOMPARE(y.firstYear, 2025);
    QCOMPARE(y.firstMonth, 3);
}

void PlayHistoryTest::streaksCrossBoundaries()
{
    // Each session starts on the day it ends on (20:00, an hour long): 30 Dec to 2 Jan.
    const QList<PlayHistory::StatsGame> games{
        {QStringLiteral("a"), QStringLiteral("A"), QStringLiteral("switch"),
         {{at(2026, 12, 30, 21, 0), 60, false}, {at(2026, 12, 31, 21, 0), 60, false},
          {at(2027, 1, 1, 21, 0), 60, false}, {at(2027, 1, 2, 21, 0), 60, false}}}};
    const QDate today(2027, 3, 1);

    // The month's run is its own days only.
    const PlayHistory::MonthStats december = PlayHistory::monthStats(games, 2026, 12, today, Qt::Monday, kPlus2);
    QCOMPARE(december.streak.days, 2);
    QCOMPARE(december.streak.first, QDate(2026, 12, 30));
    const PlayHistory::MonthStats january = PlayHistory::monthStats(games, 2027, 1, today, Qt::Monday, kPlus2);
    QCOMPARE(january.streak.days, 2);
    QCOMPARE(january.streak.first, QDate(2027, 1, 1));

    // The year counts only the days inside it, so the run that goes on into January is cut at the 31st.
    const PlayHistory::YearStats y2026 = PlayHistory::yearStats(games, 2026, today, Qt::Monday, kPlus2);
    QCOMPARE(y2026.streak.days, 2);
    QCOMPARE(y2026.streak.first, QDate(2026, 12, 30));
    const PlayHistory::YearStats y2027 = PlayHistory::yearStats(games, 2027, today, Qt::Monday, kPlus2);
    QCOMPARE(y2027.streak.days, 2);
    QCOMPARE(y2027.streak.first, QDate(2027, 1, 1));

    // Across a month boundary inside a year (the fixture's 30 September to 3 October), and the earliest wins a tie.
    const PlayHistory::YearStats fixture = PlayHistory::yearStats(fixtureGames(), 2026, today, Qt::Monday, kPlus2);
    QCOMPARE(fixture.streak.days, 4);
    QCOMPARE(fixture.streak.first, QDate(2026, 9, 30));

    QCOMPARE(PlayHistory::yearStats({}, 2026, today, Qt::Monday, kPlus2).streak.days, 0);
}

void PlayHistoryTest::yearStripSpansTheYearsWithHistory()
{
    const QList<PlayHistory::StatsGame> games{
        {QStringLiteral("a"), QStringLiteral("A"), QStringLiteral("switch"),
         {{at(2023, 6, 1, 20, 0), 60, false}, {at(2023, 7, 1, 20, 0), 30, false}, {at(2025, 2, 1, 20, 0), 45, false}}}};
    const PlayHistory::YearStats y = PlayHistory::yearStats(games, 2025, QDate(2026, 10, 25), Qt::Monday, kPlus2);
    QCOMPARE(y.years.size(), 4); // 2023 to 2026, with nothing in 2024 and 2026
    QCOMPARE(y.years.at(0).year, 2023);
    QCOMPARE(y.years.at(0).minutes, 90);
    QCOMPARE(y.years.at(1).minutes, 0);
    QCOMPARE(y.years.at(2).minutes, 45);
    QCOMPARE(y.years.at(3).year, 2026);
    QCOMPARE(y.firstYear, 2023);
    QCOMPARE(y.firstMonth, 6);

    // Months before the first session can't be picked, nor can those after the current month.
    const PlayHistory::YearStats first = PlayHistory::yearStats(games, 2023, QDate(2026, 10, 25), Qt::Monday, kPlus2);
    QVERIFY(!first.columns.at(4).pickable);
    QVERIFY(first.columns.at(5).pickable);

    QVERIFY(PlayHistory::yearStats({}, 2026, QDate(2026, 10, 25), Qt::Monday, kPlus2).years.isEmpty());
    QCOMPARE(PlayHistory::toVariantMap(y, {}, {}).value("years").toList().size(), 4);
}

void PlayHistoryTest::emptyMonths()
{
    PlayHistory::MonthStats m = PlayHistory::monthStats({}, 2026, 10, QDate(2026, 10, 25), Qt::Monday, kPlus2);
    QVERIFY(!m.hasHistory);
    QCOMPARE(m.firstYear, 0);
    QCOMPARE(m.minutes, 0);
    QCOMPARE(m.average.minutes, 0);
    QCOMPARE(m.longest.minutes, 0);

    // A month with none, in a library that has some.
    m = PlayHistory::monthStats(fixtureGames(), 2026, 8, QDate(2026, 10, 25), Qt::Monday, kPlus2);
    QVERIFY(m.hasHistory);
    QCOMPARE(m.minutes, 0);
    QCOMPARE(m.sessions, 0);
    QVERIFY(!m.previousHasHistory);
    QVERIFY(m.platforms.isEmpty());
    QVERIFY(m.top.isEmpty());
    QCOMPARE(m.streak.days, 0);

    // Only merged entries: the average falls back to them rather than being empty.
    const QList<PlayHistory::StatsGame> steamOnly{
        {QStringLiteral("steam-1"), QStringLiteral("Steam One"), QStringLiteral("steam"),
         {{at(2026, 10, 5, 22, 0), 240, true}, {at(2026, 10, 12, 22, 0), 60, true}}}};
    m = PlayHistory::monthStats(steamOnly, 2026, 10, QDate(2026, 10, 25), Qt::Monday, kPlus2);
    QCOMPARE(m.longest.minutes, 0);
    QCOMPARE(m.average.minutes, 150);
    QCOMPARE(m.average.sessions, 2);
}

void PlayHistoryTest::steamDiffRules()
{
    using PlayHistory::SteamPlayed;
    const qint64 t1 = 1790000000;

    // First sight: the total only.
    PlayHistory::SteamDiff d = PlayHistory::diffSteam({}, {SteamPlayed{"steam-1", 100, t1}}, false);
    QVERIFY(d.changed);
    QVERIFY(d.sessions.isEmpty());
    QCOMPARE(d.seen.value("steam-1"), 100);

    // A total that grew: one session, ending at LastPlayed.
    d = PlayHistory::diffSteam({{"steam-1", 100}}, {SteamPlayed{"steam-1", 160, t1}}, false);
    QVERIFY(d.changed);
    QCOMPARE(d.sessions.size(), 1);
    QCOMPARE(d.sessions.at(0).id, QStringLiteral("steam-1"));
    QCOMPARE(d.sessions.at(0).session.minutes, 60);
    QCOMPARE(d.sessions.at(0).session.end.toSecsSinceEpoch(), t1);
    QVERIFY(!d.sessions.at(0).session.merged);
    QCOMPARE(d.seen.value("steam-1"), 160);

    // The startup scan's change may be several sessions.
    d = PlayHistory::diffSteam({{"steam-1", 100}}, {SteamPlayed{"steam-1", 160, t1}}, true);
    QVERIFY(d.sessions.at(0).session.merged);

    // Nothing new.
    d = PlayHistory::diffSteam({{"steam-1", 100}}, {SteamPlayed{"steam-1", 100, t1}}, false);
    QVERIFY(!d.changed);
    QVERIFY(d.sessions.isEmpty());

    // A total that went down (a reset, another account's data): remembered, no session.
    d = PlayHistory::diffSteam({{"steam-1", 100}}, {SteamPlayed{"steam-1", 40, t1}}, false);
    QVERIFY(d.changed);
    QVERIFY(d.sessions.isEmpty());
    QCOMPARE(d.seen.value("steam-1"), 40);

    // Never played counts as 0, so the first session of a new game is seen.
    d = PlayHistory::diffSteam({}, {SteamPlayed{"steam-2", -1, 0}}, false);
    QCOMPARE(d.seen.value("steam-2"), 0);
    d = PlayHistory::diffSteam(d.seen, {SteamPlayed{"steam-2", 30, t1}}, false);
    QCOMPARE(d.sessions.size(), 1);
    QCOMPARE(d.sessions.at(0).session.minutes, 30);

    // A total that grew without a LastPlayed has no time to put the session at.
    d = PlayHistory::diffSteam({{"steam-3", 10}}, {SteamPlayed{"steam-3", 20, 0}}, false);
    QVERIFY(d.sessions.isEmpty());
    QCOMPARE(d.seen.value("steam-3"), 20);

    // Games that dropped out of the list keep their totals.
    d = PlayHistory::diffSteam({{"steam-4", 5}}, {SteamPlayed{"steam-1", 1, t1}}, false);
    QCOMPARE(d.seen.value("steam-4"), 5);
}

void PlayHistoryTest::seenTotalsRoundTrip()
{
    QVERIFY(PlayHistory::readSeen(m_data).ok);
    QVERIFY(PlayHistory::readSeen(m_data).value.isEmpty());
    const PlayHistory::SeenTotals seen{{"39734273", {{"steam-1", 100}, {"proton-my-game", 7}}}, {"39734274", {{"steam-1", 5}}}};
    QCOMPARE(PlayHistory::writeSeen(m_data, seen), QString());
    const Parsed<PlayHistory::SeenTotals> back = PlayHistory::readSeen(m_data);
    QVERIFY(back.ok);
    QVERIFY(back.warnings.isEmpty());
    QCOMPARE(back.value, seen);

    writeFile(PlayHistory::seenPath(m_data), "[\"1\"\n");
    QVERIFY(!PlayHistory::readSeen(m_data).ok);
}

void PlayHistoryTest::steamPlayBecomesSessions()
{
    const QString localConfig = m_data + QStringLiteral("/steam/userdata/39734273/config/localconfig.vdf");
    const QString id = QStringLiteral("steam-367520");
    const auto seen = [&] { return PlayHistory::readSeen(m_data).value.value("39734273").value(id, -1); };
    const auto rescan = [](Library &library) {
        library.store().refresh();
        library.store().waitForIdle();
        QCoreApplication::processEvents();
    };

    auto library = openLibrary();
    // Every game seen for the first time: its total, no session.
    QCOMPARE(seen(), 3300);
    QVERIFY(!QFileInfo::exists(historyFile(id)));
    QCOMPARE(library->historyFor(id).value("before").toInt(), 3300);

    // Closed in Steam while the launcher runs: one session at LastPlayed.
    setPlaytime(localConfig, 367520, 3400, 1790400000);
    rescan(*library);
    PlayHistory::File file = PlayHistory::read(m_data, id).value;
    QCOMPARE(file.sessions.size(), 1);
    QCOMPARE(file.sessions.at(0).minutes, 100);
    QCOMPARE(file.sessions.at(0).end.toSecsSinceEpoch(), qint64(1790400000));
    QVERIFY(!file.sessions.at(0).merged);
    QCOMPARE(seen(), 3400);
    QCOMPARE(library->historyFor(id).value("before").toInt(), 3300);
    // The game file isn't touched: the play time is Steam's.
    QVERIFY(!QFileInfo::exists(gamePath(id)) || !readFile(gamePath(id)).contains("play_minutes"));

    // A total that went down.
    setPlaytime(localConfig, 367520, 3000, 1790400000);
    rescan(*library);
    QCOMPARE(PlayHistory::read(m_data, id).value.sessions.size(), 1);
    QCOMPARE(seen(), 3000);

    // Played while the launcher was closed: the first scan of the next run can't tell how many.
    library.reset();
    setPlaytime(localConfig, 367520, 3100, 1790500000);
    library = openLibrary();
    file = PlayHistory::read(m_data, id).value;
    QCOMPARE(file.sessions.size(), 2);
    QCOMPARE(file.sessions.at(1).minutes, 100);
    QVERIFY(file.sessions.at(1).merged);
    QCOMPARE(seen(), 3100);

    // Later changes in that run are live again.
    setPlaytime(localConfig, 367520, 3130, 1790600000);
    rescan(*library);
    file = PlayHistory::read(m_data, id).value;
    QCOMPARE(file.sessions.size(), 3);
    QCOMPARE(file.sessions.at(2).minutes, 30);
    QVERIFY(!file.sessions.at(2).merged);

    // A Proton game's total is its shortcut's.
    const QString proton = QStringLiteral("proton-final-fantasy-xiv-online");
    QVERIFY(PlayHistory::readSeen(m_data).value.value("39734273").contains(proton));
    QVERIFY(!QFileInfo::exists(historyFile(proton)));
    setPlaytime(localConfig, qint64(-1836832030), 16045, 1790700000);
    rescan(*library);
    file = PlayHistory::read(m_data, proton).value;
    QCOMPARE(file.sessions.size(), 1);
    QCOMPARE(file.sessions.at(0).minutes, 45);
    const quint32 shortcut = quint32(library->store().snapshot().content.games.value(proton).shortcut);
    qint64 lastPlayTime = 0;
    for (const SteamShortcut &s : library->store().snapshot().steam.shortcuts) {
        if (s.appId == shortcut)
            lastPlayTime = s.lastPlayTime;
    }
    QVERIFY(lastPlayTime > 0);
    QCOMPARE(file.sessions.at(0).end.toSecsSinceEpoch(), lastPlayTime);
}

void PlayHistoryTest::steamUsersDontMix()
{
    installFakeSteam();
    const QString id = QStringLiteral("steam-2231450");
    // The second account has 12 minutes in this game; the first (the most recent login), 4085.
    writeFile(m_data + QStringLiteral("/settings.toml"), "[steam]\nuser = \"39734274\"\n");
    auto library = openLibrary();
    auto seen = PlayHistory::readSeen(m_data).value;
    QCOMPARE(seen.value("39734274").value(id), 12);
    QVERIFY(!seen.contains("39734273"));

    // Back to the first: its own section is new, so the difference isn't played time.
    writeFile(m_data + QStringLiteral("/settings.toml"), "[steam]\n");
    library->store().refresh();
    library->store().waitForIdle();
    QCoreApplication::processEvents();
    seen = PlayHistory::readSeen(m_data).value;
    QCOMPARE(seen.value("39734273").value(id), 4085);
    QCOMPARE(seen.value("39734274").value(id), 12);
    QVERIFY(!QFileInfo::exists(historyFile(id)));
}

void PlayHistoryTest::statsOnAWorkerThread()
{
    auto library = openLibrary();
    const QString rom = QStringLiteral("switch-mario-kart-8-deluxe");
    const QDateTime now = QDateTime::currentDateTime();
    QCOMPARE(library->recordPlaySession(rom, 90, now), QString());
    QCOMPARE(library->recordPlaySession(QStringLiteral("switch-pikmin-4"), 30, now), QString());
    QCOMPARE(library->setGameValue(QStringLiteral("switch-pikmin-4"), QStringLiteral("hidden"), true), QString());

    QSignalSpy ready(library.get(), &Library::monthStatsReady);
    library->monthStats(now.date().year(), now.date().month());
    QTRY_COMPARE_WITH_TIMEOUT(ready.size(), 1, 5000);
    QCOMPARE(ready.at(0).at(0).toInt(), now.date().year());
    const QVariantMap stats = ready.at(0).at(2).toMap();
    // The hidden game is left out, unless hidden games show.
    QCOMPARE(stats.value("minutes").toInt(), 90);
    QCOMPARE(stats.value("games").toInt(), 1);
    const QVariantMap top = stats.value("top").toList().at(0).toMap();
    QCOMPARE(top.value("id").toString(), rom);
    QCOMPARE(top.value("title").toString(), QStringLiteral("Mario Kart 8 Deluxe"));
    QCOMPARE(stats.value("platforms").toList().at(0).toMap().value("name").toString(), QStringLiteral("Switch"));

    library->setShowHidden(true);
    library->monthStats(now.date().year(), now.date().month());
    QTRY_COMPARE_WITH_TIMEOUT(ready.size(), 2, 5000);
    QCOMPARE(ready.at(1).at(2).toMap().value("minutes").toInt(), 120);
}

namespace {
// Today is Wednesday 21 October 2026: a Monday-first week starts on the 19th, a Sunday-first one on the
// 18th, and the last two weeks on the 8th.
const QDate kPageToday(2026, 10, 21);

PlayHistory::File pageFile()
{
    PlayHistory::File file;
    file.sessions = {{at(2025, 12, 31, 20, 0), 60, false},
                     {at(2026, 9, 30, 22, 0), 30, false},
                     {at(2026, 10, 7, 12, 0), 20, false},
                     {at(2026, 10, 8, 12, 0), 10, false},
                     // Started at 23:00 on Sunday the 18th.
                     {at(2026, 10, 19, 1, 0), 120, false},
                     {at(2026, 10, 20, 12, 0), 500, true}};
    return file;
}
}

void PlayHistoryTest::pageStatsPerPeriod()
{
    const PlayHistory::PageStats s = PlayHistory::pageStats(pageFile(), kPageToday, Qt::Monday, kPlus2);
    // The merged entry may be several sessions, but it's one entry.
    QCOMPARE(s.sessions, 6);
    QCOMPARE(s.first, at(2025, 12, 31, 19, 0));
    QCOMPARE(s.week, 500);
    QCOMPARE(s.twoWeeks, 10 + 120 + 500);
    QCOMPARE(s.month, 20 + 10 + 120 + 500);
    QCOMPARE(s.year, 30 + 20 + 10 + 120 + 500);

    // A merged entry's start means nothing, so the first time played is its end.
    PlayHistory::File merged;
    merged.sessions = {{at(2026, 10, 20, 12, 0), 500, true}};
    QCOMPARE(PlayHistory::pageStats(merged, kPageToday, Qt::Monday, kPlus2).first, at(2026, 10, 20, 12, 0));
}

void PlayHistoryTest::pageStatsWeekFollowsTheFirstDay()
{
    // The overnight session counts on the Sunday it started: in a Sunday-first week, not a Monday-first one.
    QCOMPARE(PlayHistory::pageStats(pageFile(), kPageToday, Qt::Monday, kPlus2).week, 500);
    QCOMPARE(PlayHistory::pageStats(pageFile(), kPageToday, Qt::Sunday, kPlus2).week, 120 + 500);
    // Ten hours further east it started on Monday the 19th.
    QCOMPARE(PlayHistory::pageStats(pageFile(), kPageToday, Qt::Monday, QTimeZone::fromSecondsAheadOfUtc(10 * 3600)).week,
             120 + 500);
}

void PlayHistoryTest::pageStatsWithoutHistory()
{
    const PlayHistory::PageStats s = PlayHistory::pageStats({}, kPageToday, Qt::Monday, kPlus2);
    QCOMPARE(s.sessions, 0);
    QVERIFY(!s.first.isValid());
    QCOMPARE(s.week + s.twoWeeks + s.month + s.year, 0);
}

void PlayHistoryTest::pageHistoryFromTheLibrary()
{
    auto library = openLibrary();
    const QString rom = QStringLiteral("switch-mario-kart-8-deluxe");
    const QDateTime now = QDateTime::currentDateTime();
    QCOMPARE(library->recordPlaySession(rom, 90, now), QString());

    const QVariantMap played = library->pageHistory(rom);
    QCOMPARE(played.value("timesPlayed").toString(), QStringLiteral("1"));
    QCOMPARE(played.value("firstPlayed").toString(), DateFormat::shortDate(now.addSecs(-90 * 60).date()));
    const QVariantMap recent = played.value("recent").toMap();
    QCOMPARE(recent.keys(), (QStringList{"month", "two_weeks", "week", "year"}));
    QVERIFY(!recent.value("two_weeks").toString().isEmpty());

    const QVariantMap never = library->pageHistory(QStringLiteral("switch-pikmin-4"));
    QCOMPARE(never.value("timesPlayed").toString(), QString());
    QCOMPARE(never.value("firstPlayed").toString(), QString());
    QCOMPARE(never.value("recent").toMap().value("year").toString(), QString());
}

QTEST_GUILESS_MAIN(PlayHistoryTest)
#include "play_history_test.moc"
