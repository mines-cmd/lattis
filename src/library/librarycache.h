#pragma once

#include "libraryfiles.h"
#include "steamscan.h"

#include <QHash>
#include <QStringList>

#include <optional>

// See docs/code/library.md.

struct FileStamp
{
    qint64 mtimeMs = 0;
    qint64 size = -1;

    bool operator==(const FileStamp &) const = default;
};

struct CachedFile
{
    FileStamp stamp;
    QStringList warnings;
    bool ok = true;       // false while it has a syntax error; the app then doesn't write it
};

struct LibrarySnapshot
{
    int version = 0; // 0 = unknown
    QHash<QString, CachedFile> files;
    LibraryContent content;
    QHash<QString, ArtSet> art;
    QHash<QString, ArtCheck> artChecks;

    RomFiles romFiles;
    QStringList romWarnings;
    QHash<QString, QStringList> romMissingFolders;
    QHash<QString, RomFile> romGameFiles;
    QHash<QString, ProgramFiles> programFiles;
    bool romsScanned = false;      // since this snapshot was loaded; not cached
    bool installsDetected = false; // likewise

    // Not cached: installs change outside the app, and finding them is cheap.
    QHash<QString, EmulatorInstall> installs;

    SteamState steam;
    // Cached, so the first frame shows the badge.
    QHash<QString, QStringList> steamQueued;

    QStringList warnings() const;
};

namespace LibraryCache {
// Bump whenever what's stored (or how) changes.
constexpr quint32 kFormatVersion = 25;

QString pathFor(const QString &cacheDir, const QString &dataDir);

std::optional<LibrarySnapshot> load(const QString &cachePath, const QString &dataDir);
bool save(const QString &cachePath, const QString &dataDir, const LibrarySnapshot &snapshot);

struct Changes
{
    QStringList added;
    QStringList changed;
    QStringList removed;

    bool isEmpty() const { return added.isEmpty() && changed.isEmpty() && removed.isEmpty(); }
};

Changes diff(const QHash<QString, FileStamp> &onDisk, const QHash<QString, CachedFile> &cached);
}
