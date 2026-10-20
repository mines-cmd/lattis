#include "librarycache.h"
#include "apppaths.h"

#include <QDataStream>
#include <QDir>
#include <QFile>
#include <QSaveFile>

namespace {
constexpr quint32 kMagic = 0x474c4c43;
constexpr QDataStream::Version kStreamVersion = QDataStream::Qt_6_8;
}

// Global and static: Qt's container streaming finds these by argument-dependent lookup.

static QDataStream &operator<<(QDataStream &s, const GameFile &g)
{
    return s << g.title << g.platform << g.description << qint32(g.year) << g.rating << g.tags << g.favorite << g.status
             << qint32(g.igdb) << qint32(g.steamgriddb) << qint32(g.playMinutes) << g.lastPlayed << g.added << g.hidden << g.emulator << g.rom << qint32(g.romLine) << g.args << g.launchOptions << g.proton << g.skipSteamArt
             << g.exe << g.startDir << g.installDir << g.shortcut << g.installed << g.logoScale << g.logoPosition << g.logoOffsetX << g.logoOffsetY << g.logoScrim << g.logoScrimSide << g.logoScrimStrength << g.ratingSource;
}

static QDataStream &operator>>(QDataStream &s, GameFile &g)
{
    qint32 year = 0;
    qint32 minutes = 0;
    qint32 romLine = 0;
    qint32 igdb = -1;
    qint32 steamgriddb = -1;
    s >> g.title >> g.platform >> g.description >> year >> g.rating >> g.tags >> g.favorite >> g.status >> igdb
        >> steamgriddb >> minutes
        >> g.lastPlayed >> g.added >> g.hidden >> g.emulator >> g.rom >> romLine >> g.args >> g.launchOptions >> g.proton >> g.skipSteamArt
        >> g.exe >> g.startDir >> g.installDir >> g.shortcut >> g.installed >> g.logoScale >> g.logoPosition >> g.logoOffsetX >> g.logoOffsetY >> g.logoScrim >> g.logoScrimSide >> g.logoScrimStrength >> g.ratingSource;
    g.romLine = romLine;
    g.igdb = igdb;
    g.steamgriddb = steamgriddb;
    g.year = year;
    g.playMinutes = minutes;
    return s;
}

static QDataStream &operator<<(QDataStream &s, const Platform &p)
{
    return s << p.id << p.name << p.icon << p.emulators << p.romFolders << p.igdbPlatforms << p.inRail << qint32(p.line);
}

static QDataStream &operator>>(QDataStream &s, Platform &p)
{
    qint32 line = 0;
    s >> p.id >> p.name >> p.icon >> p.emulators >> p.romFolders >> p.igdbPlatforms >> p.inRail >> line;
    p.line = line;
    return s;
}

static QDataStream &operator<<(QDataStream &s, const TagDef &t)
{
    return s << t.name << qint32(t.color) << t.igdb << t.igdbGiven;
}

static QDataStream &operator>>(QDataStream &s, TagDef &t)
{
    qint32 color = 0;
    s >> t.name >> color >> t.igdb >> t.igdbGiven;
    t.color = color;
    return s;
}

static QDataStream &operator<<(QDataStream &s, const IndexEntry &e)
{
    return s << e.appId << e.title;
}

static QDataStream &operator>>(QDataStream &s, IndexEntry &e)
{
    return s >> e.appId >> e.title;
}

static QDataStream &operator<<(QDataStream &s, const RomEntry &e)
{
    return s << e.path << e.id << e.title << e.titleId << e.size;
}

static QDataStream &operator<<(QDataStream &s, const RomFile &f)
{
    return s << f.path << f.size;
}

static QDataStream &operator>>(QDataStream &s, RomFile &f)
{
    return s >> f.path >> f.size;
}

static QDataStream &operator>>(QDataStream &s, RomEntry &e)
{
    return s >> e.path >> e.id >> e.title >> e.titleId >> e.size;
}

static QDataStream &operator<<(QDataStream &s, const EmulatorEntry &e)
{
    return s << e.id << e.values << e.at;
}

static QDataStream &operator>>(QDataStream &s, EmulatorEntry &e)
{
    return s >> e.id >> e.values >> e.at;
}

static QDataStream &operator<<(QDataStream &s, const ArtSet &a)
{
    return s << a.hero << a.logo << a.icon << a.cover << a.modified;
}

static QDataStream &operator>>(QDataStream &s, ArtSet &a)
{
    return s >> a.hero >> a.logo >> a.icon >> a.cover >> a.modified;
}

static QDataStream &operator<<(QDataStream &s, const ArtCheck &c)
{
    return s << c.art << c.oversized << c.trimLogo;
}

static QDataStream &operator>>(QDataStream &s, ArtCheck &c)
{
    return s >> c.art >> c.oversized >> c.trimLogo;
}

static QDataStream &operator<<(QDataStream &s, const SteamStamp &t)
{
    return s << t.mtimeMs << t.size;
}

static QDataStream &operator>>(QDataStream &s, SteamStamp &t)
{
    return s >> t.mtimeMs >> t.size;
}

static QDataStream &operator<<(QDataStream &s, const SteamApp &a)
{
    return s << a.appId << a.name << a.installDir << a.installed << a.sizeBytes << qint8(a.update) << a.updateBytes
             << qint32(a.updatePercent);
}

static QDataStream &operator>>(QDataStream &s, SteamApp &a)
{
    qint8 update = 0;
    qint32 percent = -1;
    s >> a.appId >> a.name >> a.installDir >> a.installed >> a.sizeBytes >> update >> a.updateBytes >> percent;
    a.updatePercent = percent;
    a.update = update >= qint8(SteamUpdate::None) && update <= qint8(SteamUpdate::Paused) ? SteamUpdate(update)
                                                                                           : SteamUpdate::None;
    return s;
}

static QDataStream &operator<<(QDataStream &s, const SteamManifest &m)
{
    return s << m.stamp << m.app << m.ok;
}

static QDataStream &operator>>(QDataStream &s, SteamManifest &m)
{
    return s >> m.stamp >> m.app >> m.ok;
}

static QDataStream &operator<<(QDataStream &s, const SteamAppConfig &c)
{
    return s << qint32(c.playMinutes) << c.lastPlayed << c.launchOptions;
}

static QDataStream &operator>>(QDataStream &s, SteamAppConfig &c)
{
    qint32 minutes = -1;
    s >> minutes >> c.lastPlayed >> c.launchOptions;
    c.playMinutes = minutes;
    return s;
}

static QDataStream &operator<<(QDataStream &s, const ProgramFiles &f)
{
    return s << f.exe << f.exeFound << f.installDir << f.sizeBytes << f.singleFile;
}

static QDataStream &operator>>(QDataStream &s, ProgramFiles &f)
{
    return s >> f.exe >> f.exeFound >> f.installDir >> f.sizeBytes >> f.singleFile;
}

static QDataStream &operator<<(QDataStream &s, const SteamShortcut &c)
{
    return s << c.appId << c.name << c.exe << c.exeArgs << c.startDir << c.icon << c.launchOptions << c.lastPlayTime
             << c.hidden;
}

static QDataStream &operator>>(QDataStream &s, SteamShortcut &c)
{
    return s >> c.appId >> c.name >> c.exe >> c.exeArgs >> c.startDir >> c.icon >> c.launchOptions >> c.lastPlayTime
        >> c.hidden;
}

static QDataStream &operator<<(QDataStream &s, const CompatTool &t)
{
    return s << t.name << t.displayName;
}

static QDataStream &operator>>(QDataStream &s, CompatTool &t)
{
    return s >> t.name >> t.displayName;
}

static QDataStream &operator<<(QDataStream &s, const SteamUser &u)
{
    return s << u.accountId << u.steamId64 << u.personaName << u.accountName << u.mostRecent << u.timestamp;
}

static QDataStream &operator>>(QDataStream &s, SteamUser &u)
{
    return s >> u.accountId >> u.steamId64 >> u.personaName >> u.accountName >> u.mostRecent >> u.timestamp;
}

static QDataStream &operator<<(QDataStream &s, const SteamState &t)
{
    return s << t.enabled << qint32(t.install.kind) << t.install.root << t.install.pathMissing << t.install.users
             << t.install.userId << t.install.userMissing << t.libraryFoldersStamp << t.libraries << t.manifests
             << t.localConfigPath << t.localConfigStamp << t.apps << t.localConfigOk << t.shortcutsPath
             << t.shortcutsStamp << t.shortcuts << t.shortcutsOk << t.configStamp << t.configOk
             << t.compatTools << t.toolDirs << t.toolFiles << t.customTools << t.warnings;
}

static QDataStream &operator>>(QDataStream &s, SteamState &t)
{
    qint32 kind = 0;
    s >> t.enabled >> kind >> t.install.root >> t.install.pathMissing >> t.install.users >> t.install.userId
        >> t.install.userMissing >> t.libraryFoldersStamp >> t.libraries >> t.manifests >> t.localConfigPath
        >> t.localConfigStamp >> t.apps >> t.localConfigOk >> t.shortcutsPath >> t.shortcutsStamp >> t.shortcuts
        >> t.shortcutsOk >> t.configStamp >> t.configOk >> t.compatTools
        >> t.toolDirs >> t.toolFiles >> t.customTools >> t.warnings;
    t.install.kind = SteamInstall::Kind(kind);
    return s;
}

static QDataStream &operator<<(QDataStream &s, const CachedFile &f)
{
    return s << f.stamp.mtimeMs << f.stamp.size << f.warnings << f.ok;
}

static QDataStream &operator>>(QDataStream &s, CachedFile &f)
{
    return s >> f.stamp.mtimeMs >> f.stamp.size >> f.warnings >> f.ok;
}

QStringList LibrarySnapshot::warnings() const
{
    QStringList paths = files.keys();
    paths.sort();
    QStringList out;
    for (const QString &path : std::as_const(paths))
        out += files.value(path).warnings;
    return out + romWarnings + steam.warnings;
}

namespace LibraryCache {
QString pathFor(const QString &cacheDir, const QString &dataDir)
{
    return QStringLiteral("%1/library-%2.bin").arg(cacheDir, AppPaths::dataDirKey(dataDir));
}

std::optional<LibrarySnapshot> load(const QString &cachePath, const QString &dataDir)
{
    QFile file(cachePath);
    if (!file.open(QIODevice::ReadOnly))
        return std::nullopt;
    QDataStream in(&file);

    quint32 magic = 0;
    quint32 format = 0;
    qint32 streamVersion = 0;
    QString cachedDataDir;
    in >> magic >> format >> streamVersion;
    if (in.status() != QDataStream::Ok || magic != kMagic || format != kFormatVersion
        || streamVersion != qint32(kStreamVersion))
        return std::nullopt;
    in.setVersion(kStreamVersion);
    in >> cachedDataDir;
    if (cachedDataDir != QDir(dataDir).absolutePath())
        return std::nullopt;

    LibrarySnapshot s;
    qint32 version = 0;
    in >> version >> s.files >> s.content.platforms >> s.content.tags >> s.content.steam >> s.content.roms
        >> s.content.emulators >> s.content.games >> s.art >> s.artChecks >> s.romFiles >> s.romWarnings >> s.romMissingFolders
        >> s.romGameFiles >> s.programFiles >> s.steam >> s.steamQueued;
    s.version = version;
    if (in.status() != QDataStream::Ok || !in.atEnd())
        return std::nullopt;
    return s;
}

bool save(const QString &cachePath, const QString &dataDir, const LibrarySnapshot &s)
{
    QDir().mkpath(QFileInfo(cachePath).absolutePath());
    QSaveFile file(cachePath);
    if (!file.open(QIODevice::WriteOnly))
        return false;
    QDataStream out(&file);
    out << kMagic << kFormatVersion << qint32(kStreamVersion);
    out.setVersion(kStreamVersion);
    out << QDir(dataDir).absolutePath();
    out << qint32(s.version) << s.files << s.content.platforms << s.content.tags << s.content.steam
        << s.content.roms << s.content.emulators << s.content.games << s.art << s.artChecks << s.romFiles << s.romWarnings << s.romMissingFolders
        << s.romGameFiles << s.programFiles << s.steam << s.steamQueued;
    if (out.status() != QDataStream::Ok) {
        file.cancelWriting();
        return false;
    }
    return file.commit();
}

Changes diff(const QHash<QString, FileStamp> &onDisk, const QHash<QString, CachedFile> &cached)
{
    Changes out;
    for (auto it = onDisk.cbegin(); it != onDisk.cend(); ++it) {
        const auto old = cached.constFind(it.key());
        if (old == cached.cend())
            out.added.append(it.key());
        else if (old->stamp != it.value())
            out.changed.append(it.key());
    }
    for (auto it = cached.cbegin(); it != cached.cend(); ++it) {
        if (!onDisk.contains(it.key()))
            out.removed.append(it.key());
    }
    out.added.sort();
    out.changed.sort();
    out.removed.sort();
    return out;
}
}
