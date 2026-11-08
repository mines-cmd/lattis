#include "savesbackuplibrary.h"
#include "backupplan.h"
#include "besiderom.h"
#include "emulators.h"
#include "gameid.h"
#include "library.h"
#include "libraryfiles.h"
#include "protonsaves.h"
#include "savesmanifest.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>

namespace SavesBackup {
namespace {
BackupPlan::Settings readSettings(const QString &dataDir)
{
    QFile file(dataDir + QStringLiteral("/settings.toml"));
    const QByteArray bytes = file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
    return BackupPlan::parseSettings(bytes, file.fileName()).value;
}

QString resolvedFolder(const BackupPlan::Settings &settings, const QString &home)
{
    QString folder = settings.autoFolder;
    if (folder.isEmpty())
        folder = QStringLiteral("~/.local/share/lattis-saves");
    if (folder == QLatin1String("~") || folder.startsWith(QLatin1String("~/")))
        folder = home + folder.mid(1);
    return QDir::cleanPath(folder);
}

bool autoBackupOn(const LibrarySnapshot &snapshot, const Game &game)
{
    const int own = snapshot.content.games.value(game.id).autoBackup;
    bool platformOn = false;
    for (const Platform &platform : snapshot.content.platforms) {
        if (platform.id == game.platform)
            platformOn = platform.autoBackup;
    }
    return autoBackupResolved(own, platformOn);
}

QString steamPrefix(const QStringList &libraries, int appId)
{
    for (const QString &steamapps : libraries) {
        const QString prefix = steamapps + QStringLiteral("/compatdata/%1/pfx").arg(appId);
        if (QFileInfo(prefix + QStringLiteral("/drive_c")).isDir())
            return prefix;
    }
    return {};
}

bool eligible(const Game &game)
{
    return !game.rom
           && (game.platform == kSteamPlatform || game.platform == kProtonPlatform
               || game.platform == kLinuxPlatform);
}

QList<SavesFinder::Query> copiesOf(const Library &library, const LibrarySnapshot &snapshot, const QString &main)
{
    QList<SavesFinder::Query> out;
    QStringList ids{main};
    if (const Game *mainGame = library.game(main))
        ids += mainGame->linked;
    for (const QString &id : ids) {
        const Game *copy = library.game(id);
        if (!copy)
            continue;
        const GameFile gameFile = snapshot.content.games.value(id);
        SavesFinder::Query query;
        query.id = copy->id;
        query.title = copy->title;
        query.steamAppId = copy->platform == kSteamPlatform ? int(GameId::steamAppId(copy->id)) : 0;
        query.savesGame = gameFile.savesGame;
        query.baseDir = copy->installDir;
        query.saveFolder = gameFile.saveFolder;
        query.savePaths = gameFile.savePaths;
        if (copy->platform == kProtonPlatform && copy->shortcut != 0) {
            if (const QString prefix = library.protonPrefix(copy->shortcut); !prefix.isEmpty())
                query.prefix = prefix + QStringLiteral("/pfx");
        } else if (query.steamAppId > 0) {
            query.prefix = steamPrefix(snapshot.steam.libraries, query.steamAppId);
        }
        out.append(query);
    }
    return out;
}
}

QList<BackupPlan::GameQueries> queriesFor(const Library &library)
{
    const LibrarySnapshot &snapshot = library.store().snapshot();
    QList<BackupPlan::GameQueries> out;
    for (const Game &game : library.allGames()) {
        if (!eligible(game) || (!game.groupId.isEmpty() && game.groupId != game.id))
            continue;
        QList<SavesFinder::Query> copies = copiesOf(library, snapshot, game.id);
        if (!copies.isEmpty())
            out.append({game.id, game.title, std::move(copies)});
    }
    return out;
}

bool anyOn(const Library &library)
{
    const LibraryContent &content = library.store().snapshot().content;
    // Emulator backups need no manifest, so only the other games' choices count.
    const auto usesManifest = [](const QString &platform) {
        return platform == kSteamPlatform || platform == kProtonPlatform || platform == kLinuxPlatform;
    };
    for (const Platform &platform : content.platforms) {
        if (platform.autoBackup && usesManifest(platform.id))
            return true;
    }
    for (const GameFile &file : content.games) {
        if (file.autoBackup == 1 && usesManifest(file.platform))
            return true;
    }
    return false;
}

QString autoFolder(const Library &library, const QString &dataDir)
{
    return resolvedFolder(readSettings(dataDir), library.store().installLocations().home);
}

std::optional<Input> inputFor(const Library &library, const QString &gameId, const QString &dataDir,
                              const QString &appVersion, bool ignoreChoice)
{
    const Game *game = library.game(gameId);
    if (!game || (!game->rom && game->platform != kSteamPlatform && game->platform != kProtonPlatform
                  && game->platform != kLinuxPlatform))
        return std::nullopt;
    const LibraryStore &store = library.store();
    const LibrarySnapshot &snapshot = store.snapshot();
    if (!ignoreChoice && !autoBackupOn(snapshot, *game))
        return std::nullopt;

    const BackupPlan::Settings settings = readSettings(dataDir);

    Input input;
    input.env = SavesFinder::Env::system();
    input.env.home = store.installLocations().home;
    if (game->rom) {
        const Platform *platform = library.platform(game->platform);
        const QString chosen =
            Emulators::choose(game->emulator, platform ? platform->emulators : QStringList(), library.installs());
        const EmulatorDef *def = Emulators::find(library.emulators(), chosen);
        const EmulatorInstall install = library.installs().value(chosen);
        if (!def || !install.found())
            return std::nullopt;
        input.emulatorId = def->id;
        // Only what a restore accepts back: inside the home.
        const auto inHome = [&](const QString &path) { return path.startsWith(input.env.home + u'/'); };
        const auto exists = [](const QString &path) { return QFileInfo(path).exists() || QFileInfo(path).isSymLink(); };
        const GameFile gameFile = snapshot.content.games.value(gameId);
        QStringList own = gameFile.savePaths;
        if (!gameFile.saveFolder.isEmpty())
            own.prepend(gameFile.saveFolder);
        for (const QString &path : std::as_const(own)) {
            const QString resolved = resolveRomFolder(path, dataDir, input.env.home);
            if (inHome(resolved) && exists(resolved))
                input.emulatorPaths.append(resolved);
        }
        input.ownSaves = !input.emulatorPaths.isEmpty();
        input.gameId = input.ownSaves ? game->id : backupKey(input);
        input.title = input.ownSaves ? game->title : def->name.isEmpty() ? def->id : def->name;
        if (!input.ownSaves) {
            for (const QString &path : Emulators::installPaths(def->saves, install, input.env.home)) {
                if (inHome(path) && exists(path))
                    input.emulatorPaths.append(path);
            }
            for (const BesideRom::GameSaves &saves :
                 BesideRom::find(*def, snapshot.content.platforms, library.allGames())) {
                for (const BesideRom::SaveFile &file : saves.files) {
                    if (inHome(file.path))
                        input.emulatorPaths.append(file.path);
                }
            }
        }
        if (input.emulatorPaths.isEmpty())
            return std::nullopt;
    } else {
        const QString main = game->groupId.isEmpty() ? game->id : game->groupId;
        const Game *mainGame = library.game(main);
        input.gameId = main;
        input.title = mainGame ? mainGame->title : game->title;

        input.copies = copiesOf(library, snapshot, main);
        if (input.copies.isEmpty())
            return std::nullopt;

        if (game->platform == kProtonPlatform && !snapshot.steam.libraries.isEmpty()) {
            const ProtonSaves::GameSaves saves =
                ProtonSaves::locate(*game, snapshot.steam.libraries.first() + QStringLiteral("/compatdata"));
            if (saves.skipped == ProtonSaves::Skipped::No)
                input.protonPrefix = saves.prefix;
        }
    }

    input.manifestDir = SavesManifest::defaultDir();
    input.options.folder = resolvedFolder(settings, input.env.home);
    input.options.keep = settings.autoKeep;
    input.options.maxBytes = qint64(settings.autoMaxMb) << 20;
    input.appVersion = appVersion;
    input.now = QDateTime::currentDateTime();
    return input;
}

QList<Input> inputsForPlayed(const Library &library, const QString &dataDir, const QString &appVersion)
{
    QList<Input> out;
    QSet<QString> keys;
    for (const Game &game : library.allGames()) {
        if (game.hidden || !game.installed || (game.playMinutes <= 0 && !game.lastPlayed.isValid()))
            continue;
        std::optional<Input> input = inputFor(library, game.id, dataDir, appVersion, true);
        if (input && !keys.contains(backupKey(*input))) {
            keys.insert(backupKey(*input));
            out.append(std::move(*input));
        }
    }
    return out;
}
}
