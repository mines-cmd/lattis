#include "librarystore.h"
#include "apppaths.h"
#include "artimage.h"
#include "gameid.h"
#include "romscan.h"
#include "steamart.h"
#include "vdf.h"
#include "tomlpatch.h"

#include <QDir>
#include <QDirIterator>
#include <QElapsedTimer>
#include <QFile>
#include <QImageReader>
#include <QRegularExpression>
#include <QSaveFile>
#include <QTimeZone>
#include <QtConcurrent/QtConcurrentRun>

#include <cstdio>

Q_LOGGING_CATEGORY(lcLibrary, "launcher.library")

namespace {
const QString kLibraryInfo = QStringLiteral("library/library.toml");
const QString kPlatforms = QStringLiteral("library/platforms.toml");

// Moving a platform only reorders platforms.toml, which needs no ROM scan.
bool samePlatformsBesidesOrder(QList<Platform> a, QList<Platform> b)
{
    if (a.size() != b.size())
        return false;
    const auto byId = [](const Platform &x, const Platform &y) { return x.id < y.id; };
    std::sort(a.begin(), a.end(), byId);
    std::sort(b.begin(), b.end(), byId);
    return a == b;
}
const QString kTags = QStringLiteral("library/tags.toml");
const QString kSteamIndex = QStringLiteral("library/steam.toml");
const QString kEmulators = QStringLiteral("library/emulators.toml");
const QString kGamesPrefix = QStringLiteral("library/games/");
const QString kRomsPrefix = QStringLiteral("library/roms/");

QString defaultPlatforms()
{
    QFile file(QStringLiteral(":/qt/qml/Launcher/resources/platforms.toml"));
    return file.open(QIODevice::ReadOnly) ? QString::fromUtf8(file.readAll()) : QString();
}

QString tomlList(const QStringList &list)
{
    QStringList items;
    for (const QString &s : list)
        items.append(tomlString(s));
    return u'[' + items.join(QStringLiteral(", ")) + u']';
}

QStringList normalizedExtensions(const QStringList &list)
{
    QStringList out;
    for (const QString &item : list) {
        for (QString part : item.split(QRegularExpression(QStringLiteral("[\\s,]+")), Qt::SkipEmptyParts)) {
            while (part.startsWith(u'.'))
                part.remove(0, 1);
            part = part.toLower();
            if (!part.isEmpty() && !out.contains(part))
                out.append(part);
        }
    }
    return out;
}

TomlPatch::Result changeEmulatorLists(QString text, const QList<Platform> &platforms, const QString &emulator,
                                      const QStringList &join)
{
    TomlPatch::Result r{true, text, {}};
    for (const Platform &p : platforms) {
        QStringList list = p.emulators;
        if (join.isEmpty() ? list.removeAll(emulator) == 0 : !join.contains(p.id) || list.contains(emulator))
            continue;
        if (!join.isEmpty())
            list.append(emulator);
        const TomlPatch::Block block{QStringLiteral("platform"), p.id};
        r = list.isEmpty() ? TomlPatch::removeBlockValue(r.text, block, QStringLiteral("emulators"))
                           : TomlPatch::setBlockValue(r.text, block, QStringLiteral("emulators"), tomlList(list));
        if (!r.ok)
            return r;
    }
    return r;
}

QString defaultTagsFile()
{
    QFile file(QStringLiteral(":/qt/qml/Launcher/resources/tags.toml"));
    return file.open(QIODevice::ReadOnly) ? QString::fromUtf8(file.readAll()) : QString();
}

const QString kEmulatorsHeader = QStringLiteral(
    "# Emulators. Ryubing and Azahar are built in: list one here to set its install or\n"
    "# change a key. Any other id defines a new emulator; see emulators.reference.toml.\n");

const QString kSteamIndexHeader = QStringLiteral(
    "# Steam games found by the import, kept when uninstalled, as app ID = title. Written by\n"
    "# the Steam import, which adds games and never renames one: you can fix a title here.\n");

const QString kTagsHeader = QStringLiteral(
    "# Tags and their colors. `color` is an index into the theme's tags.palette.\n"
    "# A game can use a tag that isn't listed here; it shows in the first color.\n");

FileStamp stampOf(const QFileInfo &info)
{
    return {info.lastModified(QTimeZone::UTC).toMSecsSinceEpoch(), info.size()};
}

QString gameIdOf(const QString &relativePath)
{
    if (!relativePath.startsWith(kGamesPrefix))
        return {};
    return relativePath.mid(kGamesPrefix.size()).chopped(5);
}

QString romPlatformOf(const QString &relativePath)
{
    if (!relativePath.startsWith(kRomsPrefix))
        return {};
    return relativePath.mid(kRomsPrefix.size()).chopped(5);
}

QHash<QString, FileStamp> listLibraryFiles(const QString &dataDir)
{
    QHash<QString, FileStamp> out;
    const QDir root(dataDir);
    for (const QString &path : {kLibraryInfo, kPlatforms, kTags, kSteamIndex, kEmulators}) {
        const QFileInfo info(root.filePath(path));
        if (info.isFile())
            out.insert(path, stampOf(info));
    }
    for (const QString &prefix : {kGamesPrefix, kRomsPrefix}) {
        QDirIterator it(root.filePath(prefix), {QStringLiteral("*.toml")}, QDir::Files);
        while (it.hasNext()) {
            const QFileInfo info = it.nextFileInfo();
            out.insert(prefix + info.fileName(), stampOf(info));
        }
    }
    return out;
}

ArtSet scanArtFolder(const QString &folder)
{
    ArtSet art;
    int bestRank[4] = {99, 99, 99, 99}; // prefer png, then jpg, jpeg, webp
    QDirIterator files(folder, QDir::Files);
    while (files.hasNext()) {
        const QFileInfo file = files.nextFileInfo();
        const int rank = int(kArtExtensions.indexOf(file.suffix().toLower()));
        const int kind = int(kArtKinds.indexOf(file.completeBaseName()));
        if (rank < 0 || kind < 0 || rank >= bestRank[kind])
            continue;
        bestRank[kind] = rank;
        QString *paths[] = {&art.hero, &art.logo, &art.icon, &art.cover};
        *paths[kind] = file.absoluteFilePath();
        art.modified = qMax(art.modified, file.lastModified(QTimeZone::UTC).toMSecsSinceEpoch());
    }
    return art;
}

QHash<QString, ArtSet> scanArt(const QString &dataDir)
{
    QHash<QString, ArtSet> out;
    QDirIterator games(dataDir + QStringLiteral("/art"), QDir::Dirs | QDir::NoDotAndDotDot);
    while (games.hasNext()) {
        const QFileInfo gameDir = games.nextFileInfo();
        const ArtSet art = scanArtFolder(gameDir.filePath());
        if (art != ArtSet{})
            out.insert(gameDir.fileName(), art);
    }
    return out;
}

// Reading a header costs about 20 µs, so every file of a 1000-game library adds up to 80 ms:
// only the folders that changed since the last check are read again.
QHash<QString, ArtCheck> checkArt(const QHash<QString, ArtSet> &art, const QHash<QString, ArtCheck> &previous)
{
    QHash<QString, ArtCheck> out;
    for (auto it = art.cbegin(); it != art.cend(); ++it) {
        if (const auto before = previous.constFind(it.key()); before != previous.cend() && before->art == it.value()) {
            out.insert(it.key(), *before);
            continue;
        }
        ArtCheck check{it.value(), {}, !it->logo.isEmpty()};
        const QString paths[] = {it->hero, it->logo, it->icon, it->cover};
        for (qsizetype i = 0; i < kArtKinds.size(); ++i) {
            if (paths[i].isEmpty())
                continue;
            const QSize size = QImageReader(paths[i]).size();
            const int limit = ArtImage::maxSide(kArtKinds.at(i));
            if (size.width() > limit || size.height() > limit)
                check.oversized.append(kArtKinds.at(i));
        }
        out.insert(it.key(), check);
    }
    return out;
}

bool parseInto(LibrarySnapshot &s, const QString &path, const QByteArray &bytes, QStringList *warnings)
{
    if (path == kLibraryInfo) {
        const auto r = parseLibraryInfo(bytes, path);
        *warnings = r.warnings;
        if (r.ok)
            s.version = r.value.version;
        return r.ok;
    } else if (path == kPlatforms) {
        const auto r = parsePlatforms(bytes, path);
        *warnings = r.warnings;
        if (r.ok)
            s.content.platforms = r.value;
        return r.ok;
    } else if (path == kTags) {
        const auto r = parseTags(bytes, path);
        *warnings = r.warnings;
        if (r.ok)
            s.content.tags = r.value;
        return r.ok;
    } else if (path == kSteamIndex) {
        const auto r = parseSteamIndex(bytes, path);
        *warnings = r.warnings;
        if (r.ok)
            s.content.steam = r.value;
        return r.ok;
    } else if (path == kEmulators) {
        const auto r = Emulators::parseFile(bytes, path);
        *warnings = r.warnings;
        if (r.ok)
            s.content.emulators = r.value;
        return r.ok;
    } else if (path.startsWith(kRomsPrefix)) {
        const auto r = parseRomIndex(bytes, path);
        *warnings = r.warnings;
        if (r.ok)
            s.content.roms.insert(romPlatformOf(path), r.value);
        else if (s.content.roms.contains(romPlatformOf(path)))
            warnings->append(QStringLiteral("%1: showing the last version that loaded").arg(path));
        return r.ok;
    }
    const auto r = parseGameFile(bytes, path);
    *warnings = r.warnings;
    if (r.ok)
        s.content.games.insert(gameIdOf(path), r.game);
    else if (s.content.games.contains(gameIdOf(path)))
        warnings->append(QStringLiteral("%1: showing the last version that loaded").arg(path));
    return r.ok;
}

void forget(LibrarySnapshot &s, const QString &path)
{
    s.files.remove(path);
    if (path == kLibraryInfo)
        s.version = 0;
    else if (path == kPlatforms)
        s.content.platforms.clear();
    else if (path == kTags)
        s.content.tags.clear();
    else if (path == kSteamIndex)
        s.content.steam.clear();
    else if (path == kEmulators)
        s.content.emulators.clear();
    else if (path.startsWith(kRomsPrefix))
        s.content.roms.remove(romPlatformOf(path));
    else
        s.content.games.remove(gameIdOf(path));
}

QSet<QString> claimedRoms(const QHash<QString, RomFile> &romGameFiles)
{
    QSet<QString> out;
    for (const RomFile &f : romGameFiles)
        out.insert(f.path);
    return out;
}

QSet<QString> takenIds(const LibraryContent &c)
{
    QSet<QString> out;
    for (auto it = c.games.cbegin(); it != c.games.cend(); ++it)
        out.insert(it.key());
    for (const IndexEntry &e : c.steam)
        out.insert(GameId::forSteamApp(e.appId));
    for (const QList<RomEntry> &index : c.roms) {
        for (const RomEntry &e : index)
            out.insert(e.id);
    }
    return out;
}

QString *artPath(ArtSet &art, const QString &kind)
{
    QString *paths[] = {&art.hero, &art.logo, &art.icon, &art.cover};
    return paths[kArtKinds.indexOf(kind)];
}

// Copied under a hidden name, then renamed, so a scan never sees half a file.
QStringList copySteamArt(const QString &dataDir, LibrarySnapshot &s)
{
    QList<std::pair<QString, qint64>> games;
    QSet<QString> seen;
    const auto add = [&](const QString &id, qint64 appId) {
        if (appId > 0 && !seen.contains(id)) {
            seen.insert(id);
            games.append({id, appId});
        }
    };
    for (const IndexEntry &e : std::as_const(s.content.steam))
        add(GameId::forSteamApp(e.appId), e.appId);
    for (auto it = s.content.games.cbegin(); it != s.content.games.cend(); ++it) {
        if (it->platform == kSteamPlatform)
            add(it.key(), GameId::steamAppId(it.key()));
        else if (it->platform == kProtonPlatform)
            add(it.key(), it->shortcut);
    }

    QStringList copied;
    for (const auto &[id, appId] : std::as_const(games)) {
        ArtSet have = s.art.value(id);
        const QStringList skip = s.content.games.value(id).skipSteamArt;
        QStringList missing;
        for (const QString &kind : kArtKinds) {
            if (artPath(have, kind)->isEmpty() && !skip.contains(kind))
                missing.append(kind);
        }
        if (missing.isEmpty())
            continue;
        const QHash<QString, QString> found = SteamArt::find(s.steam.install, appId, missing);
        if (found.isEmpty())
            continue;

        const QString folder = dataDir + QStringLiteral("/art/") + id;
        QDir().mkpath(folder);
        for (auto it = found.cbegin(); it != found.cend(); ++it) {
            const QString ext = QFileInfo(it.value()).suffix().toLower();
            if (!kArtExtensions.contains(ext))
                continue;
            const QString temp = folder + QStringLiteral("/.steam-") + it.key() + u'.' + ext;
            const QString target = folder + u'/' + it.key() + u'.' + ext;
            if (const QString error = ArtImage::copy(it.value(), temp, it.key()); !error.isEmpty()
                || !QFile::rename(temp, target)) {
                QFile::remove(temp);
                qCWarning(lcSteam).noquote() << "could not copy" << it.value() << "to" << target << error;
                continue;
            }
            copied.append(id + u'/' + it.key());
        }
        if (const ArtSet art = scanArtFolder(folder); art != ArtSet{})
            s.art.insert(id, art);
        else
            QDir().rmdir(folder);
    }
    return copied;
}

QList<SteamSync::Game> steamSyncGames(const LibraryContent &c, const QHash<QString, QString> &launchOptions = {})
{
    QList<SteamSync::Game> out;
    QSet<QString> seen;
    const auto add = [&](const QString &id) {
        if (seen.contains(id))
            return;
        seen.insert(id);
        const qint64 appId = GameId::steamAppId(id);
        const GameFile file = c.games.value(id);
        if (appId <= 0 || (!file.platform.isEmpty() && file.platform != kSteamPlatform))
            return;
        out.append({id, appId, launchOptions.value(id, file.launchOptions), file.proton});
    };
    for (const IndexEntry &e : c.steam)
        add(GameId::forSteamApp(e.appId));
    for (auto it = c.games.cbegin(); it != c.games.cend(); ++it) {
        if (it->platform == kSteamPlatform)
            add(it.key());
    }
    return out;
}

QString shortcutStartFolder(QString path)
{
    while (path.size() > 1 && path.endsWith(u'/'))
        path.chop(1);
    return path;
}

QList<SteamSync::ShortcutGame> shortcutSyncGames(const LibraryContent &c, const QString &dataDir, const QString &home,
                                                 const SteamArt::GridFiles &grid, const SteamInstall &install)
{
    QList<SteamSync::ShortcutGame> out;
    for (auto it = c.games.cbegin(); it != c.games.cend(); ++it) {
        const GameFile &f = *it;
        if (f.platform != kProtonPlatform || f.shortcut <= 0 || f.exe.isEmpty())
            continue;
        const QString icon = grid.value(f.shortcut).contains(QStringLiteral("icon"))
            ? SteamArt::gridIconPath(install, f.shortcut)
            : QString();
        out.append({it.key(), quint32(f.shortcut), f.title, resolveRomFolder(f.exe, dataDir, home),
                    f.startDir.isEmpty() ? QString() : resolveRomFolder(f.startDir, dataDir, home), f.launchOptions,
                    f.proton, f.installed, icon});
    }
    return out;
}

QHash<qint64, QHash<QString, QString>> shortcutArt(const LibrarySnapshot &s)
{
    QHash<qint64, QHash<QString, QString>> out;
    for (auto it = s.content.games.cbegin(); it != s.content.games.cend(); ++it) {
        if (it->platform != kProtonPlatform || it->shortcut <= 0)
            continue;
        ArtSet art = s.art.value(it.key());
        QHash<QString, QString> &kinds = out[it->shortcut];
        for (const QString &kind : kArtKinds) {
            if (const QString *path = artPath(art, kind); !path->isEmpty())
                kinds.insert(kind, *path);
        }
    }
    return out;
}

void syncSteam(LibrarySnapshot &s, ScanResult &out, const SteamSettings &settings, const QString &recordPath,
               const QString &libraryId,
               const QString &dataDir, const QString &home)
{
    out.steamRecordBefore = SteamSync::readRecord(recordPath, libraryId, &out.steamSyncLog);

    // On top of what Steam has, so only once Steam's launch options are known.
    QHash<QString, QString> defaults;
    if (s.steam.localConfigOk && !settings.defaultPresets.isEmpty()) {
        for (const IndexEntry &e : std::as_const(out.steamWrites)) {
            const QString id = GameId::forSteamApp(e.appId);
            if (!s.content.games.value(id).launchOptions.isEmpty())
                continue;
            const QString options = SteamSync::withDefaults(s.steam.apps.value(e.appId).launchOptions, settings);
            if (!options.isEmpty())
                defaults.insert(id, options);
        }
    }

    // The launcher's art into grid/ for Proton games, right away: Steam doesn't overwrite it on
    // exit. Before copySteamArt(), so a file of the launcher's that goes isn't copied back.
    SteamSync::Record record = out.steamRecordBefore;
    if (s.steam.shortcutsOk)
        out.steamGridLog = SteamArt::writeGrid(SteamArt::gridDir(s.steam.install), shortcutArt(s), &record.grid);

    SteamSync::Result r = SteamSync::sync(steamSyncGames(s.content, defaults), s.steam, record,
                                          shortcutSyncGames(s.content, dataDir, home, record.grid, s.steam.install));
    for (auto it = defaults.cbegin(); it != defaults.cend(); ++it)
        out.steamGameWrites[it.key()].append({QStringLiteral("launch_options"), it.value()});
    for (auto it = r.copies.cbegin(); it != r.copies.cend(); ++it)
        out.steamGameWrites[it.key()] += it.value();
    s.steamQueued = std::move(r.queued);
    out.steamRecord = std::move(r.record);
    out.steamSyncLog += r.log;
}

// Steam's play time against what was seen last: what grew becomes sessions. Nothing is written
// here; the store does that when the scan is kept.
void diffSteamPlay(const LibrarySnapshot &s, ScanResult &out, const QString &dataDir, bool startup)
{
    Parsed<PlayHistory::SeenTotals> seen = PlayHistory::readSeen(dataDir);
    if (!seen.ok) {
        out.historyProblems += seen.warnings;
        return;
    }
    out.steamDiffed = true;

    QList<PlayHistory::SteamPlayed> played;
    for (const SteamApp &app : s.steam.games()) {
        const auto config = s.steam.apps.constFind(app.appId);
        played.append({GameId::forSteamApp(app.appId), config != s.steam.apps.cend() ? config->playMinutes : -1,
                       config != s.steam.apps.cend() ? config->lastPlayed : 0});
    }
    if (s.steam.shortcutsOk) {
        QHash<quint32, qint64> lastPlayed;
        for (const SteamShortcut &shortcut : s.steam.shortcuts)
            lastPlayed.insert(shortcut.appId, shortcut.lastPlayTime);
        for (auto it = s.content.games.cbegin(); it != s.content.games.cend(); ++it) {
            if (it->platform == kProtonPlatform && it->shortcut > 0)
                played.append({it.key(), s.steam.shortcutConfig(quint32(it->shortcut)).playMinutes,
                               lastPlayed.value(quint32(it->shortcut))});
        }
    }

    // A change found by the first diff of a run may hold sessions played while the launcher was closed.
    const QString user = s.steam.install.userId;
    PlayHistory::SteamDiff diff = PlayHistory::diffSteam(seen.value.value(user), played, startup);
    if (!diff.changed)
        return;
    seen.value.insert(user, diff.seen);
    out.historySeen = std::move(seen.value);
    out.historySeenChanged = true;
    out.historySessions = std::move(diff.sessions);
}
}

ScanResult scanLibrary(const QString &dataDir, const LibrarySnapshot &previous, const ScanOptions &options)
{
    QElapsedTimer timer;
    timer.start();

    ScanResult out;
    out.snapshot = previous;
    LibrarySnapshot &s = out.snapshot;

    const QHash<QString, FileStamp> onDisk = listLibraryFiles(dataDir);
    const LibraryCache::Changes changes = LibraryCache::diff(onDisk, previous.files);
    for (const QString &path : changes.removed)
        forget(s, path);

    const QDir root(dataDir);
    for (const QStringList *list : {&changes.added, &changes.changed}) {
        for (const QString &path : *list) {
            CachedFile &entry = s.files[path];
            entry.stamp = onDisk.value(path);
            entry.warnings.clear();

            const QString id = gameIdOf(path);
            if (path.startsWith(kGamesPrefix) && !GameId::isValid(id)) {
                entry.warnings.append(
                    QStringLiteral("%1: skipped; game file names may only use lowercase a-z, 0-9 and -").arg(path));
                continue;
            }
            if (path.startsWith(kRomsPrefix) && !GameId::isValid(romPlatformOf(path))) {
                entry.warnings.append(
                    QStringLiteral("%1: skipped; ROM index names are platform ids (lowercase a-z, 0-9 and -)").arg(path));
                continue;
            }
            QFile file(root.filePath(path));
            if (!file.open(QIODevice::ReadOnly)) {
                entry.warnings.append(QStringLiteral("%1: could not read: %2").arg(path, file.errorString()));
                continue;
            }
            entry.ok = parseInto(s, path, file.readAll(), &entry.warnings);
            ++out.parsed;
        }
    }

    s.art = scanArt(dataDir);
    s.artChecks = checkArt(s.art, previous.artChecks);
    s.romGameFiles = findRomGameFiles(s.content.games, dataDir, options.installs.home);
    s.programFiles = findProgramFiles(s.content.games, dataDir, options.installs.home, previous.programFiles);
    out.changed = !changes.isEmpty() || s.art != previous.art || s.artChecks != previous.artChecks || s.romGameFiles != previous.romGameFiles
        || s.programFiles != previous.programFiles;

    const auto touched = [&](const QString &path) {
        return changes.added.contains(path) || changes.changed.contains(path) || changes.removed.contains(path);
    };
    const bool emulatorsChanged = (touched(kPlatforms) && !samePlatformsBesidesOrder(previous.content.platforms, s.content.platforms))
        || touched(kEmulators);
    const bool romsDue = !options.setup && (options.roms || !previous.romsScanned || emulatorsChanged);
    if (romsDue || (options.setup && (!previous.installsDetected || emulatorsChanged))) {
        QElapsedTimer romTimer;
        romTimer.start();
        QStringList ignored;
        out.emulators = Emulators::merge(options.presets, s.content.emulators, &ignored);
        if (romsDue) {
            RomScanResult roms = scanRomFolders(s.content.platforms, out.emulators, s.content.roms,
                                                takenIds(s.content), dataDir, options.installs.home,
                                                claimedRoms(s.romGameFiles));
            for (auto it = roms.indexes.cbegin(); it != roms.indexes.cend(); ++it) {
                s.content.roms.insert(it.key(), it.value());
                const QString path = kRomsPrefix + it.key() + QStringLiteral(".toml");
                if (s.files.value(path).ok) {
                    out.romWrites.insert(it.key(), it.value());
                    out.romAdded += roms.added.value(it.key());
                }
            }
            s.romsScanned = true;
            out.changed = out.changed || !roms.indexes.isEmpty() || roms.files != previous.romFiles
                || roms.warnings != previous.romWarnings || roms.missingFolders != previous.romMissingFolders;
            s.romFiles = std::move(roms.files);
            s.romWarnings = std::move(roms.warnings);
            s.romMissingFolders = std::move(roms.missingFolders);
            out.romLog = std::move(roms.log);
            out.romsScanned = true;
        }
        s.installs = Emulators::detectAll(out.emulators, options.installs);
        s.installsDetected = true;
        out.installsDetected = true;
        out.changed = out.changed || s.installs != previous.installs;
        out.romsMs = romTimer.nsecsElapsed() / 1e6;
    }

    QByteArray settings;
    if (QFile file(root.filePath(QStringLiteral("settings.toml"))); file.open(QIODevice::ReadOnly))
        settings = file.readAll();
    Parsed<SteamSettings> steamSettings = parseSteamSettings(settings, QStringLiteral("settings.toml"));
    if (QString &path = steamSettings.value.path; !path.isEmpty() && !path.startsWith(u'~'))
        path = QDir(dataDir).absoluteFilePath(path);
    s.steam = SteamScan::scan(steamSettings.value, options.steam, previous.steam);
    if (steamSettings.ok)
        s.steam.warnings = steamSettings.warnings + s.steam.warnings;
    out.steamChanged = s.steam != previous.steam;
    if (s.steam.install.found() && !options.setup) {
        QSet<qint64> known;
        for (const IndexEntry &e : std::as_const(s.content.steam))
            known.insert(e.appId);
        for (const SteamApp &app : s.steam.games()) {
            if (!known.contains(app.appId))
                out.steamWrites.append({app.appId, app.name.isEmpty() ? QStringLiteral("App %1").arg(app.appId) : app.name});
        }
        s.content.steam += out.steamWrites;
        if (!s.files.value(kSteamIndex).ok)
            out.steamWrites.clear();
    }
    out.changed = out.changed || out.steamChanged || s.content.steam != previous.content.steam;

    s.steamQueued.clear();
    if (!options.steamSyncRecord.isEmpty() && s.steam.install.found() && !options.setup)
        syncSteam(s, out, steamSettings.value, options.steamSyncRecord, options.libraryId, dataDir, options.installs.home);
    out.changed = out.changed || s.steamQueued != previous.steamQueued || !out.steamGameWrites.isEmpty();

    if (options.steamHistory && !options.setup && s.steam.install.found() && s.steam.localConfigOk
        && !s.steam.install.userId.isEmpty() && (options.steamStartup || out.steamChanged))
        diffSteamPlay(s, out, dataDir, options.steamStartup);

    if (options.copySteamArt && s.steam.install.found() && !options.setup)
        out.steamArtCopied = copySteamArt(dataDir, s);
    out.changed = out.changed || !out.steamArtCopied.isEmpty();

    out.elapsedMs = timer.elapsed();
    return out;
}

LibraryStore::LibraryStore(const QString &dataDir, const QString &cacheDir, NewLibrary newLibrary, QObject *parent)
    : QObject(parent)
    , m_dataDir(QDir(dataDir).absolutePath())
    , m_cachePath(LibraryCache::pathFor(cacheDir, dataDir))
    , m_versionStatus(LibraryVersion::prepare(m_dataDir, LibraryVersion::kCurrent, LibraryVersion::upgrades(),
                                              newLibrary == NewLibrary::WithSetup))
    , m_setupPending(m_versionStatus.setupPending)
{
    if (!m_versionStatus.readOnly)
        QDir().mkpath(m_dataDir + QStringLiteral("/library/games"));
    if (!m_versionStatus.readOnly && !QFileInfo::exists(m_dataDir + u'/' + kPlatforms))
        fileToOpen(kPlatforms, defaultPlatforms());
    // Only a new library gets the default tags: in an older one, deleted tags stay deleted.
    if (m_versionStatus.created)
        fileToOpen(kTags, defaultTagsFile());

    QElapsedTimer timer;
    timer.start();
    if (std::optional<LibrarySnapshot> cached = LibraryCache::load(m_cachePath, m_dataDir)) {
        m_snapshot = std::move(*cached);
        qCInfo(lcLibrary).noquote() << QStringLiteral("loaded %1 game files from the cache in %2 ms")
                                           .arg(m_snapshot.content.games.size())
                                           .arg(timer.nsecsElapsed() / 1e6, 0, 'f', 1);
    } else {
        m_loading = true;
        qCInfo(lcLibrary) << "no usable cache; the library loads in the background";
    }

    connect(&m_scan, &QFutureWatcher<ScanResult>::finished, this, &LibraryStore::onScanFinished);
    connect(&m_shrink, &QFutureWatcher<ArtShrinkJob>::finished, this, &LibraryStore::onShrinkFinished);

    // Editors save by writing in place or by replacing the file, and a save can arrive as
    // several events; wait for them to settle.
    m_watchTimer.setSingleShot(true);
    m_watchTimer.setInterval(200);
    connect(&m_watcher, &QFileSystemWatcher::fileChanged, &m_watchTimer, qOverload<>(&QTimer::start));
    connect(&m_watcher, &QFileSystemWatcher::directoryChanged, &m_watchTimer, qOverload<>(&QTimer::start));
    connect(&m_watchTimer, &QTimer::timeout, this, &LibraryStore::refresh);
    m_steamTimer.setSingleShot(true);
    m_steamTimer.setInterval(1000);
    connect(&m_steamWatcher, &QFileSystemWatcher::fileChanged, &m_steamTimer, qOverload<>(&QTimer::start));
    connect(&m_steamWatcher, &QFileSystemWatcher::directoryChanged, &m_steamTimer, qOverload<>(&QTimer::start));
    connect(&m_steamTimer, &QTimer::timeout, this, &LibraryStore::refresh);
    m_steamApplyTimer.setSingleShot(true);
    m_steamApplyTimer.setInterval(0);
    connect(&m_steamApplyTimer, &QTimer::timeout, this, &LibraryStore::applySteamQueue);
    // Steam has written its files by the time its process is gone.
    connect(&m_steamExit, &SteamExitWatch::exited, this, [this] {
        qCInfo(lcSteam) << "Steam closed";
        applySteamQueue();
    });

    m_saveTimer.setSingleShot(true);
    m_saveTimer.setInterval(500);
    connect(&m_saveTimer, &QTimer::timeout, this, &LibraryStore::saveCache);
    connect(&m_save, &QFutureWatcher<bool>::finished, this, [this] {
        if (!m_save.result())
            qCWarning(lcLibrary).noquote() << "could not save the library cache to" << m_cachePath;
        if (m_saveAgain) {
            m_saveAgain = false;
            if (!m_held)
                saveCache();
        }
    });
}

LibraryStore::~LibraryStore()
{
    // Don't lose a pending save on quit, nor one waiting for the running save to finish (its watcher's
    // signal doesn't come from here).
    if ((m_saveTimer.isActive() || m_saveAgain) && !m_held) {
        m_saveTimer.stop();
        m_save.waitForFinished();
        LibraryCache::save(m_cachePath, m_dataDir, m_snapshot);
    }
    waitForIdle();
}

void LibraryStore::refresh()
{
    if (m_held) {
        m_refreshOnResume = true;
        return;
    }
    if (m_scan.isRunning()) {
        m_scanAgain = true;
        return;
    }
    m_scanGeneration = m_generation;
    ScanOptions options;
    options.setup = m_setupPending;
    options.roms = !m_setupPending && std::exchange(m_scanRoms, false);
    options.presets = Emulators::presets();
    options.installs = m_installLocations;
    options.steam = m_steamLocations;
    options.copySteamArt = !m_versionStatus.readOnly;
    if (!m_versionStatus.readOnly)
        options.steamSyncRecord = m_steamSyncRecord;
    options.libraryId = m_versionStatus.id;
    options.steamHistory = !m_versionStatus.readOnly;
    options.steamStartup = !m_steamHistoryStarted;
    m_scan.setFuture(QtConcurrent::run(scanLibrary, m_dataDir, m_snapshot, options));
    emit scanningChanged();
}

void LibraryStore::scanRoms()
{
    m_scanRoms = true;
    refresh();
}

void LibraryStore::hold(bool on)
{
    m_held = on;
    if (on) {
        m_saveTimer.stop();
        m_watchTimer.stop();
        m_steamTimer.stop();
    } else if (std::exchange(m_refreshOnResume, false)) {
        refresh();
    }
}

QString LibraryStore::finishSetup()
{
    if (!m_setupPending)
        return {};
    m_setupPending = false;
    emit setupPendingChanged();
    QString error;
    if (!TomlPatch::removeValueInFile(m_dataDir + u'/' + kLibraryInfo, {}, QStringLiteral("setup"), &error))
        qCWarning(lcLibrary).noquote() << QStringLiteral("could not end the setup in %1: %2").arg(kLibraryInfo, error);
    scanRoms();
    return error;
}

void LibraryStore::forgetCache()
{
    m_save.waitForFinished();
    QFile::remove(m_cachePath);
}

void LibraryStore::waitForIdle()
{
    m_scan.waitForFinished();
    m_save.waitForFinished();
}

void LibraryStore::onScanFinished()
{
    if (m_held) {
        // It read the files as they were; what it would write must wait for the hold to end.
        m_refreshOnResume = true;
        emit scanningChanged();
        return;
    }
    const ScanResult result = m_scan.result();
    m_filesParsed += result.parsed;
    if (m_scanGeneration != m_generation) {
        // Something changed here while the scan ran; its result is out of date.
        m_scanAgain = true;
        m_scanRoms = m_scanRoms || result.romsScanned;
        // Except for the grid files it wrote or deleted, which the record must know about.
        if (result.steamRecord && result.steamRecord->grid != result.steamRecordBefore.grid
            && !m_versionStatus.readOnly) {
            SteamSync::Record record = m_steamRecord.value_or(result.steamRecordBefore);
            record.grid = result.steamRecord->grid;
            m_steamRecord = record;
            QString error;
            if (!SteamSync::writeRecord(m_steamSyncRecord, record, m_dataDir, &error))
                qCWarning(lcSteam).noquote() << error;
        }
    } else {
        if (result.changed) {
            m_snapshot = result.snapshot;
            writeRomIndexes(result.romWrites);
            writeSteamIndex(result.steamWrites);
            addBuiltInPlatforms();
            ++m_generation;
            emit changed();
            scheduleCacheSave();
        } else if (addBuiltInPlatforms()) {
            // Nothing else differs, but a built-in platform was missing (linux, in a library
            // from before it).
            m_snapshot.romsScanned = m_snapshot.romsScanned || result.romsScanned;
            m_snapshot.installsDetected = m_snapshot.installsDetected || result.installsDetected;
            ++m_generation;
            emit changed();
            scheduleCacheSave();
        } else if ((result.romsScanned && !m_snapshot.romsScanned)
                   || (result.installsDetected && !m_snapshot.installsDetected)) {
            m_snapshot.romsScanned = m_snapshot.romsScanned || result.romsScanned;
            m_snapshot.installsDetected = true;
            emit changed();
        }
        QStringList added = std::exchange(m_protonAdded, {});
        if (result.changed) {
            added += result.romAdded;
            for (const IndexEntry &e : result.steamWrites)
                added.append(GameId::forSteamApp(e.appId));
        }
        if (!added.isEmpty() && !m_versionStatus.readOnly)
            emit gamesAdded(added);
        applySteamSync(result);
        applySteamHistory(result);
        if (!m_snapshot.steamQueued.isEmpty())
            m_steamApplyTimer.start();
        if (result.romsScanned && !m_versionStatus.readOnly
            && !Emulators::writeReference(m_dataDir, result.emulators))
            qCWarning(lcLibrary) << "could not write library/emulators.reference.toml";
    }
    qCInfo(lcLibrary).noquote() << QStringLiteral("checked the library in %1 ms: %2 file(s) parsed%3")
                                       .arg(result.elapsedMs)
                                       .arg(result.parsed)
                                       .arg(result.changed ? QString() : QStringLiteral(", nothing changed"));
    if (result.romsScanned) {
        qCInfo(lcLibrary).noquote() << QStringLiteral("scanned the ROM folders in %1 ms").arg(result.romsMs, 0, 'f', 1);
        for (const QString &line : result.romLog)
            qCInfo(lcLibrary).noquote() << line;
        for (const EmulatorDef &e : result.emulators) {
            const EmulatorInstall install = result.snapshot.installs.value(e.id);
            qCInfo(lcLibrary).noquote() << QStringLiteral("emulator %1: %2%3")
                                               .arg(e.id, install.describe(m_installLocations.home),
                                                    install.pathMissing ? QStringLiteral(" (its path doesn't exist)")
                                                                        : QString());
        }
    }

    if (!result.steamArtCopied.isEmpty())
        qCInfo(lcSteam).noquote() << QStringLiteral("copied Steam's art: %1").arg(result.steamArtCopied.join(QStringLiteral(", ")));
    for (const QString &line : result.steamGridLog)
        qCInfo(lcSteam).noquote() << line;
    if (result.steamChanged) {
        const SteamState &steam = result.snapshot.steam;
        if (!steam.enabled) {
            qCInfo(lcSteam) << "the Steam import is off";
        } else if (!steam.install.found()) {
            qCInfo(lcSteam) << "Steam not found";
        } else {
            const QList<SteamApp> games = steam.games();
            const qsizetype installed = std::count_if(games.cbegin(), games.cend(), [](const SteamApp &a) { return a.installed; });
            qCInfo(lcSteam).noquote() << QStringLiteral("Steam at %1, user %2: %3 game(s), %4 installed; %5 shortcut(s); "
                                                        "%6 compatibility tool(s)")
                                             .arg(steam.install.root, steam.install.userId.isEmpty() ? QStringLiteral("none") : steam.install.userId)
                                             .arg(games.size())
                                             .arg(installed)
                                             .arg(steam.shortcuts.size())
                                             .arg(steam.tools().size());
        }
    }

    if (m_loading) {
        m_loading = false;
        emit loadingChanged();
        scheduleCacheSave(); // even for an empty library, so the next start has a cache
    }
    updateWatches();

    if (m_scanAgain) {
        m_scanAgain = false;
        refresh();
    } else {
        emit scanningChanged();
    }
}

void LibraryStore::updateWatches()
{
    // Directories catch added, removed and replaced files; file watches catch edits in
    // place. A replaced file drops its watch, so re-add whatever is missing each time.
    QStringList wanted = {m_dataDir + QStringLiteral("/library"), m_dataDir + QStringLiteral("/library/games")};
    if (QFileInfo::exists(m_dataDir + QStringLiteral("/library/roms")))
        wanted.append(m_dataDir + QStringLiteral("/library/roms"));
    const QString art = m_dataDir + QStringLiteral("/art");
    if (QFileInfo::exists(art)) {
        wanted.append(art);
        for (auto it = m_snapshot.art.cbegin(); it != m_snapshot.art.cend(); ++it)
            wanted.append(art + u'/' + it.key());
    }
    for (auto it = m_snapshot.files.cbegin(); it != m_snapshot.files.cend(); ++it)
        wanted.append(m_dataDir + u'/' + it.key());

    const auto watch = [](QFileSystemWatcher &watcher, const QStringList &paths) {
        const QStringList watched = watcher.files() + watcher.directories();
        const QSet<QString> have(watched.cbegin(), watched.cend());
        QStringList missing;
        for (const QString &path : paths) {
            if (!have.contains(path) && QFileInfo::exists(path))
                missing.append(path);
        }
        if (!missing.isEmpty())
            watcher.addPaths(missing);
    };
    watch(m_watcher, wanted);
    // [steam] can change where Steam is, or turn it off. The data dir catches settings.toml
    // being created or replaced.
    watch(m_steamWatcher, m_snapshot.steam.watchPaths() << m_dataDir + QStringLiteral("/settings.toml") << m_dataDir);
}

void LibraryStore::scheduleCacheSave()
{
    // A read-only library writes nothing, not even a cache claiming its files are current.
    if (m_versionStatus.readOnly || m_held)
        return;
    m_saveTimer.start();
}

void LibraryStore::saveCache()
{
    if (m_save.isRunning()) {
        m_saveAgain = true;
        return;
    }
    m_save.setFuture(QtConcurrent::run(LibraryCache::save, m_cachePath, m_dataDir, m_snapshot));
}

void LibraryStore::writeRomIndexes(const QMap<QString, QList<RomEntry>> &indexes)
{
    if (indexes.isEmpty() || m_versionStatus.readOnly)
        return;
    for (auto it = indexes.cbegin(); it != indexes.cend(); ++it) {
        if (const QString problem = writeRomIndex(it.key(), it.value()); !problem.isEmpty())
            qCWarning(lcLibrary).noquote() << problem;
    }
}

QString LibraryStore::writeRomIndex(const QString &platform, const QList<RomEntry> &entries)
{
    QDir().mkpath(m_dataDir + u'/' + kRomsPrefix);
    QString name = platform;
    for (const Platform &p : std::as_const(m_snapshot.content.platforms)) {
        if (p.id == platform)
            name = p.name;
    }
    const QString relative = kRomsPrefix + platform + QStringLiteral(".toml");
    const QString path = m_dataDir + u'/' + relative;
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly) || file.write(formatRomIndex(name, entries).toUtf8()) < 0 || !file.commit())
        return tr("Could not save %1: %2").arg(relative, file.errorString());
    // Record the new stamp, so the watcher's scan sees the file as already up to date.
    m_snapshot.files[relative] = {stampOf(QFileInfo(path)), {}, true};
    qCInfo(lcLibrary).noquote() << "wrote" << relative;
    return {};
}

void LibraryStore::writeSteamIndex(const QList<IndexEntry> &added)
{
    if (added.isEmpty() || m_versionStatus.readOnly)
        return;
    const QString problem = patchAndLoad(kSteamIndex, kSteamIndexHeader, [&](const QString &text) {
        TomlPatch::Result r{true, text, {}};
        for (const IndexEntry &e : added) {
            r = TomlPatch::setValue(r.text, QStringLiteral("games"), QString::number(e.appId), tomlString(e.title));
            if (!r.ok)
                break;
        }
        return r;
    });
    if (problem.isEmpty())
        qCInfo(lcSteam).noquote() << QStringLiteral("added %1 game(s) to %2").arg(added.size()).arg(kSteamIndex);
    else
        qCWarning(lcSteam).noquote() << problem;
}

bool LibraryStore::addBuiltInPlatforms()
{
    const QList<Platform> &platforms = m_snapshot.content.platforms;
    if (m_versionStatus.readOnly || !m_snapshot.files.value(kPlatforms).ok)
        return false;
    bool added = false;
    const auto add = [&](const QString &id, const QString &name) {
        if (std::any_of(platforms.cbegin(), platforms.cend(), [&](const Platform &p) { return p.id == id; }))
            return;
        const QString problem = patchAndLoad(kPlatforms, defaultPlatforms(), [&](const QString &text) {
            if (TomlPatch::blockValues(text, QStringLiteral("platform"), QStringLiteral("id")).contains(id))
                return TomlPatch::Result{true, text, {}};
            return TomlPatch::appendBlock(text, QStringLiteral("platform"),
                                          {{QStringLiteral("id"), tomlString(id)},
                                           {QStringLiteral("name"), tomlString(name)},
                                           {QStringLiteral("icon"), tomlString(id)}});
        });
        if (problem.isEmpty()) {
            added = true;
            qCInfo(lcLibrary).noquote() << "added the" << name << "platform to" << kPlatforms;
        } else {
            qCWarning(lcLibrary).noquote() << problem;
        }
    };
    if (m_snapshot.steam.install.found())
        add(kSteamPlatform, QStringLiteral("Steam"));
    const QMap<QString, GameFile> &games = m_snapshot.content.games;
    if (std::any_of(games.cbegin(), games.cend(), [](const GameFile &g) { return g.platform == kProtonPlatform; }))
        add(kProtonPlatform, QStringLiteral("Proton"));
    add(kLinuxPlatform, QStringLiteral("Linux"));
    return added;
}

void LibraryStore::setStateDir(const QString &stateDir)
{
    m_steamSyncRecord = stateDir.isEmpty() || m_versionStatus.id.isEmpty()
        ? QString()
        : SteamSync::recordPath(stateDir, AppPaths::dataDirKey(m_dataDir));
    m_steamBackupDir = stateDir.isEmpty() ? QString() : stateDir + QStringLiteral("/steam-backup");
}

qint64 LibraryStore::steamPid() const
{
    return SteamLocate::runningPid(m_snapshot.steam.install, m_steamLocations);
}

QString LibraryStore::applySteamQueue()
{
    m_steamApplyTimer.stop();
    SteamState &steam = m_snapshot.steam;
    if (m_versionStatus.readOnly || !m_steamRecord || m_snapshot.steamQueued.isEmpty() || !steam.install.found())
        return {};
    if (const qint64 pid = steamPid(); pid > 0) {
        if (!m_steamExit.watching() && m_steamExit.watch(pid))
            qCInfo(lcSteam).noquote() << QStringLiteral("Steam is running; %1 game(s)' launch options or Proton "
                                                        "versions will be written when it closes")
                                             .arg(m_snapshot.steamQueued.size());
        return tr("Steam is running; this is applied when it closes");
    }

    QList<SteamSync::Edit> edits;
    QList<SteamSync::ShortcutEdit> shortcutEdits;
    QHash<qint64, QString> shortcutGames;
    const QList<SteamSync::ShortcutGame> protonGames = shortcutSyncGames(
        m_snapshot.content, m_dataDir, m_installLocations.home, m_steamRecord->grid, steam.install);
    QSet<quint32> removed;
    for (auto it = m_snapshot.steamQueued.cbegin(); it != m_snapshot.steamQueued.cend(); ++it) {
        if (const quint32 appId = SteamSync::removedAppId(it.key())) {
            removed.insert(appId);
            shortcutGames.insert(appId, m_steamRecord->removed.value(appId));
            const auto current = std::find_if(steam.shortcuts.cbegin(), steam.shortcuts.cend(),
                                              [&](const SteamShortcut &s) { return s.appId == appId; });
            if (current != steam.shortcuts.cend())
                shortcutEdits.append({SteamSync::ShortcutEdit::Kind::Remove, *current, *current});
            if (steam.configOk && steam.compatTools.contains(appId))
                edits.append({appId, QStringLiteral("proton"), QString(), steam.compatTools.value(appId)});
            continue;
        }
        const GameFile file = m_snapshot.content.games.value(it.key());
        if (file.platform == kProtonPlatform) {
            const auto game = std::find_if(protonGames.cbegin(), protonGames.cend(),
                                           [&](const SteamSync::ShortcutGame &g) { return g.id == it.key(); });
            if (game == protonGames.cend())
                continue;
            shortcutGames.insert(game->appId, game->id);
            const auto current = std::find_if(steam.shortcuts.cbegin(), steam.shortcuts.cend(),
                                              [&](const SteamShortcut &s) { return s.appId == game->appId; });
            const std::optional<SteamShortcut> now =
                current == steam.shortcuts.cend() ? std::nullopt : std::optional<SteamShortcut>(*current);
            // The game file's values on top of what Steam has; Steam writes StartDir with a slash.
            SteamShortcut wanted = now.value_or(SteamShortcut());
            wanted.appId = game->appId;
            wanted.name = game->title;
            wanted.exe = game->exe;
            wanted.startDir = game->startFolder() + u'/';
            wanted.launchOptions = game->launchOptions;
            const QStringList &keys = it.value();
            if (keys.contains(SteamSync::kAddShortcut) || keys.contains(SteamSync::kShortcutIcon))
                wanted.icon = game->icon;
            if (keys.contains(SteamSync::kRemoveShortcut)) {
                shortcutEdits.append({SteamSync::ShortcutEdit::Kind::Remove, wanted, now});
                continue;
            }
            if (keys.contains(SteamSync::kAddShortcut)) {
                shortcutEdits.append({SteamSync::ShortcutEdit::Kind::Add, wanted, std::nullopt});
                if (!game->proton.isEmpty() && steam.configOk)
                    edits.append({game->appId, QStringLiteral("proton"), game->proton, steam.compatTools.value(game->appId)});
                continue;
            }
            if (keys.size() > int(keys.contains(QStringLiteral("proton"))))
                shortcutEdits.append({SteamSync::ShortcutEdit::Kind::Update, wanted, now});
            if (keys.contains(QStringLiteral("proton")))
                edits.append({game->appId, QStringLiteral("proton"), game->proton, steam.compatTools.value(game->appId)});
            continue;
        }
        const qint64 appId = GameId::steamAppId(it.key());
        for (const QString &key : it.value()) {
            const bool proton = key == QLatin1String("proton");
            edits.append({appId, key, proton ? file.proton : file.launchOptions,
                          proton ? steam.compatTools.value(appId) : steam.apps.value(appId).launchOptions});
        }
    }
    const SteamSync::ApplyResult r =
        SteamSync::apply(steam, edits, m_steamBackupDir, [this] { return steamPid() > 0; }, shortcutEdits);
    for (const QString &line : r.log)
        qCInfo(lcSteam).noquote() << line;

    for (const SteamSync::ShortcutEdit &e : r.shortcutsApplied) {
        const quint32 appId = e.shortcut.appId;
        const auto current = std::find_if(steam.shortcuts.begin(), steam.shortcuts.end(),
                                          [&](const SteamShortcut &s) { return s.appId == appId; });
        const char *what = "";
        switch (e.kind) {
        case SteamSync::ShortcutEdit::Kind::Remove:
            if (current != steam.shortcuts.end())
                steam.shortcuts.erase(current);
            m_steamRecord->shortcuts.remove(appId);
            what = "removed its shortcut from";
            break;
        case SteamSync::ShortcutEdit::Kind::Add:
        case SteamSync::ShortcutEdit::Kind::Update: {
            if (current != steam.shortcuts.end())
                *current = e.shortcut;
            else
                steam.shortcuts.append(e.shortcut);
            SteamSync::ShortcutRecord record = SteamSync::recordOf(e.shortcut, steam.compatTools.value(appId));
            record.startDir = shortcutStartFolder(record.startDir);
            m_steamRecord->shortcuts.insert(appId, record);
            what = e.kind == SteamSync::ShortcutEdit::Kind::Add ? "added its shortcut to" : "applied its shortcut to";
            break;
        }
        }
        qCInfo(lcSteam).noquote() << QStringLiteral("%1: %2 Steam").arg(shortcutGames.value(appId), QLatin1String(what));
    }
    for (const SteamSync::Edit &e : r.applied) {
        const bool shortcut = shortcutGames.contains(e.appId);
        qCInfo(lcSteam).noquote() << QStringLiteral("%1: applied %2 \"%3\" to Steam")
                                         .arg(shortcut ? shortcutGames.value(e.appId) : GameId::forSteamApp(e.appId),
                                              e.key, e.value);
        if (e.key == QLatin1String("proton")) {
            if (e.value.isEmpty())
                steam.compatTools.remove(e.appId);
            else
                steam.compatTools.insert(e.appId, e.value);
            if (shortcut) {
                if (!removed.contains(quint32(e.appId)))
                    m_steamRecord->shortcuts[e.appId].proton = e.value;
            } else
                m_steamRecord->proton.insert(e.appId, e.value);
        } else {
            steam.apps[e.appId].launchOptions = e.value;
            m_steamRecord->launchOptions.insert(e.appId, e.value);
        }
    }
    for (const quint32 appId : std::as_const(removed)) {
        const bool inSteam = std::any_of(steam.shortcuts.cbegin(), steam.shortcuts.cend(),
                                         [&](const SteamShortcut &s) { return s.appId == appId; });
        if (!inSteam && !steam.compatTools.contains(appId)) {
            m_steamRecord->removed.remove(appId);
            m_steamRecord->shortcuts.remove(appId);
        }
    }
    if (!r.applied.isEmpty() || !r.shortcutsApplied.isEmpty()) {
        QString error;
        if (!SteamSync::writeRecord(m_steamSyncRecord, *m_steamRecord, m_dataDir, &error))
            qCWarning(lcSteam).noquote() << error;
        updateSteamQueue();
        changedHere();
    }
    if (!r.stale.isEmpty() || !r.shortcutsStale.isEmpty())
        refresh();
    if (!r.error.isEmpty()) {
        qCWarning(lcSteam).noquote() << QStringLiteral("not applied to Steam: %1").arg(r.error);
        if (const qint64 pid = steamPid(); pid > 0 && !m_steamExit.watching())
            m_steamExit.watch(pid);
        return tr("Not applied to Steam: %1").arg(r.error);
    }
    return {};
}

void LibraryStore::applySteamSync(const ScanResult &result)
{
    if (!result.steamRecord || m_versionStatus.readOnly)
        return;
    for (const QString &line : result.steamSyncLog)
        qCInfo(lcSteam).noquote() << line;
    m_steamRecord = result.steamRecord;

    if (!result.steamGameWrites.isEmpty()) {
        beginBatch();
        for (auto it = result.steamGameWrites.cbegin(); it != result.steamGameWrites.cend(); ++it) {
            QStringList keys;
            QList<std::pair<QString, QVariant>> values = it.value();
            for (auto &[key, value] : values) {
                keys.append(key);
                if ((key == QLatin1String("exe") || key == QLatin1String("start_dir")) && !value.toString().isEmpty())
                    value = writtenPath(value.toString());
            }
            const QString problem = setGameValues(it.key(), values);
            if (problem.isEmpty()) {
                qCInfo(lcSteam).noquote() << QStringLiteral("%1: wrote %2 to its game file")
                                                 .arg(it.key(), keys.join(QStringLiteral(", ")));
                continue;
            }
            qCWarning(lcSteam).noquote() << problem;
            // Not written: the record forgets it matched, so the next scan compares again.
            if (const GameFile file = m_snapshot.content.games.value(it.key()); file.platform == kProtonPlatform) {
                const qint64 shortcut = file.shortcut;
                if (const auto old = result.steamRecordBefore.shortcuts.constFind(shortcut);
                    old != result.steamRecordBefore.shortcuts.cend())
                    m_steamRecord->shortcuts.insert(shortcut, *old);
                else
                    m_steamRecord->shortcuts.remove(shortcut);
                continue;
            }
            const qint64 appId = GameId::steamAppId(it.key());
            for (const QString &key : std::as_const(keys)) {
                const bool proton = key == QLatin1String("proton");
                QHash<qint64, QString> &now = proton ? m_steamRecord->proton : m_steamRecord->launchOptions;
                const QHash<qint64, QString> &before =
                    proton ? result.steamRecordBefore.proton : result.steamRecordBefore.launchOptions;
                if (const auto old = before.constFind(appId); old != before.cend())
                    now.insert(appId, *old);
                else
                    now.remove(appId);
            }
        }
        updateSteamQueue();
        endBatch();
    }

    if (*m_steamRecord != result.steamRecordBefore) {
        QString error;
        if (!SteamSync::writeRecord(m_steamSyncRecord, *m_steamRecord, m_dataDir, &error))
            qCWarning(lcSteam).noquote() << error;
    }
}

void LibraryStore::applySteamHistory(const ScanResult &result)
{
    for (const QString &problem : result.historyProblems)
        qCWarning(lcSteam).noquote() << problem;
    if (!result.steamDiffed)
        return;
    m_steamHistoryStarted = true;
    if (!result.historySeenChanged)
        return;

    QStringList ids;
    for (const PlayHistory::NewSession &added : result.historySessions) {
        if (const QString error = PlayHistory::append(m_dataDir, added.id, added.session); !error.isEmpty()) {
            qCWarning(lcSteam).noquote() << error;
            continue;
        }
        qCInfo(lcSteam).noquote() << QStringLiteral("%1: %2 min played in Steam%3")
                                         .arg(added.id).arg(added.session.minutes)
                                         .arg(added.session.merged ? QStringLiteral(" (while Lattis was closed)") : QString());
        if (!ids.contains(added.id))
            ids.append(added.id);
    }
    // Written even if a session wasn't: the total was seen, and the session can't be told again.
    if (const QString error = PlayHistory::writeSeen(m_dataDir, result.historySeen); !error.isEmpty())
        qCWarning(lcSteam).noquote() << error;
    if (!ids.isEmpty())
        emit historyChanged(ids);
}

void LibraryStore::updateSteamQueue()
{
    if (!m_steamRecord)
        return;
    m_snapshot.steamQueued = m_snapshot.steam.install.found()
        ? SteamSync::sync(steamSyncGames(m_snapshot.content), m_snapshot.steam, *m_steamRecord,
                          shortcutSyncGames(m_snapshot.content, m_dataDir, m_installLocations.home,
                                            m_steamRecord->grid, m_snapshot.steam.install))
              .queued
        : QHash<QString, QStringList>();
}

void LibraryStore::changedHere()
{
    if (m_batching) {
        m_batchChanged = true;
        return;
    }
    m_batchChanged = false;
    ++m_generation;
    emit changed();
    scheduleCacheSave();
}

QString LibraryStore::editFile(const QString &relative, const QString &header,
                               const std::function<TomlPatch::Result(const QString &)> &patch)
{
    if (m_versionStatus.readOnly)
        return tr("Not saved: %1").arg(m_versionStatus.reason);

    const QString path = m_dataDir + u'/' + relative;
    QString original;
    const bool exists = QFileInfo::exists(path);
    if (QFile file(path); file.open(QIODevice::ReadOnly))
        original = QString::fromUtf8(file.readAll());
    else if (exists)
        return tr("Could not read %1: %2").arg(relative, file.errorString());

    const QString before = exists ? original : header;
    const TomlPatch::Result r = patch(before);
    if (!r.ok) {
        const QString problem = tr("%1 has an error, so the change wasn't saved: %2").arg(relative, r.error);
        qCWarning(lcLibrary).noquote() << problem;
        return problem;
    }
    if (r.text == before)
        return {};

    QDir().mkpath(QFileInfo(path).absolutePath());
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly) || file.write(r.text.toUtf8()) < 0 || !file.commit()) {
        const QString problem = tr("Could not save %1: %2").arg(relative, file.errorString());
        qCWarning(lcLibrary).noquote() << problem;
        return problem;
    }
    qCInfo(lcLibrary).noquote() << "wrote" << relative;
    refresh();
    return {};
}

QString LibraryStore::setEmulatorValue(const QString &id, const QString &key, const QVariant &value)
{
    const EmulatorFields::Field *field = EmulatorFields::find(key);
    if (!field || key == QLatin1String("id") || field->type == EmulatorFields::Type::PathTable)
        return tr("%1 can't be changed here; edit emulators.toml").arg(key);

    const QList<EmulatorEntry> &presets = Emulators::presets();
    const auto preset = std::find_if(presets.cbegin(), presets.cend(), [&](const EmulatorEntry &e) { return e.id == id; });
    const QList<EmulatorEntry> &user = m_snapshot.content.emulators;
    if (preset == presets.cend()
        && std::none_of(user.cbegin(), user.cend(), [&](const EmulatorEntry &e) { return e.id == id; }))
        return tr("There's no emulator with the id %1").arg(id);

    QString formatted;
    if (value.isValid() && field->type == EmulatorFields::Type::String) {
        const QString s = value.toString().trimmed();
        if (!s.isEmpty() && (preset == presets.cend() || preset->values.value(key).toString() != s))
            formatted = tomlString(s);
    } else if (value.isValid()) {
        QStringList list = value.toStringList();
        if (key == QLatin1String("extensions"))
            list = normalizedExtensions(list);
        if (preset == presets.cend() ? !list.isEmpty() : preset->values.value(key).toStringList() != list) {
            QStringList items;
            for (const QString &s : list)
                items.append(tomlString(s));
            formatted = u'[' + items.join(QStringLiteral(", ")) + u']';
        }
    }

    const bool isPreset = preset != presets.cend();
    // The key stays for a user emulator: without extensions or files it would be skipped as a typo.
    if (!isPreset && formatted.isEmpty() && key == QLatin1String("extensions") && value.isValid())
        formatted = QStringLiteral("[]");
    if (!isPreset && formatted.isEmpty() && (key == QLatin1String("name") || key == QLatin1String("command")))
        return key == QLatin1String("name") ? tr("An emulator needs a name")
                                            : tr("The command needs {exe}, where the program goes");
    if (key == QLatin1String("command") && !formatted.isEmpty() && !formatted.contains(QLatin1String("{exe}")))
        return tr("The command needs {exe}, where the program goes");

    const TomlPatch::Block block{QStringLiteral("emulator"), id};
    return editFile(kEmulators, kEmulatorsHeader, [&](const QString &text) {
        if (!formatted.isEmpty())
            return TomlPatch::setBlockValue(text, block, key, formatted);
        TomlPatch::Result r = TomlPatch::removeBlockValue(text, block, key);
        if (r.ok && isPreset && TomlPatch::blockKeys(r.text, block) == QStringList{QStringLiteral("id")})
            r = TomlPatch::removeBlock(r.text, block);
        return r;
    });
}

QString LibraryStore::addEmulator(const QString &id, const QString &name, const QString &startFrom)
{
    if (!GameId::isValid(id))
        return tr("An emulator id uses only a-z, 0-9 and -; the name needs at least one of those letters or digits");
    if (name.trimmed().isEmpty())
        return tr("An emulator needs a name");

    const QList<EmulatorEntry> &presets = Emulators::presets();
    const auto isId = [&](const EmulatorEntry &e) { return e.id == id; };
    const QString taken = tr("There's already an emulator with the id %1").arg(id);
    if (std::any_of(presets.cbegin(), presets.cend(), isId))
        return taken;

    QList<std::pair<QString, QString>> values{{QStringLiteral("id"), tomlString(id)},
                                              {QStringLiteral("name"), tomlString(name.trimmed())}};
    QStringList platforms;
    if (startFrom.isEmpty()) {
        values.append({QStringLiteral("command"), tomlList({QStringLiteral("{exe}"), QStringLiteral("{args}"),
                                                              QStringLiteral("{rom}")})});
        values.append({QStringLiteral("extensions"), QStringLiteral("[]")});
    } else {
        const auto from = std::find_if(presets.cbegin(), presets.cend(),
                                       [&](const EmulatorEntry &e) { return e.id == startFrom; });
        if (from == presets.cend())
            return tr("There's no built-in emulator with the id %1").arg(startFrom);
        // Not the install (the copy would find the original's), nor saves and keys (the original's folders).
        static const QStringList left{QStringLiteral("name"), QStringLiteral("flatpak"), QStringLiteral("executables"),
                                      QStringLiteral("appimage"), QStringLiteral("path"), QStringLiteral("saves"),
                                      QStringLiteral("keys")};
        for (const EmulatorFields::Field &f : EmulatorFields::all()) {
            const QString key = QLatin1String(f.key);
            if (left.contains(key) || !from->values.contains(key))
                continue;
            const QVariant &v = from->values[key];
            values.append({key, f.type == EmulatorFields::Type::String ? tomlString(v.toString())
                                                                       : tomlList(v.toStringList())});
            if (key == QLatin1String("platforms"))
                platforms = v.toStringList();
        }
    }

    // Checked against the file, not the snapshot, which may not have caught up yet.
    bool exists = false;
    const QString error = editFile(kEmulators, kEmulatorsHeader, [&](const QString &text) {
        if (TomlPatch::blockValues(text, QStringLiteral("emulator"), QStringLiteral("id")).contains(id)) {
            exists = true;
            return TomlPatch::Result{true, text, {}};
        }
        return TomlPatch::appendBlock(text, QStringLiteral("emulator"), values);
    });
    if (exists)
        return taken;
    if (!error.isEmpty() || platforms.isEmpty())
        return error;
    return editFile(kPlatforms, defaultPlatforms(), [&](const QString &text) {
        return changeEmulatorLists(text, m_snapshot.content.platforms, id, platforms);
    });
}

QString LibraryStore::removeEmulator(const QString &id)
{
    const QList<EmulatorEntry> &presets = Emulators::presets();
    const QList<EmulatorEntry> &user = m_snapshot.content.emulators;
    const auto isId = [&](const EmulatorEntry &e) { return e.id == id; };
    if (std::any_of(presets.cbegin(), presets.cend(), isId) || std::none_of(user.cbegin(), user.cend(), isId))
        return tr("%1 isn't an emulator of yours").arg(id);

    // The lists first, so no platform names an emulator that's gone.
    const QString error = editFile(kPlatforms, {}, [&](const QString &text) {
        return changeEmulatorLists(text, m_snapshot.content.platforms, id, {});
    });
    if (!error.isEmpty())
        return error;
    return editFile(kEmulators, kEmulatorsHeader, [&](const QString &text) {
        return TomlPatch::removeBlock(text, {QStringLiteral("emulator"), id});
    });
}

QString LibraryStore::setPlatformValue(const QString &id, const QString &key, const QVariant &value)
{
    const QList<Platform> &platforms = m_snapshot.content.platforms;
    if (std::none_of(platforms.cbegin(), platforms.cend(), [&](const Platform &p) { return p.id == id; }))
        return tr("%1 isn't in platforms.toml").arg(id);

    QString formatted;
    if (key == QLatin1String("name") || key == QLatin1String("icon")) {
        const QString s = value.toString().trimmed();
        if (key == QLatin1String("name") && s.isEmpty())
            return tr("A platform needs a name");
        if (!s.isEmpty())
            formatted = tomlString(s);
    } else if (key == QLatin1String("emulators") || key == QLatin1String("rom_folders")) {
        QStringList items;
        for (const QString &s : value.toStringList())
            items.append(tomlString(s));
        if (!items.isEmpty())
            formatted = u'[' + items.join(QStringLiteral(", ")) + u']';
    } else if (key == QLatin1String("in_rail")) {
        if (!value.toBool())
            formatted = QStringLiteral("false"); // true is the default, so it's the key's absence
    } else {
        return tr("%1 can't be changed here; edit platforms.toml").arg(key);
    }

    const TomlPatch::Block block{QStringLiteral("platform"), id};
    return editFile(kPlatforms, {}, [&](const QString &text) {
        return formatted.isEmpty() ? TomlPatch::removeBlockValue(text, block, key)
                                   : TomlPatch::setBlockValue(text, block, key, formatted);
    });
}

QString LibraryStore::addPlatform(const QString &id, const QString &name)
{
    if (!GameId::isValid(id))
        return tr("A platform id uses only a-z, 0-9 and -; the name needs at least one of those letters or digits");
    if (name.trimmed().isEmpty())
        return tr("A platform needs a name");
    // Checked against the file, not the snapshot, which may not have caught up yet.
    bool taken = false;
    const QString error = editFile(kPlatforms, defaultPlatforms(), [&](const QString &text) {
        if (TomlPatch::blockValues(text, QStringLiteral("platform"), QStringLiteral("id")).contains(id)) {
            taken = true;
            return TomlPatch::Result{true, text, {}};
        }
        return TomlPatch::appendBlock(text, QStringLiteral("platform"),
                                      {{QStringLiteral("id"), tomlString(id)},
                                       {QStringLiteral("name"), tomlString(name.trimmed())}});
    });
    return taken ? tr("There's already a platform with the id %1").arg(id) : error;
}

QString LibraryStore::removePlatform(const QString &id)
{
    return editFile(kPlatforms, {}, [&](const QString &text) {
        return TomlPatch::removeBlock(text, {QStringLiteral("platform"), id});
    });
}

QString LibraryStore::movePlatform(const QString &id, int step)
{
    // The order comes from the file itself, not the snapshot, which a quick second click
    // could find out of date.
    return editFile(kPlatforms, {}, [&](const QString &text) {
        const QStringList ids = TomlPatch::blockValues(text, QStringLiteral("platform"), QStringLiteral("id"));
        const qsizetype from = ids.indexOf(id);
        const qsizetype to = from + step;
        if (from < 0 || step == 0 || to < 0 || to >= ids.size())
            return TomlPatch::Result{true, text, {}};
        // Before the platform now at `to` when moving up; before the one after it when
        // moving down, or after the last.
        const QString before = step < 0 ? ids.at(to) : to + 1 < ids.size() ? ids.at(to + 1) : QString();
        return TomlPatch::moveBlock(text, {QStringLiteral("platform"), id}, before);
    });
}

QString LibraryStore::movePlatformBefore(const QString &id, const QString &beforeId)
{
    return editFile(kPlatforms, defaultPlatforms(), [&](const QString &text) {
        QStringList ids = TomlPatch::blockValues(text, QStringLiteral("platform"), QStringLiteral("id"));
        QString current = text;
        if (!ids.contains(id)) {
            const TomlPatch::Result added = TomlPatch::appendBlock(
                text, QStringLiteral("platform"),
                {{QStringLiteral("id"), tomlString(id)}, {QStringLiteral("name"), tomlString(id)}});
            if (!added.ok)
                return added;
            current = added.text;
            ids.append(id);
        }
        // A target the file doesn't have (an undefined platform below the drop) means last.
        const QString before = beforeId != id && ids.contains(beforeId) ? beforeId : QString();
        return TomlPatch::moveBlock(current, {QStringLiteral("platform"), id}, before);
    });
}

QString LibraryStore::emulatorsFile()
{
    return fileToOpen(kEmulators, kEmulatorsHeader);
}

QString LibraryStore::platformsFile()
{
    return fileToOpen(kPlatforms, defaultPlatforms());
}

QString LibraryStore::fileToOpen(const QString &relative, const QString &header)
{
    const QString path = m_dataDir + u'/' + relative;
    if (!QFileInfo::exists(path) && !m_versionStatus.readOnly) {
        QDir().mkpath(QFileInfo(path).absolutePath());
        QSaveFile file(path);
        if (!file.open(QIODevice::WriteOnly) || file.write(header.toUtf8()) < 0 || !file.commit())
            qCWarning(lcLibrary).noquote() << QStringLiteral("Could not create %1: %2").arg(relative, file.errorString());
    }
    return path;
}

QString LibraryStore::setGameValues(const QString &id, const QList<std::pair<QString, QVariant>> &values)
{
    if (m_versionStatus.readOnly)
        return tr("Not saved: %1").arg(m_versionStatus.reason);

    const QString relative = kGamesPrefix + id + QStringLiteral(".toml");
    const QString path = m_dataDir + u'/' + relative;
    const bool hasFile = m_snapshot.content.games.contains(id);
    const bool indexed = takenIds(m_snapshot.content).contains(id);
    if (!hasFile && !indexed)
        return tr("There's no game with the id %1").arg(id);

    QList<std::pair<QString, QVariant>> checked;
    for (const auto &[key, value] : values) {
        if (GameFields::isCleared(key, value)) {
            checked.append({key, QVariant()});
            continue;
        }
        QString error;
        const QVariant v = GameFields::validate(key, value, &error);
        if (!v.isValid())
            return error;
        checked.append({key, v});
    }
    // A rating written without its source is the user's.
    const auto has = [&](QLatin1String key) {
        return std::any_of(checked.cbegin(), checked.cend(), [&](const auto &p) { return p.first == key; });
    };
    if (has(QLatin1String("rating")) && !has(QLatin1String("rating_source")))
        checked.append({QStringLiteral("rating_source"), QVariant()});

    QString text;
    if (QFile file(path); file.open(QIODevice::ReadOnly))
        text = QString::fromUtf8(file.readAll());
    else if (QFileInfo::exists(path))
        return tr("Could not read %1: %2").arg(relative, file.errorString());
    const QString original = text;
    for (const auto &[key, value] : std::as_const(checked)) {
        const TomlPatch::Result r = value.isValid() ? TomlPatch::setValue(text, {}, key, GameFields::format(key, value))
                                                    : TomlPatch::removeValue(text, {}, key);
        if (!r.ok) {
            const QString problem = tr("%1 has an error, so the change wasn't saved: %2").arg(relative, r.error);
            qCWarning(lcLibrary).noquote() << problem;
            return problem;
        }
        text = r.text;
    }
    if (text == original)
        return {};

    // A file left with nothing in it, for a game an index still has, goes: it only held
    // changes, and they're all undone.
    if (indexed && text.trimmed().isEmpty()) {
        if (!QFile::remove(path)) {
            const QString problem = tr("Could not remove %1").arg(relative);
            qCWarning(lcLibrary).noquote() << problem;
            return problem;
        }
        forget(m_snapshot, relative);
    } else {
        QSaveFile file(path);
        if (!file.open(QIODevice::WriteOnly) || file.write(text.toUtf8()) < 0 || !file.commit()) {
            const QString problem = tr("Could not save %1: %2").arg(relative, file.errorString());
            qCWarning(lcLibrary).noquote() << problem;
            return problem;
        }
        // Record the new stamp, so the watcher's scan sees the file as already up to date.
        CachedFile &entry = m_snapshot.files[relative];
        entry.stamp = stampOf(QFileInfo(path));
        entry.ok = parseInto(m_snapshot, relative, text.toUtf8(), &entry.warnings);
    }
    if (std::any_of(checked.cbegin(), checked.cend(), [](const auto &pair) { return pair.first == QLatin1String("rom"); }))
        m_snapshot.romGameFiles = findRomGameFiles(m_snapshot.content.games, m_dataDir, m_installLocations.home);
    if (std::any_of(checked.cbegin(), checked.cend(), [](const auto &pair) {
            return pair.first == QLatin1String("exe") || pair.first == QLatin1String("install_dir")
                || pair.first == QLatin1String("platform");
        }))
        m_snapshot.programFiles = findProgramFiles(m_snapshot.content.games, m_dataDir, m_installLocations.home,
                                                  m_snapshot.programFiles, false);
    static const QStringList steamKeys = {QStringLiteral("launch_options"), QStringLiteral("proton"),
                                          QStringLiteral("platform"),       QStringLiteral("title"),
                                          QStringLiteral("exe"),            QStringLiteral("start_dir"),
                                          QStringLiteral("shortcut"),       QStringLiteral("installed")};
    if (std::any_of(checked.cbegin(), checked.cend(), [](const auto &pair) { return steamKeys.contains(pair.first); })) {
        updateSteamQueue();
        if (!m_snapshot.steamQueued.isEmpty())
            m_steamApplyTimer.start();
    }
    changedHere();
    return {};
}

void LibraryStore::endBatch()
{
    if (--m_batching == 0 && m_batchChanged)
        changedHere();
}

QString LibraryStore::writtenPath(const QString &path) const
{
    const QString home = m_installLocations.home;
    return !home.isEmpty() && path.startsWith(home + u'/') ? u'~' + path.mid(home.size()) : path;
}

bool LibraryStore::inIndex(const QString &id) const
{
    const LibraryContent &c = m_snapshot.content;
    if (std::any_of(c.steam.cbegin(), c.steam.cend(), [&](const IndexEntry &e) { return GameId::forSteamApp(e.appId) == id; }))
        return true;
    for (const QList<RomEntry> &index : c.roms) {
        if (std::any_of(index.cbegin(), index.cend(), [&](const RomEntry &e) { return e.id == id; }))
            return true;
    }
    return false;
}

QString LibraryStore::gameWithRom(const QString &romPath) const
{
    const QString path = QDir::cleanPath(QFileInfo(romPath).absoluteFilePath());
    for (auto it = m_snapshot.romGameFiles.cbegin(); it != m_snapshot.romGameFiles.cend(); ++it) {
        if (it->path == path)
            return it.key();
    }
    for (auto platform = m_snapshot.romFiles.cbegin(); platform != m_snapshot.romFiles.cend(); ++platform) {
        for (auto file = platform->cbegin(); file != platform->cend(); ++file) {
            if (file.value() + u'/' + file.key() != path)
                continue;
            for (const RomEntry &e : m_snapshot.content.roms.value(platform.key())) {
                if (e.path == file.key())
                    return e.id;
            }
        }
    }
    return {};
}

QString LibraryStore::relocateRom(const QString &id, const QString &romPath)
{
    if (m_versionStatus.readOnly)
        return tr("Not saved: %1").arg(m_versionStatus.reason);
    const QString path = QDir::cleanPath(QFileInfo(romPath).absoluteFilePath());
    const QFileInfo info(path);
    if (!info.isFile())
        return tr("%1 isn't a file").arg(path);

    const auto indexOf = [this](const QString &game) {
        for (auto it = m_snapshot.content.roms.cbegin(); it != m_snapshot.content.roms.cend(); ++it) {
            if (std::any_of(it->cbegin(), it->cend(), [&](const RomEntry &e) { return e.id == game; }))
                return it.key();
        }
        return QString();
    };
    const QString platform = indexOf(id);
    const GameFile mine = m_snapshot.content.games.value(id);
    if (platform.isEmpty() && mine.rom.isEmpty())
        return tr("There's no ROM game with the id %1").arg(id);
    const QString other = gameWithRom(path);
    if (other == id)
        return {};
    const QString otherPlatform = other.isEmpty() ? QString() : indexOf(other);

    QString folder;
    for (const Platform &p : std::as_const(m_snapshot.content.platforms)) {
        if (p.id != platform)
            continue;
        for (const QString &written : p.romFolders) {
            const QString resolved = resolveRomFolder(written, m_dataDir, m_installLocations.home);
            if (path.startsWith(resolved + u'/')) {
                folder = resolved;
                break;
            }
        }
    }
    const QString relative = folder.isEmpty() ? QString() : path.mid(folder.size() + 1);
    for (const QString &touched : {folder.isEmpty() ? QString() : platform, otherPlatform}) {
        const QString file = kRomsPrefix + touched + QStringLiteral(".toml");
        if (!touched.isEmpty() && !m_snapshot.files.value(file).ok)
            return tr("%1 has an error, so the change wasn't saved").arg(file);
    }

    QList<std::pair<QString, QVariant>> values;
    QMap<QString, QList<RomEntry>> indexes;
    if (!other.isEmpty()) {
        const GameFile theirs = m_snapshot.content.games.value(other);
        if (theirs.playMinutes > 0)
            values.append({QStringLiteral("play_minutes"), mine.playMinutes + theirs.playMinutes});
        if (theirs.lastPlayed.isValid() && (!mine.lastPlayed.isValid() || theirs.lastPlayed > mine.lastPlayed))
            values.append({QStringLiteral("last_played"), theirs.lastPlayed});
        if (theirs.favorite && !mine.favorite)
            values.append({QStringLiteral("favorite"), true});
        if (!theirs.status.isEmpty() && mine.status.isEmpty())
            values.append({QStringLiteral("status"), theirs.status});
        if (theirs.igdb >= 0 && mine.igdb < 0)
            values.append({QStringLiteral("igdb"), theirs.igdb});
        if (theirs.steamgriddb >= 0 && mine.steamgriddb < 0)
            values.append({QStringLiteral("steamgriddb"), theirs.steamgriddb});
        QStringList tags = mine.tags + theirs.tags;
        tags.removeDuplicates();
        if (tags != mine.tags)
            values.append({QStringLiteral("tags"), tags});
        if (!otherPlatform.isEmpty()) {
            QList<RomEntry> entries = m_snapshot.content.roms.value(otherPlatform);
            entries.removeIf([&](const RomEntry &e) { return e.id == other; });
            indexes.insert(otherPlatform, entries);
        }
    }
    if (!folder.isEmpty()) {
        QList<RomEntry> entries = indexes.value(platform, m_snapshot.content.roms.value(platform));
        for (RomEntry &e : entries) {
            if (e.id == id) {
                e.path = relative;
                e.size = info.size();
            }
        }
        indexes.insert(platform, entries);
        if (!mine.rom.isEmpty())
            values.append({QStringLiteral("rom"), QVariant()}); // the index entry has it now
    } else {
        values.append({QStringLiteral("rom"), writtenPath(path)});
    }

    ++m_batching;
    // The game file first: if it can't be written, nothing else changes.
    QString error = setGameValues(id, values);
    for (auto it = indexes.cbegin(); error.isEmpty() && it != indexes.cend(); ++it) {
        error = writeRomIndex(it.key(), it.value());
        if (error.isEmpty())
            m_snapshot.content.roms.insert(it.key(), it.value());
    }
    if (error.isEmpty() && !folder.isEmpty())
        m_snapshot.romFiles[platform].insert(relative, folder);
    if (error.isEmpty() && !other.isEmpty()) {
        const ArtSet theirs = m_snapshot.art.value(other);
        const ArtSet ours = m_snapshot.art.value(id);
        const QString to = m_dataDir + QStringLiteral("/art/") + id;
        const QString theirPaths[] = {theirs.hero, theirs.logo, theirs.icon, theirs.cover};
        const QString ourPaths[] = {ours.hero, ours.logo, ours.icon, ours.cover};
        for (qsizetype i = 0; i < kArtKinds.size(); ++i) {
            if (theirPaths[i].isEmpty() || !ourPaths[i].isEmpty())
                continue;
            QDir().mkpath(to);
            if (!QFile::rename(theirPaths[i], to + u'/' + QFileInfo(theirPaths[i]).fileName()))
                qCWarning(lcLibrary).noquote() << "could not move" << theirPaths[i] << "to" << to;
        }
        if (const ArtSet art = scanArtFolder(to); art != ArtSet{})
            m_snapshot.art.insert(id, art);
        for (const PlayHistory::Session &session : PlayHistory::read(m_dataDir, other).value.sessions) {
            if (const QString problem = PlayHistory::append(m_dataDir, id, session); !problem.isEmpty())
                qCWarning(lcLibrary).noquote() << problem;
        }
        error = removeGameFile(other, true);
        if (error.isEmpty())
            qCInfo(lcLibrary).noquote() << "merged" << other << "into" << id;
    }
    --m_batching;
    changedHere();
    return error;
}

LibraryStore::AddedGame LibraryStore::addGame(const QString &romPath, const QString &platform, const QString &title,
                                              const QList<std::pair<QString, QVariant>> &values)
{
    AddedGame out;
    if (m_versionStatus.readOnly) {
        out.error = tr("Not added: %1").arg(m_versionStatus.reason);
        return out;
    }
    const QString path = QDir::cleanPath(QFileInfo(romPath).absoluteFilePath());
    if (!QFileInfo(path).isFile()) {
        out.error = tr("%1 isn't a file").arg(path);
        return out;
    }
    if (const QString id = gameWithRom(path); !id.isEmpty()) {
        out.id = id;
        out.existing = true;
        return out;
    }
    const auto platformIt = std::find_if(m_snapshot.content.platforms.cbegin(), m_snapshot.content.platforms.cend(),
                                         [&](const Platform &p) { return p.id == platform; });
    if (platformIt == m_snapshot.content.platforms.cend()) {
        out.error = tr("There's no platform with the id %1").arg(platform);
        return out;
    }
    if (title.trimmed().isEmpty()) {
        out.error = tr("The game needs a title");
        return out;
    }

    // Ids of games that are gone may still have art; a new game doesn't take it over.
    QSet<QString> taken = takenIds(m_snapshot.content);
    for (auto it = m_snapshot.art.cbegin(); it != m_snapshot.art.cend(); ++it)
        taken.insert(it.key());
    const QString id = GameId::forTitle(platform, title.trimmed(), taken);

    for (const QString &written : platformIt->romFolders) {
        const QString folder = resolveRomFolder(written, m_dataDir, m_installLocations.home);
        if (!path.startsWith(folder + u'/'))
            continue;
        const QString relative = kRomsPrefix + platform + QStringLiteral(".toml");
        if (!m_snapshot.files.value(relative).ok) {
            out.error = tr("%1 has an error, so the game wasn't added").arg(relative);
            return out;
        }
        RomEntry entry;
        entry.path = path.mid(folder.size() + 1);
        entry.id = id;
        entry.title = title.trimmed();
        entry.titleId = RomName::titleId(QFileInfo(path).fileName());
        entry.size = QFileInfo(path).size();
        QList<RomEntry> entries = m_snapshot.content.roms.value(platform);
        entries.append(entry);
        std::sort(entries.begin(), entries.end(), [](const RomEntry &a, const RomEntry &b) { return a.path < b.path; });
        if (const QString problem = writeRomIndex(platform, entries); !problem.isEmpty()) {
            out.error = problem;
            return out;
        }
        m_snapshot.content.roms.insert(platform, entries);
        m_snapshot.romFiles[platform].insert(entry.path, folder);
        changedHere();
        out.id = id;
        if (!values.isEmpty())
            out.error = setGameValues(id, values);
        return out;
    }

    const QString written = writtenPath(path);
    QList<std::pair<QString, QVariant>> all = {{QStringLiteral("title"), title.trimmed()},
                                               {QStringLiteral("platform"), platform},
                                               {QStringLiteral("rom"), written},
                                               {QStringLiteral("added"), QDate::currentDate()}};
    for (const auto &[key, value] : values) {
        const auto same = [&](const auto &pair) { return pair.first == key; };
        all.erase(std::remove_if(all.begin(), all.end(), same), all.end());
        all.append({key, value});
    }
    if (const QString problem = writeNewGameFile(id, all); !problem.isEmpty()) {
        out.error = problem;
        return out;
    }
    m_snapshot.romGameFiles.insert(id, {path, QFileInfo(path).size()});
    qCInfo(lcLibrary).noquote() << "added" << id << "from" << written;
    changedHere();
    out.id = id;
    return out;
}

QString LibraryStore::writeNewGameFile(const QString &id, const QList<std::pair<QString, QVariant>> &values)
{
    for (const auto &[key, value] : values) {
        if (!GameFields::find(key))
            return tr("unknown key %1").arg(key);
    }
    // In the order of GameFields, so the file reads title, platform, rom, then the rest.
    QString text;
    for (const GameFields::Field &field : GameFields::all()) {
        const QString key = QLatin1String(field.key);
        const auto it = std::find_if(values.cbegin(), values.cend(), [&](const auto &pair) { return pair.first == key; });
        if (it == values.cend() || GameFields::isCleared(key, it->second))
            continue;
        QString error;
        const QVariant value = GameFields::validate(key, it->second, &error);
        if (!value.isValid())
            return error;
        text += key + QStringLiteral(" = ") + GameFields::format(key, value) + u'\n';
    }

    const QString relative = kGamesPrefix + id + QStringLiteral(".toml");
    const QString filePath = m_dataDir + u'/' + relative;
    QDir().mkpath(QFileInfo(filePath).absolutePath());
    QSaveFile file(filePath);
    if (!file.open(QIODevice::WriteOnly) || file.write(text.toUtf8()) < 0 || !file.commit())
        return tr("Could not save %1: %2").arg(relative, file.errorString());
    CachedFile &entry = m_snapshot.files[relative];
    entry.stamp = stampOf(QFileInfo(filePath));
    entry.ok = parseInto(m_snapshot, relative, text.toUtf8(), &entry.warnings);
    return {};
}

LibraryStore::AddedShortcuts LibraryStore::addShortcuts(const QList<quint32> &appIds)
{
    AddedShortcuts out;
    if (m_versionStatus.readOnly) {
        out.error = tr("Not added: %1").arg(m_versionStatus.reason);
        return out;
    }
    const SteamState &steam = m_snapshot.steam;
    if (!steam.install.found() || !steam.shortcutsOk) {
        out.error = tr("Steam's shortcuts couldn't be read");
        return out;
    }
    QSet<quint32> inLibrary;
    for (const GameFile &g : std::as_const(m_snapshot.content.games)) {
        if (g.platform == kProtonPlatform && g.shortcut > 0)
            inLibrary.insert(quint32(g.shortcut));
    }
    QSet<QString> taken = takenIds(m_snapshot.content);
    for (auto it = m_snapshot.art.cbegin(); it != m_snapshot.art.cend(); ++it)
        taken.insert(it.key());

    // Recorded as in step with Steam, so nothing is queued for these shortcuts.
    QStringList ignored;
    SteamSync::Record record =
        m_steamRecord ? *m_steamRecord : SteamSync::readRecord(m_steamSyncRecord, m_versionStatus.id, &ignored);

    beginBatch();
    for (const quint32 appId : appIds) {
        const auto s = std::find_if(steam.shortcuts.cbegin(), steam.shortcuts.cend(),
                                    [&](const SteamShortcut &c) { return c.appId == appId; });
        if (s == steam.shortcuts.cend() || inLibrary.contains(appId))
            continue;
        const QString title = s->name.trimmed().isEmpty() ? QFileInfo(s->exe).completeBaseName() : s->name.trimmed();
        const QString id = GameId::forTitle(kProtonPlatform, title, taken);
        const QString proton = steam.compatTools.value(appId);
        QList<std::pair<QString, QVariant>> values = {
            {QStringLiteral("title"), title},
            {QStringLiteral("platform"), kProtonPlatform},
            {QStringLiteral("exe"), writtenPath(s->exe)},
            {QStringLiteral("install_dir"), writtenPath(protonInstallDir(s->exe, m_installLocations.home))},
            {QStringLiteral("shortcut"), qint64(appId)},
            {QStringLiteral("launch_options"), s->launchOptions},
            {QStringLiteral("proton"), proton},
            {QStringLiteral("added"), QDate::currentDate()},
        };
        const auto trimmed = [](QString path) {
            while (path.size() > 1 && path.endsWith(u'/'))
                path.chop(1);
            return path;
        };
        if (!s->startDir.isEmpty() && trimmed(s->startDir) != QFileInfo(s->exe).absolutePath())
            values.append({QStringLiteral("start_dir"), writtenPath(trimmed(s->startDir))});
        if (const QString problem = writeNewGameFile(id, values); !problem.isEmpty()) {
            out.error = problem;
            break;
        }
        taken.insert(id);
        inLibrary.insert(appId);
        record.shortcuts.insert(appId, SteamSync::recordOf(*s, proton));
        out.ids.append(id);
    }
    if (!out.ids.isEmpty()) {
        m_snapshot.programFiles = findProgramFiles(m_snapshot.content.games, m_dataDir, m_installLocations.home,
                                                  m_snapshot.programFiles, false);
        if (!m_steamSyncRecord.isEmpty()) {
            QString error;
            if (SteamSync::writeRecord(m_steamSyncRecord, record, m_dataDir, &error))
                m_steamRecord = record;
            else
                qCWarning(lcSteam).noquote() << error;
        }
        addBuiltInPlatforms();
        qCInfo(lcSteam).noquote() << QStringLiteral("added %1 shortcut(s) from Steam: %2")
                                         .arg(out.ids.size())
                                         .arg(out.ids.join(QStringLiteral(", ")));
        changedHere();
        m_protonAdded += out.ids;
        refresh();
    }
    endBatch();
    return out;
}

QString LibraryStore::gameWithProgram(const QString &exePath) const
{
    const QString path = QDir::cleanPath(QFileInfo(exePath).absoluteFilePath());
    for (auto it = m_snapshot.programFiles.cbegin(); it != m_snapshot.programFiles.cend(); ++it) {
        if (!it->exe.isEmpty() && QDir::cleanPath(it->exe) == path)
            return it.key();
    }
    return {};
}

quint32 LibraryStore::shortcutWithProgram(const QString &exePath) const
{
    const QString path = QDir::cleanPath(QFileInfo(exePath).absoluteFilePath());
    QSet<qint64> inLibrary;
    for (const GameFile &g : std::as_const(m_snapshot.content.games)) {
        if (g.platform == kProtonPlatform && g.shortcut > 0)
            inLibrary.insert(g.shortcut);
    }
    for (const SteamShortcut &s : m_snapshot.steam.shortcuts) {
        if (!inLibrary.contains(s.appId) && !s.exe.isEmpty() && QDir::cleanPath(s.exe) == path)
            return s.appId;
    }
    return 0;
}

LibraryStore::AddedGame LibraryStore::addProtonGame(const QString &exePath, const QString &title,
                                                    const QList<std::pair<QString, QVariant>> &values)
{
    AddedGame out;
    if (m_versionStatus.readOnly) {
        out.error = tr("Not added: %1").arg(m_versionStatus.reason);
        return out;
    }
    const SteamState &steam = m_snapshot.steam;
    if (!steam.install.found()) {
        out.error = tr("Proton games need Steam, and it wasn't found");
        return out;
    }
    if (!steam.shortcutsOk) {
        out.error = tr("Steam's shortcuts couldn't be read");
        return out;
    }
    const QString path = QDir::cleanPath(QFileInfo(exePath).absoluteFilePath());
    if (!QFileInfo(path).isFile()) {
        out.error = tr("%1 isn't a file").arg(path);
        return out;
    }
    if (const QString id = gameWithProgram(path); !id.isEmpty()) {
        out.id = id;
        out.existing = true;
        return out;
    }
    if (shortcutWithProgram(path) != 0) {
        out.error = tr("%1 is already a shortcut in Steam; add it with Add existing shortcuts").arg(QFileInfo(path).fileName());
        return out;
    }
    if (title.trimmed().isEmpty()) {
        out.error = tr("The game needs a title");
        return out;
    }

    QSet<QString> taken = takenIds(m_snapshot.content);
    for (auto it = m_snapshot.art.cbegin(); it != m_snapshot.art.cend(); ++it)
        taken.insert(it.key());
    const QString id = GameId::forTitle(kProtonPlatform, title.trimmed(), taken);

    QSet<quint32> appIds;
    for (const SteamShortcut &s : steam.shortcuts)
        appIds.insert(s.appId);
    for (const GameFile &g : std::as_const(m_snapshot.content.games)) {
        if (g.platform == kProtonPlatform && g.shortcut > 0)
            appIds.insert(quint32(g.shortcut));
    }
    if (m_steamRecord) {
        for (auto it = m_steamRecord->shortcuts.cbegin(); it != m_steamRecord->shortcuts.cend(); ++it)
            appIds.insert(quint32(it.key()));
    }
    const quint32 appId = SteamShortcuts::newAppId(SteamShortcuts::exeField(path), title.trimmed(), appIds);

    QList<std::pair<QString, QVariant>> all = {
        {QStringLiteral("title"), title.trimmed()},
        {QStringLiteral("platform"), kProtonPlatform},
        {QStringLiteral("exe"), writtenPath(path)},
        {QStringLiteral("install_dir"), writtenPath(protonInstallDir(path, m_installLocations.home))},
        {QStringLiteral("shortcut"), qint64(appId)},
        {QStringLiteral("added"), QDate::currentDate()},
    };
    for (const auto &[key, value] : values) {
        const auto same = [&](const auto &pair) { return pair.first == key; };
        all.erase(std::remove_if(all.begin(), all.end(), same), all.end());
        QVariant v = value;
        if ((key == QLatin1String("install_dir") || key == QLatin1String("start_dir")) && !v.toString().isEmpty())
            v = writtenPath(QDir::cleanPath(v.toString()));
        all.append({key, v});
    }
    if (const QString problem = writeNewGameFile(id, all); !problem.isEmpty()) {
        out.error = problem;
        return out;
    }
    qCInfo(lcSteam).noquote() << QStringLiteral("added %1 from %2, shortcut %3").arg(id, writtenPath(path)).arg(appId);
    m_snapshot.programFiles = findProgramFiles(m_snapshot.content.games, m_dataDir, m_installLocations.home,
                                              m_snapshot.programFiles, false);
    addBuiltInPlatforms();
    updateSteamQueue();
    if (!m_snapshot.steamQueued.isEmpty())
        m_steamApplyTimer.start();
    changedHere();
    refresh();
    out.id = id;
    return out;
}

QString LibraryStore::makeExecutable(const QString &path)
{
    if (QFileInfo(path).isExecutable())
        return {};
    QFile file(path);
    QFileDevice::Permissions p = file.permissions();
    if (p & QFileDevice::ReadOwner)
        p |= QFileDevice::ExeOwner | QFileDevice::ExeUser;
    if (p & QFileDevice::ReadGroup)
        p |= QFileDevice::ExeGroup;
    if (p & QFileDevice::ReadOther)
        p |= QFileDevice::ExeOther;
    if (!file.setPermissions(p))
        return tr("Could not make %1 executable: %2").arg(QFileInfo(path).fileName(), file.errorString());
    return {};
}

LibraryStore::AddedGame LibraryStore::addLinuxGame(const QString &exePath, const QString &title,
                                                   const QList<std::pair<QString, QVariant>> &values, bool makeExecutable)
{
    AddedGame out;
    if (m_versionStatus.readOnly) {
        out.error = tr("Not added: %1").arg(m_versionStatus.reason);
        return out;
    }
    const QString path = QDir::cleanPath(QFileInfo(exePath).absoluteFilePath());
    if (!QFileInfo(path).isFile()) {
        out.error = tr("%1 isn't a file").arg(path);
        return out;
    }
    if (const QString id = gameWithProgram(path); !id.isEmpty()) {
        out.id = id;
        out.existing = true;
        return out;
    }
    const LinuxProgramKind kind = linuxProgramKind(path);
    if (kind == LinuxProgramKind::None || kind == LinuxProgramKind::SharedLibrary) {
        out.error = tr("%1 isn't a Linux program").arg(QFileInfo(path).fileName());
        return out;
    }
    if (title.trimmed().isEmpty()) {
        out.error = tr("The game needs a title");
        return out;
    }
    if (makeExecutable) {
        if (const QString error = LibraryStore::makeExecutable(path); !error.isEmpty()) {
            out.error = error;
            return out;
        }
    }

    QSet<QString> taken = takenIds(m_snapshot.content);
    for (auto it = m_snapshot.art.cbegin(); it != m_snapshot.art.cend(); ++it)
        taken.insert(it.key());
    const QString id = GameId::forTitle(kLinuxPlatform, title.trimmed(), taken);
    QList<std::pair<QString, QVariant>> all = {
        {QStringLiteral("title"), title.trimmed()},
        {QStringLiteral("platform"), kLinuxPlatform},
        {QStringLiteral("exe"), writtenPath(path)},
        {QStringLiteral("install_dir"), writtenPath(linuxInstallDir(path, m_installLocations.home))},
        {QStringLiteral("added"), QDate::currentDate()},
    };
    for (const auto &[key, value] : values) {
        const auto same = [&](const auto &pair) { return pair.first == key; };
        all.erase(std::remove_if(all.begin(), all.end(), same), all.end());
        QVariant v = value;
        if ((key == QLatin1String("install_dir") || key == QLatin1String("start_dir")) && !v.toString().isEmpty())
            v = writtenPath(QDir::cleanPath(v.toString()));
        all.append({key, v});
    }
    if (const QString problem = writeNewGameFile(id, all); !problem.isEmpty()) {
        out.error = problem;
        return out;
    }
    qCInfo(lcLibrary).noquote() << QStringLiteral("added %1 from %2").arg(id, writtenPath(path));
    m_snapshot.programFiles = findProgramFiles(m_snapshot.content.games, m_dataDir, m_installLocations.home,
                                              m_snapshot.programFiles, false);
    addBuiltInPlatforms();
    changedHere();
    refresh();
    out.id = id;
    return out;
}

QString LibraryStore::removeGameFile(const QString &id, bool withArt)
{
    if (m_versionStatus.readOnly)
        return tr("Not removed: %1").arg(m_versionStatus.reason);
    const QString relative = kGamesPrefix + id + QStringLiteral(".toml");
    const QString path = m_dataDir + u'/' + relative;
    if (QFileInfo::exists(path) && !QFile::remove(path))
        return tr("Could not remove %1").arg(relative);
    forget(m_snapshot, relative);
    m_snapshot.romGameFiles.remove(id);
    if (const QString error = PlayHistory::removeFile(m_dataDir, id); !error.isEmpty())
        qCWarning(lcLibrary).noquote() << error;
    if (withArt) {
        if (QDir art(m_dataDir + QStringLiteral("/art/") + id); art.exists() && !art.removeRecursively())
            qCWarning(lcLibrary).noquote() << "could not remove all of" << QStringLiteral("art/%1").arg(id);
        m_snapshot.art.remove(id);
    }
    qCInfo(lcLibrary).noquote() << "removed" << relative;
    changedHere();
    return {};
}

void LibraryStore::reloadFiles(const QStringList &relatives)
{
    const QString artPrefix = QStringLiteral("art/");
    for (const QString &relative : relatives) {
        if (relative.startsWith(artPrefix)) {
            const QString id = relative.mid(artPrefix.size()).section(u'/', 0, 0);
            const ArtSet art = scanArtFolder(m_dataDir + u'/' + artPrefix + id);
            if (art == ArtSet{})
                m_snapshot.art.remove(id);
            else
                m_snapshot.art.insert(id, art);
            continue;
        }
        if (!relative.startsWith(QStringLiteral("library/")) || relative.startsWith(QStringLiteral("library/history/")))
            continue;
        const QString path = m_dataDir + u'/' + relative;
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly)) {
            forget(m_snapshot, relative);
            continue;
        }
        const QByteArray bytes = file.readAll();
        CachedFile &entry = m_snapshot.files[relative];
        entry.stamp = stampOf(QFileInfo(path));
        entry.ok = parseInto(m_snapshot, relative, bytes, &entry.warnings);
    }
    m_snapshot.romGameFiles = findRomGameFiles(m_snapshot.content.games, m_dataDir, m_installLocations.home);
    m_snapshot.programFiles = findProgramFiles(m_snapshot.content.games, m_dataDir, m_installLocations.home,
                                              m_snapshot.programFiles, false);
    updateSteamQueue();
    if (!m_snapshot.steamQueued.isEmpty())
        m_steamApplyTimer.start();
    changedHere();
    refresh();
}

QString LibraryStore::removeProtonGame(const QString &id, bool fromSteam)
{
    if (m_versionStatus.readOnly)
        return tr("Not removed: %1").arg(m_versionStatus.reason);
    const GameFile game = m_snapshot.content.games.value(id);
    if (!runsProgram(game.platform))
        return tr("%1 isn't a Proton or Linux game").arg(id);
    // Recorded before the file goes: should that fail, the sync sees the game again and forgets it.
    if (game.shortcut > 0 && m_steamRecord && !m_steamSyncRecord.isEmpty()) {
        SteamSync::Record record = *m_steamRecord;
        if (fromSteam) {
            record.removed.insert(game.shortcut, game.title);
        } else {
            // Forgotten: the shortcut stays in Steam as it is, and the grid files the launcher
            // wrote become Steam's.
            record.removed.remove(game.shortcut);
            record.shortcuts.remove(game.shortcut);
            record.grid.remove(game.shortcut);
        }
        QString error;
        if (!SteamSync::writeRecord(m_steamSyncRecord, record, m_dataDir, &error)) {
            qCWarning(lcSteam).noquote() << error;
            return tr("Not removed: %1").arg(error);
        }
        m_steamRecord = record;
    }
    beginBatch();
    const QString error = removeGameFile(id, true);
    m_snapshot.programFiles.remove(id);
    updateSteamQueue();
    endBatch();
    if (!m_snapshot.steamQueued.isEmpty())
        m_steamApplyTimer.start();
    refresh();
    return error;
}

QString LibraryStore::undoSteamQueue(const QString &key)
{
    if (m_versionStatus.readOnly)
        return tr("Not undone: %1").arg(m_versionStatus.reason);
    const QStringList queued = m_snapshot.steamQueued.value(key);
    if (queued.isEmpty() || !m_steamRecord)
        return {};
    SteamSync::Record record = *m_steamRecord;

    if (const quint32 appId = SteamSync::removedAppId(key); appId != 0) {
        record.removed.remove(appId);
        record.shortcuts.remove(appId);
    } else if (queued.contains(SteamSync::kAddShortcut)) {
        return removeProtonGame(key);
    } else if (queued.contains(SteamSync::kRemoveShortcut)) {
        return setGameValues(key, {{QStringLiteral("installed"), QVariant()}});
    } else {
        // The record takes the game file's values, so the next scan sees Steam's as changed
        // there and copies them into the game file (see SteamSync).
        const GameFile file = m_snapshot.content.games.value(key);
        if (file.platform == kProtonPlatform) {
            const QList<SteamSync::ShortcutGame> games = shortcutSyncGames(
                m_snapshot.content, m_dataDir, m_installLocations.home, record.grid, m_snapshot.steam.install);
            const auto g = std::find_if(games.cbegin(), games.cend(),
                                        [&](const SteamSync::ShortcutGame &s) { return s.id == key; });
            if (g == games.cend())
                return {};
            if (!record.shortcuts.contains(g->appId)) { // queued for want of a record
                const QList<SteamShortcut> &shortcuts = m_snapshot.steam.shortcuts;
                const auto s = std::find_if(shortcuts.cbegin(), shortcuts.cend(),
                                            [&](const SteamShortcut &c) { return c.appId == g->appId; });
                if (s == shortcuts.cend())
                    return {};
                record.shortcuts.insert(g->appId, SteamSync::recordOf(*s, m_snapshot.steam.compatTools.value(g->appId)));
            }
            SteamSync::ShortcutRecord &r = record.shortcuts[g->appId];
            if (queued.contains(QStringLiteral("title")))
                r.title = g->title;
            if (queued.contains(QStringLiteral("exe")))
                r.exe = g->exe;
            if (queued.contains(QStringLiteral("start_dir")))
                r.startDir = g->startFolder();
            if (queued.contains(QStringLiteral("launch_options")))
                r.launchOptions = g->launchOptions;
            if (queued.contains(QStringLiteral("proton")))
                r.proton = g->proton;
        } else {
            const QList<SteamSync::Game> games = steamSyncGames(m_snapshot.content);
            const auto g = std::find_if(games.cbegin(), games.cend(),
                                        [&](const SteamSync::Game &s) { return s.id == key; });
            if (g == games.cend())
                return {};
            if (queued.contains(QStringLiteral("launch_options")))
                record.launchOptions.insert(g->appId, g->launchOptions);
            if (queued.contains(QStringLiteral("proton")))
                record.proton.insert(g->appId, g->proton);
        }
    }

    QString error;
    if (!SteamSync::writeRecord(m_steamSyncRecord, record, m_dataDir, &error)) {
        qCWarning(lcSteam).noquote() << error;
        return tr("Not undone: %1").arg(error);
    }
    m_steamRecord = record;
    qCInfo(lcSteam).noquote() << QStringLiteral("%1: undid what waited for Steam").arg(key);
    updateSteamQueue();
    changedHere();
    refresh();
    return {};
}

QStringList LibraryStore::artToShrink(const QString &id) const
{
    const ArtCheck check = m_snapshot.artChecks.value(id);
    const ArtSet art = m_snapshot.art.value(id);
    if (m_versionStatus.readOnly || check.art != art)
        return {};
    QStringList kinds = check.oversized;
    if (check.trimLogo && !kinds.contains(u"logo"))
        kinds.append(QStringLiteral("logo"));
    QStringList out;
    for (const QString &kind : std::as_const(kinds)) {
        if (m_shrinkFailed.value(id + u'/' + kind, -1) != art.modified)
            out.append(kind);
    }
    return out;
}

bool LibraryStore::shrinkArt(const QString &id, const QString &kind)
{
    if (m_shrink.isRunning() || m_loading || scanning() || !artToShrink(id).contains(kind))
        return false;
    const ArtSet art = m_snapshot.art.value(id);
    ArtShrinkJob job;
    job.id = id;
    job.kind = kind;
    job.path = kind == u"hero" ? art.hero : kind == u"logo" ? art.logo : kind == u"icon" ? art.icon : art.cover;
    job.temporary = QFileInfo(job.path).absolutePath() + QStringLiteral("/.") + kind + QStringLiteral(".shrink");
    job.modified = QFileInfo(job.path).lastModified(QTimeZone::UTC).toMSecsSinceEpoch();
    m_shrink.setFuture(QtConcurrent::run([job]() mutable {
        ArtImage::allowLargeImages();
        job.before = QImageReader(job.path).size();
        job.error = ArtImage::copy(job.path, job.temporary, job.kind, &job.changed);
        if (job.error.isEmpty())
            job.after = job.changed ? QImageReader(job.temporary).size() : job.before;
        return job;
    }));
    return true;
}

void LibraryStore::onShrinkFinished()
{
    const ArtShrinkJob job = m_shrink.result();
    const ArtSet before = m_snapshot.art.value(job.id);
    const qint64 now = QFileInfo(job.path).lastModified(QTimeZone::UTC).toMSecsSinceEpoch();
    if (now != job.modified) {
        // Replaced meanwhile: the replacement stays, and the next scan checks it.
        QFile::remove(job.temporary);
        m_shrinkFailed.insert(job.id + u'/' + job.kind, before.modified);
        refresh();
        emit artShrunk();
        return;
    }
    const auto settle = [&](const ArtSet &art) {
        const ArtCheck check = m_snapshot.artChecks.value(job.id);
        if (check.art != before)
            return;
        ArtCheck updated{art, check.oversized, check.trimLogo && job.kind != u"logo"};
        updated.oversized.removeAll(job.kind);
        m_snapshot.artChecks.insert(job.id, updated);
    };
    if (job.error.isEmpty() && !job.changed) {
        settle(before);
        scheduleCacheSave();
        emit artShrunk();
        return;
    }
    // rename(2) replaces the original in one step, so a failure leaves it.
    QString error = job.error;
    if (error.isEmpty() && ::rename(QFile::encodeName(job.temporary), QFile::encodeName(job.path)) != 0)
        error = tr("Could not replace %1").arg(job.path);
    if (!error.isEmpty()) {
        QFile::remove(job.temporary);
        m_shrinkFailed.insert(job.id + u'/' + job.kind, before.modified);
        if (!m_shrinkFailureLogged) {
            m_shrinkFailureLogged = true;
            qCWarning(lcLibrary).noquote() << "could not shrink art (others may follow):" << job.path << error;
        }
        emit artShrunk();
        return;
    }

    const ArtSet art = scanArtFolder(QFileInfo(job.path).absolutePath());
    settle(art);
    m_snapshot.art.insert(job.id, art);
    qCInfo(lcLibrary).noquote() << QStringLiteral("art/%1/%2: %3×%4 → %5×%6")
                                       .arg(job.id, QFileInfo(job.path).fileName())
                                       .arg(job.before.width()).arg(job.before.height())
                                       .arg(job.after.width()).arg(job.after.height());
    changedHere();
    emit artShrunk();
}

QString LibraryStore::setArt(const QString &id, const QString &kind, const QString &source)
{
    if (m_versionStatus.readOnly)
        return tr("Not saved: %1").arg(m_versionStatus.reason);
    if (!kArtKinds.contains(kind))
        return tr("Unknown kind of art %1").arg(kind);
    if (!takenIds(m_snapshot.content).contains(id))
        return tr("There's no game with the id %1").arg(id);

    QImageReader reader(source);
    if (!reader.canRead())
        return tr("%1 can't be shown: %2").arg(QFileInfo(source).fileName(), reader.errorString());
    QString extension = QFileInfo(source).suffix().toLower();
    if (!kArtExtensions.contains(extension)) {
        extension = QString::fromLatin1(reader.format()).toLower();
        if (!kArtExtensions.contains(extension))
            return tr("Art must be a PNG, JPEG or WebP image");
    }

    const QString folder = m_dataDir + QStringLiteral("/art/") + id;
    const QString target = folder + u'/' + kind + u'.' + extension;
    if (QFileInfo(source).canonicalFilePath() == QFileInfo(target).canonicalFilePath())
        return {};
    // Copied under a hidden name first (the scan skips hidden files), so a failed copy
    // leaves the old art in place.
    const QString temporary = folder + QStringLiteral("/.") + kind + QStringLiteral(".new");
    QDir().mkpath(folder);
    if (const QString error = ArtImage::copy(source, temporary, kind); !error.isEmpty())
        return error;
    for (const QString &ext : kArtExtensions)
        QFile::remove(folder + u'/' + kind + u'.' + ext);
    if (!QFile::rename(temporary, target)) {
        QFile::remove(temporary);
        return tr("Could not save art/%1/%2.%3").arg(id, kind, extension);
    }
    // A copy keeps the source's permissions, which may be read-only; and a fresh time,
    // so the new image loads even if the source is older than the old one.
    QFile::setPermissions(target, QFile::permissions(target) | QFile::WriteOwner);
    if (QFile file(target); file.open(QIODevice::ReadWrite))
        file.setFileTime(QDateTime::currentDateTime(), QFileDevice::FileModificationTime);

    m_snapshot.art.insert(id, scanArtFolder(folder));
    qCInfo(lcLibrary).noquote() << QStringLiteral("art/%1/%2.%3 from %4").arg(id, kind, extension, source);
    changedHere();
    return {};
}

QString LibraryStore::patchAndLoad(const QString &relative, const QString &header,
                                   const std::function<TomlPatch::Result(const QString &)> &patch)
{
    if (m_versionStatus.readOnly)
        return tr("Not saved: %1").arg(m_versionStatus.reason);
    const QString path = m_dataDir + u'/' + relative;
    const bool exists = QFileInfo::exists(path);
    QString before = header;
    if (QFile file(path); file.open(QIODevice::ReadOnly))
        before = QString::fromUtf8(file.readAll());
    else if (exists)
        return tr("Could not read %1: %2").arg(relative, file.errorString());

    const TomlPatch::Result r = patch(before);
    if (!r.ok) {
        const QString problem = tr("%1 has an error, so the change wasn't saved: %2").arg(relative, r.error);
        qCWarning(lcLibrary).noquote() << problem;
        return problem;
    }
    if (r.text == before)
        return {};
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly) || file.write(r.text.toUtf8()) < 0 || !file.commit()) {
        const QString problem = tr("Could not save %1: %2").arg(relative, file.errorString());
        qCWarning(lcLibrary).noquote() << problem;
        return problem;
    }
    CachedFile &entry = m_snapshot.files[relative];
    entry.stamp = stampOf(QFileInfo(path));
    entry.ok = parseInto(m_snapshot, relative, r.text.toUtf8(), &entry.warnings);
    return {};
}

QHash<QString, int> LibraryStore::tagUse() const
{
    QHash<QString, int> out;
    for (const GameFile &game : m_snapshot.content.games) {
        for (const QString &tag : game.tags)
            ++out[tag];
    }
    return out;
}

QStringList LibraryStore::retagGames(const QString &name, const std::function<QStringList(QStringList)> &change)
{
    QStringList ids;
    for (auto it = m_snapshot.content.games.cbegin(); it != m_snapshot.content.games.cend(); ++it) {
        if (it->tags.contains(name))
            ids.append(it.key());
    }
    QStringList errors;
    ++m_batching;
    for (const QString &id : std::as_const(ids)) {
        QStringList tags = change(m_snapshot.content.games.value(id).tags);
        tags.removeDuplicates();
        if (const QString error = setGameValues(id, {{QStringLiteral("tags"), tags}}); !error.isEmpty())
            errors.append(error);
    }
    --m_batching;
    return errors;
}

namespace {
TomlPatch::Block tagBlock(const QString &name)
{
    return {QStringLiteral("tag"), name, QStringLiteral("name")};
}

bool tagListed(const QString &text, const QString &name)
{
    return TomlPatch::blockValues(text, QStringLiteral("tag"), QStringLiteral("name")).contains(name);
}

QString igdbList(const QStringList &entries)
{
    QStringList items;
    for (const QString &e : entries)
        items.append(tomlString(e));
    return u'[' + items.join(QLatin1String(", ")) + u']';
}
}

QString LibraryStore::addTag(const QString &name, int color)
{
    const QString tag = name.trimmed();
    if (tag.isEmpty())
        return tr("A tag needs a name");
    if (tagUse().contains(tag))
        return tr("There's already a tag named %1").arg(tag);
    bool listed = false;
    QString error = patchAndLoad(kTags, kTagsHeader, [&](const QString &text) {
        listed = tagListed(text, tag);
        if (listed)
            return TomlPatch::Result{true, text, {}};
        return TomlPatch::appendBlock(text, QStringLiteral("tag"),
                                      {{QStringLiteral("name"), tomlString(tag)},
                                       {QStringLiteral("color"), QString::number(qMax(0, color))}});
    });
    if (listed)
        return tr("There's already a tag named %1").arg(tag);
    if (error.isEmpty())
        changedHere();
    return error;
}

QString LibraryStore::setTagColor(const QString &name, int color)
{
    const QString error = patchAndLoad(kTags, kTagsHeader, [&](const QString &text) {
        const QString value = QString::number(qMax(0, color));
        if (tagListed(text, name))
            return TomlPatch::setBlockValue(text, tagBlock(name), QStringLiteral("color"), value);
        return TomlPatch::appendBlock(text, QStringLiteral("tag"),
                                      {{QStringLiteral("name"), tomlString(name)}, {QStringLiteral("color"), value}});
    });
    if (error.isEmpty())
        changedHere();
    return error;
}

QString LibraryStore::renameTag(const QString &from, const QString &to)
{
    const QString tag = to.trimmed();
    if (tag.isEmpty())
        return tr("A tag needs a name");
    if (tag == from)
        return {};
    bool taken = tagUse().contains(tag);
    for (const TagDef &t : std::as_const(m_snapshot.content.tags))
        taken = taken || t.name == tag;
    if (taken)
        return tr("There's already a tag named %1").arg(tag);

    // A tag that has a default's IGDB list by its name keeps it under the new one.
    QStringList implicitIgdb;
    for (const TagDef &t : std::as_const(m_snapshot.content.tags)) {
        if (t.name == from && !t.igdbGiven)
            implicitIgdb = igdbOf(t);
    }
    const QString error = patchAndLoad(kTags, kTagsHeader, [&](const QString &text) {
        if (!tagListed(text, from))
            return TomlPatch::Result{true, text, {}};
        TomlPatch::Result r = TomlPatch::setBlockValue(text, tagBlock(from), QStringLiteral("name"), tomlString(tag));
        if (!r.ok || implicitIgdb.isEmpty())
            return r;
        return TomlPatch::setBlockValue(r.text, tagBlock(tag), QStringLiteral("igdb"), igdbList(implicitIgdb));
    });
    if (!error.isEmpty())
        return error; // the games keep the old name, so the tag still matches its block
    const QStringList errors = retagGames(from, [&](QStringList tags) {
        for (QString &t : tags) {
            if (t == from)
                t = tag;
        }
        return tags;
    });
    changedHere();
    return errors.join(u'\n');
}

QString LibraryStore::removeTag(const QString &name)
{
    const QString error = patchAndLoad(kTags, kTagsHeader, [&](const QString &text) {
        return TomlPatch::removeBlock(text, tagBlock(name));
    });
    if (!error.isEmpty())
        return error;
    const QStringList errors = retagGames(name, [&](QStringList tags) {
        tags.removeAll(name);
        return tags;
    });
    changedHere();
    return errors.join(u'\n');
}

const QList<TagDef> &LibraryStore::defaultTags()
{
    static const QList<TagDef> tags = parseTags(defaultTagsFile().toUtf8(), QStringLiteral("resources/tags.toml")).value;
    return tags;
}

QStringList LibraryStore::igdbOf(const TagDef &tag)
{
    if (tag.igdbGiven)
        return tag.igdb;
    static const QHash<QString, QStringList> byName = [] {
        QHash<QString, QStringList> lists;
        for (const TagDef &t : defaultTags())
            lists.insert(t.name.toCaseFolded(), t.igdb);
        return lists;
    }();
    return byName.value(tag.name.toCaseFolded());
}

QList<TagDef> LibraryStore::missingDefaultTags() const
{
    QSet<QString> have;
    for (const TagDef &t : std::as_const(m_snapshot.content.tags))
        have.insert(t.name.toCaseFolded());
    const QHash<QString, int> use = tagUse();
    for (auto it = use.cbegin(); it != use.cend(); ++it)
        have.insert(it.key().toCaseFolded());
    QList<TagDef> missing;
    for (const TagDef &t : defaultTags()) {
        if (!have.contains(t.name.toCaseFolded()))
            missing.append(t);
    }
    return missing;
}

QString LibraryStore::addDefaultTags()
{
    const QList<TagDef> missing = missingDefaultTags();
    if (missing.isEmpty())
        return {};
    const QString error = patchAndLoad(kTags, kTagsHeader, [&](QString text) {
        for (const TagDef &t : missing) {
            QList<std::pair<QString, QString>> values = {{QStringLiteral("name"), tomlString(t.name)},
                                                         {QStringLiteral("color"), QString::number(t.color)}};
            if (!t.igdb.isEmpty())
                values.append({QStringLiteral("igdb"), igdbList(t.igdb)});
            TomlPatch::Result r = TomlPatch::appendBlock(text, QStringLiteral("tag"), values);
            if (!r.ok)
                return r;
            text = r.text;
        }
        return TomlPatch::Result{true, text, {}};
    });
    if (error.isEmpty())
        changedHere();
    return error;
}

QString LibraryStore::removeArt(const QString &id, const QString &kind)
{
    if (m_versionStatus.readOnly)
        return tr("Not saved: %1").arg(m_versionStatus.reason);
    if (!kArtKinds.contains(kind))
        return tr("Unknown kind of art %1").arg(kind);
    const QString folder = m_dataDir + QStringLiteral("/art/") + id;
    for (const QString &ext : kArtExtensions) {
        const QString path = folder + u'/' + kind + u'.' + ext;
        if (QFileInfo::exists(path) && !QFile::remove(path))
            return tr("Could not remove art/%1/%2.%3").arg(id, kind, ext);
    }
    QDir().rmdir(folder);
    const ArtSet art = scanArtFolder(folder);
    if (art == ArtSet{})
        m_snapshot.art.remove(id);
    else
        m_snapshot.art.insert(id, art);
    changedHere();
    return {};
}
