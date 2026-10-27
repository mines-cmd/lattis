#pragma once

#include "emulators.h"
#include "game.h"
#include "playhistory.h"
#include "rominfo.h"

#include <QDateTime>
#include <QHash>
#include <QList>
#include <QString>

// See docs/code/library.md.
namespace EmulatorScreenshots {
struct Shot
{
    QString id;
    QString path;
    QDateTime time;

    bool operator==(const Shot &) const = default;
};

// The ROM file's (or folder game's) own name and IDs. Slow: opens the ROM.
RomInfo readInfo(const Game &game);

// readInfo() kept for the session by the ROM's path and modified time, so reopening the Gallery doesn't open the ROMs
// again. Safe to call from several threads.
RomInfo readInfoCached(const Game &game);

// The ID in a screenshot's file name or folder: the serial, else the title ID, the PSP disc ID or the N64 header name.
QString idOf(const RomInfo &info);

// The time in a file name (several stamp layouts); invalid when it has none.
QDateTime timeInName(const QString &fileName);

// The folders it looks in for this install: screenshot_folders when set, else where the screenshots patterns start.
// "{romdir}" stands for the ROM's folder.
QStringList folders(const EmulatorDef &emulator, const EmulatorInstall &install, const QString &home);

// Whether some pattern names no game, so only play sessions say whose a screenshot is (Cemu, or a folder of your own).
bool usesSessions(const EmulatorDef &emulator, const EmulatorInstall &install, const QString &home);

QList<Shot> list(const EmulatorDef &emulator, const EmulatorInstall &install, const Game &game, const RomInfo &info,
                 const QList<PlayHistory::Session> &sessions, const QString &home);

struct Entry
{
    QString gameId;
    Shot shot;
};

// The screenshots of all these games (one emulator's) in a single pass over the folders: each file goes to the first
// game it matches. infos and sessions are keyed by game ID.
QList<Entry> listAll(const EmulatorDef &emulator, const EmulatorInstall &install, const QList<Game> &games,
                     const QHash<QString, RomInfo> &infos,
                     const QHash<QString, QList<PlayHistory::Session>> &sessions, const QString &home);
}
