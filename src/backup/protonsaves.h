#pragma once

#include "backuparchive.h"
#include "game.h"

#include <QList>
#include <QSet>
#include <QString>
#include <QStringList>

#include <optional>

// See docs/code/backup.md.
namespace ProtonSaves {
struct Prefix
{
    QString path;
    QSet<QString> protonFiles;

    static std::optional<Prefix> read(const QString &path);
};

enum class Entry {
    Save,   // backed up
    Proton, // Proton's: left out, and carried over from the current prefix on a restore
    Drop,   // neither (locks, old registry copies, temporary files)
};

Entry classify(const Prefix &prefix, const QString &relative, bool isFolder, const QString &linkTarget);

Archive::Skip skip(const Prefix &prefix);

QStringList carryOver(const Prefix &prefix);

struct SaveFile
{
    QString path;
    qint64 size = 0;

    bool operator==(const SaveFile &) const = default;
};

enum class Skipped {
    No,
    NeverLaunched,
    Uninstalled,
    Unlisted,      // its Proton doesn't write tracked_files
};

struct GameSaves
{
    QString gameId;
    quint32 shortcut = 0;
    QString prefix;
    Skipped skipped = Skipped::No;
    QList<SaveFile> files;
    qint64 bytes = 0;
};

GameSaves locate(const Game &game, const QString &compatdata);

void addFiles(GameSaves &saves);

QList<GameSaves> find(const QList<Game> &games, const QString &compatdata);
}
