#include "savesbackuplibrary.h"
#include "backupplan.h"
#include "gameid.h"
#include "library.h"
#include "protonsaves.h"
#include "savesmanifest.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>

namespace SavesBackup {
namespace {
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
}

bool anyOn(const Library &library)
{
    const LibraryContent &content = library.store().snapshot().content;
    for (const Platform &platform : content.platforms) {
        if (platform.autoBackup)
            return true;
    }
    for (const GameFile &file : content.games) {
        if (file.autoBackup == 1)
            return true;
    }
    return false;
}

std::optional<Input> inputFor(const Library &library, const QString &gameId, const QString &dataDir,
                              const QString &appVersion, bool ignoreChoice)
{
    const Game *game = library.game(gameId);
    if (!game || game->rom
        || (game->platform != kSteamPlatform && game->platform != kProtonPlatform && game->platform != kLinuxPlatform))
        return std::nullopt;
    const LibraryStore &store = library.store();
    const LibrarySnapshot &snapshot = store.snapshot();
    if (!ignoreChoice && !autoBackupOn(snapshot, *game))
        return std::nullopt;

    QFile file(dataDir + QStringLiteral("/settings.toml"));
    const QByteArray bytes = file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
    const BackupPlan::Settings settings = BackupPlan::parseSettings(bytes, file.fileName()).value;

    Input input;
    input.env = SavesFinder::Env::system();
    input.env.home = store.installLocations().home;
    const QString main = game->groupId.isEmpty() ? game->id : game->groupId;
    const Game *mainGame = library.game(main);
    input.gameId = main;
    input.title = mainGame ? mainGame->title : game->title;

    QStringList ids{main};
    if (mainGame)
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
        input.copies.append(query);
    }
    if (input.copies.isEmpty())
        return std::nullopt;

    if (game->platform == kProtonPlatform && !snapshot.steam.libraries.isEmpty()) {
        const ProtonSaves::GameSaves saves =
            ProtonSaves::locate(*game, snapshot.steam.libraries.first() + QStringLiteral("/compatdata"));
        if (saves.skipped == ProtonSaves::Skipped::No)
            input.protonPrefix = saves.prefix;
    }

    input.manifestDir = SavesManifest::defaultDir();
    QString folder = settings.autoFolder;
    if (folder.isEmpty())
        folder = QStringLiteral("~/.local/share/lattis-saves");
    if (folder == QLatin1String("~") || folder.startsWith(QLatin1String("~/")))
        folder = input.env.home + folder.mid(1);
    input.options.folder = QDir::cleanPath(folder);
    input.options.keep = settings.autoKeep;
    input.options.maxBytes = qint64(settings.autoMaxMb) << 20;
    input.appVersion = appVersion;
    input.now = QDateTime::currentDateTime();
    return input;
}
}
