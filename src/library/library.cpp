#include "library.h"
#include "dateformat.h"
#include "gameid.h"
#include "igdb.h"
#include "launchcommand.h"
#include "romfoldermatch.h"
#include "romscan.h"
#include "searchtext.h"
#include "steamart.h"
#include "tomlpatch.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJSEngine>
#include <QLocale>
#include <QStandardPaths>
#include <QUrl>
#include <QtConcurrent>

#include <algorithm>
#include <cstdlib>
#include <iterator>
#include <numeric>

namespace {
qreal hueOf(const QString &text)
{
    return qreal(qHash(text) % 360) / 360.0;
}

QString playTimeText(int minutes)
{
    if (minutes <= 0)
        return {};
    if (minutes < 60)
        return Library::tr("%n minute(s)", nullptr, minutes);
    return Library::tr("%1 hours").arg(QLocale().toString(minutes / 60.0, 'f', 1));
}

// -1 when Steam gave no size, or while nothing is downloading.
int updatePercent(const Game &g)
{
    if (g.steamUpdate != SteamUpdate::Downloading && g.steamUpdate != SteamUpdate::Paused)
        return -1;
    return g.updatePercent;
}

QString updateStateName(SteamUpdate update)
{
    switch (update) {
    case SteamUpdate::Waiting: return QStringLiteral("waiting");
    case SteamUpdate::Downloading: return QStringLiteral("downloading");
    case SteamUpdate::Paused: return QStringLiteral("paused");
    default: return QStringLiteral("none");
    }
}

QString updateText(const Game &g)
{
    const int percent = updatePercent(g);
    switch (g.steamUpdate) {
    case SteamUpdate::Waiting:
        if (g.updateBytes <= 0)
            return Library::tr("Waiting");
        return Library::tr("Waiting · %1")
            .arg(QLocale().formattedDataSize(g.updateBytes, 1, QLocale::DataSizeTraditionalFormat));
    case SteamUpdate::Downloading:
        return percent < 0 ? Library::tr("Downloading") : Library::tr("Downloading · %1%").arg(percent);
    case SteamUpdate::Paused:
        return percent < 0 ? Library::tr("Paused") : Library::tr("Paused · %1%").arg(percent);
    default:
        return {};
    }
}

QString shortPlayTimeText(int minutes)
{
    if (minutes <= 0)
        return {};
    if (minutes < 60)
        return Library::tr("%1 min").arg(minutes);
    const double hours = minutes / 60.0;
    return Library::tr("%1 h").arg(QLocale().toString(hours, 'f', hours < 10 ? 1 : 0));
}

QString lastPlayedText(const QDateTime &time)
{
    if (!time.isValid())
        return {};
    // In this machine's time zone, whatever offset the time was saved with.
    const QDate date = time.toLocalTime().date();
    const qint64 days = date.daysTo(QDate::currentDate());
    if (days <= 0)
        return Library::tr("Today");
    if (days == 1)
        return Library::tr("Yesterday");
    if (days < 7)
        return Library::tr("%n day(s) ago", nullptr, int(days));
    return DateFormat::shortDate(date);
}

QString shellQuoted(const QString &arg)
{
    static const QString kSafe = QStringLiteral("-_./=:+,@%~");
    if (std::all_of(arg.cbegin(), arg.cend(), [](QChar c) { return c.isLetterOrNumber() || kSafe.contains(c); }))
        return arg;
    return u'\'' + QString(arg).replace(u'\'', QLatin1String("'\\''")) + u'\'';
}

QString statusOf(const Game &game)
{
    return game.status.isEmpty() ? QStringLiteral("none") : game.status;
}

QString statusName(const QString &status)
{
    if (status == QLatin1String("playing"))
        return Library::tr("Playing");
    if (status == QLatin1String("not_started"))
        return Library::tr("Backlog");
    if (status == QLatin1String("finished"))
        return Library::tr("Finished");
    if (status == QLatin1String("completed"))
        return Library::tr("Completed");
    if (status == QLatin1String("dropped"))
        return Library::tr("Dropped");
    return Library::tr("No status");
}

QString localPath(const QString &file)
{
    return file.startsWith(QLatin1String("file:")) ? QUrl(file).toLocalFile() : file;
}

// Values from QML for LibraryStore::setGameValues(): JS null and undefined clear a key.
QList<std::pair<QString, QVariant>> gameValues(const QVariantMap &values)
{
    QList<std::pair<QString, QVariant>> out;
    for (auto it = values.cbegin(); it != values.cend(); ++it) {
        const bool none = !it->isValid() || it->typeId() == QMetaType::Nullptr;
        out.append({it.key(), none ? QVariant() : *it});
    }
    return out;
}
}

QList<Library::PlatformProblem> Library::platformProblems(const Platform &known, const Platform &written,
                                                          const QList<EmulatorDef> &emulators) const
{
    const LibrarySnapshot &snapshot = m_store.snapshot();
    const InstallLocations &where = m_store.installLocations();
    QList<PlatformProblem> out;

    const QStringList missing = snapshot.romMissingFolders.value(written.id);
    for (const QString &folder : missing) {
        out.append({tr("ROM folder %1 not found; its games show as \"ROM not found\"").arg(folder), {}, false});
    }
    if (!snapshot.installsDetected || known.emulators.isEmpty() || written.romFolders.isEmpty())
        return out;

    const QString chosen = Emulators::choose({}, known.emulators, snapshot.installs);
    if (chosen.isEmpty()) {
        QStringList names;
        for (const QString &id : known.emulators)
            names.append(Emulators::find(emulators, id)->name);
        out.append({known.emulators.size() == 1 ? tr("%1 isn't installed, so these games can't launch").arg(names.first())
                                                : tr("None of %1 is installed, so these games can't launch")
                                                      .arg(names.join(QStringLiteral(", "))),
                    {}, true});
        return out;
    }

    const EmulatorInstall install = snapshot.installs.value(chosen);
    if (install.kind != EmulatorInstall::Kind::Flatpak)
        return out;
    const QString name = Emulators::find(emulators, chosen)->name;
    for (const QString &folder : written.romFolders) {
        if (missing.contains(folder))
            continue;
        const QString path = resolveRomFolder(folder, m_store.dataDir(), where.home);
        if (Emulators::flatpakCanRead(install.filesystems, path, where))
            continue;
        const QString shown = folder.startsWith(QLatin1String("~/")) ? folder : path;
        out.append({tr("%1 is a Flatpak and can't read %2; to let it, run:").arg(name, shown),
                    QStringLiteral("flatpak override --user --filesystem=%1 %2").arg(shellQuoted(shown), install.program),
                    true});
    }
    return out;
}

Library::Library(const QString &dataDir, const QString &cacheDir, NewLibrary newLibrary, QObject *parent)
    : QAbstractListModel(parent)
    , m_store(dataDir, cacheDir, newLibrary)
    , m_cacheDir(cacheDir)
    , m_heroFiles(HeroCache::list(cacheDir))
{
    s_instance = this;
    connect(&m_heroCopy, &QFutureWatcher<HeroCache::Result>::finished, this, &Library::heroCopyFinished);
    connect(&m_store, &LibraryStore::changed, this, &Library::reloadFromStore);
    connect(&m_store, &LibraryStore::artShrunk, this, &Library::makeNextHeroCopy);
    // changed() comes while the store still counts as loading or scanning, which holds a shrink back.
    connect(&m_store, &LibraryStore::loadingChanged, this, &Library::makeNextHeroCopy);
    connect(&m_store, &LibraryStore::scanningChanged, this, &Library::makeNextHeroCopy);
    connect(&m_store, &LibraryStore::historyChanged, this, &Library::historyChanged);
    connect(&m_downloads, &SteamDownloads::changed, this, [this] {
        if (!applyDownloads())
            return;
        updateDownloadBar();
        if (!m_rows.isEmpty())
            emit dataChanged(index(0), index(int(m_rows.size()) - 1));
    });
    connect(&m_store, &LibraryStore::loadingChanged, this, &Library::loadingChanged);
    connect(&m_store, &LibraryStore::scanningChanged, this, &Library::scanningChanged);
    connect(&m_store, &LibraryStore::setupPendingChanged, this, &Library::setupPendingChanged);
    m_settingsTimer.setSingleShot(true);
    m_settingsTimer.setInterval(100);
    connect(&m_settingsWatcher, &QFileSystemWatcher::fileChanged, &m_settingsTimer, qOverload<>(&QTimer::start));
    connect(&m_settingsWatcher, &QFileSystemWatcher::directoryChanged, &m_settingsTimer, qOverload<>(&QTimer::start));
    connect(&m_settingsTimer, &QTimer::timeout, this, &Library::loadListSettings);
    m_groupChangeTimer.setSingleShot(true);
    m_groupChangeTimer.setInterval(500);
    connect(&m_groupChangeTimer, &QTimer::timeout, this, [this] {
        m_groupChange.clear();
        emit groupChangeChanged();
    });

    // Before the first rows, so the first frame is already sorted and filtered.
    QFile file(settingsFile());
    const Parsed<ListSettings> list =
        parseListSettings(file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray(), QStringLiteral("settings.toml"));
    m_listProblems = list.warnings;
    for (const QString &warning : std::as_const(m_listProblems))
        qCWarning(lcLibrary).noquote() << warning;
    m_savedList = list.value;
    m_sort = list.value.sort;
    m_sortDescending = list.value.descending;
    m_installedOnly = list.value.installedOnly;
    m_favoritesOnly = list.value.favoritesOnly;
    m_tagFilter = list.value.tags;
    m_tagMatchAll = list.value.matchAllTags;
    m_statusFilter = list.value.statuses;
    m_showHidden = list.value.showHidden;
    m_showSortValue = list.value.showSortValue;
    m_groupBy = list.value.groupBy;
    reloadFromStore();
}

void Library::startRefresh()
{
    m_store.refresh();
    m_heroCopiesStarted = true;
    makeNextHeroCopy();
    loadListSettings();
    m_undo.clearLeftovers();
}

ListSettings Library::listSettings() const
{
    ListSettings s;
    s.sort = m_sort;
    s.descending = m_sortDescending;
    s.installedOnly = m_installedOnly;
    s.favoritesOnly = m_favoritesOnly;
    s.tags = m_tagFilter;
    s.matchAllTags = m_tagMatchAll;
    s.statuses = m_statusFilter;
    s.showHidden = m_showHidden;
    s.showSortValue = m_showSortValue;
    s.groupBy = m_groupBy;
    return s;
}

void Library::loadListSettings()
{
    // The folder catches the file being replaced (editors, TomlPatch's atomic writes).
    const QStringList watched = m_settingsWatcher.files() + m_settingsWatcher.directories();
    for (const QString &path : {settingsFile(), m_store.dataDir()}) {
        if (!watched.contains(path) && QFileInfo::exists(path))
            m_settingsWatcher.addPath(path);
    }

    QFile file(settingsFile());
    const QByteArray bytes = file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
    if (const Parsed<SteamSettings> steam = parseSteamSettings(bytes, QStringLiteral("settings.toml")); steam.ok) {
        m_steamSettings = steam.value;
        updateSteam();
    }
    const Parsed<ListSettings> parsed = parseListSettings(bytes, QStringLiteral("settings.toml"));
    if (parsed.warnings != m_listProblems) {
        for (const QString &warning : parsed.warnings) {
            if (!m_listProblems.contains(warning))
                qCWarning(lcLibrary).noquote() << warning;
        }
        for (const QString &old : std::as_const(m_listProblems))
            m_problems.removeOne(old);
        m_listProblems = parsed.warnings;
        m_problems += m_listProblems;
        emit problemsChanged();
    }
    if (!parsed.ok)
        return;
    m_savedList = parsed.value;
    applyListSettings(parsed.value); // the app's own writes change nothing here
}

void Library::applyListSettings(const ListSettings &s)
{
    const ListSettings now = listSettings();
    if (s == now)
        return;
    const bool sortDiffers = s.sort != now.sort || s.descending != now.descending;
    const bool hiddenDiffers = s.showHidden != now.showHidden;
    const bool groupingDiffers = s.groupBy != now.groupBy;
    m_groupBy = s.groupBy;
    m_sort = s.sort;
    m_sortDescending = s.descending;
    m_installedOnly = s.installedOnly;
    m_favoritesOnly = s.favoritesOnly;
    m_tagFilter = s.tags;
    m_tagMatchAll = s.matchAllTags;
    m_statusFilter = s.statuses;
    m_showHidden = s.showHidden;
    if (s.showSortValue != m_showSortValue) {
        m_showSortValue = s.showSortValue;
        emit showSortValueChanged();
    }
    if (sortDiffers) {
        updateOrder();
        emit sortChanged();
    }
    if (hiddenDiffers && updatePlatforms())
        emit platformsChanged();
    if (groupingDiffers)
        emit groupByChanged();
    emit filtersChanged();
    rebuild();
}

void Library::saveListSettings()
{
    const ListSettings now = listSettings();
    const QList<std::pair<QString, QString>> changes = listSettingsChanges(m_savedList, now);
    if (changes.isEmpty())
        return;
    const QString path = settingsFile();
    if (!ensureSettingsFile()) {
        qCWarning(lcLibrary).noquote() << QStringLiteral("could not create %1 to save the list's sort and filters").arg(path);
        return;
    }
    for (const auto &[key, value] : changes) {
        QString error;
        const bool ok = value.isEmpty()
            ? TomlPatch::removeValueInFile(path, QStringLiteral("list"), key, &error)
            : TomlPatch::setValueInFile(path, QStringLiteral("list"), key, value, &error);
        if (!ok && !error.isEmpty())
            qCWarning(lcLibrary).noquote() << QStringLiteral("could not save %1 in settings.toml: %2").arg(key, error);
    }
    m_savedList = now;
}

bool Library::ensureSettingsFile() const
{
    if (QFileInfo::exists(settingsFile()))
        return true;
    QFile created(settingsFile());
    return created.open(QIODevice::WriteOnly);
}

QVariantMap Library::buildSteam() const
{
    const SteamState &state = m_store.snapshot().steam;
    const SteamInstall &install = state.install;
    const QString home = QDir::homePath();
    const auto tilde = [&](const QString &path) {
        return path == home || path.startsWith(home + u'/') ? u'~' + path.sliced(home.size()) : path;
    };

    QVariantList users;
    for (const SteamUser &u : install.users) {
        users.append(QVariantMap{{QStringLiteral("id"), u.accountId},
                                 {QStringLiteral("name"), u.personaName.isEmpty() ? u.accountName : u.personaName},
                                 {QStringLiteral("accountName"), u.accountName},
                                 {QStringLiteral("mostRecent"), u.mostRecent}});
    }
    QList<const Game *> steamGames;
    for (const Game &g : m_games) {
        if (g.platform == kSteamPlatform)
            steamGames.append(&g);
    }
    QVariantList presets;
    for (const SteamPreset &p : m_steamSettings.presets) {
        const auto on = std::count_if(steamGames.cbegin(), steamGames.cend(),
                                      [&](const Game *g) { return LaunchOptions::isOn(g->launchOptions, p); });
        const QString program = p.wrapper.value(0);
        presets.append(QVariantMap{{QStringLiteral("id"), p.id}, {QStringLiteral("name"), p.name},
                                   {QStringLiteral("note"), p.note}, {QStringLiteral("adds"), p.describe()},
                                   {QStringLiteral("builtIn"), p.builtIn}, {QStringLiteral("games"), int(on)},
                                   {QStringLiteral("program"), program},
                                   {QStringLiteral("programFound"),
                                    !program.isEmpty() && !QStandardPaths::findExecutable(program).isEmpty()}});
    }

    QHash<QString, QString> toolNames;
    for (const CompatTool &t : state.tools())
        toolNames.insert(t.name, t.displayName);
    const auto toolName = [&](const QString &name) {
        return name.isEmpty() ? tr("Steam's default") : toolNames.value(name, name);
    };
    QVariantList queue;
    const QHash<QString, QStringList> &queued = m_store.snapshot().steamQueued;
    const auto change = [](const QString &name, const QString &from, const QString &to) {
        return QVariantMap{{QStringLiteral("name"), name}, {QStringLiteral("from"), from}, {QStringLiteral("to"), to}};
    };
    for (const Game &g : m_games) {
        const QStringList keys = queued.value(g.id);
        if (g.platform != kProtonPlatform || keys.isEmpty())
            continue;
        const auto s = std::find_if(state.shortcuts.cbegin(), state.shortcuts.cend(),
                                    [&](const SteamShortcut &c) { return c.appId == g.shortcut; });
        const SteamShortcut now = s != state.shortcuts.cend() ? *s : SteamShortcut();
        const GameFile file = m_store.snapshot().content.games.value(g.id);
        const auto protonName = [&](const QString &name) {
            return name.isEmpty() ? tr("None (runs natively)") : toolNames.value(name, name);
        };
        QVariantList changes;
        if (keys.contains(SteamSync::kAddShortcut))
            changes.append(change(tr("Shortcut"), tr("Not in Steam"), tr("Added")));
        if (keys.contains(SteamSync::kRemoveShortcut))
            changes.append(change(tr("Shortcut"), tr("In Steam"), tr("Removed")));
        if (keys.contains(QStringLiteral("title")))
            changes.append(change(tr("Title"), now.name, g.title));
        if (keys.contains(QStringLiteral("exe")))
            changes.append(change(tr("Program"), tilde(now.exe), tilde(g.exe)));
        if (keys.contains(QStringLiteral("start_dir")))
            changes.append(change(tr("Start in"), tilde(now.startDir),
                                  file.startDir.isEmpty() ? tilde(QFileInfo(g.exe).absolutePath()) : file.startDir));
        if (keys.contains(QStringLiteral("launch_options")))
            changes.append(change(tr("Launch options"), now.launchOptions, g.launchOptions));
        if (keys.contains(QStringLiteral("proton")))
            changes.append(change(tr("Proton"), protonName(state.compatTools.value(g.shortcut)), protonName(g.proton)));
        if (keys.contains(SteamSync::kShortcutIcon))
            changes.append(now.icon.isEmpty() ? change(tr("Icon"), tr("None"), tr("The game's icon"))
                                              : change(tr("Icon"), tr("The game's icon"), tr("None")));
        queue.append(QVariantMap{{QStringLiteral("id"), g.id}, {QStringLiteral("title"), g.title},
                                 {QStringLiteral("changes"), changes},
                                 {QStringLiteral("adds"), keys.contains(SteamSync::kAddShortcut)}});
    }
    for (auto it = queued.cbegin(); it != queued.cend(); ++it) {
        const quint32 appId = SteamSync::removedAppId(it.key());
        if (appId == 0)
            continue;
        QVariantList changes;
        if (std::any_of(state.shortcuts.cbegin(), state.shortcuts.cend(), [&](const SteamShortcut &s) { return s.appId == appId; }))
            changes.append(change(tr("Shortcut"), tr("In Steam"), tr("Removed")));
        if (const QString tool = state.compatTools.value(appId); !tool.isEmpty())
            changes.append(change(tr("Proton"), toolNames.value(tool, tool), tr("Removed")));
        const QString title = m_store.removedShortcutTitle(appId);
        queue.append(QVariantMap{{QStringLiteral("id"), it.key()},
                                 {QStringLiteral("title"), tr("%1 (removed)").arg(title.isEmpty() ? QString::number(appId) : title)},
                                 {QStringLiteral("changes"), changes}});
    }
    for (const Game *g : std::as_const(steamGames)) {
        const QStringList keys = queued.value(g->id);
        if (keys.isEmpty())
            continue;
        const qint64 appId = GameId::steamAppId(g->id);
        QVariantList changes;
        if (keys.contains(QStringLiteral("launch_options"))) {
            changes.append(QVariantMap{{QStringLiteral("name"), tr("Launch options")},
                                       {QStringLiteral("from"), state.apps.value(appId).launchOptions},
                                       {QStringLiteral("to"), g->launchOptions}});
        }
        if (keys.contains(QStringLiteral("proton"))) {
            changes.append(QVariantMap{{QStringLiteral("name"), tr("Proton")},
                                       {QStringLiteral("from"), toolName(state.compatTools.value(appId))},
                                       {QStringLiteral("to"), toolName(g->proton)}});
        }
        queue.append(QVariantMap{{QStringLiteral("id"), g->id}, {QStringLiteral("title"), g->title},
                                 {QStringLiteral("changes"), changes}});
    }

    const QList<SteamApp> games = state.games();
    const int installed = int(std::count_if(games.cbegin(), games.cend(), [](const SteamApp &a) { return a.installed; }));
    const int updating = int(std::count_if(games.cbegin(), games.cend(), [](const SteamApp &a) {
        return a.installed && a.update != SteamUpdate::None;
    }));

    QSet<quint32> shortcutIds;
    for (const SteamShortcut &s : state.shortcuts)
        shortcutIds.insert(s.appId);
    int shortcutsInLibrary = 0;
    for (const GameFile &f : m_store.snapshot().content.games) {
        if (f.platform == kProtonPlatform && shortcutIds.contains(quint32(f.shortcut)))
            ++shortcutsInLibrary;
    }
    QVariantList tools;
    for (const CompatTool &t : state.tools())
        tools.append(QVariantMap{{QStringLiteral("name"), t.name}, {QStringLiteral("displayName"), t.displayName}});

    // The import reads settings.toml on its own; until it has, say nothing it hasn't checked.
    QString status;
    if (!m_steamSettings.enabled)
        status = tr("Off: Steam games aren't imported, and those in the library show as not installed.");
    else if (!state.enabled)
        status = tr("Looking for Steam…");
    else if (install.pathMissing)
        status = tr("There's no Steam in %1: the folder needs steamapps/libraryfolders.vdf.").arg(m_steamSettings.path);
    else if (!install.found())
        status = tr("Steam wasn't found in ~/.steam/steam or the Flatpak's folder. If it's elsewhere, choose its folder.");
    else {
        status = install.kind == SteamInstall::Kind::Flatpak
            ? tr("Steam (Flatpak) found at %1, %n game(s) installed.", "", installed).arg(tilde(install.root))
            : tr("Steam found at %1, %n game(s) installed.", "", installed).arg(tilde(install.root));
        if (updating > 0) {
            status.chop(1);
            status += tr(", %n with an update.", "", updating);
        }
        if (install.userMissing)
            status += u' ' + tr("Account %1 hasn't logged in to this Steam; using the most recent login.").arg(m_steamSettings.user);
        else if (install.userId.isEmpty())
            status += u' ' + tr("No account has logged in to it yet.");
    }

    return {
        {QStringLiteral("enabled"), m_steamSettings.enabled},
        {QStringLiteral("path"), m_steamSettings.path},
        {QStringLiteral("user"), m_steamSettings.user},
        {QStringLiteral("found"), m_steamSettings.enabled && install.found()},
        {QStringLiteral("flatpak"), install.kind == SteamInstall::Kind::Flatpak},
        {QStringLiteral("root"), install.root},
        {QStringLiteral("shownRoot"), tilde(install.root)},
        {QStringLiteral("users"), users},
        {QStringLiteral("userId"), install.userId},
        {QStringLiteral("installed"), installed},
        {QStringLiteral("defaultPresets"), m_steamSettings.defaultPresets},
        {QStringLiteral("defaultProton"), m_steamSettings.defaultProton},
        {QStringLiteral("steamDefaultProton"),
         toolNames.value(state.compatTools.value(0), state.compatTools.value(0, QStringLiteral("proton_experimental")))},
        {QStringLiteral("tools"), tools},
        {QStringLiteral("shortcuts"), int(state.shortcuts.size())},
        {QStringLiteral("shortcutsInLibrary"), shortcutsInLibrary},
        {QStringLiteral("shortcutsOk"), state.shortcutsOk},
        {QStringLiteral("presets"), presets},
        {QStringLiteral("games"), int(steamGames.size())},
        {QStringLiteral("queue"), queue},
        {QStringLiteral("status"), status},
        {QStringLiteral("problem"), m_steamSettings.enabled && state.enabled
                                        && (!install.found() || install.userMissing || install.userId.isEmpty())},
    };
}

QVariantMap Library::steam() const
{
    if (m_steam.isEmpty())
        m_steam = buildSteam();
    return m_steam;
}

QVariantList Library::buildSteamWaiting() const
{
    struct Entry
    {
        QString id;
        QString title;
        bool adds;
    };
    QList<Entry> entries;
    const QHash<QString, QStringList> &queued = m_store.snapshot().steamQueued;
    for (auto it = queued.cbegin(); it != queued.cend(); ++it) {
        if (it.value().isEmpty() || it.value() == QStringList{SteamSync::kShortcutIcon})
            continue;
        if (const quint32 appId = SteamSync::removedAppId(it.key()); appId != 0) {
            const QString title = m_store.removedShortcutTitle(appId);
            entries.append({it.key(), tr("%1 (removed)").arg(title.isEmpty() ? QString::number(appId) : title), false});
            continue;
        }
        const auto g = std::find_if(m_games.cbegin(), m_games.cend(), [&](const Game &g) { return g.id == it.key(); });
        if (g != m_games.cend())
            entries.append({g->id, g->title, it.value().contains(SteamSync::kAddShortcut)});
    }
    std::sort(entries.begin(), entries.end(),
              [](const Entry &a, const Entry &b) { return a.title.compare(b.title, Qt::CaseInsensitive) < 0; });
    QVariantList out;
    for (const Entry &e : std::as_const(entries))
        out.append(QVariantMap{{QStringLiteral("id"), e.id}, {QStringLiteral("title"), e.title},
                               {QStringLiteral("adds"), e.adds}});
    return out;
}

QString Library::undoSteamEdits(const QString &id)
{
    return m_store.undoSteamQueue(id);
}

QString Library::undoAllSteamEdits()
{
    const QStringList keys = m_store.snapshot().steamQueued.keys();
    QString error;
    m_store.beginBatch();
    for (const QString &key : keys) {
        if (const QString e = m_store.undoSteamQueue(key); error.isEmpty())
            error = e;
    }
    m_store.endBatch();
    return error;
}

// A manifest says only that an update started: Steam rewrites it when the update starts or
// stops, not while it runs. Whether it runs now and how far it got come from Steam's log and
// the download folder.
void Library::updateDownloads()
{
    QList<SteamDownloads::App> apps;
    for (const Game &g : std::as_const(m_games)) {
        const qint64 appId = GameId::steamAppId(g.id);
        if (g.platform != kSteamPlatform || appId <= 0 || g.installDir.isEmpty()
            || (g.steamUpdate != SteamUpdate::Downloading && g.steamUpdate != SteamUpdate::Paused))
            continue;
        // installDir is <library>/steamapps/common/<name>.
        apps.append({appId, QDir::cleanPath(g.installDir + QStringLiteral("/../../downloading/%1").arg(appId))});
    }
    const QString root = m_store.snapshot().steam.install.root;
    m_downloads.setApps(apps.isEmpty() ? QString() : root + QStringLiteral("/logs/content_log.txt"), apps);
    applyDownloads();
    updateDownloadBar();
}

bool Library::applyDownloads()
{
    const QHash<qint64, SteamDownloads::Progress> progress = m_downloads.progress();
    bool changed = false;
    for (Game &g : m_games) {
        const auto it = progress.constFind(GameId::steamAppId(g.id));
        if (g.platform != kSteamPlatform || it == progress.cend()
            || (g.steamUpdate != SteamUpdate::Downloading && g.steamUpdate != SteamUpdate::Paused))
            continue;
        const SteamUpdate update = it->running ? SteamUpdate::Downloading : SteamUpdate::Paused;
        const int percent = it->percent >= 0 ? it->percent : g.updatePercent;
        changed = changed || update != g.steamUpdate || percent != g.updatePercent;
        g.steamUpdate = update;
        g.updatePercent = percent;
    }
    return changed;
}

void Library::updateDownloadBar()
{
    const Game *shown = nullptr;
    for (const Game &g : std::as_const(m_games)) {
        if (g.steamUpdate == SteamUpdate::Downloading) {
            shown = &g;
            break;
        }
        if (g.steamUpdate == SteamUpdate::Paused && !shown)
            shown = &g;
    }
    QVariantMap download;
    if (shown) {
        download = {{QStringLiteral("gameId"), shown->id},
                    {QStringLiteral("name"), shown->title},
                    {QStringLiteral("icon"), shown->iconUrl},
                    {QStringLiteral("state"), updateStateName(shown->steamUpdate)},
                    {QStringLiteral("percent"), updatePercent(*shown)},
                    {QStringLiteral("installing"), !shown->installed}};
    }
    if (download == m_download)
        return;
    m_download = std::move(download);
    emit downloadChanged();
}

void Library::updateSteam()
{
    if (QVariantList waiting = buildSteamWaiting(); waiting != m_steamWaiting) {
        m_steamWaiting = std::move(waiting);
        emit steamWaitingChanged();
    }
    if (m_steam.isEmpty())
        return;
    QVariantMap steam = buildSteam();
    if (steam == m_steam)
        return;
    m_steam = std::move(steam);
    emit steamChanged();
}

QString Library::setSteamValue(const QString &key, const QVariant &value)
{
    QString formatted;
    if (key == u"enabled") {
        if (!value.toBool())
            formatted = QStringLiteral("false");
    } else if (key == u"path") {
        if (const QString path = value.toString().trimmed(); !path.isEmpty())
            formatted = tomlString(path);
    } else if (key == u"user") {
        const QString user = value.toString().trimmed();
        bool number = false;
        const qint64 id = user.toLongLong(&number);
        if (!user.isEmpty())
            formatted = number ? QString::number(id) : tomlString(user);
    } else if (key == u"default_proton") {
        if (const QString name = value.toString().trimmed(); !name.isEmpty())
            formatted = tomlString(name);
    } else if (key == u"default_presets") {
        QStringList ids;
        for (const QString &id : value.toStringList())
            ids.append(tomlString(id));
        if (!ids.isEmpty())
            formatted = u'[' + ids.join(QStringLiteral(", ")) + u']';
    } else {
        return tr("[steam] has no setting %1").arg(key);
    }

    if (!ensureSettingsFile())
        return tr("Couldn't create %1").arg(settingsFile());
    QString error;
    const bool ok = formatted.isEmpty()
        ? TomlPatch::removeValueInFile(settingsFile(), QStringLiteral("steam"), key, &error)
        : TomlPatch::setValueInFile(settingsFile(), QStringLiteral("steam"), key, formatted, &error);
    if (!ok)
        return tr("Couldn't save %1 in settings.toml: %2").arg(key, error);
    // Shows the change now, and runs the import with it, not when the watchers catch it.
    loadListSettings();
    m_store.refresh();
    return {};
}

QVariantList Library::steamShortcuts() const
{
    const LibrarySnapshot &snapshot = m_store.snapshot();
    const SteamState &state = snapshot.steam;
    QSet<quint32> inLibrary;
    for (const GameFile &f : snapshot.content.games) {
        if (f.platform == kProtonPlatform && f.shortcut > 0)
            inLibrary.insert(quint32(f.shortcut));
    }
    QHash<QString, QString> toolNames;
    for (const CompatTool &t : state.tools())
        toolNames.insert(t.name, t.displayName);
    const QString home = QDir::homePath();
    const auto tilde = [&](const QString &path) {
        return path == home || path.startsWith(home + u'/') ? u'~' + path.sliced(home.size()) : path;
    };

    QVariantList out;
    for (const SteamShortcut &s : state.shortcuts) {
        if (inLibrary.contains(s.appId))
            continue;
        const QString tool = state.compatTools.value(s.appId);
        QString art = !s.icon.isEmpty() && QFileInfo(s.icon).isFile() ? s.icon : QString();
        if (art.isEmpty()) {
            const QHash<QString, QString> found = SteamArt::find(
                state.install, s.appId, {QStringLiteral("icon"), QStringLiteral("cover"), QStringLiteral("logo"),
                                         QStringLiteral("hero")});
            for (const QString &kind : {QStringLiteral("icon"), QStringLiteral("cover"), QStringLiteral("logo"),
                                         QStringLiteral("hero")}) {
                if (found.contains(kind)) {
                    art = found.value(kind);
                    break;
                }
            }
        }
        out.append(QVariantMap{
            {QStringLiteral("appId"), qint64(s.appId)},
            {QStringLiteral("name"), s.name},
            {QStringLiteral("target"), s.exeArgs.isEmpty() ? tilde(s.exe) : tilde(s.exe) + u' ' + s.exeArgs},
            {QStringLiteral("tool"), tool.isEmpty() ? QString() : toolNames.value(tool, tool)},
            {QStringLiteral("checked"), !tool.isEmpty()},
            {QStringLiteral("art"), art.isEmpty() ? QString() : QUrl::fromLocalFile(art).toString()},
        });
    }
    return out;
}

QVariantList Library::steamCovers(int limit) const
{
    const SteamState &state = m_store.snapshot().steam;
    QList<SteamApp> games = state.games();
    games.removeIf([](const SteamApp &a) { return !a.installed; });
    std::sort(games.begin(), games.end(), [&](const SteamApp &a, const SteamApp &b) {
        const qint64 pa = state.apps.value(a.appId).lastPlayed;
        const qint64 pb = state.apps.value(b.appId).lastPlayed;
        return pa != pb ? pa > pb : a.name.localeAwareCompare(b.name) < 0;
    });
    QVariantList out;
    // Only as many lookups as covers wanted: each one lists a folder of Steam's cache.
    for (const SteamApp &a : std::as_const(games)) {
        if (out.size() >= limit)
            break;
        const QString cover = SteamArt::find(state.install, a.appId, {QStringLiteral("cover")}).value(QStringLiteral("cover"));
        if (!cover.isEmpty())
            out.append(QVariantMap{{QStringLiteral("name"), a.name}, {QStringLiteral("cover"), QUrl::fromLocalFile(cover).toString()}});
    }
    return out;
}

QString Library::addSteamShortcuts(const QVariantList &appIds)
{
    QList<quint32> ids;
    for (const QVariant &v : appIds)
        ids.append(quint32(v.toLongLong()));
    return m_store.addShortcuts(ids).error;
}

QString Library::setSteamPresetForAll(const QString &preset, bool on)
{
    const SteamPreset *p = m_steamSettings.preset(preset);
    if (!p)
        return tr("There's no preset %1").arg(preset);
    // Copied first: writing a game file changes m_games once the batch ends.
    QList<std::pair<QString, QString>> changes;
    for (const Game &g : std::as_const(m_games)) {
        if (g.platform != kSteamPlatform)
            continue;
        const QString options = on ? LaunchOptions::turnOn(g.launchOptions, *p) : LaunchOptions::turnOff(g.launchOptions, *p);
        if (options != g.launchOptions)
            changes.append({g.id, options});
    }
    QStringList errors;
    m_store.beginBatch();
    for (const auto &[id, options] : std::as_const(changes)) {
        const QString error = m_store.setGameValues(id, {{QStringLiteral("launch_options"), options}});
        if (!error.isEmpty())
            errors.append(error);
    }
    m_store.endBatch();
    qCInfo(lcLibrary).noquote() << QStringLiteral("%1 %2 in %3 Steam game(s)")
                                       .arg(on ? QStringLiteral("turned on") : QStringLiteral("turned off"), preset)
                                       .arg(changes.size() - errors.size());
    return errors.isEmpty() ? QString() : errors.first();
}

static void sortTagsByName(QVariantList &tags)
{
    std::stable_sort(tags.begin(), tags.end(), [](const QVariant &a, const QVariant &b) {
        return a.toMap().value(QStringLiteral("name")).toString().localeAwareCompare(
                   b.toMap().value(QStringLiteral("name")).toString()) < 0;
    });
}

QVariantList Library::platforms() const
{
    QVariantList out;
    for (const Platform &p : m_platforms) {
        if (!p.inRail)
            continue;
        out.append(QVariantMap{
            {QStringLiteral("id"), p.id},
            {QStringLiteral("name"), p.name},
            {QStringLiteral("icon"), p.icon},
        });
    }
    return out;
}

QVariantList Library::gameCounts() const
{
    QHash<QString, int> counts;
    for (const Game &g : m_games) {
        if (!g.hidden)
            ++counts[g.platform];
    }
    QVariantList out;
    for (const Platform &p : m_platforms) {
        if (const int count = counts.value(p.id)) {
            out.append(QVariantMap{
                {QStringLiteral("id"), p.id},
                {QStringLiteral("name"), p.name},
                {QStringLiteral("count"), count},
            });
        }
    }
    return out;
}

void Library::reloadFromStore()
{
    const LibrarySnapshot &snapshot = m_store.snapshot();
    BuiltLibrary built = buildLibrary(snapshot.content, snapshot.art, snapshot.romFiles, Emulators::presets(),
                                      snapshot.romGameFiles, snapshot.steam, snapshot.steamQueued,
                                      snapshot.programFiles);

    QStringList problems = m_store.versionStatus().problems + snapshot.warnings() + built.warnings + m_listProblems;
    m_platformProblems.clear();
    for (const Platform &written : snapshot.content.platforms) {
        const auto known = std::find_if(built.platforms.cbegin(), built.platforms.cend(),
                                        [&](const Platform &p) { return p.id == written.id; });
        if (known == built.platforms.cend())
            continue;
        const QList<PlatformProblem> found = platformProblems(*known, written, built.emulators);
        for (const PlatformProblem &problem : found) {
            if (problem.addToProblems) {
                problems.append(QStringLiteral("library/platforms.toml:%1: %2: %3%4")
                                    .arg(written.line)
                                    .arg(written.name, problem.text,
                                         problem.command.isEmpty() ? QString() : u' ' + problem.command));
            }
        }
        if (!found.isEmpty())
            m_platformProblems.insert(written.id, found);
    }
    problems.removeDuplicates(); // library.toml is read by both the version check and the scan
    if (problems != m_problems) {
        for (const QString &problem : problems) {
            if (!m_problems.contains(problem))
                qCWarning(lcLibrary).noquote() << problem;
        }
        m_problems = problems;
        emit problemsChanged();
    }

    QStringList oldKeys;
    for (const Row &row : std::as_const(m_rows))
        oldKeys.append(rowKey(row));

    m_allPlatforms = built.platforms;
    m_emulators = std::move(built.emulators);
    const QCollator collator = titleCollator();
    m_games.clear();
    m_games.reserve(built.games.size() + built.hidden.size());
    std::merge(built.games.cbegin(), built.games.cend(), built.hidden.cbegin(), built.hidden.cend(),
               std::back_inserter(m_games), [&](const Game &a, const Game &b) { return titleBefore(collator, a, b); });
    m_visibleCount = int(built.games.size());
    m_foldedTitles.clear();
    m_foldedTitles.reserve(m_games.size());
    for (const Game &g : std::as_const(m_games))
        m_foldedTitles.append(SearchText::fold(g.title));
    updateOrder();
    updateDownloads();
    const bool platformsDiffer = updatePlatforms();

    // The tag filter ignores tags that don't exist, so a renamed or deleted tag changes it.
    const QHash<QString, int> use = m_store.tagUse();
    const QStringList tagsBefore = effectiveTags();
    m_tagNames.clear();
    for (const TagDef &t : snapshot.content.tags)
        m_tagNames.insert(t.name);
    for (auto it = use.cbegin(); it != use.cend(); ++it)
        m_tagNames.insert(it.key());
    if (effectiveTags() != tagsBefore)
        emit filtersChanged();

    bool showingGroups = false;
    int notInstalled = 0;
    QList<Row> rows = computeRows(&showingGroups, &notInstalled);
    QStringList newKeys;
    for (const Row &row : std::as_const(rows))
        newKeys.append(rowKey(row));

    if (newKeys == oldKeys && showingGroups == m_showingGroups) {
        m_rows = std::move(rows);
        if (!m_rows.isEmpty())
            emit dataChanged(index(0), index(int(m_rows.size()) - 1));
    } else if (showingGroups == m_showingGroups && applyRowChanges(rows, oldKeys, newKeys)) {
    } else if (showingGroups == m_showingGroups && applyRowMove(rows, oldKeys, newKeys)) {
    } else {
        beginResetModel();
        m_rows = std::move(rows);
        m_showingGroups = showingGroups;
        endResetModel();
        emit reloaded();
    }
    if (notInstalled != m_notInstalledHidden) {
        m_notInstalledHidden = notInstalled;
        emit notInstalledHiddenChanged();
    }
    if (platformsDiffer)
        emit platformsChanged();
    emit gamesChanged();

    QVariantList emulators = buildEmulatorList();
    if (emulators != m_emulatorList) {
        m_emulatorList = std::move(emulators);
        emit emulatorsChanged();
    }
    QVariantList platformSettings = buildPlatformSettings();
    if (platformSettings != m_platformSettings) {
        m_platformSettings = std::move(platformSettings);
        emit platformSettingsChanged();
    }
    updateSteam();

    QVariantList tags;
    for (const TagDef &t : snapshot.content.tags) {
        tags.append(QVariantMap{{QStringLiteral("name"), t.name}, {QStringLiteral("color"), t.color},
                                {QStringLiteral("games"), use.value(t.name)}, {QStringLiteral("listed"), true}});
    }
    QStringList unlisted;
    for (auto it = use.cbegin(); it != use.cend(); ++it) {
        if (std::none_of(snapshot.content.tags.cbegin(), snapshot.content.tags.cend(),
                         [&](const TagDef &t) { return t.name == it.key(); }))
            unlisted.append(it.key());
    }
    unlisted.sort(Qt::CaseInsensitive);
    for (const QString &name : std::as_const(unlisted)) {
        tags.append(QVariantMap{{QStringLiteral("name"), name}, {QStringLiteral("color"), 0},
                                {QStringLiteral("games"), use.value(name)}, {QStringLiteral("listed"), false}});
    }
    sortTagsByName(tags);
    QStringList missingDefaults;
    for (const TagDef &t : m_store.missingDefaultTags())
        missingDefaults.append(t.name);
    if (tags != m_tags || missingDefaults != m_missingDefaultTags) {
        m_tags = std::move(tags);
        m_missingDefaultTags = std::move(missingDefaults);
        emit tagsChanged();
    }

    QVariantList hidden;
    for (const Game &g : std::as_const(built.hidden)) {
        hidden.append(QVariantMap{{QStringLiteral("id"), g.id}, {QStringLiteral("title"), g.title},
                                  {QStringLiteral("platformName"), g.platformName}});
    }
    if (hidden != m_hiddenGames) {
        m_hiddenGames = std::move(hidden);
        emit hiddenGamesChanged();
    }
    makeNextHeroCopy();
}

bool Library::applyRowChanges(const QList<Row> &rows, const QStringList &oldKeys, const QStringList &newKeys)
{
    constexpr int kMostAnimated = 6;
    if (m_rows.isEmpty() || rows.isEmpty())
        return false;
    // A header's key carries its count, which changes when a game joins or leaves its group.
    const auto identity = [](const QString &key) {
        return key.startsWith(u'#') ? key.left(key.lastIndexOf(u':')) : key;
    };
    QStringList oldIds, newIds;
    for (const QString &key : oldKeys)
        oldIds.append(identity(key));
    for (const QString &key : newKeys)
        newIds.append(identity(key));
    const QSet<QString> oldSet(oldIds.cbegin(), oldIds.cend());
    const QSet<QString> newSet(newIds.cbegin(), newIds.cend());
    QStringList keptOld, keptNew;
    int games = 0;
    for (const QString &id : std::as_const(oldIds)) {
        if (newSet.contains(id))
            keptOld.append(id);
        else if (!id.startsWith(u'#'))
            ++games;
    }
    for (const QString &id : std::as_const(newIds)) {
        if (oldSet.contains(id))
            keptNew.append(id);
        else if (!id.startsWith(u'#'))
            ++games;
    }
    // Games moving (a rename, a new platform) or many at once (a first scan) reset instead.
    if (keptOld != keptNew || games == 0 || games > kMostAnimated)
        return false;

    // The rows that stay take their new values first: m_games is already the new list.
    QHash<QString, Row> newByIdentity;
    for (qsizetype i = 0; i < newIds.size(); ++i)
        newByIdentity.insert(newIds.at(i), rows.at(i));
    for (qsizetype i = 0; i < oldIds.size(); ++i) {
        if (newSet.contains(oldIds.at(i)))
            m_rows[i] = newByIdentity.value(oldIds.at(i));
    }
    for (qsizetype i = oldIds.size() - 1; i >= 0; --i) {
        if (newSet.contains(oldIds.at(i)))
            continue;
        beginRemoveRows({}, int(i), int(i));
        m_rows.removeAt(i);
        endRemoveRows();
    }
    for (qsizetype i = 0; i < newIds.size(); ++i) {
        if (oldSet.contains(newIds.at(i)))
            continue;
        beginInsertRows({}, int(i), int(i));
        m_rows.insert(i, rows.at(i));
        endInsertRows();
    }
    emit dataChanged(index(0), index(int(m_rows.size()) - 1));
    return true;
}

QString Library::rowKey(const Row &row) const
{
    if (row.game < 0)
        return QStringLiteral("#%1:%2").arg(row.group).arg(row.count);
    return m_games.at(row.game).id;
}

Library *Library::create(QQmlEngine *, QJSEngine *)
{
    Q_ASSERT(s_instance);
    QJSEngine::setObjectOwnership(s_instance, QJSEngine::CppOwnership);
    return s_instance;
}

int Library::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : int(m_rows.size());
}

QVariant Library::data(const QModelIndex &index, int role) const
{
    if (!checkIndex(index, CheckIndexOption::IndexIsValid))
        return {};
    const Row &row = m_rows.at(index.row());
    const Game *game = row.game >= 0 ? &m_games.at(row.game) : nullptr;

    switch (role) {
    case IsHeaderRole:
        return game == nullptr;
    case GameIdRole:
        return game ? game->id : QString();
    case TitleRole:
        if (game)
            return game->title;
        if (m_groupBy == QLatin1String("status"))
            return statusName(row.group);
        for (const Platform &p : m_platforms) {
            if (p.id == row.group)
                return p.name;
        }
        return row.group;
    case PlatformRole:
        return game ? game->platform : QString();
    case GroupRole:
        return row.group;
    case CountRole:
        return row.count;
    case CollapsedRole:
        return m_collapsed.contains(collapseKey(row.group));
    case InstalledRole:
        return game ? game->installed : true;
    case HueRole:
        return game ? hueOf(game->title) : 0.0;
    case IconUrlRole:
        return game ? game->iconUrl : QUrl();
    case UpdatingRole:
        return game ? game->steamUpdate != SteamUpdate::None : false;
    case HiddenRole:
        return game ? game->hidden : false;
    case SortValueRole:
        if (!game)
            return QString();
        if (m_sort == QLatin1String("play_time"))
            return shortPlayTimeText(game->playMinutes);
        if (m_sort == QLatin1String("last_played") && game->lastPlayed.isValid())
            return lastPlayedText(game->lastPlayed);
        return QString();
    }
    return {};
}

QHash<int, QByteArray> Library::roleNames() const
{
    return {
        {IsHeaderRole, "isHeader"},
        {GameIdRole, "gameId"},
        {TitleRole, "title"},
        {PlatformRole, "platform"},
        {GroupRole, "group"},
        {CountRole, "count"},
        {CollapsedRole, "collapsed"},
        {InstalledRole, "installed"},
        {HueRole, "hue"},
        {IconUrlRole, "iconUrl"},
        {HiddenRole, "hidden"},
        {UpdatingRole, "updating"},
        {SortValueRole, "sortValue"},
    };
}

void Library::setPlatformFilter(const QString &platform)
{
    if (m_platformFilter == platform)
        return;
    m_platformFilter = platform;
    emit platformFilterChanged();
    rebuild();
}

void Library::setSearch(const QString &search)
{
    const QString trimmed = search.trimmed();
    if (m_search == trimmed)
        return;
    const bool expanded = expandsGroups();
    m_search = trimmed;
    emit searchChanged();
    QStringList words = SearchText::words(trimmed);
    // Typing a space or punctuation after a word doesn't change the rows.
    if (words == m_searchWords && expanded == expandsGroups())
        return;
    m_searchWords = std::move(words);
    rebuild();
}

void Library::setGroupBy(const QString &groupBy)
{
    if (m_groupBy == groupBy || !listGroupings().contains(groupBy))
        return;
    m_groupBy = groupBy;
    emit groupByChanged();
    rebuild();
    saveListSettings();
}

const QStringList &Library::sortOrders()
{
    static const QStringList orders = {QStringLiteral("title"), QStringLiteral("last_played"),
                                       QStringLiteral("play_time")};
    return orders;
}

void Library::setSort(const QString &sort)
{
    if (m_sort == sort || !sortOrders().contains(sort))
        return;
    m_sort = sort;
    m_sortDescending = sort != QLatin1String("title");
    emit sortChanged();
    updateOrder();
    rebuild();
    saveListSettings();
}

void Library::setSortDescending(bool descending)
{
    if (m_sortDescending == descending)
        return;
    m_sortDescending = descending;
    emit sortChanged();
    updateOrder();
    rebuild();
    saveListSettings();
}

void Library::updateOrder()
{
    m_order.resize(m_games.size());
    std::iota(m_order.begin(), m_order.end(), 0);
    // m_games is in title order, so a stable sort keeps ties (and games without a value) by title.
    if (m_sort == QLatin1String("title")) {
        if (m_sortDescending)
            std::reverse(m_order.begin(), m_order.end());
        return;
    }
    QList<qint64> value(m_games.size());
    for (int i = 0; i < m_games.size(); ++i) {
        const Game &g = m_games.at(i);
        if (m_sort == QLatin1String("play_time"))
            value[i] = g.playMinutes > 0 ? g.playMinutes : -1;
        else
            value[i] = g.lastPlayed.isValid() ? g.lastPlayed.toMSecsSinceEpoch() : -1;
    }
    std::stable_sort(m_order.begin(), m_order.end(), [&](int a, int b) {
        const qint64 va = value.at(a), vb = value.at(b);
        if (va < 0 || vb < 0)
            return vb < 0 && va >= 0;
        return m_sortDescending ? va > vb : va < vb;
    });
}

void Library::setInstalledOnly(bool on)
{
    if (m_installedOnly == on)
        return;
    m_installedOnly = on;
    filtersEdited();
}

void Library::setFavoritesOnly(bool on)
{
    if (m_favoritesOnly == on)
        return;
    m_favoritesOnly = on;
    filtersEdited();
}

void Library::setTagFilter(const QStringList &tags)
{
    QStringList unique = tags;
    unique.removeDuplicates();
    if (m_tagFilter == unique)
        return;
    m_tagFilter = unique;
    filtersEdited();
}

void Library::setTagMatchAll(bool all)
{
    if (m_tagMatchAll == all)
        return;
    m_tagMatchAll = all;
    filtersEdited();
}

void Library::setStatusFilter(const QStringList &statuses)
{
    QStringList known;
    for (const QString &status : statuses) {
        if (listStatuses().contains(status) && !known.contains(status))
            known.append(status);
    }
    if (m_statusFilter == known)
        return;
    m_statusFilter = known;
    filtersEdited();
}

QVariantList Library::statuses() const
{
    QHash<QString, int> counts;
    for (const Game &g : m_games) {
        if (!g.hidden)
            ++counts[statusOf(g)];
    }
    QVariantList out;
    for (const QString &status : listStatuses()) {
        out.append(QVariantMap{{QStringLiteral("id"), status}, {QStringLiteral("name"), statusName(status)},
                               {QStringLiteral("games"), counts.value(status)}});
    }
    return out;
}

void Library::setShowHidden(bool show)
{
    if (m_showHidden == show)
        return;
    m_showHidden = show;
    if (updatePlatforms())
        emit platformsChanged();
    filtersEdited();
}

void Library::setShowSortValue(bool show)
{
    if (m_showSortValue == show)
        return;
    m_showSortValue = show;
    emit showSortValueChanged();
    saveListSettings();
}

bool Library::filtersActive() const
{
    return !m_installedOnly || m_favoritesOnly || m_showHidden || !effectiveTags().isEmpty()
        || !m_statusFilter.isEmpty();
}

void Library::clearFilters()
{
    if (m_installedOnly && !m_favoritesOnly && m_tagFilter.isEmpty() && !m_tagMatchAll && m_statusFilter.isEmpty()
        && !m_showHidden)
        return;
    const bool platformsDiffer = m_showHidden;
    m_installedOnly = true;
    m_favoritesOnly = false;
    m_tagFilter.clear();
    m_tagMatchAll = false;
    m_statusFilter.clear();
    m_showHidden = false;
    if (platformsDiffer && updatePlatforms())
        emit platformsChanged();
    filtersEdited();
}

void Library::filtersEdited()
{
    emit filtersChanged();
    rebuild();
    saveListSettings();
}

QString Library::renameTag(const QString &from, const QString &to)
{
    const QString error = m_store.renameTag(from, to);
    if (error.isEmpty() && m_tagFilter.contains(from)) {
        const qsizetype at = m_tagFilter.indexOf(from);
        if (m_tagFilter.contains(to))
            m_tagFilter.removeAt(at);
        else
            m_tagFilter[at] = to;
        m_tagNames.insert(to);
        filtersEdited();
    }
    return error;
}

QString Library::removeTag(const QString &name)
{
    QStringList games;
    for (auto it = m_store.snapshot().content.games.cbegin(); it != m_store.snapshot().content.games.cend(); ++it) {
        if (it->tags.contains(name))
            games.append(it.key());
    }
    const bool filtered = m_tagFilter.contains(name);
    m_undo.start(name);
    m_undo.keepCopy(QStringLiteral("library/tags.toml"));
    for (const QString &id : std::as_const(games))
        m_undo.keepCopy(gameFileOf(id));

    const QString error = m_store.removeTag(name);
    if (error.isEmpty() && m_tagFilter.removeAll(name) > 0)
        filtersEdited();
    const QString text = games.isEmpty() ? tr("Tag %1 deleted").arg(name)
        : games.size() == 1               ? tr("Tag %1 deleted from 1 game").arg(name)
                                          : tr("Tag %1 deleted from %2 games").arg(name).arg(games.size());
    if (commitUndo(text) != 0 && filtered) {
        m_afterUndo = [this, name] {
            if (!m_tagFilter.contains(name)) {
                m_tagFilter.append(name);
                filtersEdited();
            }
        };
    }
    return error;
}

QStringList Library::effectiveTags() const
{
    QStringList out;
    for (const QString &tag : m_tagFilter) {
        if (m_tagNames.contains(tag))
            out.append(tag);
    }
    return out;
}

bool Library::passesFilters(const Game &g, const QStringList &tags) const
{
    if (g.hidden && !m_showHidden)
        return false;
    if (m_favoritesOnly && !g.favorite)
        return false;
    if (!m_statusFilter.isEmpty() && !m_statusFilter.contains(statusOf(g)))
        return false;
    if (tags.isEmpty())
        return true;
    const auto has = [&](const QString &tag) {
        return std::any_of(g.tags.cbegin(), g.tags.cend(), [&](const GameTag &t) { return t.name == tag; });
    };
    return m_tagMatchAll ? std::all_of(tags.cbegin(), tags.cend(), has)
                         : std::any_of(tags.cbegin(), tags.cend(), has);
}

bool Library::updatePlatforms()
{
    QList<Platform> platforms;
    for (const Platform &p : std::as_const(m_allPlatforms)) {
        if (std::any_of(m_games.cbegin(), m_games.cend(),
                        [&](const Game &g) { return g.platform == p.id && (!g.hidden || m_showHidden); }))
            platforms.append(p);
    }
    if (platforms == m_platforms)
        return false;
    m_platforms = std::move(platforms);
    if (!m_platformFilter.isEmpty()
        && std::none_of(m_platforms.cbegin(), m_platforms.cend(), [&](const Platform &p) { return p.id == m_platformFilter; })) {
        m_platformFilter.clear();
        emit platformFilterChanged();
    }
    return true;
}

void Library::toggleGroup(const QString &group)
{
    if (!m_collapsed.remove(collapseKey(group)))
        m_collapsed.insert(collapseKey(group));
    rebuildGroup(group);
}

int Library::nextGameRow(int from, int step) const
{
    for (int i = from + step; i >= 0 && i < m_rows.size(); i += step) {
        if (m_rows.at(i).game >= 0)
            return i;
    }
    return from;
}

int Library::nextGroupRow(int from, int step) const
{
    if (!m_showingGroups || from < 0 || from >= m_rows.size())
        return from;
    const QString group = m_rows.at(from).group;
    const auto groupStart = [this](int row) {
        while (row > 0 && m_rows.at(row - 1).game >= 0)
            --row;
        return row;
    };

    if (step > 0) {
        for (int i = from + 1; i < m_rows.size(); ++i) {
            if (m_rows.at(i).game >= 0 && m_rows.at(i).group != group)
                return i;
        }
        return from;
    }
    const int start = groupStart(from);
    if (start != from)
        return start;
    for (int i = from - 1; i >= 0; --i) {
        if (m_rows.at(i).game >= 0)
            return groupStart(i);
    }
    return from;
}

int Library::collapseGroupAt(int row)
{
    if (!m_showingGroups || expandsGroups() || row < 0 || row >= m_rows.size() || m_rows.at(row).game < 0)
        return row;
    const QString group = m_rows.at(row).group;
    m_collapsed.insert(collapseKey(group));
    rebuildGroup(group);

    int header = 0;
    while (header < m_rows.size() && !(m_rows.at(header).game < 0 && m_rows.at(header).group == group))
        ++header;
    const int next = nextGameRow(header, 1);
    if (next != header)
        return next;
    const int previous = nextGameRow(header, -1);
    return previous != header ? previous : -1;
}

int Library::expandGroupNear(int row)
{
    if (!m_showingGroups || m_collapsed.isEmpty())
        return row;
    // A collapsed group's header is followed by another header or the end of the list.
    const auto isCollapsedHeader = [this](int i) {
        return m_rows.at(i).game < 0 && m_collapsed.contains(collapseKey(m_rows.at(i).group));
    };
    int header = -1;
    for (int i = qMax(row, 0); i < m_rows.size() && header < 0; ++i) {
        if (isCollapsedHeader(i))
            header = i;
    }
    for (int i = qMin(row, int(m_rows.size()) - 1); i >= 0 && header < 0; --i) {
        if (isCollapsedHeader(i))
            header = i;
    }
    if (header < 0)
        return row;

    const QString group = m_rows.at(header).group;
    m_collapsed.remove(collapseKey(group));
    rebuildGroup(group);
    return nextGameRow(header, 1);
}

int Library::rowOfGame(const QString &id) const
{
    for (int i = 0; i < m_rows.size(); ++i) {
        const int game = m_rows.at(i).game;
        if (game >= 0 && m_games.at(game).id == id)
            return i;
    }
    return -1;
}

QString Library::removeArt(const QString &id, const QString &kind)
{
    if (const QString error = m_store.removeArt(id, kind); !error.isEmpty())
        return error;
    const LibrarySnapshot &snapshot = m_store.snapshot();
    const GameFile file = snapshot.content.games.value(id);
    const bool proton = file.platform == kProtonPlatform;
    const qint64 appId = proton ? file.shortcut : GameId::steamAppId(id);
    QStringList skip = file.skipSteamArt;
    if (appId <= 0 || skip.contains(kind) || (proton && m_store.wroteGridArt(appId, kind))
        || SteamArt::find(snapshot.steam.install, appId, {kind}).isEmpty())
        return {};
    skip.append(kind);
    return m_store.setGameValues(id, {{QStringLiteral("skip_steam_art"), skip}});
}

QString Library::setGameValue(const QString &id, const QString &key, const QVariant &value)
{
    return m_store.setGameValues(id, {{key, value}});
}

QString Library::recordPlaySession(const QString &id, int minutes, const QDateTime &endedAt)
{
    const auto it = std::find_if(m_games.cbegin(), m_games.cend(), [&](const Game &g) { return g.id == id; });
    const int total = (it != m_games.cend() ? it->playMinutes : 0) + qMax(0, minutes);
    if (const QString error = m_store.setGameValues(id, {{QStringLiteral("play_minutes"), total},
                                                         {QStringLiteral("last_played"), endedAt}});
        !error.isEmpty())
        return error;
    if (minutes <= 0)
        return {};
    // The game file has the play time now; a history that can't be written leaves it as it is.
    return PlayHistory::append(m_store.dataDir(), id, {endedAt, minutes, false});
}

namespace {
bool playedInSteam(const Game &g)
{
    return g.platform == kSteamPlatform || g.platform == kProtonPlatform;
}
}

QVariantMap Library::historyFor(const QString &id) const
{
    const auto it = std::find_if(m_games.cbegin(), m_games.cend(), [&](const Game &g) { return g.id == id; });
    if (it == m_games.cend())
        return {};
    const Parsed<PlayHistory::File> file = PlayHistory::read(m_store.dataDir(), id);
    QVariantMap out = PlayHistory::summary(file.value, it->playMinutes, QDate::currentDate(),
                                           QLocale().firstDayOfWeek(), QTimeZone::systemTimeZone());
    out.insert(QStringLiteral("id"), id);
    out.insert(QStringLiteral("fromSteam"), playedInSteam(*it));
    out.insert(QStringLiteral("problems"), file.warnings);
    return out;
}

QString Library::shortPlayTime(int minutes) const
{
    return shortPlayTimeText(minutes);
}

struct Library::StatsInput
{
    QList<PlayHistory::StatsGame> games;
    QHash<QString, QVariantMap> gameInfo;
    QHash<QString, QVariantMap> platformInfo;
};

Library::StatsInput Library::statsInput() const
{
    StatsInput in;
    for (const Game &g : m_games) {
        if (g.hidden && !m_showHidden)
            continue;
        in.games.append({g.id, g.title, g.platform, {}});
        in.gameInfo.insert(g.id, {{QStringLiteral("title"), g.title},
                                  {QStringLiteral("platform"), g.platform},
                                  {QStringLiteral("iconUrl"), g.iconUrl},
                                  {QStringLiteral("hue"), hueOf(g.title)}});
        if (!in.platformInfo.contains(g.platform)) {
            const Platform *p = platform(g.platform);
            in.platformInfo.insert(g.platform, {{QStringLiteral("name"), g.platformName},
                                                {QStringLiteral("icon"), p ? p->icon : QString()}});
        }
    }
    return in;
}

void Library::monthStats(int year, int month)
{
    const QString dataDir = m_store.dataDir();
    const QDate today = QDate::currentDate();
    const Qt::DayOfWeek firstDay = QLocale().firstDayOfWeek();
    auto *watcher = new QFutureWatcher<QVariantMap>(this);
    connect(watcher, &QFutureWatcher<QVariantMap>::finished, this, [this, watcher, year, month] {
        emit monthStatsReady(year, month, watcher->result());
        watcher->deleteLater();
    });
    watcher->setFuture(QtConcurrent::run([=, in = statsInput()]() mutable {
        PlayHistory::load(dataDir, &in.games);
        return PlayHistory::toVariantMap(
            PlayHistory::monthStats(in.games, year, month, today, firstDay, QTimeZone::systemTimeZone()), in.gameInfo,
            in.platformInfo);
    }));
}

void Library::yearStats(int year)
{
    const QString dataDir = m_store.dataDir();
    const QDate today = QDate::currentDate();
    const Qt::DayOfWeek firstDay = QLocale().firstDayOfWeek();
    auto *watcher = new QFutureWatcher<QVariantMap>(this);
    connect(watcher, &QFutureWatcher<QVariantMap>::finished, this, [this, watcher, year] {
        emit yearStatsReady(year, watcher->result());
        watcher->deleteLater();
    });
    watcher->setFuture(QtConcurrent::run([=, in = statsInput()]() mutable {
        PlayHistory::load(dataDir, &in.games);
        return PlayHistory::toVariantMap(
            PlayHistory::yearStats(in.games, year, today, firstDay, QTimeZone::systemTimeZone()), in.gameInfo,
            in.platformInfo);
    }));
}

QVariantMap Library::deleteSession(const QString &id, const QString &end)
{
    const auto fail = [](const QString &error) { return QVariantMap{{QStringLiteral("error"), error}}; };
    if (m_store.readOnly())
        return fail(tr("Not saved: %1").arg(m_store.versionStatus().reason));
    const auto it = std::find_if(m_games.cbegin(), m_games.cend(), [&](const Game &g) { return g.id == id; });
    if (it == m_games.cend())
        return fail(tr("There's no game with the id %1").arg(id));
    const QDateTime endTime = QDateTime::fromString(end, Qt::ISODate);
    if (!endTime.isValid())
        return fail(tr("%1 isn't a date and time").arg(end));

    const QString dataDir = m_store.dataDir();
    const int deletedBefore = PlayHistory::read(dataDir, id).value.deletedMinutes;
    PlayHistory::Session removed;
    if (const QString error = PlayHistory::remove(dataDir, id, endTime, &removed); !error.isEmpty())
        return fail(error);

    QString error;
    if (playedInSteam(*it)) {
        // The total is Steam's, so it stays; what was deleted must not turn into "Before history".
        error = PlayHistory::setDeletedMinutes(dataDir, id, deletedBefore + removed.minutes);
    } else {
        QList<std::pair<QString, QVariant>> values{
            {QStringLiteral("play_minutes"), qMax(0, it->playMinutes - removed.minutes)}};
        if (const auto remaining = PlayHistory::read(dataDir, id).value.sessions; !remaining.isEmpty())
            values.append({QStringLiteral("last_played"), remaining.constLast().end});
        error = m_store.setGameValues(id, values);
    }
    if (!error.isEmpty()) {
        PlayHistory::append(dataDir, id, removed);
        return fail(error);
    }
    return {{QStringLiteral("error"), QString()},
            {QStringLiteral("session"),
             QVariantMap{{QStringLiteral("end"), removed.end.toOffsetFromUtc(removed.end.offsetFromUtc()).toString(Qt::ISODate)},
                         {QStringLiteral("minutes"), removed.minutes},
                         {QStringLiteral("merged"), removed.merged}}}};
}

QString Library::restoreSession(const QString &id, const QVariantMap &session)
{
    if (m_store.readOnly())
        return tr("Not saved: %1").arg(m_store.versionStatus().reason);
    const auto it = std::find_if(m_games.cbegin(), m_games.cend(), [&](const Game &g) { return g.id == id; });
    if (it == m_games.cend())
        return tr("There's no game with the id %1").arg(id);
    const PlayHistory::Session restored{QDateTime::fromString(session.value(QStringLiteral("end")).toString(), Qt::ISODate),
                                        session.value(QStringLiteral("minutes")).toInt(),
                                        session.value(QStringLiteral("merged")).toBool()};
    if (!restored.end.isValid() || restored.minutes < 0)
        return tr("That isn't a session");

    const QString dataDir = m_store.dataDir();
    if (const QString error = PlayHistory::append(dataDir, id, restored); !error.isEmpty())
        return error;
    if (playedInSteam(*it)) {
        const int deleted = PlayHistory::read(dataDir, id).value.deletedMinutes;
        return PlayHistory::setDeletedMinutes(dataDir, id, qMax(0, deleted - restored.minutes));
    }
    QList<std::pair<QString, QVariant>> values{{QStringLiteral("play_minutes"), it->playMinutes + restored.minutes}};
    if (!it->lastPlayed.isValid() || restored.end > it->lastPlayed)
        values.append({QStringLiteral("last_played"), restored.end});
    return m_store.setGameValues(id, values);
}

QString Library::gameIdAt(int row) const
{
    if (row < 0 || row >= m_rows.size() || m_rows.at(row).game < 0)
        return {};
    return m_games.at(m_rows.at(row).game).id;
}

const Game *Library::game(const QString &id) const
{
    for (const Game &g : m_games) {
        if (g.id == id)
            return g.hidden && !m_showHidden ? nullptr : &g;
    }
    return nullptr;
}

const Platform *Library::platform(const QString &id) const
{
    for (const Platform &p : m_allPlatforms) {
        if (p.id == id)
            return &p;
    }
    return nullptr;
}

QString Library::launchTip(const Game &g) const
{
    if (g.platform == kSteamPlatform && GameId::steamAppId(g.id) > 0) {
        if (!g.installed) {
            switch (g.steamUpdate) {
            case SteamUpdate::Downloading: return tr("Steam is installing the game; open its downloads");
            case SteamUpdate::Paused: return tr("The install is paused; open Steam's downloads");
            default: return tr("Install through Steam; it asks where");
            }
        }
        switch (g.steamUpdate) {
        case SteamUpdate::Waiting: return tr("Open the game's page in Steam to update it");
        case SteamUpdate::Downloading: return tr("Steam is updating the game; open its downloads");
        case SteamUpdate::Paused: return tr("The update is paused; open Steam's downloads");
        default: return tr("Launch through Steam");
        }
    }
    if (g.platform == kProtonPlatform && g.shortcut != 0)
        return g.installed ? tr("Launch through Steam") : tr("Locate the game's program");
    if (g.platform == kLinuxPlatform) {
        if (!g.installed)
            return tr("Locate the game's program");
        const LaunchOptions::Parts parts = LaunchOptions::parse(g.launchOptions);
        if (parts.problem.isEmpty() && !parts.wrapper.isEmpty())
            return tr("Launch with %1").arg(QFileInfo(parts.wrapper.first()).fileName());
        return tr("Launch");
    }
    if (!g.rom)
        return {};
    const Platform *p = platform(g.platform);
    const QString chosen = Emulators::choose(g.emulator, p ? p->emulators : QStringList(), installs());
    const EmulatorDef *emulator = Emulators::find(m_emulators, chosen);
    if (!emulator)
        return tr("No emulator for this game is installed");
    QString how;
    switch (installs().value(chosen).kind) {
    case EmulatorInstall::Kind::Flatpak: how = tr("Flatpak"); break;
    case EmulatorInstall::Kind::AppImage: how = tr("AppImage"); break;
    case EmulatorInstall::Kind::Path: how = tr("your path"); break;
    default: how = tr("installed"); break;
    }
    return tr("Launch with %1 (%2)").arg(emulator->name, how);
}

QVariantList Library::buildEmulatorList() const
{
    static const QHash<QString, QString> defaultNames = [] {
        QHash<QString, QString> names;
        QFile file(QStringLiteral(":/qt/qml/Launcher/resources/platforms.toml"));
        if (file.open(QIODevice::ReadOnly)) {
            for (const Platform &p : parsePlatforms(file.readAll(), QStringLiteral("platforms.toml")).value)
                names.insert(p.id, p.name);
        }
        return names;
    }();

    static const QStringList editableInApp{QStringLiteral("name"), QStringLiteral("platforms"),
                                           QStringLiteral("flatpak"), QStringLiteral("path"),
                                           QStringLiteral("extensions"), QStringLiteral("command"),
                                           QStringLiteral("args")};
    const QString home = QDir::homePath();
    const bool detecting = !m_store.snapshot().installsDetected;
    QVariantList out;
    for (const EmulatorDef &e : m_emulators) {
        QStringList platforms;
        for (const QString &id : e.platforms) {
            const Platform *p = platform(id);
            platforms.append(p ? p->name : defaultNames.value(id, id));
        }
        const EmulatorInstall install = installs().value(e.id);
        QString kind;
        switch (install.kind) {
        case EmulatorInstall::Kind::None: break;
        case EmulatorInstall::Kind::Path: kind = QStringLiteral("path"); break;
        case EmulatorInstall::Kind::Flatpak: kind = QStringLiteral("flatpak"); break;
        case EmulatorInstall::Kind::Executable: kind = QStringLiteral("executable"); break;
        case EmulatorInstall::Kind::AppImage: kind = QStringLiteral("appimage"); break;
        }
        // Where the arguments go, with a stand-in for the ROM. The program's full path, not
        // ~/…: a quoted ~ wouldn't work if the command were pasted into a shell.
        EmulatorInstall shown = install;
        if (!install.found()) {
            shown.program = !e.path.isEmpty()        ? Emulators::expandHome(e.path, home)
                          : !e.executables.isEmpty() ? e.executables.first()
                                                     : e.name;
        }
        QStringList fileOnly;
        for (const QString &key : e.userKeys) {
            if (!editableInApp.contains(key))
                fileOnly.append(key);
        }
        out.append(QVariantMap{
            {QStringLiteral("id"), e.id},
            {QStringLiteral("name"), e.name},
            {QStringLiteral("platforms"), platforms},
            {QStringLiteral("platformIds"), e.platforms},
            {QStringLiteral("preset"), e.preset},
            {QStringLiteral("found"), install.found()},
            {QStringLiteral("installKind"), kind},
            {QStringLiteral("install"), install.found() ? install.describe(home) : QString()},
            {QStringLiteral("detecting"), detecting},
            {QStringLiteral("path"), e.path},
            {QStringLiteral("pathMissing"), install.pathMissing},
            {QStringLiteral("args"), e.args},
            {QStringLiteral("flatpak"), e.flatpak},
            {QStringLiteral("extensions"), e.extensions},
            {QStringLiteral("commandArgs"), e.command},
            {QStringLiteral("fileOnly"), e.preset ? QStringList() : fileOnly},
            {QStringLiteral("command"), buildLaunchCommand(e, shown, QStringLiteral("ROM")).toString()},
            {QStringLiteral("custom"), e.userKeys},
        });
    }
    return out;
}

QVariantList Library::buildPlatformSettings() const
{
    const QList<Platform> &defined = m_store.snapshot().content.platforms;
    QVariantList out;
    for (const Platform &p : m_allPlatforms) {
        // The emulators as written, unknown ids included, so saving the list from the page
        // doesn't drop an id the user mistyped (it warns instead).
        const auto written = std::find_if(defined.cbegin(), defined.cend(), [&](const Platform &d) { return d.id == p.id; });
        const int games = int(std::count_if(m_games.cbegin(), m_games.cend(),
                                            [&](const Game &g) { return g.platform == p.id && !g.hidden; }));
        if (written == defined.cend() && games == 0)
            continue;
        const QStringList emulators = written != defined.cend() ? written->emulators : p.emulators;
        QStringList emulatorNames;
        for (const QString &id : emulators) {
            const EmulatorDef *e = Emulators::find(m_emulators, id);
            emulatorNames.append(e ? e->name : id);
        }
        QVariantList problems;
        for (const PlatformProblem &problem : m_platformProblems.value(p.id))
            problems.append(QVariantMap{{QStringLiteral("text"), problem.text}, {QStringLiteral("command"), problem.command}});
        out.append(QVariantMap{
            {QStringLiteral("id"), p.id},
            {QStringLiteral("name"), p.name},
            {QStringLiteral("icon"), p.icon},
            {QStringLiteral("defined"), written != defined.cend()},
            {QStringLiteral("games"), games},
            {QStringLiteral("emulators"), emulators},
            {QStringLiteral("emulatorNames"), emulatorNames},
            {QStringLiteral("romFolders"), p.romFolders},
            {QStringLiteral("inRail"), written != defined.cend() ? written->inRail : p.inRail},
            {QStringLiteral("problems"), problems},
        });
    }
    return out;
}

QVariantList Library::matchRomFolders(const QString &parentPath) const
{
    const QString home = QDir::homePath();
    const auto tilde = [&](const QString &path) {
        return path == home || path.startsWith(home + u'/') ? u'~' + path.sliced(home.size()) : path;
    };

    QDir dir(Emulators::expandHome(parentPath, home));
    QVariantList out;
    if (!dir.exists())
        return out;

    QList<Platform> candidates;
    for (const Platform &p : m_allPlatforms) {
        if (!p.emulators.isEmpty())
            candidates.append(p);
    }

    const QFileInfoList entries = dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
    for (const QFileInfo &entry : entries) {
        const QString name = entry.fileName();
        const QString n = RomFolderMatch::normalize(name);
        QString id = RomFolderMatch::byAlias(name);
        if (id.isEmpty()) {
            const auto found = std::find_if(candidates.cbegin(), candidates.cend(), [&](const Platform &p) {
                return RomFolderMatch::normalize(p.id) == n || RomFolderMatch::normalize(p.name) == n;
            });
            if (found != candidates.cend())
                id = found->id;
        }
        const auto platform = std::find_if(candidates.cbegin(), candidates.cend(), [&](const Platform &p) { return p.id == id; });
        const QString path = tilde(entry.absoluteFilePath());
        out.append(QVariantMap{
            {QStringLiteral("folder"), name},
            {QStringLiteral("path"), path},
            {QStringLiteral("platformId"), id},
            {QStringLiteral("platformName"), platform != candidates.cend() ? platform->name : QString()},
            {QStringLiteral("already"), platform != candidates.cend() && platform->romFolders.contains(path)},
        });
    }
    return out;
}

QString Library::setEmulatorValue(const QString &id, const QString &key, const QVariant &value)
{
    static const QStringList presetKeys{QStringLiteral("path"), QStringLiteral("args")};
    static const QStringList userKeys{QStringLiteral("name"), QStringLiteral("platforms"), QStringLiteral("flatpak"),
                                      QStringLiteral("path"), QStringLiteral("extensions"), QStringLiteral("command"),
                                      QStringLiteral("args")};
    const EmulatorDef *emulator = Emulators::find(m_emulators, id);
    if (!(emulator && !emulator->preset ? userKeys : presetKeys).contains(key))
        return tr("%1 can't be changed here; edit emulators.toml").arg(key);
    return m_store.setEmulatorValue(id, key, value);
}

int Library::pinnedGames(const QString &emulatorId) const
{
    const auto &games = m_store.snapshot().content.games;
    return int(std::count_if(games.cbegin(), games.cend(),
                             [&](const GameFile &g) { return g.emulator == emulatorId; }));
}

QVariantMap Library::gameAt(int row) const
{
    if (row < 0 || row >= m_rows.size() || m_rows.at(row).game < 0)
        return {};
    const Game &g = m_games.at(m_rows.at(row).game);
    QString platformIcon;
    for (const Platform &p : m_platforms) {
        if (p.id == g.platform)
            platformIcon = p.icon;
    }

    QVariantList tags;
    for (const GameTag &tag : g.tags)
        tags.append(QVariantMap{{QStringLiteral("name"), tag.name}, {QStringLiteral("color"), tag.color}});
    sortTagsByName(tags);

    return {
        {QStringLiteral("id"), g.id},
        {QStringLiteral("title"), g.title},
        {QStringLiteral("platform"), g.platform},
        {QStringLiteral("platformName"), g.platformName},
        {QStringLiteral("platformIcon"), platformIcon},
        {QStringLiteral("description"), g.description},
        {QStringLiteral("year"), g.year > 0 ? QString::number(g.year) : QString()},
        {QStringLiteral("sizeText"),
         g.sizeBytes > 0 ? QLocale().formattedDataSize(g.sizeBytes, 1, QLocale::DataSizeTraditionalFormat)
                         : QString()},
        {QStringLiteral("playTimeText"), playTimeText(g.playMinutes)},
        {QStringLiteral("lastPlayedText"), lastPlayedText(g.lastPlayed)},
        {QStringLiteral("rating"), g.rating},
        {QStringLiteral("statusText"), g.status.isEmpty() ? QString() : statusName(g.status)},
        {QStringLiteral("logoScale"), g.logoScale},
        {QStringLiteral("logoPosition"), g.logoPosition},
        {QStringLiteral("logoOffsetX"), g.logoOffsetX},
        {QStringLiteral("logoOffsetY"), g.logoOffsetY},
        {QStringLiteral("logoScrim"), g.logoScrim},
        {QStringLiteral("logoScrimSide"), g.logoScrimSide},
        {QStringLiteral("logoScrimStrength"), g.logoScrimStrength},
        {QStringLiteral("installed"), g.installed},
        {QStringLiteral("steamUpdate"), updateStateName(g.steamUpdate)},
        {QStringLiteral("steamUninstallable"), g.installed || g.steamUpdate == SteamUpdate::Downloading
                                                   || g.steamUpdate == SteamUpdate::Paused},
        {QStringLiteral("steamUpdateText"), updateText(g)},
        {QStringLiteral("steamUpdatePercent"), updatePercent(g)},
        {QStringLiteral("rom"), g.rom},
        {QStringLiteral("romPath"), g.romPath},
        {QStringLiteral("gameNotFound"), runsProgram(g.platform) && !g.installed && !g.uninstalled},
        {QStringLiteral("launchTip"), launchTip(g)},
        {QStringLiteral("favorite"), g.favorite},
        {QStringLiteral("hidden"), g.hidden},
        {QStringLiteral("emulator"), g.emulator},
        {QStringLiteral("pendingSteamEdits"), g.pendingSteamEdits},
        {QStringLiteral("shortcutQueued"),
         g.pendingSteamEdits && m_store.snapshot().steamQueued.value(g.id).contains(SteamSync::kAddShortcut)},
        {QStringLiteral("tags"), tags},
        {QStringLiteral("hue"), hueOf(g.title)},
        {QStringLiteral("iconUrl"), g.iconUrl},
        {QStringLiteral("heroUrl"), heroDisplayUrl(g)},
        {QStringLiteral("heroArt"), g.heroUrl},
        {QStringLiteral("logoUrl"), g.logoUrl},
    };
}

QUrl Library::heroDisplayUrl(const Game &game) const
{
    const ArtSet art = m_store.snapshot().art.value(game.id);
    const QString path = m_heroFiles.value(HeroCache::stem(game.id, art.modified));
    if (path.isEmpty() || game.heroUrl.isEmpty())
        return game.heroUrl;
    QUrl url = QUrl::fromLocalFile(path);
    if (art.modified > 0)
        url.setQuery(QStringLiteral("v=%1").arg(art.modified));
    return url;
}

void Library::copyHeroesNear(int row)
{
    const QString previous = std::exchange(m_heroFocus, gameIdAt(row));
    if (previous != m_heroFocus && !m_store.artToShrink(previous).isEmpty())
        makeNextHeroCopy();
}

void Library::makeNextHeroCopy()
{
    if (!m_heroCopiesStarted || m_heroCopy.isRunning() || m_store.shrinking())
        return;

    const LibrarySnapshot &snapshot = m_store.snapshot();
    const int focus = std::max(0, rowOfGame(m_heroFocus));
    int best = -1;
    int bestDistance = 0;
    // The shown game's art waits: shrinking it would replay its switch.
    const auto shrinkable = [&](const Game &game) {
        return game.id != m_heroFocus && !m_store.artToShrink(game.id).isEmpty();
    };
    const auto pending = [&](const Game &game) {
        const ArtSet art = snapshot.art.value(game.id);
        const QString stem = HeroCache::stem(game.id, art.modified);
        return shrinkable(game)
            || (!art.hero.isEmpty() && !m_heroFiles.contains(stem) && !m_heroSettled.contains(stem));
    };
    for (int i = 0; i < m_rows.size(); ++i) {
        const int game = m_rows.at(i).game;
        if (game < 0 || !pending(m_games.at(game)))
            continue;
        if (best < 0 || std::abs(i - focus) < bestDistance) {
            best = game;
            bestDistance = std::abs(i - focus);
        }
    }
    if (best < 0) {
        const auto hidden = std::find_if(m_games.cbegin(), m_games.cend(), pending);
        if (hidden != m_games.cend())
            best = int(hidden - m_games.cbegin());
    }

    if (best < 0) {
        if (m_store.scanning() || m_store.loading() || m_store.readOnly())
            return;
        QSet<QString> keep;
        for (const Game &game : std::as_const(m_games)) {
            const ArtSet art = snapshot.art.value(game.id);
            if (!art.hero.isEmpty())
                keep.insert(HeroCache::stem(game.id, art.modified));
        }
        for (auto it = m_heroFiles.begin(); it != m_heroFiles.end();) {
            it = keep.contains(it.key()) ? std::next(it) : m_heroFiles.erase(it);
        }
        (void)QtConcurrent::run(HeroCache::prune, m_cacheDir, keep);
        return;
    }

    const Game &game = m_games.at(best);
    // Shrunk first, so the copy is made from the shrunk file and not twice.
    if (shrinkable(game)) {
        m_store.shrinkArt(game.id, m_store.artToShrink(game.id).constFirst());
        return;
    }
    const ArtSet art = snapshot.art.value(game.id);
    m_heroCopyId = game.id;
    m_heroCopyStem = HeroCache::stem(game.id, art.modified);
    m_heroCopy.setFuture(QtConcurrent::run(HeroCache::make, art.hero, game.id, art.modified, m_cacheDir));
}

void Library::heroCopyFinished()
{
    const HeroCache::Result result = m_heroCopy.result();
    m_heroSettled.insert(m_heroCopyStem);
    if (!result.error.isEmpty() && !m_heroFailureLogged) {
        m_heroFailureLogged = true;
        qCWarning(lcLibrary).noquote() << QStringLiteral("no display copy of %1's hero art (others may follow): %2")
                                              .arg(m_heroCopyId, result.error);
    }
    if (!result.path.isEmpty()) {
        m_heroFiles.insert(m_heroCopyStem, result.path);
        const int row = rowOfGame(m_heroCopyId);
        if (row >= 0)
            emit dataChanged(index(row), index(row));
    }
    makeNextHeroCopy();
}

void Library::rebuildGroup(const QString &group)
{
    int notInstalled = 0;
    bool showingGroups = false;
    QList<Row> rows = computeRows(&showingGroups, &notInstalled);

    const auto headerOf = [&group](const QList<Row> &list) {
        for (qsizetype i = 0; i < list.size(); ++i) {
            if (list.at(i).game < 0 && list.at(i).group == group)
                return i;
        }
        return qsizetype(-1);
    };
    const auto gamesAfter = [](const QList<Row> &list, qsizetype header) {
        qsizetype end = header + 1;
        while (end < list.size() && list.at(end).game >= 0)
            ++end;
        return end - header - 1;
    };
    const qsizetype header = headerOf(m_rows);
    const bool onlyThisGroup = showingGroups == m_showingGroups && header >= 0 && headerOf(rows) == header
        && m_rows.size() - gamesAfter(m_rows, header) == rows.size() - gamesAfter(rows, header);

    if (onlyThisGroup) {
        const int before = int(gamesAfter(m_rows, header));
        const int after = int(gamesAfter(rows, header));
        const auto announce = [this, header](int rows) {
            m_groupChange = {{QStringLiteral("start"), int(header) + 1}, {QStringLiteral("rows"), rows}};
            emit groupChangeChanged();
            m_groupChangeTimer.start();
        };
        if (before == 0 && after > 0) {
            announce(after);
            beginInsertRows({}, int(header) + 1, int(header) + after);
            m_rows = std::move(rows);
            endInsertRows();
        } else if (after == 0 && before > 0) {
            announce(-before);
            beginRemoveRows({}, int(header) + 1, int(header) + before);
            m_rows = std::move(rows);
            endRemoveRows();
        } else {
            m_rows = std::move(rows);
        }
        emit dataChanged(index(int(header)), index(int(header)), {CollapsedRole});
    } else {
        beginResetModel();
        m_rows = std::move(rows);
        m_showingGroups = showingGroups;
        endResetModel();
    }
    if (notInstalled != m_notInstalledHidden) {
        m_notInstalledHidden = notInstalled;
        emit notInstalledHiddenChanged();
    }
}

bool Library::applyRowMove(const QList<Row> &rows, const QStringList &oldKeys, const QStringList &newKeys)
{
    if (oldKeys.size() != newKeys.size() || oldKeys.size() < 2)
        return false;
    // A header's key carries its count, which changes when the game moves between groups.
    const auto identity = [](const QString &key) {
        return key.startsWith(u'#') ? key.left(key.lastIndexOf(u':')) : key;
    };
    qsizetype first = 0;
    while (first < oldKeys.size() && identity(oldKeys.at(first)) == identity(newKeys.at(first)))
        ++first;
    if (first == oldKeys.size())
        return false;
    qsizetype last = oldKeys.size() - 1;
    while (identity(oldKeys.at(last)) == identity(newKeys.at(last)))
        --last;

    const auto sameRange = [&](qsizetype from, qsizetype to, qsizetype length) {
        for (qsizetype i = 0; i < length; ++i) {
            if (identity(oldKeys.at(from + i)) != identity(newKeys.at(to + i)))
                return false;
        }
        return true;
    };
    const qsizetype span = last - first;
    int from = -1, to = -1;
    if (identity(oldKeys.at(first)) == identity(newKeys.at(last)) && sameRange(first + 1, first, span)) {
        from = int(first);
        to = int(last) + 1; // moveRows' destination is the row it goes in front of
    } else if (identity(oldKeys.at(last)) == identity(newKeys.at(first)) && sameRange(first, first + 1, span)) {
        from = int(last);
        to = int(first);
    } else {
        return false;
    }
    if (m_rows.at(from).game < 0)
        return false;

    beginMoveRows({}, from, from, {}, to);
    m_rows = rows;
    endMoveRows();
    emit dataChanged(index(0), index(int(m_rows.size()) - 1));
    return true;
}

void Library::rebuild()
{
    int notInstalled = 0;
    beginResetModel();
    m_rows = computeRows(&m_showingGroups, &notInstalled);
    endResetModel();
    if (notInstalled != m_notInstalledHidden) {
        m_notInstalledHidden = notInstalled;
        emit notInstalledHiddenChanged();
    }
}

QList<Library::Row> Library::computeRows(bool *showingGroups, int *notInstalled) const
{
    const bool byStatus = m_groupBy == QLatin1String("status");
    const bool grouped = byStatus || (m_groupBy == QLatin1String("platform") && m_platformFilter.isEmpty());
    const QStringList tags = effectiveTags();
    *notInstalled = 0;
    const auto listed = [&](int i) {
        const Game &g = m_games.at(i);
        if (!passesFilters(g, tags)
            || !(m_searchWords.isEmpty() || SearchText::matches(m_foldedTitles.at(i), m_searchWords)))
            return false;
        if (m_installedOnly && !g.installed && g.steamUpdate == SteamUpdate::None) {
            ++*notInstalled;
            return false;
        }
        return true;
    };

    QList<Row> rows;
    *showingGroups = grouped;

    if (!grouped) {
        for (int i : m_order) {
            const Game &g = m_games.at(i);
            if ((m_platformFilter.isEmpty() || g.platform == m_platformFilter) && listed(i))
                rows.append({i, g.platform, 0});
        }
        return rows;
    }

    QStringList groups;
    if (byStatus) {
        groups = listStatuses();
    } else {
        for (const Platform &platform : m_platforms)
            groups.append(platform.id);
    }
    QHash<QString, QList<int>> matches;
    for (int i : m_order) {
        const Game &g = m_games.at(i);
        if (byStatus ? !m_platformFilter.isEmpty() && g.platform != m_platformFilter : !groups.contains(g.platform))
            continue;
        if (listed(i))
            matches[byStatus ? statusOf(g) : g.platform].append(i);
    }

    const bool expanded = expandsGroups();
    for (const QString &group : std::as_const(groups)) {
        const QList<int> games = matches.value(group);
        if (games.isEmpty())
            continue;

        rows.append({-1, group, int(games.size())});
        if (m_collapsed.contains(collapseKey(group)) && !expanded)
            continue;
        for (int i : games)
            rows.append({i, group, 0});
    }
    return rows;
}

QList<LookupGame> Library::lookupGames() const
{
    const LibraryContent &content = m_store.snapshot().content;
    QList<LookupGame> out;
    for (const Game &g : m_games) {
        if (g.hidden)
            continue;
        const GameFile file = content.games.value(g.id);
        LookupGame l;
        l.id = g.id;
        l.title = g.title;
        l.platform = g.platform;
        l.platformName = g.platformName;
        l.icon = g.iconUrl;
        l.hasDescription = !g.description.trimmed().isEmpty();
        l.hasYear = g.year > 0;
        l.hasRating = g.rating >= 0;
        l.description = g.description;
        l.year = g.year;
        l.rating = g.rating;
        l.ratingFromIgdb = g.ratingFromIgdb;
        const std::pair<const char *, QUrl> art[] = {{"cover", g.coverUrl}, {"hero", g.heroUrl}, {"logo", g.logoUrl},
                                                     {"icon", g.iconUrl}};
        for (const auto &[kind, url] : art) {
            if (!url.isEmpty()) {
                l.art.append(QLatin1String(kind));
                l.artUrls.insert(QLatin1String(kind), url.toString());
            }
        }
        for (const GameTag &tag : g.tags)
            l.tags.append(tag.name);
        l.igdb = file.igdb;
        l.steamgriddb = file.steamgriddb;
        l.steamAppId = g.platform == kSteamPlatform ? GameId::steamAppId(g.id) : 0;
        for (const QVariant &id : igdbPlatformIds(g.platform))
            l.igdbPlatforms.append(id.toInt());
        out.append(l);
    }
    return out;
}

QHash<QString, QString> Library::saveGames(const QList<std::pair<QString, QVariantMap>> &games)
{
    QHash<QString, QString> errors;
    m_store.beginBatch();
    for (const auto &[id, values] : games) {
        if (const QString error = saveGame(id, values); !error.isEmpty())
            errors.insert(id, error);
    }
    m_store.endBatch();
    return errors;
}

QHash<QString, QString> Library::setArts(const QList<std::array<QString, 3>> &art)
{
    QHash<QString, QString> errors;
    m_store.beginBatch();
    for (const auto &[id, kind, file] : art) {
        if (const QString error = setArt(id, kind, file); !error.isEmpty())
            errors.insert(id, errors.contains(id) ? errors.value(id) + u'\n' + error : error);
    }
    m_store.endBatch();
    return errors;
}

QVariantList Library::igdbPlatformIds(const QString &platformId) const
{
    QVariantList out;
    for (const Platform &p : m_allPlatforms) {
        if (p.id != platformId)
            continue;
        for (int id : ::igdbPlatformIds(p))
            out.append(id);
    }
    return out;
}

QVariantMap Library::emulatorChoices(const QString &platformId) const
{
    const Platform *p = platform(platformId);
    QStringList ids = p ? p->emulators : QStringList();
    for (const EmulatorDef &e : m_emulators) {
        if (e.platforms.contains(platformId) && !ids.contains(e.id))
            ids.append(e.id);
    }
    QVariantList choices;
    for (const QString &id : std::as_const(ids)) {
        const EmulatorDef *e = Emulators::find(m_emulators, id);
        choices.append(QVariantMap{{QStringLiteral("id"), id},
                                   {QStringLiteral("name"), e->name},
                                   {QStringLiteral("installed"), installs().value(id).found()},
                                   {QStringLiteral("args"), e->args}});
    }
    const QStringList preferred = p ? p->emulators : QStringList();
    QString chosen = Emulators::choose({}, preferred, installs());
    if (chosen.isEmpty() && !preferred.isEmpty())
        chosen = preferred.first();
    const EmulatorDef *def = Emulators::find(m_emulators, chosen);
    return {{QStringLiteral("choices"), choices}, {QStringLiteral("defaultEmulator"), def ? def->name : QString()}};
}

QVariantMap Library::gameForEditing(const QString &id) const
{
    const Game *g = game(id);
    if (!g)
        return {};
    const GameFile file = m_store.snapshot().content.games.value(id);
    QVariantList tags;
    for (const GameTag &tag : g->tags)
        tags.append(QVariantMap{{QStringLiteral("name"), tag.name}, {QStringLiteral("color"), tag.color}});
    sortTagsByName(tags);
    const auto url = [](const QUrl &u) { return u.isEmpty() ? QString() : u.toString(); };
    const QVariantMap emulators = emulatorChoices(g->platform);
    QString platformIcon;
    for (const Platform &p : m_platforms) {
        if (p.id == g->platform)
            platformIcon = p.icon;
    }
    const QVariantMap steam =
        g->platform == kSteamPlatform || g->platform == kProtonPlatform ? steamChoices() : QVariantMap();
    return {
        {QStringLiteral("id"), g->id},
        {QStringLiteral("title"), g->title},
        {QStringLiteral("platform"), g->platform},
        {QStringLiteral("platformName"), g->platformName},
        {QStringLiteral("platformIcon"), platformIcon},
        {QStringLiteral("description"), g->description},
        {QStringLiteral("year"), g->year},
        {QStringLiteral("rating"), g->rating},
        {QStringLiteral("favorite"), g->favorite},
        {QStringLiteral("status"), g->status},
        {QStringLiteral("ratingSource"), file.ratingSource},
        {QStringLiteral("igdb"), file.igdb},
        {QStringLiteral("steamgriddb"), file.steamgriddb},
        {QStringLiteral("steamAppId"), g->platform == kSteamPlatform ? GameId::steamAppId(g->id) : 0},
        {QStringLiteral("tags"), tags},
        {QStringLiteral("emulator"), file.emulator},
        {QStringLiteral("args"), file.args},
        {QStringLiteral("launchOptions"), g->launchOptions},
        {QStringLiteral("proton"), g->proton},
        {QStringLiteral("protonTools"), steam.value(QStringLiteral("protonTools"))},
        {QStringLiteral("protonDefault"), steam.value(QStringLiteral("protonDefault"))},
        {QStringLiteral("steamPresets"),
         g->platform == kLinuxPlatform ? QVariant(linuxPresets()) : steam.value(QStringLiteral("steamPresets"))},
        {QStringLiteral("pendingSteamEdits"), g->pendingSteamEdits},
        {QStringLiteral("shortcutQueued"),
         g->pendingSteamEdits && m_store.snapshot().steamQueued.value(id).contains(SteamSync::kAddShortcut)},
        {QStringLiteral("installPath"), g->installDir},
        {QStringLiteral("exe"), file.exe},
        {QStringLiteral("exePath"), g->exe},
        {QStringLiteral("startDir"), file.startDir},
        {QStringLiteral("installDir"), file.installDir},
        {QStringLiteral("uninstalled"), g->uninstalled},
        {QStringLiteral("gameNotFound"), runsProgram(g->platform) && !g->installed && !g->uninstalled},
        {QStringLiteral("rom"), g->rom},
        {QStringLiteral("romKey"), g->romKey},
        {QStringLiteral("romPath"), g->romPath},
        {QStringLiteral("romWritten"), file.rom},
        {QStringLiteral("installed"), g->installed},
        {QStringLiteral("playMinutes"), g->playMinutes},
        {QStringLiteral("lastPlayed"), g->lastPlayed},
        {QStringLiteral("art"), QVariantMap{{QStringLiteral("hero"), url(g->heroUrl)},
                                            {QStringLiteral("logo"), url(g->logoUrl)},
                                            {QStringLiteral("icon"), url(g->iconUrl)},
                                            {QStringLiteral("cover"), url(g->coverUrl)}}},
        {QStringLiteral("removeDeletes"), !m_store.inIndex(id)},
        {QStringLiteral("hidden"), g->hidden},
        {QStringLiteral("hasArt"), m_store.snapshot().art.contains(id)},
        {QStringLiteral("emulators"), emulators.value(QStringLiteral("choices"))},
        {QStringLiteral("defaultEmulator"), emulators.value(QStringLiteral("defaultEmulator"))},
    };
}

QVariantMap Library::steamChoices() const
{
    QVariantList protonTools;
    QString protonDefault;
    QVariantList presets;
    const SteamState &steam = m_store.snapshot().steam;
    const QString defaultTool = steam.compatTools.value(0);
    for (const CompatTool &t : steam.tools()) {
        protonTools.append(QVariantMap{{QStringLiteral("name"), t.name}, {QStringLiteral("displayName"), t.displayName}});
        if (t.name == defaultTool)
            protonDefault = t.displayName;
    }
    for (const SteamPreset &p : m_steamSettings.presets) {
        presets.append(QVariantMap{{QStringLiteral("id"), p.id}, {QStringLiteral("name"), p.name},
                                   {QStringLiteral("adds"), p.describe()}});
    }
    return {{QStringLiteral("protonTools"), protonTools},
            {QStringLiteral("protonDefault"), protonDefault},
            {QStringLiteral("steamPresets"), presets}};
}

bool Library::steamPresetOn(const QString &launchOptions, const QString &preset) const
{
    const SteamPreset *p = m_steamSettings.preset(preset);
    return p && LaunchOptions::isOn(launchOptions, *p);
}

QString Library::setSteamPreset(const QString &launchOptions, const QString &preset, bool on) const
{
    const SteamPreset *p = m_steamSettings.preset(preset);
    if (!p)
        return launchOptions;
    return on ? LaunchOptions::turnOn(launchOptions, *p) : LaunchOptions::turnOff(launchOptions, *p);
}

QVariantMap Library::inspectRom(const QString &file) const
{
    const QString path = localPath(file);
    const RomPick pick = inspectRomPick(path, m_allPlatforms, m_emulators);
    const bool program = QFileInfo(path).suffix().compare(QLatin1String("exe"), Qt::CaseInsensitive) == 0;
    if (program && (m_store.snapshot().steam.install.found() || pick.platforms.isEmpty()))
        return inspectProgram(path);
    const LinuxProgramKind kind = linuxProgramKind(path);
    const auto refused = [&](const QString &problem) {
        return QVariantMap{{QStringLiteral("path"), path},
                           {QStringLiteral("fileName"), QFileInfo(path).fileName()},
                           {QStringLiteral("problem"), problem},
                           {QStringLiteral("existing"), m_store.gameWithProgram(path)},
                           {QStringLiteral("platforms"), QStringList()},
                           {QStringLiteral("choices"), QVariantList()}};
    };
    if (kind == LinuxProgramKind::SharedLibrary)
        return refused(tr("%1 is a shared library, not a program; pick the game's program or its launcher script")
                           .arg(QFileInfo(path).fileName()));
    if (kind != LinuxProgramKind::None)
        return inspectLinuxProgram(path, kind);
    static const QStringList programSuffixes = {QStringLiteral("sh"), QStringLiteral("x86_64"), QStringLiteral("x86"),
                                                QStringLiteral("appimage")};
    if (pick.platforms.isEmpty() && programSuffixes.contains(QFileInfo(path).suffix().toLower())) {
        const bool script = QFileInfo(path).suffix().toLower() == QLatin1String("sh");
        return refused(script ? tr("%1 isn't a program: a script needs a #! line at its start, like #!/bin/sh")
                                    .arg(QFileInfo(path).fileName())
                              : tr("%1 isn't a program this computer runs").arg(QFileInfo(path).fileName()));
    }
    QVariantList choices;
    for (const Platform &p : m_allPlatforms) {
        QStringList takes;
        for (const QString &id : p.emulators) {
            const EmulatorDef *e = Emulators::find(m_emulators, id);
            for (const QString &ext : e->extensions)
                takes.append(u'.' + ext);
            takes += e->files;
        }
        takes.removeDuplicates();
        if (takes.isEmpty())
            continue;
        choices.append(QVariantMap{{QStringLiteral("id"), p.id},
                                   {QStringLiteral("name"), p.name},
                                   {QStringLiteral("icon"), p.icon},
                                   {QStringLiteral("takes"), takes.join(QStringLiteral(", "))},
                                   {QStringLiteral("runs"), pick.platforms.contains(p.id)}});
    }
    return {
        {QStringLiteral("path"), path},
        {QStringLiteral("fileName"), QFileInfo(path).fileName()},
        {QStringLiteral("title"), pick.info.title},
        {QStringLiteral("titleId"), pick.info.titleId},
        {QStringLiteral("platforms"), pick.platforms},
        {QStringLiteral("problem"), pick.problem},
        {QStringLiteral("existing"), m_store.gameWithRom(path)},
        {QStringLiteral("choices"), choices},
    };
}

QVariantMap Library::inspectProgram(const QString &path) const
{
    const SteamState &steam = m_store.snapshot().steam;
    QString problem;
    if (!steam.install.found())
        problem = tr("A Windows program is added as a Proton game, and Proton games need Steam, which wasn't found");
    else if (!steam.shortcutsOk)
        problem = tr("Steam's shortcuts couldn't be read, so no game can be added to it");
    const QString installDir = protonInstallDir(QDir::cleanPath(path), m_store.installLocations().home);
    QString proton = m_steamSettings.defaultProton;
    if (proton.isEmpty())
        proton = steam.compatTools.value(0, QStringLiteral("proton_experimental"));
    QVariantMap out = steamChoices();
    out.insert({
        {QStringLiteral("path"), path},
        {QStringLiteral("fileName"), QFileInfo(path).fileName()},
        {QStringLiteral("title"), RomName::parseFolder(QFileInfo(installDir).fileName()).title},
        {QStringLiteral("titleId"), QString()},
        {QStringLiteral("platforms"), QStringList{kProtonPlatform}},
        {QStringLiteral("problem"), problem},
        {QStringLiteral("existing"), m_store.gameWithProgram(path)},
        {QStringLiteral("choices"), QVariantList{QVariantMap{{QStringLiteral("id"), kProtonPlatform},
                                                             {QStringLiteral("name"), QStringLiteral("Proton")},
                                                             {QStringLiteral("icon"), kProtonPlatform},
                                                             {QStringLiteral("takes"), QStringLiteral(".exe")},
                                                             {QStringLiteral("runs"), true}}}},
        {QStringLiteral("proton"), true},
        {QStringLiteral("installDir"), installDir},
        {QStringLiteral("shortcut"), qint64(m_store.shortcutWithProgram(path))},
        {QStringLiteral("protonVersion"), proton},
        {QStringLiteral("launchOptions"), SteamSync::withDefaults(QString(), m_steamSettings)},
    });
    return out;
}

QString Library::linuxProgramProblem(const QString &path) const
{
    const QString name = QFileInfo(path).fileName();
    switch (linuxProgramKind(path)) {
    case LinuxProgramKind::None:
        return tr("%1 isn't a Linux program").arg(name);
    case LinuxProgramKind::SharedLibrary:
        return tr("%1 is a shared library, not a program").arg(name);
    default:
        return LibraryStore::makeExecutable(path);
    }
}

QVariantList Library::linuxPresets() const
{
    QVariantList out;
    for (const SteamPreset &p : m_steamSettings.presets) {
        if (!p.protonOnly())
            out.append(QVariantMap{{QStringLiteral("id"), p.id}, {QStringLiteral("name"), p.name},
                                   {QStringLiteral("adds"), p.describe()}});
    }
    return out;
}

QVariantMap Library::inspectLinuxProgram(const QString &path, LinuxProgramKind kind) const
{
    const bool appImage = kind == LinuxProgramKind::AppImage;
    const QString installDir = appImage ? QString() : linuxInstallDir(QDir::cleanPath(path), m_store.installLocations().home);
    const QString titleFrom = appImage ? QFileInfo(path).completeBaseName() : QFileInfo(installDir).fileName();
    const auto platform = std::find_if(m_allPlatforms.cbegin(), m_allPlatforms.cend(),
                                       [](const Platform &p) { return p.id == kLinuxPlatform; });
    const QString name = platform != m_allPlatforms.cend() ? platform->name : QStringLiteral("Linux");
    const QString icon = platform != m_allPlatforms.cend() ? platform->icon : kLinuxPlatform;
    return {
        {QStringLiteral("path"), path},
        {QStringLiteral("fileName"), QFileInfo(path).fileName()},
        {QStringLiteral("title"), RomName::parseFolder(titleFrom).title},
        {QStringLiteral("titleId"), QString()},
        {QStringLiteral("platforms"), QStringList{kLinuxPlatform}},
        {QStringLiteral("problem"), QString()},
        {QStringLiteral("existing"), m_store.gameWithProgram(path)},
        {QStringLiteral("choices"), QVariantList{QVariantMap{{QStringLiteral("id"), kLinuxPlatform},
                                                             {QStringLiteral("name"), name},
                                                             {QStringLiteral("icon"), icon},
                                                             {QStringLiteral("takes"), QString()},
                                                             {QStringLiteral("runs"), true}}}},
        {QStringLiteral("linux"), true},
        {QStringLiteral("kind"), kind == LinuxProgramKind::AppImage ? QStringLiteral("appimage")
                                 : kind == LinuxProgramKind::Script ? QStringLiteral("script")
                                                                    : QStringLiteral("elf")},
        {QStringLiteral("executable"), QFileInfo(path).isExecutable()},
        {QStringLiteral("installDir"), installDir},
        {QStringLiteral("launchOptions"), SteamSync::withDefaults(QString(), m_steamSettings, true)},
        {QStringLiteral("steamPresets"), linuxPresets()},
    };
}

QStringList Library::romNameFilters() const
{
    QStringList patterns;
    for (const Platform &p : m_allPlatforms) {
        for (const QString &id : p.emulators) {
            const EmulatorDef *e = Emulators::find(m_emulators, id);
            for (const QString &ext : e->extensions)
                patterns << QStringLiteral("*.") + ext << QStringLiteral("*.") + ext.toUpper();
            for (const QString &name : e->files)
                patterns << name << name.toUpper();
        }
    }
    const bool programs = m_store.snapshot().steam.install.found();
    if (programs)
        patterns << QStringLiteral("*.exe") << QStringLiteral("*.EXE");
    patterns.removeDuplicates();
    QStringList out;
    if (!patterns.isEmpty())
        out.append(tr("Games (%1)").arg(patterns.join(u' ')));
    if (programs)
        out.append(tr("Windows programs (*.exe *.EXE)"));
    // Linux programs often have no extension at all: "All files" finds them by their content.
    out.append(tr("Linux programs (*.sh *.x86_64 *.x86 *.AppImage *.appimage *.bin)"));
    out.append(tr("All files (*)"));
    return out;
}

QVariantMap Library::addGame(const QString &file, const QString &platform, const QString &title,
                             const QVariantMap &values, const QVariantMap &art)
{
    QVariantMap keys = values;
    const bool makeExecutable = keys.take(QStringLiteral("makeExecutable")).toBool();
    const LibraryStore::AddedGame added = platform == kProtonPlatform
        ? m_store.addProtonGame(localPath(file), title, gameValues(keys))
        : platform == kLinuxPlatform ? m_store.addLinuxGame(localPath(file), title, gameValues(keys), makeExecutable)
                                     : m_store.addGame(localPath(file), platform, title, gameValues(keys));
    QStringList errors;
    if (!added.error.isEmpty())
        errors.append(added.error);
    if (!added.id.isEmpty() && !added.existing) {
        for (auto it = art.cbegin(); it != art.cend(); ++it) {
            if (const QString error = setArt(added.id, it.key(), it->toString()); !error.isEmpty())
                errors.append(error);
        }
    }
    return {{QStringLiteral("id"), added.id},
            {QStringLiteral("error"), errors.join(u'\n')},
            {QStringLiteral("existing"), added.existing}};
}

QString Library::saveGame(const QString &id, const QVariantMap &values)
{
    QList<std::pair<QString, QVariant>> pairs = gameValues(values);
    const auto rom = std::find_if(pairs.cbegin(), pairs.cend(), [](const auto &p) { return p.first == QLatin1String("rom"); });
    const auto exe = std::find_if(pairs.cbegin(), pairs.cend(), [](const auto &p) { return p.first == QLatin1String("exe"); });
    const bool newRom = rom != pairs.cend() && rom->second.isValid();
    const bool newProgram = exe != pairs.cend() && !exe->second.toString().isEmpty()
        && m_store.snapshot().content.games.value(id).platform == kLinuxPlatform;

    // A new ROM may merge another game into this one and a new program is made executable,
    // and neither can be put back from the game file alone.
    if (newRom || newProgram) {
        m_undo.discard();
    } else if (!m_undo.pending() || m_undo.subject() != id) {
        m_undo.start(id);
        m_undo.keepCopy(gameFileOf(id));
    }
    const QString title = game(id) ? game(id)->title : id;
    const auto fail = [this](const QString &error) {
        m_undo.discard();
        return error;
    };

    // A picked ROM (a file: URL or path) goes first: it may merge another game into this one.
    if (newRom) {
        if (const QString error = m_store.relocateRom(id, localPath(rom->second.toString())); !error.isEmpty())
            return fail(error);
        pairs.erase(rom);
    }
    if (newProgram) {
        if (const QString problem = linuxProgramProblem(localPath(exe->second.toString())); !problem.isEmpty())
            return fail(problem);
    }
    if (const QString error = m_store.setGameValues(id, pairs); !error.isEmpty())
        return fail(error);
    if (newRom || newProgram)
        emit undoOffered(0, newRom ? tr("Saved. The new ROM can't be undone") : tr("Saved. The new program can't be undone"));
    else
        commitUndo(tr("Changes to %1 saved").arg(title));
    return {};
}

QString Library::gameFileOf(const QString &id)
{
    return QStringLiteral("library/games/%1.toml").arg(id);
}

void Library::setUndoDir(const QString &dir)
{
    m_undo.setFolders(m_store.dataDir(), dir);
    connect(&m_undo, &LibraryUndo::expired, this, &Library::undoExpired, Qt::UniqueConnection);
}

quint64 Library::commitUndo(const QString &text)
{
    const quint64 token = m_undo.commit();
    m_afterUndo = nullptr;
    if (token != 0)
        emit undoOffered(token, text);
    return token;
}

void Library::beginEditUndo(const QString &id)
{
    m_undo.start(id);
    m_undo.keepCopy(gameFileOf(id));
    m_undo.keepCopy(QStringLiteral("art/") + id);
}

void Library::discardEditUndo()
{
    m_undo.discard();
}

QString Library::undo(quint64 token)
{
    if (m_store.readOnly())
        return tr("Not undone: %1").arg(m_store.versionStatus().reason);
    QString error;
    const QStringList put = m_undo.restore(token, &error);
    const std::function<void()> after = std::exchange(m_afterUndo, nullptr);
    if (!put.isEmpty()) {
        m_store.reloadFiles(put);
        if (after)
            after();
    }
    return error;
}

QString Library::movePlatform(const QString &id, int step)
{
    return movePlatformWith(id, [&] { return m_store.movePlatform(id, step); });
}

QString Library::movePlatformBefore(const QString &id, const QString &beforeId)
{
    return movePlatformWith(id, [&] { return m_store.movePlatformBefore(id, beforeId); });
}

QString Library::movePlatformWith(const QString &id, const std::function<QString()> &move)
{
    QString name = id;
    for (const Platform &p : m_store.snapshot().content.platforms) {
        if (p.id == id)
            name = p.name;
    }
    m_undo.start(id);
    m_undo.keepCopy(QStringLiteral("library/platforms.toml"));
    const QString error = move();
    if (!error.isEmpty()) {
        m_undo.discard();
        return error;
    }
    commitUndo(tr("%1 moved").arg(name));
    return {};
}

QString Library::locateProgram(const QString &id, const QString &file)
{
    const GameFile game = m_store.snapshot().content.games.value(id);
    if (!runsProgram(game.platform))
        return tr("Only Proton and Linux games have a program to locate");
    const QString exe = localPath(file);
    if (!QFileInfo(exe).isFile())
        return tr("%1 isn't found").arg(exe);
    if (game.platform == kLinuxPlatform) {
        if (const QString problem = linuxProgramProblem(exe); !problem.isEmpty())
            return problem;
    }
    QList<std::pair<QString, QVariant>> values = {{QStringLiteral("exe"), m_store.writtenPath(exe)}};
    if (!game.startDir.isEmpty())
        values.append({QStringLiteral("start_dir"), m_store.writtenPath(QFileInfo(exe).absolutePath())});
    if (!game.installed)
        values.append({QStringLiteral("installed"), QVariant()});
    return m_store.setGameValues(id, values);
}

QString Library::setArt(const QString &id, const QString &kind, const QString &file)
{
    return m_store.setArt(id, kind, localPath(file));
}

QString Library::removeGame(const QString &id, bool wipe)
{
    // A Proton game's files, prefix and shortcut are Uninstall's, which checks it isn't running.
    if (wipe && m_store.snapshot().content.games.value(id).platform == kProtonPlatform)
        return tr("A Proton game is removed with Uninstall");
    // A game only its file makes has nothing to bring it back, so wiping it deletes it.
    const QString title = game(id) ? game(id)->title : id;
    const bool fileOnly = wipe && !m_store.inIndex(id);
    m_undo.start(id);
    m_undo.keepCopy(gameFileOf(id));
    if (wipe) {
        m_undo.keepMove(QStringLiteral("art/") + id);
        if (fileOnly)
            m_undo.keepMove(QStringLiteral("library/history/%1.toml").arg(id));
    }
    const auto done = [&](const QString &error) {
        if (error.isEmpty())
            commitUndo(wipe ? tr("%1 deleted").arg(title) : tr("%1 is hidden").arg(title));
        else
            m_undo.discard();
        return error;
    };
    if (fileOnly)
        return done(m_store.removeGameFile(id, true));
    QList<std::pair<QString, QVariant>> values = {{QStringLiteral("hidden"), true}};
    if (wipe) {
        for (const QString &key : GameFields::keys()) {
            if (key != QLatin1String("hidden") && key != QLatin1String("rom"))
                values.append({key, QVariant()});
        }
    }
    if (const QString error = m_store.setGameValues(id, values); !error.isEmpty())
        return done(error);
    if (wipe) {
        for (const QString &kind : kArtKinds) {
            if (const QString error = m_store.removeArt(id, kind); !error.isEmpty())
                return done(error);
        }
    }
    return done({});
}

QString Library::unhideGame(const QString &id)
{
    return m_store.setGameValues(id, {{QStringLiteral("hidden"), false}});
}

QString Library::editGames(const QStringList &ids, const QString &text,
                           const std::function<QString(const QString &)> &change)
{
    m_undo.start();
    for (const QString &id : ids)
        m_undo.keepCopy(gameFileOf(id));
    QStringList errors;
    m_store.beginBatch();
    for (const QString &id : ids) {
        if (const QString error = change(id); !error.isEmpty())
            errors.append(error);
    }
    m_store.endBatch();
    commitUndo(text);
    return errors.join(u'\n');
}

QString Library::setGamesValue(const QStringList &ids, const QString &key, const QVariant &value)
{
    const QString games = ids.size() == 1 ? tr("1 game") : tr("%1 games").arg(ids.size());
    QString text;
    if (key == QLatin1String("hidden"))
        text = value.toBool() ? tr("%1 hidden").arg(games) : tr("%1 unhidden").arg(games);
    else if (key == QLatin1String("favorite"))
        text = value.toBool() ? tr("%1 added to favorites").arg(games) : tr("%1 removed from favorites").arg(games);
    else if (key == QLatin1String("emulator"))
        text = tr("Emulator changed for %1").arg(games);
    else
        text = tr("%1 changed").arg(games);
    return editGames(ids, text, [&](const QString &id) { return m_store.setGameValues(id, {{key, value}}); });
}

QString Library::retagGames(const QStringList &ids, const QStringList &add, const QStringList &remove)
{
    const QString games = ids.size() == 1 ? tr("1 game") : tr("%1 games").arg(ids.size());
    QString text;
    if (add.size() == 1 && remove.isEmpty())
        text = tr("Tag %1 added to %2").arg(add.first(), games);
    else if (remove.size() == 1 && add.isEmpty())
        text = tr("Tag %1 removed from %2").arg(remove.first(), games);
    else
        text = tr("Tags changed on %1").arg(games);
    return editGames(ids, text, [&](const QString &id) {
        QStringList tags = m_store.snapshot().content.games.value(id).tags;
        for (const QString &tag : add) {
            if (!tags.contains(tag))
                tags.append(tag);
        }
        for (const QString &tag : remove)
            tags.removeAll(tag);
        return m_store.setGameValues(id, {{QStringLiteral("tags"), tags}});
    });
}

QString Library::protonPrefix(quint32 appId) const
{
    const QStringList &libraries = m_store.snapshot().steam.libraries;
    if (appId == 0 || libraries.isEmpty())
        return {};
    const QString prefix = libraries.first() + QStringLiteral("/compatdata/%1").arg(appId);
    return QFileInfo(prefix).isDir() ? prefix : QString();
}

Uninstall::Places Library::uninstallPlaces(const QString &id) const
{
    const LibrarySnapshot &s = m_store.snapshot();
    Uninstall::Places places;
    places.home = m_store.installLocations().home;
    places.dataDir = m_store.dataDir();
    places.steamRoot = s.steam.install.root;
    for (const QString &steamapps : s.steam.libraries)
        places.steamLibraries.append(QFileInfo(steamapps).absolutePath());
    for (const auto location : {QStandardPaths::DesktopLocation, QStandardPaths::DocumentsLocation,
                                QStandardPaths::DownloadLocation, QStandardPaths::MusicLocation,
                                QStandardPaths::PicturesLocation, QStandardPaths::MoviesLocation})
        places.userDirs.append(QStandardPaths::writableLocation(location));
    for (auto it = s.programFiles.cbegin(); it != s.programFiles.cend(); ++it) {
        if (it.key() == id)
            continue;
        const QString title = s.content.games.value(it.key()).title;
        for (const QString &path : {it->exe, it->files()}) {
            if (!path.isEmpty())
                places.otherGames.append({title.isEmpty() ? it.key() : title, path});
        }
    }
    for (auto it = s.installs.cbegin(); it != s.installs.cend(); ++it) {
        if (it->kind == EmulatorInstall::Kind::Flatpak || !QFileInfo(it->program).isAbsolute())
            continue;
        const EmulatorDef *e = Emulators::find(m_emulators, it.key());
        places.otherGames.append({e ? e->name : it.key(), it->program});
    }
    return places;
}

QVariantMap Library::uninstallChoices(const QString &id)
{
    const GameFile game = m_store.snapshot().content.games.value(id);
    if (!runsProgram(game.platform))
        return {};
    const ProgramFiles programFiles = m_store.snapshot().programFiles.value(id);
    QString installDir = programFiles.files();
    if (programFiles.singleFile ? !QFileInfo(installDir).isFile() : !QFileInfo(installDir).isDir())
        installDir.clear();
    const QString prefix = game.platform == kProtonPlatform ? protonPrefix(quint32(game.shortcut)) : QString();
    const QString refused = installDir.isEmpty() ? QString() : Uninstall::refusal(installDir, uninstallPlaces(id));

    // Added up away from the UI: a prefix alone is some 750 MB of small files.
    const QString files = refused.isEmpty() ? installDir : QString();
    QtConcurrent::run([files, prefix] {
        const auto size = [](const QString &path) {
            if (path.isEmpty())
                return QString();
            const QFileInfo info(path);
            return QLocale().formattedDataSize(info.isFile() ? info.size() : folderSize(path), 1,
                                               QLocale::DataSizeTraditionalFormat);
        };
        return std::pair(size(files), size(prefix));
    }).then(this, [this, id](const std::pair<QString, QString> &sizes) {
        emit uninstallSizes(id, sizes.first, sizes.second);
    });
    return {{QStringLiteral("installDir"), installDir.isEmpty() ? QString() : m_store.writtenPath(installDir)},
            {QStringLiteral("installDirRefused"), refused},
            {QStringLiteral("prefix"), prefix.isEmpty() ? QString() : m_store.writtenPath(prefix)}};
}

QVariantMap Library::uninstallProtonGame(const QString &id, bool files, bool prefix, bool shortcut,
                                         const QString &action, bool permanent)
{
    const auto withError = [](const QString &error) { return QVariantMap{{QStringLiteral("error"), error}}; };
    const GameFile game = m_store.snapshot().content.games.value(id);
    if (!runsProgram(game.platform))
        return withError(tr("Only Proton and Linux games are uninstalled here"));
    const bool linux_ = game.platform == kLinuxPlatform;
    if (linux_ && (prefix || shortcut))
        return withError(tr("A Linux game has no Wine prefix or Steam shortcut"));
    const bool remove = action == QLatin1String("remove");
    if (!remove && action != QLatin1String("keep") && action != QLatin1String("hide"))
        return withError(tr("Unknown action: %1").arg(action));
    if (readOnly())
        return withError(tr("The library is read-only; nothing was deleted"));

    QStringList folders;
    const Uninstall::Places places = uninstallPlaces(id);
    if (files) {
        const ProgramFiles programFiles = m_store.snapshot().programFiles.value(id);
        const QString dir = programFiles.files();
        if (const QString why = Uninstall::refusal(dir, places); !why.isEmpty())
            return withError(tr("The game's files can't be deleted: %1").arg(why));
        if (programFiles.singleFile ? !QFileInfo(dir).isFile() : !QFileInfo(dir).isDir())
            return withError(tr("%1's files are gone already").arg(game.title));
        folders.append(dir);
    }
    if (prefix) {
        const QString dir = protonPrefix(quint32(game.shortcut));
        if (dir.isEmpty())
            return withError(tr("%1 has no Wine prefix").arg(game.title));
        if (const QString why = Uninstall::refusal(dir, places); !why.isEmpty())
            return withError(tr("The Wine prefix can't be deleted: %1").arg(why));
        folders.append(dir);
    }
    // Every folder first, then the game: a folder that can't go stops the rest.
    for (const QString &folder : std::as_const(folders)) {
        const Uninstall::Removal r = Uninstall::remove(folder, permanent, m_trash);
        if (r.needsPermanent)
            return {{QStringLiteral("trashless"), m_store.writtenPath(folder)}};
        if (!r.error.isEmpty()) {
            qCWarning(lcLibrary).noquote() << QStringLiteral("%1: not uninstalled: %2").arg(id, r.error);
            return withError(tr("%1. Nothing after it was done.").arg(r.error));
        }
        qCInfo(lcLibrary).noquote() << QStringLiteral("%1: removed %2").arg(id, folder);
    }
    // A kept-uninstalled game's shortcut has left already; its Proton setting goes with it.
    // For a Linux game: gone once its files are.
    const bool fromSteam = shortcut || files || !game.installed;
    if (remove)
        return withError(m_store.removeProtonGame(id, fromSteam));
    // Kept: not installed once its shortcut goes (a prefix alone is a reset), hidden or not.
    QList<std::pair<QString, QVariant>> values;
    if (fromSteam && game.installed)
        values.append({QStringLiteral("installed"), false});
    if (const bool hide = action == QLatin1String("hide"); hide != game.hidden)
        values.append({QStringLiteral("hidden"), hide ? QVariant(true) : QVariant()});
    return withError(values.isEmpty() ? QString() : m_store.setGameValues(id, values));
}

int Library::nextTagColor(int paletteSize) const
{
    QSet<int> used;
    for (const TagDef &t : m_store.snapshot().content.tags)
        used.insert(t.color);
    for (int i = 0; i < paletteSize; ++i) {
        if (!used.contains(i))
            return i;
    }
    return 0;
}

QList<std::pair<QString, QStringList>> Library::tagMappings() const
{
    QList<std::pair<QString, QStringList>> out;
    for (const TagDef &t : m_store.snapshot().content.tags)
        out.append({t.name, LibraryStore::igdbOf(t)});
    return out;
}

QStringList Library::igdbTags(const QStringList &facets, const QStringList &has) const
{
    return Igdb::fittingTags(facets, tagMappings(), has);
}
