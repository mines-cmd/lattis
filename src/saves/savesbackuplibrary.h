#pragma once

#include "backupplan.h"
#include "savesbackup.h"

class Library;

namespace SavesBackup {
// The backup input for a game that just closed, or nothing when automatic backup is off for it
// or it is no Steam, Proton or Linux game.
// Whether any platform or game has automatic backup on (what the daily manifest check waits for).
bool anyOn(const Library &library);

// Every Steam, Proton and Linux game's copies for the finder, whatever its automatic setting (the
// menu's Back up saves takes them all); copies of one game come as one entry.
QList<BackupPlan::GameQueries> queriesFor(const Library &library);

// `ignoreChoice` is the game page's Back up now: the automatic setting doesn't matter to it.
std::optional<Input> inputFor(const Library &library, const QString &gameId, const QString &dataDir,
                              const QString &appVersion, bool ignoreChoice = false);
}
