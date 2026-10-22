#pragma once

#include "parsed.h"

#include <QDate>
#include <QDateTime>
#include <QHash>
#include <QList>
#include <QMap>
#include <QString>
#include <QTimeZone>
#include <QVariantMap>

// See docs/code/library.md.
namespace PlayHistory {
struct Session
{
    QDateTime end;
    int minutes = 0;
    bool merged = false; // may hold several sessions (Steam's total grew while the launcher was closed)

    QDateTime start() const { return end.addSecs(-qint64(minutes) * 60); }
};

struct File
{
    QList<Session> sessions;
    int deletedMinutes = 0;
};

QString path(const QString &dataDir, const QString &id);
QString seenPath(const QString &dataDir);

Parsed<File> parse(const QByteArray &bytes, const QString &displayPath);
Parsed<File> read(const QString &dataDir, const QString &id);

QString append(const QString &dataDir, const QString &id, const Session &session);
QString remove(const QString &dataDir, const QString &id, const QDateTime &end, Session *removed);
QString setDeletedMinutes(const QString &dataDir, const QString &id, int minutes);
QString removeFile(const QString &dataDir, const QString &id);

using SeenTotals = QMap<QString, QMap<QString, int>>;

Parsed<SeenTotals> readSeen(const QString &dataDir);
QString writeSeen(const QString &dataDir, const SeenTotals &seen);

struct SteamPlayed
{
    QString id;
    int minutes = 0;       // Steam's total; -1 (never played) counts as 0
    qint64 lastPlayed = 0;
};

struct NewSession
{
    QString id;
    Session session;
};

struct SteamDiff
{
    QList<NewSession> sessions;
    QMap<QString, int> seen;
    bool changed = false;
};

// A game's total that went up becomes a session ending at `lastPlayed`; a game seen for the first
// time, or a total that went down, only updates the seen total.
SteamDiff diffSteam(const QMap<QString, int> &seen, const QList<SteamPlayed> &current, bool merged);

QVariantMap summary(const File &file, int total, const QDate &today, Qt::DayOfWeek firstDay, const QTimeZone &zone);

struct PageStats
{
    int sessions = 0;
    QDateTime first; // invalid without history
    int week = 0;
    int twoWeeks = 0;
    int month = 0;
    int year = 0;
};

PageStats pageStats(const File &file, const QDate &today, Qt::DayOfWeek firstDay, const QTimeZone &zone);

// The day (in `zone`) a session counts toward: the one it started on, or for a merged entry, whose
// start means nothing, the one it ended on.
QDate dayOf(const Session &session, const QTimeZone &zone);

struct StatsGame
{
    QString id;
    QString title;
    QString platform;
    QList<Session> sessions;
};

struct GameMinutes
{
    QString id;
    int minutes = 0;
};

struct DayStats
{
    int minutes = 0;
    int mergedMinutes = 0;
    int sessions = 0;
    QList<GameMinutes> games;
};

struct WeekStats
{
    int number = 0;
    int firstDay = 0;
    int lastDay = 0;
    int minutes = 0;
    int mergedMinutes = 0;
};

struct PlatformMinutes
{
    QString platform;
    int minutes = 0;
};

struct StripMonth
{
    int year = 0;
    int month = 0;
    int minutes = 0;
    bool hasHistory = false;
    bool pickable = false;
};

struct StripYear
{
    int year = 0;
    int minutes = 0;
};

struct HistoryMonth
{
    int year = 0;
    int month = 0;
    int minutes = 0;
};

struct PeriodStats
{
    QList<PlatformMinutes> platforms;
    QList<GameMinutes> top;

    int minutes = 0;
    int sessions = 0;
    int games = 0;
    int activeDays = 0;

    struct Longest
    {
        QString id;
        int minutes = 0;
        QDateTime end;
    } longest; // minutes 0: none (merged entries don't count)

    struct Streak
    {
        int days = 0;
        QDate first; // only days inside the range count, so it never runs in from outside
    } streak;

    // Merged entries may be several sessions, so they're left out unless the period has nothing else.
    struct Average
    {
        int minutes = 0;
        int sessions = 0;
    } average;
};

struct MonthStats : PeriodStats
{
    int year = 0;
    int month = 0;
    bool hasHistory = false;
    int firstYear = 0;
    int firstMonth = 0;

    QList<DayStats> days;
    QList<WeekStats> weeks;

    // The previous month's minutes: all of it, or for the current month the same days.
    int previousMinutes = 0;
    bool previousHasHistory = false;
    int comparedThrough = 0; // the last day of the previous month compared; 0 = the whole month

    QList<StripMonth> strip;
    QList<HistoryMonth> historyMonths;
};

struct MonthColumn
{
    int month = 0;
    int minutes = 0;
    int mergedMinutes = 0;
    int games = 0;
    QList<GameMinutes> top;
    bool pickable = false;
};

struct YearStats : PeriodStats
{
    int year = 0;
    bool hasHistory = false;
    int firstYear = 0;
    int firstMonth = 0;

    QList<MonthColumn> columns;
    QList<int> weekdays;

    // The previous year's minutes: all of it, or for the current year the same span.
    int previousMinutes = 0;
    bool previousHasHistory = false;
    QDate comparedThrough; // the last day of the previous year compared; invalid = the whole year

    QList<StripYear> years;
    QList<HistoryMonth> historyMonths;
};

void load(const QString &dataDir, QList<StatsGame> *games);

MonthStats monthStats(const QList<StatsGame> &games, int year, int month, const QDate &today,
                      Qt::DayOfWeek firstDay, const QTimeZone &zone);

YearStats yearStats(const QList<StatsGame> &games, int year, const QDate &today, Qt::DayOfWeek firstDay,
                    const QTimeZone &zone);

QVariantMap toVariantMap(const MonthStats &stats, const QHash<QString, QVariantMap> &gameInfo,
                         const QHash<QString, QVariantMap> &platformInfo);
QVariantMap toVariantMap(const YearStats &stats, const QHash<QString, QVariantMap> &gameInfo,
                         const QHash<QString, QVariantMap> &platformInfo);
}
