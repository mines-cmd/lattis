#pragma once

#include <QDate>
#include <QDateTime>
#include <QHash>
#include <QList>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QVariantMap>

#include <optional>

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
    QStringList tags;
    int year = 0; // 0 = unknown
    bool rom = false;
    bool emulatorMissing = false; // a ROM game none of whose emulators is found
    bool updateWaiting = false;
    bool missingArt = false; // no cover, and a lookup hasn't marked it as having none
    bool missingInfo = false; // no description, year or rating a lookup could still find
    QString backupKey; // the folder name its saves are backed up under; empty = saves can't cover it
    qint64 steamAppId = 0; // set for a Steam game whose achievements are read (installed or played); 0 = none
    QDate added; // invalid = unknown
    int longestSession = 0; // minutes of its longest session in the play history; merged entries don't count
    QDateTime longestEnd;
    // How an installed game runs: "native" (a Linux program), "proton" (through `tool`), or "steam" for a Steam game
    // with no tool of its own, which ProtonDB's tier settles; empty = not part of the card.
    QString runner;
    QString tool; // the display name of the Proton tool, for "proton"
};

struct AchievementCount
{
    int unlocked = 0;
    int total = 0; // 0 = the game has none
};
using AchievementCounts = QHash<qint64, AchievementCount>;

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

struct Attention
{
    QString kind; // neverBackedUp, missingArt, missingInfo, updates, romsNotFound, emulatorsMissing
    int count = 0;
};

struct Pick
{
    QString id;
    QString title;
    QString platform;
    QString coverUrl;
    int year = 0;
};

struct NearlyDone
{
    QString id;
    QString title;
    QString coverUrl;
    int unlocked = 0;
    int total = 0;
};

struct Achievements
{
    int games = 0; // Steam games with achievements
    int unlocked = 0;
    int total = 0;
    int perfect = 0;
    QList<NearlyDone> nearlyDone; // below 100%, highest first
};

struct TierCount
{
    QString tier; // native, platinum, gold, silver, bronze, borked, pending
    int count = 0;
};

struct Compatibility
{
    int games = 0; // installed Steam games
    int noReports = 0;
    int unchecked = 0; // no entry in ProtonDB's cache yet
    QList<TierCount> tiers; // in that order, the ones in use
    int native = 0;
    int proton = 0;
    QString topTool; // the Proton tool most installed games run through
    int topToolGames = 0;
};

struct SaveBackups
{
    int archives = 0;
    qint64 bytes = 0;
    QDateTime last; // invalid: none
};

struct SaveCoverage
{
    int played = 0; // played games whose saves can be backed up
    int backedUp = 0;
    int archives = 0;
    qint64 bytes = 0;
    QDateTime last;
};

struct LongestSession
{
    QString id;
    QString title;
    QString coverUrl;
    int minutes = 0;
    QDateTime end;
};

struct Waiting
{
    QString id;
    QString title;
    QString coverUrl;
    QDate added;
};

// What the Compatibility and Save backups cards read besides the games.
struct Sources
{
    std::optional<SaveBackups> saves; // nothing: the backup folder is unknown, so the card is left out
    QHash<qint64, QString> protonTiers; // by app id: a tier, or "none" for no reports; absent = not asked yet
    QString defaultTool; // display name of the tool Steam runs other titles with; empty = none
};

struct Overview
{
    Totals totals;
    std::optional<Achievements> achievements; // nullopt = not read yet
    QList<Attention> attention; // in this order, the ones with something to report
    QList<Pick> backlog; // installed Backlog games, for From your backlog to pick from
    QList<Counted> continuePlaying;
    QList<DriveUse> drives; // the ones holding games, most games first
    QList<PlatformSize> platformSizes;
    QList<Sized> largest;
    QList<Sized> dormant;
    QList<PlatformCount> platformCounts; // most games first
    QList<StatusCount> statusCounts; // the list's status order, none last; only the ones in use
    QList<TagCount> topTags;
    QList<YearCount> years; // oldest first, the years between filled with 0
    Compatibility compatibility;
    std::optional<SaveCoverage> saves;
    std::optional<LongestSession> longest;
    std::optional<Waiting> waiting; // the Backlog game added longest ago
};

constexpr int kContinueLimit = 7;
constexpr int kStorageListLimit = 4;
constexpr int kDormantMonths = 6;
constexpr int kTopTagLimit = 12;
constexpr int kYearSpanLimit = 100; // a wider span (a mistyped year) lists only the years in use
constexpr int kNearlyDoneLimit = 4;
constexpr int kNearlyDonePercent = 80;

// `backedUp` holds the backup keys that have a backup; nothing (not read) leaves "never backed up" out.
// `achievements` is what was read per Steam app id; nothing (not read yet) leaves the achievements out.
Overview overview(const QList<Game> &games, const QList<Drive> &drives = {},
                  const QDateTime &now = QDateTime::currentDateTimeUtc(),
                  const std::optional<QSet<QString>> &backedUp = std::nullopt,
                  const std::optional<AchievementCounts> &achievements = std::nullopt, const Sources &sources = {});
QVariantMap toVariantMap(const Overview &overview);
} // namespace LibraryOverview
