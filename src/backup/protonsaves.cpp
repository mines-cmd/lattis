#include "protonsaves.h"
#include "libraryfiles.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>

#include <algorithm>
#include <functional>

namespace ProtonSaves {
namespace {
const QString kPfx = QStringLiteral("pfx/");

void walk(const Prefix &prefix, const QString &relative,
          const std::function<bool(const QString &, const QFileInfo &, Entry)> &visit)
{
    const QDir dir(relative.isEmpty() ? prefix.path : prefix.path + u'/' + relative);
    const QFileInfoList entries =
        dir.entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System, QDir::Name);
    for (const QFileInfo &info : entries) {
        const QString path = relative.isEmpty() ? info.fileName() : relative + u'/' + info.fileName();
        const bool link = info.isSymLink();
        const bool folder = !link && info.isDir();
        const Entry kind = classify(prefix, path, folder, link ? info.readSymLink() : QString());
        if (visit(path, info, kind) && folder)
            walk(prefix, path, visit);
    }
}
}

std::optional<Prefix> Prefix::read(const QString &path)
{
    QFile tracked(path + QStringLiteral("/tracked_files"));
    if (path.isEmpty() || !tracked.open(QIODevice::ReadOnly))
        return std::nullopt;
    Prefix prefix;
    prefix.path = path;
    // Paths relative to pfx/, a folder's ending in /; only files and links are Proton's, the
    // folders hold the game's files too (drive_c/users/…).
    for (const QByteArray &line : tracked.readAll().split('\n')) {
        const QString entry = QString::fromUtf8(line).trimmed();
        if (!entry.isEmpty() && !entry.endsWith(u'/'))
            prefix.protonFiles.insert(kPfx + entry);
    }
    // GE-Proton's upscaler DLLs (FSR4's is 66 MB), which tracked_files doesn't list:
    // {"fsr4_files": {"drive_c/windows/system32/amdxcffx64.dll": {…}}}.
    QFile upscaler(path + QStringLiteral("/upscaler_files"));
    if (upscaler.open(QIODevice::ReadOnly)) {
        const QJsonObject root = QJsonDocument::fromJson(upscaler.readAll()).object();
        for (const QJsonValue &group : root) {
            for (const QString &file : group.toObject().keys())
                prefix.protonFiles.insert(kPfx + file);
        }
    }
    return prefix;
}

Entry classify(const Prefix &prefix, const QString &relative, bool isFolder, const QString &linkTarget)
{
    // At the top, the prefix and what says which Proton made it are the game's; the rest is
    // Proton's bookkeeping (tracked_files, upscaler_files, gstreamer-1.0/…).
    if (!relative.startsWith(kPfx)) {
        if (relative == QLatin1String("pfx") || relative == QLatin1String("version")
            || relative == QLatin1String("config_info"))
            return Entry::Save;
        return relative == QLatin1String("pfx.lock") ? Entry::Drop : Entry::Proton;
    }

    const QString inPfx = relative.mid(kPfx.size());
    // The drive links (z: → /) and the guard Proton makes the prefix under.
    if (inPfx == QLatin1String("dosdevices") || inPfx.startsWith(QLatin1String("dosdevices/"))
        || inPfx == QLatin1String("creation_sync_guard"))
        return Entry::Proton;
    const bool topOfPfx = !inPfx.contains(u'/');
    if (topOfPfx && inPfx.endsWith(QLatin1String(".reg.old")))
        return Entry::Drop;
    const QString temp = QStringLiteral("drive_c/users/steamuser/AppData/Local/Temp");
    if (inPfx == temp || inPfx.startsWith(temp + u'/'))
        return Entry::Drop;
    if (isFolder)
        return Entry::Save;

    // Games save settings, and sometimes progress, in the registry, though Proton lists it.
    if (topOfPfx && inPfx.endsWith(QLatin1String(".reg")))
        return Entry::Save;
    if (prefix.protonFiles.contains(relative))
        return Entry::Proton;
    // Links out of the prefix are Proton's (fonts and DLLs in its own folder); the ones
    // inside it (My Documents → ./Documents) are the prefix's.
    if (!linkTarget.isEmpty()) {
        if (QDir::isAbsolutePath(linkTarget))
            return Entry::Proton;
        const QString resolved = QDir::cleanPath(QFileInfo(relative).path() + u'/' + linkTarget);
        if (resolved == QLatin1String("..") || resolved.startsWith(QLatin1String("../")))
            return Entry::Proton;
    }
    return Entry::Save;
}

Archive::Skip skip(const Prefix &prefix)
{
    return [prefix](const QString &relative) {
        const QFileInfo info(prefix.path + u'/' + relative);
        const bool link = info.isSymLink();
        return classify(prefix, relative, !link && info.isDir(), link ? info.readSymLink() : QString()) != Entry::Save;
    };
}

QStringList carryOver(const Prefix &prefix)
{
    QStringList out;
    walk(prefix, {}, [&](const QString &path, const QFileInfo &, Entry kind) {
        if (kind == Entry::Proton)
            out.append(path);
        return kind == Entry::Save;
    });
    out.sort();
    return out;
}

GameSaves locate(const Game &game, const QString &compatdata)
{
    GameSaves saves;
    saves.gameId = game.id;
    saves.shortcut = game.shortcut;
    const QString path = compatdata + QStringLiteral("/%1").arg(game.shortcut);
    if (game.shortcut == 0 || compatdata.isEmpty() || !QFileInfo(path).isDir()) {
        saves.skipped = game.installed && !game.uninstalled ? Skipped::NeverLaunched : Skipped::Uninstalled;
        return saves;
    }
    saves.prefix = path;
    if (!QFileInfo::exists(path + QStringLiteral("/tracked_files")))
        saves.skipped = Skipped::Unlisted;
    return saves;
}

void addFiles(GameSaves &saves)
{
    saves.files.clear();
    saves.bytes = 0;
    if (saves.skipped != Skipped::No)
        return;
    const std::optional<Prefix> prefix = Prefix::read(saves.prefix);
    if (!prefix) {
        saves.skipped = Skipped::Unlisted;
        return;
    }
    walk(*prefix, {}, [&](const QString &relative, const QFileInfo &info, Entry kind) {
        if (kind != Entry::Save)
            return false;
        if (info.isSymLink() || !info.isDir()) {
            const qint64 size = info.isSymLink() ? 0 : info.size();
            saves.files.append({relative, size});
            saves.bytes += size;
        }
        return true;
    });
    std::sort(saves.files.begin(), saves.files.end(),
              [](const SaveFile &a, const SaveFile &b) { return a.path < b.path; });
}

QList<GameSaves> find(const QList<Game> &games, const QString &compatdata)
{
    QList<GameSaves> out;
    for (const Game &game : games) {
        if (game.platform != kProtonPlatform)
            continue;
        GameSaves saves = locate(game, compatdata);
        addFiles(saves);
        out.append(saves);
    }
    return out;
}
}
