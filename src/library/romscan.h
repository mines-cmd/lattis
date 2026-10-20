#pragma once

#include "libraryfiles.h"

#include <QSet>
#include <QString>

// See docs/code/library.md. Only file names are used; ROM contents are never read.
namespace RomName {
enum class Kind { Game, Update, Dlc };

struct Info
{
    QString title;
    QString titleId;
    Kind kind = Kind::Game;

    bool operator==(const Info &) const = default;
};

Info parse(const QString &fileName);

QString titleId(const QString &fileName);

Kind kindOf(const QString &titleId);

QString cleanTitle(const QString &fileName);
QString cleanName(const QString &name);

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
};

RomIndexUpdate updateRomIndex(const QString &platform, const QList<RomEntry> &index, const QStringList &found,
                              QSet<QString> *taken, const QSet<QString> &folderGameFiles = {},
                              const QHash<QString, qint64> &sizes = {});

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
