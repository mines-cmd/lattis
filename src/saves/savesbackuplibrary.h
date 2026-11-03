#pragma once

#include "savesbackup.h"

class Library;

namespace SavesBackup {
// The backup input for a game that just closed, or nothing when automatic backup is off for it
// or it is no Steam, Proton or Linux game.
std::optional<Input> inputFor(const Library &library, const QString &gameId, const QString &dataDir,
                              const QString &appVersion);
}
