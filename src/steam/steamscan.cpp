#include "steamscan.h"

#include "vdf.h"

#include <QCollator>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QTimeZone>

#include <algorithm>
#include <limits>

namespace {
SteamStamp stampOf(const QString &path)
{
    const QFileInfo info(path);
    if (!info.isFile())
        return {};
    return {info.lastModified(QTimeZone::UTC).toMSecsSinceEpoch(), info.size()};
}

bool readVdf(const QString &path, Vdf::Document *doc, QStringList *warnings)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        warnings->append(QStringLiteral("%1: could not read: %2").arg(path, file.errorString()));
        return false;
    }
    *doc = Vdf::parse(file.readAll());
    if (!doc->ok)
        warnings->append(QStringLiteral("%1: %2").arg(path, doc->error));
    return doc->ok;
}

constexpr int kUpdateRequired = 2;
constexpr int kFullyInstalled = 4;
constexpr int kUpdateRunning = 256;
constexpr int kUpdatePaused = 512;
constexpr int kUpdateStarted = 1024;

SteamManifest readManifest(const QString &path, const QString &library, QStringList *warnings)
{
    SteamManifest m;
    m.stamp = stampOf(path);
    Vdf::Document doc;
    if (!readVdf(path, &doc, warnings))
        return m;
    const Vdf::Node *state = doc.root.child(u"AppState");
    if (!state || !state->isBlock) {
        warnings->append(QStringLiteral("%1: not an app manifest").arg(path));
        return m;
    }
    SteamApp &app = m.app;
    app.appId = state->valueOf(u"appid").toLongLong();
    if (app.appId <= 0) {
        static const QRegularExpression re(QStringLiteral("appmanifest_(\\d+)\\.acf$"));
        app.appId = re.match(path).captured(1).toLongLong();
    }
    if (app.appId <= 0) {
        warnings->append(QStringLiteral("%1: no app id").arg(path));
        return m;
    }
    app.name = state->valueOf(u"name");
    const QString installDir = state->valueOf(u"installdir");
    if (!installDir.isEmpty())
        app.installDir = library + QStringLiteral("/common/") + installDir;
    const qint64 flags = state->valueOf(u"StateFlags").toLongLong();
    const qint64 buildId = state->valueOf(u"buildid").toLongLong();
    const qint64 targetBuildId = state->valueOf(u"TargetBuildID").toLongLong();
    // Pausing in Steam clears "running" and leaves "started" (its log says "Suspended"); no real
    // manifest has had the paused bit, so it's kept as a guess.
    if (flags & kUpdatePaused)
        app.update = SteamUpdate::Paused;
    else if (flags & kUpdateRunning)
        app.update = SteamUpdate::Downloading;
    else if (flags & kUpdateStarted)
        app.update = SteamUpdate::Paused;
    else if ((flags & kUpdateRequired) || (targetBuildId != 0 && targetBuildId != buildId))
        app.update = SteamUpdate::Waiting;
    // An update under way clears the installed bit, but a build on disk (buildid is 0 until a
    // first install completes) means the game still launches.
    app.installed = (flags & kFullyInstalled) != 0 || (app.update != SteamUpdate::None && buildId != 0);
    app.sizeBytes = state->valueOf(u"SizeOnDisk").toLongLong();
    // Steam keeps the last update's byte counts after it finishes.
    if (app.update != SteamUpdate::None) {
        app.updateBytes = state->valueOf(u"BytesToDownload").toLongLong();
        // Steam's percentage is the staging progress (unpacked bytes), which runs ahead of the
        // download's.
        const qint64 toStage = state->valueOf(u"BytesToStage").toLongLong();
        const qint64 downloaded = state->valueOf(u"BytesDownloaded").toLongLong();
        const qint64 total = toStage > 0 ? toStage : app.updateBytes;
        const qint64 done = toStage > 0 ? state->valueOf(u"BytesStaged").toLongLong() : downloaded;
        if (total > 0)
            app.updatePercent = int(std::clamp<qint64>(done * 100 / total, 0, 100));
    }
    m.ok = true;
    return m;
}

QStringList readLibraries(const QString &root, const QString &path, QStringList *warnings)
{
    QStringList out = {root + QStringLiteral("/steamapps")};
    Vdf::Document doc;
    if (!readVdf(path, &doc, warnings))
        return out;
    const Vdf::Node *folders = doc.root.child(u"libraryfolders");
    if (!folders)
        return out;
    for (const Vdf::Node &folder : folders->children) {
        // Before 2021 it was "<n>" "<path>", next to other values; now "<n>" { "path" "<path>" … }.
        const QString folderPath = folder.isBlock ? folder.valueOf(u"path") : folder.value;
        if (folderPath.isEmpty() || (!folder.isBlock && folder.key.toLongLong() <= 0))
            continue;
        const QString steamapps = QFileInfo(folderPath + QStringLiteral("/steamapps")).canonicalFilePath();
        if (!steamapps.isEmpty() && !out.contains(steamapps))
            out.append(steamapps);
    }
    return out;
}

QHash<qint64, SteamAppConfig> readLocalConfig(const QString &path, QStringList *warnings, bool *ok)
{
    QHash<qint64, SteamAppConfig> out;
    Vdf::Document doc;
    *ok = QFileInfo::exists(path) && readVdf(path, &doc, warnings);
    if (!*ok)
        return out;
    const Vdf::Node *apps = doc.root.find({QStringLiteral("UserLocalConfigStore"), QStringLiteral("Software"),
                                           QStringLiteral("Valve"), QStringLiteral("Steam"), QStringLiteral("apps")});
    if (!apps)
        return out;
    for (const Vdf::Node &app : apps->children) {
        // Shortcuts' play time is under their signed app id: "-1974090208".
        bool isNumber = false;
        const qint64 appId = app.key.toLongLong(&isNumber);
        if (!isNumber || appId == 0 || appId < std::numeric_limits<qint32>::min() || !app.isBlock)
            continue;
        SteamAppConfig config;
        bool ok = false;
        const int minutes = app.valueOf(u"Playtime").toInt(&ok);
        if (ok && minutes >= 0)
            config.playMinutes = minutes;
        config.lastPlayed = app.valueOf(u"LastPlayed").toLongLong();
        config.launchOptions = app.valueOf(u"LaunchOptions");
        if (config != SteamAppConfig())
            out.insert(appId, config);
    }
    return out;
}

QHash<qint64, QString> readCompatToolMapping(const QString &path, QStringList *warnings, bool *ok)
{
    QHash<qint64, QString> out;
    Vdf::Document doc;
    *ok = QFileInfo::exists(path) && readVdf(path, &doc, warnings);
    if (!*ok)
        return out;
    const Vdf::Node *mapping = doc.root.find({QStringLiteral("InstallConfigStore"), QStringLiteral("Software"),
                                              QStringLiteral("Valve"), QStringLiteral("Steam"),
                                              QStringLiteral("CompatToolMapping")});
    if (!mapping)
        return out;
    for (const Vdf::Node &app : mapping->children) {
        bool ok = false;
        const qint64 appId = app.key.toLongLong(&ok);
        const QString name = app.valueOf(u"name");
        if (ok && appId >= 0 && !name.isEmpty())
            out.insert(appId, name);
    }
    return out;
}

QList<SteamShortcut> readShortcuts(const QString &path, QStringList *warnings, bool *ok)
{
    *ok = !QFileInfo::exists(path);
    if (*ok)
        return {};
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        warnings->append(QStringLiteral("%1: could not read: %2").arg(path, file.errorString()));
        return {};
    }
    const BinaryVdf::Document doc = BinaryVdf::parse(file.readAll());
    if (!doc.ok) {
        warnings->append(QStringLiteral("%1: %2").arg(path, doc.error));
        return {};
    }
    *ok = true;
    return SteamShortcuts::read(doc.root);
}

QList<CompatTool> readToolFile(const QString &path, QStringList *warnings)
{
    QList<CompatTool> out;
    Vdf::Document doc;
    if (!readVdf(path, &doc, warnings))
        return out;
    const Vdf::Node *tools = doc.root.find({QStringLiteral("compatibilitytools"), QStringLiteral("compat_tools")});
    if (!tools)
        return out;
    for (const Vdf::Node &tool : tools->children) {
        if (!tool.isBlock)
            continue;
        out.append({tool.key, tool.valueOf(u"display_name", tool.key)});
    }
    return out;
}

int toolOrder(const QString &displayName)
{
    if (displayName == u"Proton Experimental")
        return 0;
    if (displayName == u"Proton Hotfix")
        return 1;
    static const QRegularExpression re(QStringLiteral("^Proton (\\d+)\\.(\\d+)"));
    const QRegularExpressionMatch m = re.match(displayName);
    return m.hasMatch() ? 1'000'000 - (m.captured(1).toInt() * 1000 + m.captured(2).toInt()) : 2;
}
}

QList<SteamApp> SteamState::games() const
{
    QList<SteamApp> out;
    for (const SteamManifest &m : manifests) {
        if (m.ok && !SteamScan::isBlocked(m.app.appId, m.app.name))
            out.append(m.app);
    }
    std::sort(out.begin(), out.end(), [](const SteamApp &a, const SteamApp &b) { return a.appId < b.appId; });
    for (qsizetype i = out.size() - 1; i > 0; --i) {
        if (out.at(i).appId == out.at(i - 1).appId) {
            if (out.at(i).installed && !out.at(i - 1).installed)
                out[i - 1] = out.at(i);
            out.removeAt(i);
        }
    }
    return out;
}

QList<CompatTool> SteamState::tools() const
{
    QList<std::pair<int, CompatTool>> valve;
    for (const SteamManifest &m : manifests) {
        const QString name = SteamScan::valveToolName(m.app.appId, m.app.name);
        if (m.ok && m.app.installed && !name.isEmpty()) {
            const bool known = std::any_of(valve.cbegin(), valve.cend(),
                                           [&](const auto &t) { return t.second.name == name; });
            if (!known)
                valve.append({toolOrder(m.app.name), {name, m.app.name}});
        }
    }
    std::sort(valve.begin(), valve.end(), [](const auto &a, const auto &b) {
        return a.first != b.first ? a.first < b.first : a.second.name < b.second.name;
    });
    QList<CompatTool> out;
    for (const auto &t : valve)
        out.append(t.second);
    QCollator collator;
    collator.setNumericMode(true);
    collator.setCaseSensitivity(Qt::CaseInsensitive);
    QList<CompatTool> custom = customTools;
    std::sort(custom.begin(), custom.end(), [&](const CompatTool &a, const CompatTool &b) {
        return collator.compare(a.displayName, b.displayName) > 0;
    });
    for (const CompatTool &t : custom) {
        const bool known = std::any_of(out.cbegin(), out.cend(), [&](const CompatTool &o) { return o.name == t.name; });
        if (!known)
            out.append(t);
    }
    return out;
}

SteamAppConfig SteamState::shortcutConfig(quint32 appId) const
{
    return apps.value(SteamShortcuts::signedAppId(appId));
}

QStringList SteamState::watchPaths() const
{
    if (!install.found())
        return {};
    QStringList out = {install.root + QStringLiteral("/steamapps/libraryfolders.vdf")};
    out += libraries;
    if (!localConfigPath.isEmpty() && localConfigStamp.size >= 0)
        out.append(localConfigPath);
    if (!shortcutsPath.isEmpty() && shortcutsStamp.size >= 0)
        out.append(shortcutsPath);
    if (configStamp.size >= 0)
        out.append(install.root + QStringLiteral("/config/config.vdf"));
    out += toolDirs;
    return out;
}

namespace SteamScan {
bool isBlocked(qint64 appId, const QString &name)
{
    static const QList<qint64> known = {
        228980,  // Steamworks Common Redistributables
        1007,    // Steamworks SDK Redist
        1070560, // Steam Linux Runtime 1.0 (scout)
        1391110, // Steam Linux Runtime 2.0 (soldier)
        1628350, // Steam Linux Runtime 3.0 (sniper)
        4183110, // Steam Linux Runtime 4.0
        1161040, // Proton BattlEye Runtime
        1826330, // Proton EasyAntiCheat Runtime
        4690330, // Legacy Steam Runtime
    };
    return known.contains(appId) || name.startsWith(QStringLiteral("Proton "))
        || name.startsWith(QStringLiteral("Steam Linux Runtime")) || name.endsWith(QStringLiteral("Steam Runtime"))
        || name == u"Steamworks Common Redistributables";
}

QString valveToolName(qint64 appId, const QString &name)
{
    Q_UNUSED(appId)
    if (!name.startsWith(QStringLiteral("Proton ")) || name.endsWith(QStringLiteral("Runtime")))
        return {};
    static const QRegularExpression version(QStringLiteral("^Proton (\\d+)\\.(\\d+)"));
    if (const QRegularExpressionMatch m = version.match(name); m.hasMatch()) {
        const QString minor = m.captured(2);
        return QStringLiteral("proton_") + m.captured(1) + (minor == u'0' ? QString() : minor);
    }
    return QStringLiteral("proton_") + name.mid(7).toLower().replace(u' ', u'_');
}

SteamState scan(const SteamSettings &settings, const SteamLocations &where, const SteamState &previous)
{
    SteamState s;
    s.enabled = settings.enabled;
    if (!s.enabled)
        return s;
    s.install = SteamLocate::find(where, settings.path, settings.user);
    if (s.install.pathMissing)
        s.warnings.append(QStringLiteral("settings.toml: [steam] path: no Steam at %1").arg(settings.path));
    if (s.install.userMissing)
        s.warnings.append(QStringLiteral("settings.toml: [steam] user: account %1 hasn't used Steam here").arg(settings.user));
    if (!s.install.found())
        return s;

    const SteamState none;
    const SteamState &prev = previous.install.root == s.install.root ? previous : none;
    const QString root = s.install.root;

    const QString libraryFolders = root + QStringLiteral("/steamapps/libraryfolders.vdf");
    s.libraryFoldersStamp = stampOf(libraryFolders);
    if (s.libraryFoldersStamp == prev.libraryFoldersStamp && !prev.libraries.isEmpty()) {
        s.libraries = prev.libraries;
    } else {
        s.libraries = readLibraries(root, libraryFolders, &s.warnings);
    }

    for (const QString &library : std::as_const(s.libraries)) {
        QDirIterator it(library, {QStringLiteral("appmanifest_*.acf")}, QDir::Files);
        while (it.hasNext()) {
            const QString path = it.next();
            const SteamStamp stamp = stampOf(path);
            const auto old = prev.manifests.constFind(path);
            if (old != prev.manifests.cend() && old->stamp == stamp)
                s.manifests.insert(path, *old);
            else
                s.manifests.insert(path, readManifest(path, library, &s.warnings));
        }
    }

    const QString userDir = s.install.userDataDir();
    if (!userDir.isEmpty()) {
        s.localConfigPath = userDir + QStringLiteral("/config/localconfig.vdf");
        s.localConfigStamp = stampOf(s.localConfigPath);
        if (s.localConfigPath == prev.localConfigPath && s.localConfigStamp == prev.localConfigStamp) {
            s.apps = prev.apps;
            s.localConfigOk = prev.localConfigOk;
        } else {
            s.apps = readLocalConfig(s.localConfigPath, &s.warnings, &s.localConfigOk);
        }
        s.shortcutsPath = userDir + QStringLiteral("/config/shortcuts.vdf");
        s.shortcutsStamp = stampOf(s.shortcutsPath);
        if (s.shortcutsPath == prev.shortcutsPath && s.shortcutsStamp == prev.shortcutsStamp) {
            s.shortcuts = prev.shortcuts;
            s.shortcutsOk = prev.shortcutsOk;
        } else {
            s.shortcuts = readShortcuts(s.shortcutsPath, &s.warnings, &s.shortcutsOk);
        }
    }

    const QString config = root + QStringLiteral("/config/config.vdf");
    s.configStamp = stampOf(config);
    if (s.configStamp == prev.configStamp) {
        s.compatTools = prev.compatTools;
        s.configOk = prev.configOk;
    } else {
        s.compatTools = readCompatToolMapping(config, &s.warnings, &s.configOk);
    }

    QStringList dirs = {root + QStringLiteral("/compatibilitytools.d")};
    dirs += where.systemToolDirs;
    for (const QString &dir : std::as_const(dirs)) {
        if (!QFileInfo(dir).isDir())
            continue;
        s.toolDirs.append(dir);
        QDirIterator it(dir, QDir::Dirs | QDir::NoDotAndDotDot);
        while (it.hasNext()) {
            const QString file = it.next() + QStringLiteral("/compatibilitytool.vdf");
            const SteamStamp stamp = stampOf(file);
            if (stamp.size >= 0)
                s.toolFiles.insert(file, stamp);
        }
    }
    if (s.toolFiles == prev.toolFiles) {
        s.customTools = prev.customTools;
    } else {
        QStringList files = s.toolFiles.keys();
        files.sort();
        for (const QString &file : std::as_const(files))
            s.customTools += readToolFile(file, &s.warnings);
    }

    // Warnings from files that weren't read again stay until the file is fixed.
    const auto keep = [&](const QString &path, bool reused) {
        if (!reused)
            return;
        for (const QString &w : prev.warnings) {
            if (w.startsWith(path + u':') && !s.warnings.contains(w))
                s.warnings.append(w);
        }
    };
    keep(libraryFolders, s.libraryFoldersStamp == prev.libraryFoldersStamp);
    for (auto it = s.manifests.cbegin(); it != s.manifests.cend(); ++it)
        keep(it.key(), prev.manifests.value(it.key()).stamp == it->stamp);
    keep(s.localConfigPath, s.localConfigStamp == prev.localConfigStamp);
    keep(s.shortcutsPath, s.shortcutsStamp == prev.shortcutsStamp);
    keep(config, s.configStamp == prev.configStamp);
    if (s.toolFiles == prev.toolFiles) {
        for (auto it = s.toolFiles.cbegin(); it != s.toolFiles.cend(); ++it)
            keep(it.key(), true);
    }
    s.warnings.sort();
    return s;
}
}
