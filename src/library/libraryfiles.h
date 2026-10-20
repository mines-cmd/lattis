#pragma once

#include "emulators.h"
#include "game.h"
#include "gamefile.h"
#include "parsed.h"
#include "steamscan.h"

#include <QCollator>
#include <QHash>
#include <QList>
#include <QMap>
#include <QStringList>

// See docs/code/library.md.

struct Platform
{
    QString id;
    QString name;
    QString icon;
    QStringList emulators;
    QStringList romFolders;
    int line = 0;
    QList<int> igdbPlatforms;
    bool inRail = true;

    bool operator==(const Platform &) const = default;
};

struct RomEntry
{
    QString path;
    QString id;
    QString title;
    QString titleId;
    // 0 = unknown. Kept while the file is gone, so a renamed file can be recognized by it.
    qint64 size = 0;

    bool operator==(const RomEntry &) const = default;
};

struct RomFile
{
    QString path;
    qint64 size = 0;

    bool operator==(const RomFile &) const = default;
};

struct TagDef
{
    QString name;
    int color = 0;
    QStringList igdb;
    bool igdbGiven = false;

    bool operator==(const TagDef &) const = default;
};

struct IndexEntry
{
    qint64 appId = 0;
    QString title;

    bool operator==(const IndexEntry &) const = default;
};

struct LibraryInfo
{
    int version = 1;
    QString id;
    bool setupPending = false;

    bool operator==(const LibraryInfo &) const = default;
};

Parsed<LibraryInfo> parseLibraryInfo(const QByteArray &bytes, const QString &displayPath);
Parsed<QList<Platform>> parsePlatforms(const QByteArray &bytes, const QString &displayPath);
QList<int> igdbPlatformIds(const Platform &platform);

Parsed<QList<TagDef>> parseTags(const QByteArray &bytes, const QString &displayPath);
Parsed<QList<IndexEntry>> parseSteamIndex(const QByteArray &bytes, const QString &displayPath);
Parsed<QList<RomEntry>> parseRomIndex(const QByteArray &bytes, const QString &displayPath);
QString formatRomIndex(const QString &platformName, QList<RomEntry> entries);

QString resolveRomFolder(const QString &folder, const QString &dataDir, const QString &home);

QHash<QString, RomFile> findRomGameFiles(const QMap<QString, GameFile> &games, const QString &dataDir,
                                         const QString &home);

struct ProgramFiles
{
    QString exe;
    bool exeFound = false;
    QString installDir;
    qint64 sizeBytes = 0;
    bool singleFile = false;

    QString files() const { return singleFile ? exe : installDir; }

    bool operator==(const ProgramFiles &) const = default;
};

QString protonInstallDir(const QString &exe, const QString &home);
QString linuxInstallDir(const QString &exe, const QString &home);

enum class LinuxProgramKind { None, Elf, AppImage, Script, SharedLibrary };
LinuxProgramKind linuxProgramKind(const QString &path);

// `addUpSizes` walks install folders: not on the main thread.
QHash<QString, ProgramFiles> findProgramFiles(const QMap<QString, GameFile> &games, const QString &dataDir,
                                              const QString &home, const QHash<QString, ProgramFiles> &previous = {},
                                              bool addUpSizes = true);

struct ArtSet
{
    QString hero;
    QString logo;
    QString icon;
    QString cover;
    // ms since the epoch. It goes into the URLs, so an image replaced under the same name
    // loads again.
    qint64 modified = 0;

    bool operator==(const ArtSet &) const = default;
};

// Which of an art set's files were over ArtImage::maxSide() when the scan read their headers.
// Valid only while `art` equals the game's current ArtSet.
struct ArtCheck
{
    ArtSet art;
    QStringList oversized;
    // Finding a logo's margins means decoding it, which the scan doesn't do, so the queue does.
    bool trimLogo = false;

    bool operator==(const ArtCheck &) const = default;
};

struct LibraryContent
{
    QList<Platform> platforms;
    QList<TagDef> tags;
    QList<IndexEntry> steam;
    QMap<QString, QList<RomEntry>> roms;
    QList<EmulatorEntry> emulators;
    QMap<QString, GameFile> games;
};

using RomFiles = QHash<QString, QHash<QString, QString>>;

struct BuiltLibrary
{
    QList<Game> games;
    QList<Game> hidden;
    QList<Platform> platforms;
    QList<EmulatorDef> emulators;
    QStringList warnings;
};

// When a kind has several files, the first extension listed wins.
inline const QStringList kArtKinds = {QStringLiteral("hero"), QStringLiteral("logo"), QStringLiteral("icon"),
                                      QStringLiteral("cover")};
inline const QStringList kArtExtensions = {QStringLiteral("png"), QStringLiteral("jpg"), QStringLiteral("jpeg"),
                                           QStringLiteral("webp")};

inline const QString kSteamPlatform = QStringLiteral("steam");
inline const QString kProtonPlatform = QStringLiteral("proton");
inline const QString kLinuxPlatform = QStringLiteral("linux");

inline bool runsProgram(const QString &platform)
{
    return platform == kProtonPlatform || platform == kLinuxPlatform;
}

QCollator titleCollator();
bool titleBefore(const QCollator &collator, const Game &a, const Game &b);

BuiltLibrary buildLibrary(const LibraryContent &content, const QHash<QString, ArtSet> &art,
                          const RomFiles &romFiles = {}, const QList<EmulatorEntry> &presets = {},
                          const QHash<QString, RomFile> &romGameFiles = {}, const SteamState &steam = {},
                          const QHash<QString, QStringList> &steamQueued = {},
                          const QHash<QString, ProgramFiles> &programFiles = {});
