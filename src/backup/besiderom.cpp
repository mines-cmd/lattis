#include "besiderom.h"

#include <QDir>
#include <QFileInfo>
#include <QHash>

#include <algorithm>

namespace BesideRom {
bool runs(const EmulatorDef &emulator, const Game &game, const QList<Platform> &platforms)
{
    if (!game.emulator.isEmpty())
        return game.emulator == emulator.id;
    const auto platform = std::find_if(platforms.cbegin(), platforms.cend(),
                                       [&](const Platform &p) { return p.id == game.platform; });
    return platform != platforms.cend() && platform->emulators.contains(emulator.id);
}

QList<GameSaves> find(const EmulatorDef &emulator, const QList<Platform> &platforms, const QList<Game> &games)
{
    QList<GameSaves> out;
    if (emulator.savesBesideRom.isEmpty())
        return out;

    QHash<QString, QList<QFileInfo>> folders;
    for (const Game &game : games) {
        if (!game.rom || game.romPath.isEmpty() || !runs(emulator, game, platforms))
            continue;
        const QFileInfo rom(game.romPath);
        const QString folder = rom.absolutePath();
        auto it = folders.find(folder);
        if (it == folders.end())
            it = folders.insert(folder, QDir(folder).entryInfoList(QDir::Files, QDir::Name));

        const QString prefix = rom.completeBaseName() + u'.';
        GameSaves saves{game.id, {}};
        for (const QFileInfo &file : std::as_const(*it)) {
            const QString name = file.fileName();
            if (name.startsWith(prefix)
                && emulator.savesBesideRom.contains(name.mid(prefix.size()).toLower())
                && file.absoluteFilePath() != rom.absoluteFilePath())
                saves.files.append({file.absoluteFilePath(), file.size()});
        }
        if (!saves.files.isEmpty())
            out.append(saves);
    }
    return out;
}

QString restorePath(const QString &romPath, const QString &fileName)
{
    const qsizetype dot = fileName.lastIndexOf(u'.');
    if (romPath.isEmpty() || dot < 0 || dot == fileName.size() - 1)
        return {};
    const QFileInfo rom(romPath);
    return rom.absolutePath() + u'/' + rom.completeBaseName() + fileName.mid(dot);
}
}
