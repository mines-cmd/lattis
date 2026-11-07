#pragma once

#include <QDateTime>
#include <QList>
#include <QString>
#include <QStringList>
#include <QVariantMap>

// See docs/code/library.md.
namespace LibraryOverview {
struct Game
{
    QString id;
    QString title;
    QString platform;
    bool hidden = false;
    bool installed = true;
    QString status; // as in the game file; empty = none
    QString groupId; // empty: not linked to other copies
    int playMinutes = 0;
    QDateTime lastPlayed;
    int groupPlayMinutes = 0;
    QDateTime groupLastPlayed;
    QString coverUrl;
    qint64 sizeBytes = 0; // 0 = unknown
    QString path; // the folder (or ROM) it takes space in, resolved; empty = nowhere known
    bool uninstallable = false;
    QStringList tags;
    int year = 0; // 0 = unknown
};

struct Drive
{
    QString device; // copies of one drive (btrfs subvolumes, bind mounts) share it
    QString name;
    QString root;
    qint64 total = 0;
    qint64 free = 0;
};

struct Counted
{
    QString id;
    QString title;
    QString coverUrl;
    QDateTime lastPlayed;
};

struct PlatformSize
{
    QString platform;
    qint64 bytes = 0;
    int unknown = 0; // installed copies of unknown size
};

struct DriveUse
{
    Drive drive;
    QList<PlatformSize> platforms; // largest first
    qint64 gamesBytes = 0;
    qint64 otherBytes = 0;
    int unknown = 0;
};

struct Sized
{
    QString id;
    QString title;
    QString platform;
    QString coverUrl;
    QDateTime lastPlayed;
    qint64 bytes = 0;
    bool uninstallable = false;
};

struct PlatformCount
{
    QString platform;
    int games = 0;
    int installed = 0;
};

struct StatusCount
{
    QString status; // as in the game file; "none" = no status
    int count = 0;
};

struct TagCount
{
    QString tag;
    int count = 0;
};

struct YearCount
{
    int year = 0;
    int count = 0;
};

struct Totals
{
    int games = 0;
    int installed = 0;
    int platforms = 0;
    qint64 minutes = 0;
    int backlog = 0;
    int played = 0;
};

struct Overview
{
    Totals totals;
    QList<Counted> continuePlaying;
    QList<DriveUse> drives; // the ones holding games, most games first
    QList<PlatformSize> platformSizes;
    QList<Sized> largest;
    QList<Sized> dormant;
    QList<PlatformCount> platformCounts; // most games first
    QList<StatusCount> statusCounts; // the list's status order, none last; only the ones in use
    QList<TagCount> topTags;
    QList<YearCount> years; // oldest first, the years between filled with 0
};

constexpr int kContinueLimit = 7;
constexpr int kStorageListLimit = 4;
constexpr int kDormantMonths = 6;
constexpr int kTopTagLimit = 12;
constexpr int kYearSpanLimit = 100; // a wider span (a mistyped year) lists only the years in use

Overview overview(const QList<Game> &games, const QList<Drive> &drives = {},
                  const QDateTime &now = QDateTime::currentDateTimeUtc());
QVariantMap toVariantMap(const Overview &overview);
} // namespace LibraryOverview
