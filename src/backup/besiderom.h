#pragma once

#include "emulators.h"
#include "game.h"
#include "libraryfiles.h"

#include <QList>
#include <QString>

// See docs/code/backup.md.
namespace BesideRom {
struct SaveFile
{
    QString path;
    qint64 size = 0;

    bool operator==(const SaveFile &) const = default;
};

struct GameSaves
{
    QString gameId;
    QList<SaveFile> files;

    bool operator==(const GameSaves &) const = default;
};

bool runs(const EmulatorDef &emulator, const Game &game, const QList<Platform> &platforms);

QList<GameSaves> find(const EmulatorDef &emulator, const QList<Platform> &platforms, const QList<Game> &games);

QString restorePath(const QString &romPath, const QString &fileName);
}
