#pragma once

#include "libraryfiles.h"
#include "rominfo.h"

#include <QSet>
#include <QString>

// See docs/code/library.md. Titles come from file names; the scan also reads small headers of new files (rominfo.h).
namespace RomName {
enum class Kind { Game, Update, Dlc };

struct Info
{
    QString title;
    QString titleId;
    Kind kind = Kind::Game;

    bool operator==(const Info &) const = default;
};

// The platform id is optional: a few rules (leading release numbers) apply only to some platforms.
Info parse(const QString &fileName, const QString &platform = {});

QString titleId(const QString &fileName);

Kind kindOf(const QString &titleId);

QString cleanTitle(const QString &fileName, const QString &platform = {});
QString cleanName(const QString &name, const QString &platform = {});

QString fixCase(const QString &title);

Info parseFolder(const QString &folderName);
}

struct RomPick
{
    RomName::Info info;
    QStringList platforms;
    QString problem;

    bool operator==(const RomPick &) const = default;
};

RomPick inspectRomPick(const QString &path, const QList<Platform> &platforms, const QList<EmulatorDef> &emulators);

struct RomIndexUpdate
{
    QList<RomEntry> entries;
    bool changed = false;
    int added = 0;
    QStringList addedIds;
    int renamed = 0;
    int updates = 0;
    int dlc = 0;
    int dropped = 0;
    int named = 0;
};

RomIndexUpdate updateRomIndex(const QString &platform, const QList<RomEntry> &index, const QStringList &found,
                              QSet<QString> *taken, const QSet<QString> &folderGameFiles = {},
                              const QHash<QString, qint64> &sizes = {},
                              const QHash<QString, RomInfo> &fromFiles = {});

// The "Update names from the files" action: which games to read, what they said, what changes.
struct RomNameJob
{
    QString path;
    QString source;
    bool folder = false;
};

struct RomNameRead
{
    QString path;
    RomInfo info;
};

struct RomNamesUpdate
{
    QList<RomEntry> entries;
    int renamed = 0;
    bool changed = false;
};

QList<RomNameJob> romNameJobs(const Platform &platform, const QList<EmulatorDef> &emulators,
                              const QList<RomEntry> &index, const QHash<QString, QString> &files,
                              const QSet<QString> &editedIds);

// Opens files: keep it off the main thread.
QList<RomNameRead> readRomNames(const QString &platform, const QList<RomNameJob> &jobs);

RomNamesUpdate applyRomNames(const QList<RomEntry> &index, const QList<RomNameRead> &reads);

struct RomScanResult
{
    RomFiles files;
    QMap<QString, QList<RomEntry>> indexes;
    QStringList warnings;
    QHash<QString, QStringList> missingFolders;
    QStringList log;
    QMap<QString, QStringList> added;
};

// Walks the whole folder: keep it off the main thread.
qint64 folderSize(const QString &folder);

RomScanResult scanRomFolders(const QList<Platform> &platforms, const QList<EmulatorDef> &emulators,
                             const QMap<QString, QList<RomEntry>> &indexes, QSet<QString> taken,
                             const QString &dataDir, const QString &home, const QSet<QString> &claimed = {});
