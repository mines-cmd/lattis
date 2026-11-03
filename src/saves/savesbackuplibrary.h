#pragma once

#include "savesbackup.h"

class Library;

namespace SavesBackup {
// The backup input for a game that just closed, or nothing when automatic backup is off for it
// or it is no Steam, Proton or Linux game.
// Whether any platform or game has automatic backup on (what the daily manifest check waits for).
bool anyOn(const Library &library);

// `ignoreChoice` is the game page's Back up now: the automatic setting doesn't matter to it.
std::optional<Input> inputFor(const Library &library, const QString &gameId, const QString &dataDir,
                              const QString &appVersion, bool ignoreChoice = false);
}
