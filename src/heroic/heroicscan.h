#pragma once

#include "heroiclocate.h"

#include <QHash>
#include <QList>
#include <QSet>
#include <QString>
#include <QStringList>

#include <optional>

struct HeroicStamp
{
    qint64 mtimeMs = 0;
    qint64 size = -1;

    bool operator==(const HeroicStamp &) const = default;
};

struct HeroicGame
{
    QString appName;
    QString title;
    bool installed = false;
    QString installPath;
    qint64 sizeBytes = 0;   // only installed.json has it in bytes; the cache's is text like "308.78 MiB"
    QString artCover;
    QString artSquare;
    QString artLogo;

    bool operator==(const HeroicGame &) const = default;
};

struct HeroicPlayTime
{
    int minutes = 0;
    qint64 lastPlayed = 0;

    bool operator==(const HeroicPlayTime &) const = default;
};

struct HeroicState
{
    QString root;

    HeroicStamp libraryStamp;
    QList<HeroicGame> library;
    // The cache was read and lists something, so a game it lacks has left. Heroic can blank a store
    // it can't read, so an empty cache says nothing about what's owned.
    bool libraryKnown = false;
    QString libraryWarning;

    QList<HeroicGame> installed;
    // installed.json was read, or doesn't exist (nothing installed yet).
    bool installedOk = false;

    QHash<QString, HeroicPlayTime> playTime;
    // timestamp.json was read, or doesn't exist; only then does a lower total mean a reset.
    bool playTimeOk = false;

    QStringList warnings;

    QList<HeroicGame> games() const;
    QStringList watchPaths() const;

    bool operator==(const HeroicState &) const = default;
};

struct HeroicInstalled
{
    QString installDir;
    qint64 sizeBytes = 0;

    bool operator==(const HeroicInstalled &) const = default;
};

// What the library needs from a scan, by game id. The disk cache keeps this and not HeroicState, whose
// art URLs would be read before every first frame.
struct HeroicListing
{
    HeroicInstall install;
    QHash<QString, HeroicInstalled> installed;
    // In heroic.toml, but off the list for now.
    QSet<QString> gone;
    QStringList warnings;

    bool operator==(const HeroicListing &) const = default;
};

namespace HeroicScan {
QString libraryPath(const QString &root);
QString installedPath(const QString &root);
QString playTimePath(const QString &root);

HeroicState scan(const QString &root, const HeroicState &previous = {});

// Seconds since the epoch, 0 for a game Heroic hasn't closed yet; nothing when timestamp.json can't be read.
std::optional<qint64> lastPlayed(const QString &root, const QString &appName);
}
