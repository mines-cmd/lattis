#include "library.h"
#include "romdbfetch.h"
#include "apppaths.h"
#include "dateformat.h"
#include "emulatorscreenshots.h"
#include "gameid.h"
#include "heroicpresets.h"
#include "heroicsaves.h"
#include "heroicwines.h"
#include "igdb.h"
#include "launchcommand.h"
#include "libraryoverview.h"
#include "managericon.h"
#include "mediafavorites.h"
#include "mediafolders.h"
#include "romfoldermatch.h"
#include "romscan.h"
#include "savesbackup.h"
#include "searchtext.h"
#include "steamart.h"
#include "steamachievements.h"
#include "steamcollections.h"
#include "steammedia.h"
#include "tomlpatch.h"
#include "videothumbnails.h"

#include <QDir>
#include <QFile>
#include <QImageReader>
#include <QFileInfo>
#include <QGuiApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJSEngine>
#include <QLocale>
#include <QPointer>
#include <QSaveFile>
#include <QStandardPaths>
#include <QStorageInfo>
#include <QUrl>
#include <QMutex>
#include <QtConcurrent>

#include <algorithm>
#include <cmath>
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

QString hoursMinutesText(int minutes)
{
    if (minutes <= 0)
        return {};
    const int hours = minutes / 60;
    const int rest = minutes % 60;
    if (hours == 0)
        return Library::tr("%1m").arg(rest);
    if (rest == 0)
        return Library::tr("%1h").arg(QLocale().toString(hours));
    return Library::tr("%1h %2m").arg(QLocale().toString(hours)).arg(rest);
}

// -1 when Steam gave no size, or while nothing is downloading.
int updatePercent(const Game &g)
{
    if (g.update != GameUpdate::Downloading && g.update != GameUpdate::Paused)
        return -1;
    return g.updatePercent;
}

QString updateStateName(GameUpdate update)
{
    switch (update) {
    case GameUpdate::Waiting: return QStringLiteral("waiting");
    case GameUpdate::Downloading: return QStringLiteral("downloading");
    case GameUpdate::Paused: return QStringLiteral("paused");
    case GameUpdate::Queued: return QStringLiteral("queued");
    default: return QStringLiteral("none");
    }
}

QString updateText(const Game &g)
{
    const int percent = updatePercent(g);
    if (g.platform == kEpicPlatform) {
        switch (g.update) {
        case GameUpdate::Waiting: return Library::tr("Available");
        case GameUpdate::Downloading:
            if (g.verifying)
                return percent < 0 ? Library::tr("Verifying") : Library::tr("Verifying · %1%").arg(percent);
            if (!g.installed)
                return percent < 0 ? Library::tr("Installing") : Library::tr("Installing · %1%").arg(percent);
            return percent < 0 ? Library::tr("Updating") : Library::tr("Updating · %1%").arg(percent);
        case GameUpdate::Queued: return Library::tr("Queued");
        default: return {};
        }
    }
    switch (g.update) {
    case GameUpdate::Waiting:
        if (g.updateBytes <= 0)
            return Library::tr("Waiting");
        return Library::tr("Waiting · %1")
            .arg(QLocale().formattedDataSize(g.updateBytes, 1, QLocale::DataSizeTraditionalFormat));
    case GameUpdate::Downloading:
        return percent < 0 ? Library::tr("Downloading") : Library::tr("Downloading · %1%").arg(percent);
    case GameUpdate::Paused:
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

QString daysAgoText(qint64 days)
{
    return days == 1 ? Library::tr("1 day ago") : Library::tr("%1 days ago").arg(days);
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
        return daysAgoText(days);
    return DateFormat::shortDate(date);
}

QString playedAgoText(const QDateTime &time)
{
    if (!time.isValid())
        return {};
    const qint64 seconds = qMax<qint64>(0, time.secsTo(QDateTime::currentDateTimeUtc()));
    if (seconds < 60)
        return Library::tr("just now");
    if (seconds < 3600)
        return Library::tr("%n min ago", nullptr, int(seconds / 60));
    if (seconds < 86400)
        return Library::tr("%n h ago", nullptr, int(seconds / 3600));
    const qint64 days = qMax<qint64>(0, time.toLocalTime().date().daysTo(QDate::currentDate()));
    if (days < 7)
        return lastPlayedText(time);
    if (days < 30)
        return (days < 14 ? Library::tr("1 week ago") : Library::tr("%1 weeks ago").arg(days / 7));
    if (days < 365)
        return (days < 60 ? Library::tr("1 month ago") : Library::tr("%1 months ago").arg(days / 30));
    return (days < 730 ? Library::tr("1 year ago") : Library::tr("%1 years ago").arg(days / 365));
}

QString playedWhenText(const QDateTime &time)
{
    if (!time.isValid())
        return {};
    const qint64 days = time.toLocalTime().date().daysTo(QDate::currentDate());
    if (days <= 0)
        return Library::tr("Played today");
    if (days == 1)
        return Library::tr("Played yesterday");
    return Library::tr("Played %1").arg(playedAgoText(time));
}

QString waitingText(const QDate &added)
{
    const qint64 days = qMax<qint64>(0, added.daysTo(QDate::currentDate()));
    if (days < 60)
        return Library::tr("%n day(s)", nullptr, int(days));
    if (days < 730)
        return Library::tr("%n month(s)", nullptr, int(days / 30));
    return Library::tr("%n year(s)", nullptr, int(days / 365));
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

QString playTimeName(const QString &bucket)
{
    if (bucket == QLatin1String("never"))
        return Library::tr("Never");
    if (bucket == QLatin1String("under_2h"))
        return Library::tr("Under 2 h");
    if (bucket == QLatin1String("2_10h"))
        return Library::tr("2–10 h");
    if (bucket == QLatin1String("10_50h"))
        return Library::tr("10–50 h");
    if (bucket == QLatin1String("50_100h"))
        return Library::tr("50–100 h");
    return Library::tr("Over 100 h");
}

QString achievementStateName(const QString &state)
{
    if (state == QLatin1String("not_started"))
        return Library::tr("Not started");
    if (state == QLatin1String("in_progress"))
        return Library::tr("In progress");
    if (state == QLatin1String("completed"))
        return Library::tr("Completed");
    return Library::tr("None to earn");
}

// The narrowest of the Last played filter's periods, so a game is in one group.
QString lastPlayedGroup(const QDateTime &played, const QDateTime &now)
{
    if (!played.isValid())
        return QStringLiteral("never");
    for (const char *period : {"week", "month", "year"}) {
        if (playedWithin(QLatin1String(period), played, now))
            return QLatin1String(period);
    }
    return QStringLiteral("older");
}

QString ratingGroup(double rating)
{
    if (rating < 0)
        return QStringLiteral("unrated");
    if (rating >= 5)
        return QStringLiteral("5");
    if (rating >= 4)
        return QStringLiteral("4");
    if (rating >= 3)
        return QStringLiteral("3");
    return QStringLiteral("under_3");
}

QString groupHeaderName(const QString &grouping, const QString &group)
{
    if (grouping == QLatin1String("year"))
        return group == QLatin1String("none") ? Library::tr("No year") : group;
    if (grouping == QLatin1String("rating")) {
        if (group == QLatin1String("5"))
            return QStringLiteral("5");
        if (group == QLatin1String("4"))
            return Library::tr("4 – 4.5");
        if (group == QLatin1String("3"))
            return Library::tr("3 – 3.5");
        return group == QLatin1String("under_3") ? Library::tr("Under 3") : Library::tr("Unrated");
    }
    if (group == QLatin1String("week"))
        return Library::tr("This week");
    if (group == QLatin1String("month"))
        return Library::tr("This month");
    if (group == QLatin1String("year"))
        return Library::tr("This year");
    return group == QLatin1String("older") ? Library::tr("Older") : Library::tr("Never");
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

static BuiltLibrary buildFrom(const LibrarySnapshot &snapshot)
{
    return buildLibrary(snapshot.content, snapshot.art, snapshot.romFiles, Emulators::presets(), snapshot.romGameFiles,
                        snapshot.steam, snapshot.steamQueued, snapshot.programFiles, snapshot.heroic);
}

LibraryPreload Library::preload(const QString &dataDir, const QString &cacheDir)
{
    LibraryPreload out;
    out.cache = readCache(dataDir, cacheDir);
    out.built = buildFrom(out.cache.snapshot.value_or(LibrarySnapshot()));
    out.heroFiles = HeroCache::list(cacheDir);
    return out;
}

Library::Library(const QString &dataDir, const QString &cacheDir, NewLibrary newLibrary,
                 std::optional<LibraryPreload> preload, QObject *parent)
    : QAbstractListModel(parent)
    , m_store(dataDir, cacheDir, newLibrary, preload ? std::optional(std::move(preload->cache)) : std::nullopt)
    , m_cacheDir(cacheDir)
    , m_heroFiles(preload ? std::move(preload->heroFiles) : HeroCache::list(cacheDir))
{
    s_instance = this;
    m_heroicPendingTimer.setInterval(2000);
    connect(&m_heroicPendingTimer, &QTimer::timeout, this, &Library::applyHeroicPending);
    connect(&m_heroCopy, &QFutureWatcher<HeroCache::Result>::finished, this, &Library::heroCopyFinished);
    connect(&m_store, &LibraryStore::changed, this, &Library::reloadFromStore);
    connect(&m_store, &LibraryStore::artShrunk, this, &Library::makeNextHeroCopy);
    // changed() comes while the store still counts as loading or scanning, which holds a shrink back.
    connect(&m_store, &LibraryStore::loadingChanged, this, &Library::makeNextHeroCopy);
    connect(&m_store, &LibraryStore::scanningChanged, this, &Library::makeNextHeroCopy);
    connect(&m_store, &LibraryStore::historyChanged, this, [this](const QStringList &ids) {
        QStringList withCopies = ids;
        for (const QString &id : ids) {
            if (const Game *g = gameIncludingHidden(id))
                withCopies += g->linked;
        }
        withCopies.removeDuplicates();
        emit historyChanged(withCopies);
    });
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
    m_yearFrom = list.value.yearFrom;
    m_yearTo = list.value.yearTo;
    m_minRating = list.value.minRating;
    m_playTimeFilter = list.value.playTimes;
    m_lastPlayedFilter = list.value.lastPlayed;
    m_achievementFilter = list.value.achievements;
    m_moreOpen = list.value.moreOpen;
    m_showHidden = list.value.showHidden;
    m_showSortValue = list.value.showSortValue;
    m_groupBy = list.value.groupBy;
    if (preload)
        reloadFromBuilt(std::move(preload->built));
    else
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
    ListSettings s = workingSettings();
    if (m_held) {
        if (m_viewOverridden) {
            s.sort = m_held->list.sort;
            s.descending = m_held->list.descending;
            s.groupBy = m_held->list.groupBy;
        }
        s.installedOnly = m_held->list.installedOnly;
        s.favoritesOnly = m_held->list.favoritesOnly;
        s.tags = m_held->list.tags;
        s.matchAllTags = m_held->list.matchAllTags;
        s.statuses = m_held->list.statuses;
        s.yearFrom = m_held->list.yearFrom;
        s.yearTo = m_held->list.yearTo;
        s.minRating = m_held->list.minRating;
        s.playTimes = m_held->list.playTimes;
        s.lastPlayed = m_held->list.lastPlayed;
        s.achievements = m_held->list.achievements;
        s.showHidden = m_held->list.showHidden;
    }
    return s;
}

ListSettings Library::workingSettings() const
{
    ListSettings s;
    s.sort = m_sort;
    s.descending = m_sortDescending;
    s.installedOnly = m_installedOnly;
    s.favoritesOnly = m_favoritesOnly;
    s.tags = m_tagFilter;
    s.matchAllTags = m_tagMatchAll;
    s.statuses = m_statusFilter;
    s.yearFrom = m_yearFrom;
    s.yearTo = m_yearTo;
    s.minRating = m_minRating;
    s.playTimes = m_playTimeFilter;
    s.lastPlayed = m_lastPlayedFilter;
    s.achievements = m_achievementFilter;
    s.moreOpen = m_moreOpen;
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
    if (const Parsed<HeroicSettings> heroic = parseHeroicSettings(bytes, QStringLiteral("settings.toml")); heroic.ok) {
        m_heroicSettings = heroic.value;
        updateHeroic();
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

void Library::applyListSettings(const ListSettings &incoming)
{
    ListSettings s = incoming;
    const ListSettings now = workingSettings();
    if (m_held) {
        // The file holds the normal view: what changed there lands in the held copy, never in the open collection.
        if (m_viewOverridden) {
            m_held->list.sort = s.sort;
            m_held->list.descending = s.descending;
            m_held->list.groupBy = s.groupBy;
            s.sort = now.sort;
            s.descending = now.descending;
            s.groupBy = now.groupBy;
        }
        m_held->list.installedOnly = s.installedOnly;
        m_held->list.favoritesOnly = s.favoritesOnly;
        m_held->list.tags = s.tags;
        m_held->list.matchAllTags = s.matchAllTags;
        m_held->list.statuses = s.statuses;
        m_held->list.yearFrom = s.yearFrom;
        m_held->list.yearTo = s.yearTo;
        m_held->list.minRating = s.minRating;
        m_held->list.playTimes = s.playTimes;
        m_held->list.lastPlayed = s.lastPlayed;
        m_held->list.achievements = s.achievements;
        m_held->list.showHidden = s.showHidden;
        s.installedOnly = now.installedOnly;
        s.favoritesOnly = now.favoritesOnly;
        s.tags = now.tags;
        s.matchAllTags = now.matchAllTags;
        s.statuses = now.statuses;
        s.yearFrom = now.yearFrom;
        s.yearTo = now.yearTo;
        s.minRating = now.minRating;
        s.playTimes = now.playTimes;
        s.lastPlayed = now.lastPlayed;
        s.achievements = now.achievements;
        s.showHidden = now.showHidden;
    }
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
    m_yearFrom = s.yearFrom;
    m_yearTo = s.yearTo;
    m_minRating = s.minRating;
    m_playTimeFilter = s.playTimes;
    m_lastPlayedFilter = s.lastPlayed;
    m_achievementFilter = s.achievements;
    m_showHidden = s.showHidden;
    if (s.moreOpen != m_moreOpen) {
        m_moreOpen = s.moreOpen;
        emit moreOpenChanged();
    }
    if (achievementsWanted())
        readAchievements();
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
        return a.installed && a.update != GameUpdate::None;
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
        {QStringLiteral("familyGames"),
         int(std::count_if(steamGames.cbegin(), steamGames.cend(),
                           [](const Game *g) { return g->steamOwnership == u"family"; }))},
        {QStringLiteral("ownedGames"),
         int(std::count_if(steamGames.cbegin(), steamGames.cend(),
                           [](const Game *g) { return g->steamOwnership == u"owned"; }))},
        {QStringLiteral("wholeLibrary"), m_steamSettings.wholeLibrary},
        {QStringLiteral("friendsActivity"), m_steamSettings.friendsActivity},
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
        QString changes;
    };
    const auto describe = [](const QStringList &keys) {
        QStringList names;
        for (const QString &key : keys) {
            if (key == SteamSync::kShortcutIcon)
                continue;
            names.append(key == u"launch_options" ? tr("launch options")
                         : key == u"proton"       ? tr("Proton version")
                         : key == u"title"        ? tr("name")
                         : key == u"exe"          ? tr("program")
                         : key == u"start_dir"    ? tr("start folder")
                         : key == SteamSync::kAddShortcut ? tr("addition to Steam")
                         : key == SteamSync::kRemoveShortcut ? tr("removal from Steam")
                                                  : QString(key).replace(u'_', u' '));
        }
        names.removeDuplicates();
        return names.join(QStringLiteral(", "));
    };
    QList<Entry> entries;
    const QHash<QString, QStringList> &queued = m_store.snapshot().steamQueued;
    for (auto it = queued.cbegin(); it != queued.cend(); ++it) {
        if (it.value().isEmpty() || it.value() == QStringList{SteamSync::kShortcutIcon})
            continue;
        if (const quint32 appId = SteamSync::removedAppId(it.key()); appId != 0) {
            const QString title = m_store.removedShortcutTitle(appId);
            entries.append({it.key(), tr("%1 (removed)").arg(title.isEmpty() ? QString::number(appId) : title), false,
                            describe(it.value())});
            continue;
        }
        if (it.key().startsWith(ModManagers::kKeyPrefix)) {
            const QString managerId = it.key().mid(ModManagers::kKeyPrefix.size());
            for (const QVariant &v : std::as_const(m_modManagers)) {
                const QVariantMap manager = v.toMap();
                if (manager.value(QStringLiteral("id")).toString() == managerId) {
                    entries.append({it.key(), manager.value(QStringLiteral("name")).toString(),
                                    it.value().contains(SteamSync::kAddShortcut), describe(it.value())});
                    break;
                }
            }
            continue;
        }
        const auto g = std::find_if(m_games.cbegin(), m_games.cend(), [&](const Game &g) { return g.id == it.key(); });
        if (g != m_games.cend())
            entries.append({g->id, g->title, it.value().contains(SteamSync::kAddShortcut), describe(it.value())});
    }
    std::sort(entries.begin(), entries.end(),
              [](const Entry &a, const Entry &b) { return a.title.compare(b.title, Qt::CaseInsensitive) < 0; });
    QVariantList out;
    for (const Entry &e : std::as_const(entries))
        out.append(QVariantMap{{QStringLiteral("id"), e.id}, {QStringLiteral("title"), e.title},
                               {QStringLiteral("adds"), e.adds}, {QStringLiteral("changes"), e.changes}});
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
            || (g.update != GameUpdate::Downloading && g.update != GameUpdate::Paused))
            continue;
        // installDir is <library>/steamapps/common/<name>.
        apps.append({appId, QDir::cleanPath(g.installDir + QStringLiteral("/../../downloading/%1").arg(appId))});
    }
    const QString root = m_store.snapshot().steam.install.root;
    m_downloads.setApps(apps.isEmpty() ? QString() : root + QStringLiteral("/logs/content_log.txt"), apps);
    applyDownloads();
    applyEpicJobs();
    updateDownloadBar();
}

void Library::setEpicJobs(const QVariantList &jobs)
{
    QHash<QString, EpicJob> next;
    for (const QVariant &v : jobs) {
        const QVariantMap job = v.toMap();
        const QString state = job.value(QStringLiteral("state")).toString();
        const QString kind = job.value(QStringLiteral("kind")).toString();
        if ((kind != QLatin1String("update") && kind != QLatin1String("install") && kind != QLatin1String("repair"))
            || (state != QLatin1String("running") && state != QLatin1String("queued")))
            continue;
        next.insert(job.value(QStringLiteral("appName")).toString(),
                    {state == QLatin1String("running"), int(job.value(QStringLiteral("percent")).toDouble()),
                     kind == QLatin1String("repair")});
    }
    if (next == m_epicJobs)
        return;
    m_epicJobs = next;
    const QSet<QString> before = m_epicOverlaid;
    applyEpicJobs();
    updateDownloadBar();
    // With Installed only on, a game that isn't installed joins the list while its install runs, and leaves
    // when that stops: the rows change, not only their data.
    bool listChanged = false;
    if (m_installedOnly) {
        for (const Game &g : std::as_const(m_games)) {
            if (!g.installed && before.contains(g.id) != m_epicOverlaid.contains(g.id))
                listChanged = true;
        }
    }
    if (listChanged)
        rebuild();
    else if (!m_rows.isEmpty())
        emit dataChanged(index(0), index(int(m_rows.size()) - 1));
}

// A job's state lives here and not in the scan, which only knows the build differs; when a job leaves, its
// game goes back to Waiting, and the scan corrects that if the update finished.
bool Library::applyEpicJobs()
{
    if (m_epicJobs.isEmpty() && m_epicOverlaid.isEmpty())
        return false;
    QHash<QString, EpicJob> byGame;
    for (const HeroicEntry &entry : m_store.snapshot().content.heroic) {
        if (const auto it = m_epicJobs.constFind(entry.appName); it != m_epicJobs.cend())
            byGame.insert(entry.id, it.value());
    }
    bool changed = false;
    QSet<QString> overlaid;
    for (Game &g : m_games) {
        if (g.platform != kEpicPlatform)
            continue;
        const auto it = byGame.constFind(g.id);
        if (it == byGame.cend()) {
            if (m_epicOverlaid.contains(g.id) && (g.update == GameUpdate::Downloading || g.update == GameUpdate::Queued)) {
                g.update = g.installed ? m_epicOriginal.value(g.id, GameUpdate::Waiting) : GameUpdate::None;
                g.updatePercent = -1;
                g.verifying = false;
                changed = true;
            }
            continue;
        }
        overlaid.insert(g.id);
        if (!m_epicOverlaid.contains(g.id))
            m_epicOriginal.insert(g.id, g.update);
        const GameUpdate update = it->running ? GameUpdate::Downloading : GameUpdate::Queued;
        const int percent = it->running ? it->percent : -1;
        changed = changed || update != g.update || percent != g.updatePercent || it->verify != g.verifying;
        g.update = update;
        g.updatePercent = percent;
        g.verifying = it->verify;
    }
    m_epicOverlaid = overlaid;
    m_epicOriginal.removeIf([&](auto it) { return !overlaid.contains(it.key()); });
    return changed;
}

bool Library::applyDownloads()
{
    const QHash<qint64, SteamDownloads::Progress> progress = m_downloads.progress();
    bool changed = false;
    for (Game &g : m_games) {
        const auto it = progress.constFind(GameId::steamAppId(g.id));
        if (g.platform != kSteamPlatform || it == progress.cend()
            || (g.update != GameUpdate::Downloading && g.update != GameUpdate::Paused))
            continue;
        const GameUpdate update = it->running ? GameUpdate::Downloading : GameUpdate::Paused;
        const int percent = it->percent >= 0 ? it->percent : g.updatePercent;
        changed = changed || update != g.update || percent != g.updatePercent;
        g.update = update;
        g.updatePercent = percent;
    }
    return changed;
}

void Library::updateDownloadBar()
{
    const Game *shown = nullptr;
    const Game *epic = nullptr;
    for (const Game &g : std::as_const(m_games)) {
        if (g.platform == kEpicPlatform) {
            if (g.update == GameUpdate::Downloading && !epic)
                epic = &g;
            continue;
        }
        if (g.update == GameUpdate::Downloading) {
            shown = &g;
            break;
        }
        if (g.update == GameUpdate::Paused && !shown)
            shown = &g;
    }
    // Steam's download comes first: Lattis can't control it, while the Legendary job it can cancel.
    const bool epicShown = !shown && epic;
    if (epicShown)
        shown = epic;
    QVariantMap download;
    if (shown) {
        download = {{QStringLiteral("gameId"), shown->id},
                    {QStringLiteral("name"), shown->title},
                    {QStringLiteral("icon"), shown->iconUrl},
                    {QStringLiteral("state"), updateStateName(shown->update)},
                    {QStringLiteral("percent"), updatePercent(*shown)},
                    {QStringLiteral("installing"), !shown->installed},
                    {QStringLiteral("verifying"), shown->verifying},
                    {QStringLiteral("epic"), epicShown},
                    {QStringLiteral("appName"), epicShown ? epicAppName(shown->id) : QString()}};
    }
    if (download == m_download)
        return;
    m_download = std::move(download);
    emit downloadChanged();
}

void Library::updateHeroicWaiting()
{
    const auto describe = [](const HeroicGameEdits &e) {
        QStringList names;
        if (e.wine)
            names << tr("Wine version");
        if (e.launcherArgs)
            names << tr("launch options");
        if (e.showMangohud || e.useGameMode || e.environment || e.wrappers)
            names << tr("presets");
        return names.join(QStringLiteral(", "));
    };
    QVariantList out;
    if (!m_heroicPending.isEmpty()) {
        for (const Game &g : std::as_const(m_games)) {
            if (g.launcher() != QLatin1String("heroic"))
                continue;
            const auto it = m_heroicPending.constFind(epicAppName(g.id));
            if (it != m_heroicPending.cend())
                out.append(QVariantMap{{QStringLiteral("id"), g.id}, {QStringLiteral("title"), g.title},
                                       {QStringLiteral("changes"), describe(it.value())}});
        }
        std::sort(out.begin(), out.end(), [](const QVariant &a, const QVariant &b) {
            return a.toMap().value(QStringLiteral("title")).toString().compare(
                       b.toMap().value(QStringLiteral("title")).toString(), Qt::CaseInsensitive) < 0;
        });
    }
    if (out == m_heroicWaiting)
        return;
    m_heroicWaiting = std::move(out);
    emit heroicWaitingChanged();
}

void Library::updateSteam()
{
    updateHeroicWaiting();
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
    } else if (key == u"whole_library") {
        if (!value.toBool())
            formatted = QStringLiteral("false");
    } else if (key == u"friends_activity") {
        if (!value.toBool())
            formatted = QStringLiteral("false");
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

    return saveSettingsValue(QStringLiteral("steam"), key, formatted);
}

QVariantMap Library::heroic() const
{
    if (m_heroic.isEmpty())
        m_heroic = buildHeroic();
    return m_heroic;
}

QVariantMap Library::buildHeroic() const
{
    const HeroicInstall &install = m_store.snapshot().heroic.install;
    const QString home = QDir::homePath();
    const QString shownRoot = install.root == home || install.root.startsWith(home + u'/')
        ? u'~' + install.root.sliced(home.size()) : install.root;
    const int games = int(std::count_if(m_games.cbegin(), m_games.cend(),
                                         [](const Game &g) { return g.platform == kEpicPlatform; }));
    const int installed = int(std::count_if(m_games.cbegin(), m_games.cend(), [](const Game &g) {
        return g.platform == kEpicPlatform && g.installed;
    }));
    // The merge's own list: with installed.json unreadable, the cache says what's installed.
    const QList<HeroicGame> scanned = m_store.snapshot().heroicScan.games();
    const int scanInstalled = int(std::count_if(scanned.cbegin(), scanned.cend(),
                                                [](const HeroicGame &g) { return g.installed; }));

    QString status;
    bool problem = false;
    if (!m_heroicSettings.enabled) {
        status = tr("Off: Epic games aren't imported, and those in the library show as not installed.");
    } else if (install.pathMissing) {
        status = tr("There's no Heroic in %1: the folder needs its Epic library or installed games.").arg(m_heroicSettings.path);
        problem = true;
    } else if (!install.found()) {
        status = tr("Heroic wasn't found in ~/.config/heroic or the Flatpak's folder. If it's elsewhere, choose its folder.");
        problem = !m_heroicSettings.path.isEmpty();
    } else {
        status = install.kind == HeroicInstall::Kind::Flatpak
            ? tr("Heroic (Flatpak) found at %1, %n Epic game(s) in the library", "", games).arg(shownRoot)
            : tr("Heroic found at %1, %n Epic game(s) in the library", "", games).arg(shownRoot);
        status += tr(", %n installed.", "", installed);
        if (!install.program.found()) {
            status += u' ' + tr("Its program wasn't found, so these games can't be launched.");
            problem = true;
        }
    }

    return {
        {QStringLiteral("enabled"), m_heroicSettings.enabled},
        {QStringLiteral("path"), m_heroicSettings.path},
        {QStringLiteral("wholeLibrary"), m_heroicSettings.wholeLibrary},
        {QStringLiteral("checkUpdates"), m_heroicSettings.checkUpdates},
        {QStringLiteral("found"), m_heroicSettings.enabled && install.found()},
        {QStringLiteral("programFound"), install.program.found()},
        {QStringLiteral("flatpak"), install.kind == HeroicInstall::Kind::Flatpak},
        {QStringLiteral("root"), install.root},
        {QStringLiteral("shownRoot"), shownRoot},
        {QStringLiteral("games"), games},
        {QStringLiteral("installed"), installed},
        {QStringLiteral("scanInstalled"), scanInstalled},
        {QStringLiteral("scanOwned"), int(scanned.size())},
        {QStringLiteral("status"), status},
        {QStringLiteral("problem"), problem},
    };
}

void Library::updateHeroic()
{
    if (m_heroic.isEmpty())
        return;
    QVariantMap heroic = buildHeroic();
    if (heroic == m_heroic)
        return;
    m_heroic = std::move(heroic);
    emit heroicChanged();
}

QString Library::setHeroicValue(const QString &key, const QVariant &value)
{
    QString formatted;
    if (key == u"enabled" || key == u"whole_library" || key == u"check_updates") {
        if (!value.toBool())
            formatted = QStringLiteral("false");
    } else if (key == u"path" || key == u"epic_install_folder" || key == u"epic_install_wine") {
        if (const QString path = value.toString().trimmed(); !path.isEmpty())
            formatted = tomlString(path);
    } else {
        return tr("[heroic] has no setting %1").arg(key);
    }
    return saveSettingsValue(QStringLiteral("heroic"), key, formatted);
}

QString Library::saveSettingsValue(const QString &section, const QString &key, const QString &formatted)
{
    if (!ensureSettingsFile())
        return tr("Couldn't create %1").arg(settingsFile());
    QString error;
    const bool ok = formatted.isEmpty()
        ? TomlPatch::removeValueInFile(settingsFile(), section, key, &error)
        : TomlPatch::setValueInFile(settingsFile(), section, key, formatted, &error);
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
    for (const ModManager &m : snapshot.content.modManagers) {
        if (m.shortcut > 0)
            inLibrary.insert(m.shortcut);
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
        if (inLibrary.contains(s.appId) || SteamShortcuts::isHeroic(s))
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
            {QStringLiteral("exe"), s.exe},
            {QStringLiteral("target"), s.exeArgs.isEmpty() ? tilde(s.exe) : tilde(s.exe) + u' ' + s.exeArgs},
            {QStringLiteral("tool"), tool.isEmpty() ? QString() : toolNames.value(tool, tool)},
            {QStringLiteral("checked"), !tool.isEmpty()},
            {QStringLiteral("canManage"), s.exe.endsWith(QLatin1String(".exe"), Qt::CaseInsensitive)},
            {QStringLiteral("art"), art.isEmpty() ? QString() : QUrl::fromLocalFile(art).toString()},
        });
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

QString Library::addSteamShortcutAsModManager(qint64 appId, const QStringList &games)
{
    return m_store.addShortcutAsModManager(quint32(appId), games).error;
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
        if (!g.hidden || m_showHidden)
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
    reloadFromBuilt(buildFrom(m_store.snapshot()));
}

void Library::reloadFromBuilt(BuiltLibrary built)
{
    const LibrarySnapshot &snapshot = m_store.snapshot();

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
    updateAchievementStates();
    updateOrder();
    if (achievementsWanted())
        readAchievements();
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
    updateCollections();

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
    QVariantList modManagers = buildModManagers();
    if (modManagers != m_modManagers) {
        m_modManagers = std::move(modManagers);
        emit modManagersChanged();
    }
    QVariantList platformSettings = buildPlatformSettings();
    if (platformSettings != m_platformSettings) {
        m_platformSettings = std::move(platformSettings);
        emit platformSettingsChanged();
    }
    updateSteam();
    updateHeroic();
    applyHeroicPending();

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
        if (m_groupBy != QLatin1String("platform"))
            return groupHeaderName(m_groupBy, row.group);
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
        return game ? game->update != GameUpdate::None : false;
    case CoverUrlRole:
        return game ? game->coverUrl : QUrl();
    case FavoriteRole:
        return game ? game->favorite : false;
    case PlatformIconRole: {
        const Platform *p = game ? platform(game->platform) : nullptr;
        return p ? p->icon : QString();
    }
    case HiddenRole:
        return game ? game->hidden : false;
    case SortValueRole:
        if (!game)
            return QString();
        if (m_sort == QLatin1String("play_time"))
            return shortPlayTimeText(game->groupPlayMinutes);
        if (m_sort == QLatin1String("last_played") && game->groupLastPlayed.isValid())
            return lastPlayedText(game->groupLastPlayed);
        if (m_sort == QLatin1String("rating") && game->rating >= 0)
            return QLocale().toString(game->rating, 'f', 1);
        if (m_sort == QLatin1String("year") && game->year > 0)
            return QString::number(game->year);
        if (m_sort == QLatin1String("added") && game->added.isValid())
            return DateFormat::shortDate(game->added);
        if (m_sort == QLatin1String("size") && game->sizeBytes > 0)
            return sizeText(game->sizeBytes);
        if (m_sort == QLatin1String("achievements")) {
            const auto it = m_achievementOf.constFind(game->id);
            if (it != m_achievementOf.cend() && it->total > 0)
                return QStringLiteral("%1/%2").arg(it->unlocked).arg(it->total);
        }
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
        {CoverUrlRole, "coverUrl"},
        {FavoriteRole, "favorite"},
        {PlatformIconRole, "platformIcon"},
    };
}

void Library::setPlatformFilter(const QString &platform)
{
    if (m_platformFilter == platform)
        return;
    m_platformFilter = platform;
    emit platformFilterChanged();
    rebuild();
    m_viewStateTimer.start();
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

void Library::setFlat(bool flat)
{
    if (m_flat == flat)
        return;
    m_flat = flat;
    emit flatChanged();
    rebuild();
}

const QStringList &Library::sortOrders()
{
    static const QStringList orders = {QStringLiteral("title"),  QStringLiteral("last_played"),
                                       QStringLiteral("play_time"), QStringLiteral("rating"),
                                       QStringLiteral("year"),   QStringLiteral("achievements"),
                                       QStringLiteral("added"),  QStringLiteral("size"),
                                       QStringLiteral("platform")};
    return orders;
}

void Library::setSort(const QString &sort)
{
    if (m_sort == sort || !sortOrders().contains(sort))
        return;
    m_sort = sort;
    m_sortDescending = sort != QLatin1String("title") && sort != QLatin1String("platform");
    emit sortChanged();
    if (achievementsWanted())
        readAchievements();
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
    QHash<QString, int> platformPlace;
    if (m_sort == QLatin1String("platform")) {
        for (const Platform &p : std::as_const(m_allPlatforms))
            platformPlace.insert(p.id, int(platformPlace.size()));
    }
    // Games with no value are -1; a platform the rail doesn't know goes after those it does.
    QList<qint64> value(m_games.size());
    for (int i = 0; i < m_games.size(); ++i) {
        const Game &g = m_games.at(i);
        if (m_sort == QLatin1String("play_time")) {
            value[i] = g.groupPlayMinutes > 0 ? g.groupPlayMinutes : -1;
        } else if (m_sort == QLatin1String("rating")) {
            value[i] = g.rating >= 0 ? qint64(std::round(g.rating * 2)) : -1;
        } else if (m_sort == QLatin1String("year")) {
            value[i] = g.year > 0 ? g.year : -1;
        } else if (m_sort == QLatin1String("added")) {
            value[i] = g.added.isValid() ? g.added.toJulianDay() : -1;
        } else if (m_sort == QLatin1String("size")) {
            value[i] = g.sizeBytes > 0 ? g.sizeBytes : -1;
        } else if (m_sort == QLatin1String("achievements")) {
            const auto it = m_achievementOf.constFind(g.id);
            value[i] = it != m_achievementOf.cend() && it->total > 0 ? it->unlocked * 1000000LL / it->total : -1;
        } else if (m_sort == QLatin1String("platform")) {
            value[i] = platformPlace.value(g.platform, int(platformPlace.size()));
        } else {
            value[i] = g.groupLastPlayed.isValid() ? g.groupLastPlayed.toMSecsSinceEpoch() : -1;
        }
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

void Library::setYearFrom(int year)
{
    year = qBound(0, year, 9999);
    if (m_yearFrom == year)
        return;
    m_yearFrom = year;
    filtersEdited();
}

void Library::setYearTo(int year)
{
    year = qBound(0, year, 9999);
    if (m_yearTo == year)
        return;
    m_yearTo = year;
    filtersEdited();
}

void Library::setMinRating(double rating)
{
    rating = rating < 0 ? -1 : std::round(qMin(rating, 5.0) * 2) / 2;
    if (m_minRating == rating)
        return;
    m_minRating = rating;
    filtersEdited();
}

void Library::setPlayTimeFilter(const QStringList &times)
{
    QStringList known;
    for (const QString &time : times) {
        if (listPlayTimes().contains(time) && !known.contains(time))
            known.append(time);
    }
    if (m_playTimeFilter == known)
        return;
    m_playTimeFilter = known;
    filtersEdited();
}

void Library::setLastPlayedFilter(const QString &period)
{
    if ((!period.isEmpty() && !listLastPlayed().contains(period)) || m_lastPlayedFilter == period)
        return;
    m_lastPlayedFilter = period;
    filtersEdited();
}

void Library::setAchievementFilter(const QStringList &states)
{
    QStringList known;
    for (const QString &state : states) {
        if (listAchievementStates().contains(state) && !known.contains(state))
            known.append(state);
    }
    if (m_achievementFilter == known)
        return;
    m_achievementFilter = known;
    if (achievementsWanted())
        readAchievements();
    filtersEdited();
}

void Library::setMoreOpen(bool open)
{
    if (m_moreOpen == open)
        return;
    m_moreOpen = open;
    emit moreOpenChanged();
    saveListSettings();
}

QVariantList Library::playTimeChoices() const
{
    QHash<QString, int> counts;
    for (const Game &g : m_games) {
        if (!g.hidden)
            ++counts[playTimeBucket(g.groupPlayMinutes)];
    }
    QVariantList out;
    for (const QString &time : listPlayTimes()) {
        out.append(QVariantMap{{QStringLiteral("id"), time}, {QStringLiteral("name"), playTimeName(time)},
                               {QStringLiteral("games"), counts.value(time)}});
    }
    return out;
}

QVariantList Library::achievementChoices() const
{
    QHash<QString, int> counts;
    for (const Game &g : m_games) {
        if (!g.hidden)
            ++counts[achievementStateOf(g)];
    }
    QVariantList out;
    for (const QString &state : listAchievementStates()) {
        out.append(QVariantMap{{QStringLiteral("id"), state}, {QStringLiteral("name"), achievementStateName(state)},
                               {QStringLiteral("games"), counts.value(state)}});
    }
    return out;
}

QVariantList Library::yearHistogram() const
{
    QMap<int, int> counts;
    for (const Game &g : m_games) {
        if (!g.hidden && g.year > 0)
            ++counts[g.year];
    }
    QVariantList out;
    if (counts.isEmpty())
        return out;
    for (int year = counts.firstKey(); year <= counts.lastKey(); ++year)
        out.append(QVariantMap{{QStringLiteral("year"), year}, {QStringLiteral("games"), counts.value(year)}});
    return out;
}

// A rating counts in the half star at or below it, which is how the filter reads it: at least 3.5 takes 3.5 up.
QVariantList Library::ratingHistogram() const
{
    QList<int> counts(11, 0);
    for (const Game &g : m_games) {
        if (!g.hidden && g.rating >= 0)
            ++counts[qBound(0, int(std::floor(g.rating * 2)), 10)];
    }
    QVariantList out;
    for (int step = 0; step <= 10; ++step)
        out.append(QVariantMap{{QStringLiteral("rating"), step / 2.0}, {QStringLiteral("games"), counts.at(step)}});
    return out;
}

int Library::gamesWithoutYear() const
{
    return int(std::count_if(m_games.cbegin(), m_games.cend(), [](const Game &g) { return !g.hidden && g.year <= 0; }));
}

int Library::gamesWithoutRating() const
{
    return int(std::count_if(m_games.cbegin(), m_games.cend(), [](const Game &g) { return !g.hidden && g.rating < 0; }));
}

void Library::showFiltered(const QString &platform, const QStringList &statuses, const QStringList &tags)
{
    leaveCollection(false);
    clearFilters();
    setInstalledOnly(false);
    setSearch({});
    setStatusFilter(statuses);
    setTagFilter(tags);
    setPlatformFilter(platform);
}

void Library::setSteamFilter(const QStringList &kinds)
{
    QStringList known;
    for (const QString &kind : {QStringLiteral("owned"), QStringLiteral("family")}) {
        if (kinds.contains(kind))
            known.append(kind);
    }
    if (m_steamFilter == known)
        return;
    m_steamFilter = known;
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
    // Installed off is how a collection starts, so there it's Installed on that counts as a filter.
    return m_installedOnly == bool(m_held) || m_favoritesOnly || m_showHidden || !effectiveTags().isEmpty()
        || !m_statusFilter.isEmpty() || !m_steamFilter.isEmpty() || m_yearFrom > 0 || m_yearTo > 0 || m_minRating >= 0
        || !m_playTimeFilter.isEmpty() || !m_lastPlayedFilter.isEmpty() || !m_achievementFilter.isEmpty();
}

void Library::clearFilters()
{
    const bool baseInstalled = !m_held;
    if (m_installedOnly == baseInstalled && !m_favoritesOnly && m_tagFilter.isEmpty() && !m_tagMatchAll
        && m_statusFilter.isEmpty() && m_steamFilter.isEmpty() && !m_showHidden && m_yearFrom == 0 && m_yearTo == 0
        && m_minRating < 0 && m_playTimeFilter.isEmpty() && m_lastPlayedFilter.isEmpty() && m_achievementFilter.isEmpty())
        return;
    const bool platformsDiffer = m_showHidden;
    m_installedOnly = baseInstalled;
    m_favoritesOnly = false;
    m_tagFilter.clear();
    m_tagMatchAll = false;
    m_statusFilter.clear();
    m_steamFilter.clear();
    m_yearFrom = 0;
    m_yearTo = 0;
    m_minRating = -1;
    m_playTimeFilter.clear();
    m_lastPlayedFilter.clear();
    m_achievementFilter.clear();
    m_showHidden = false;
    if (platformsDiffer && updatePlatforms())
        emit platformsChanged();
    filtersEdited();
}

QVariantMap Library::collection() const
{
    for (const QVariant &c : m_collections) {
        const QVariantMap map = c.toMap();
        if (map.value(QStringLiteral("id")).toString() == m_collectionId)
            return map;
    }
    return {};
}

void Library::setTagsOpen(bool open)
{
    if (m_tagsOpen == open)
        return;
    m_tagsOpen = open;
    emit tagsOpenChanged();
    m_viewStateTimer.start();
}

void Library::updateCollections()
{
    const QList<Collection> &all = m_store.collections();
    QList<std::pair<QString, QVariantMap>> entries;
    entries.reserve(all.size());
    for (const Collection &c : all) {
        const CollectionMatcher matcher = matcherOf(c);
        const int count = int(std::count_if(m_games.cbegin(), m_games.cend(),
                                            [&](const Game &g) { return !g.hidden && matcher.isMember(g); }));
        entries.append({c.name, QVariantMap{{QStringLiteral("id"), c.id}, {QStringLiteral("name"), c.name},
                                            {QStringLiteral("count"), count}}});
    }
    const QCollator collator = titleCollator();
    std::stable_sort(entries.begin(), entries.end(),
                     [&](const auto &a, const auto &b) { return collator.compare(a.first, b.first) < 0; });
    QVariantList list;
    for (const auto &entry : std::as_const(entries))
        list.append(entry.second);

    if (!m_collectionId.isEmpty()) {
        const auto open = std::find_if(all.cbegin(), all.cend(), [&](const Collection &c) { return c.id == m_collectionId; });
        if (open == all.cend())
            leaveCollection(false); // deleted while open; the caller builds the rows
        else {
            m_matcher.emplace(matcherOf(*open));
            applyCollectionView(*open); // Undo and a hand edit of the file change the view too
        }
    }
    if (!m_restoreCollection.isEmpty()) {
        if (std::any_of(all.cbegin(), all.cend(), [&](const Collection &c) { return c.id == m_restoreCollection; }))
            enterCollection(std::exchange(m_restoreCollection, QString()), false);
        else if (!m_store.loading())
            m_restoreCollection.clear();
    }

    if (list != m_collections) {
        m_collections = std::move(list);
        emit collectionsChanged();
        if (!m_collectionId.isEmpty())
            emit collectionChanged();
    }
}

void Library::openCollection(const QString &id)
{
    enterCollection(id, true);
}

void Library::closeCollection()
{
    leaveCollection(true);
}

bool Library::enterCollection(const QString &id, bool rebuildNow)
{
    if (id == m_collectionId)
        return false;
    const QList<Collection> &all = m_store.collections();
    const auto found = std::find_if(all.cbegin(), all.cend(), [&](const Collection &c) { return c.id == id; });
    if (found == all.cend())
        return false;

    if (!m_held)
        m_held = HeldFilters{workingSettings(), m_steamFilter, m_platformFilter};
    const bool platformsDiffer = m_showHidden;
    m_installedOnly = false;
    m_favoritesOnly = false;
    m_tagFilter.clear();
    m_tagMatchAll = false;
    m_statusFilter.clear();
    m_steamFilter.clear();
    m_yearFrom = 0;
    m_yearTo = 0;
    m_minRating = -1;
    m_playTimeFilter.clear();
    m_lastPlayedFilter.clear();
    m_achievementFilter.clear();
    m_showHidden = false;
    const bool platformChanged = !m_platformFilter.isEmpty();
    m_platformFilter.clear();
    m_collectionId = id;
    m_matcher.emplace(matcherOf(*found));
    applyCollectionView(*found);
    if (platformsDiffer && updatePlatforms())
        emit platformsChanged();
    if (platformChanged)
        emit platformFilterChanged();
    emit collectionChanged();
    emit filtersChanged();
    if (rebuildNow)
        rebuild();
    m_viewStateTimer.start();
    return true;
}

void Library::applyCollectionView(const Collection &collection)
{
    const bool overrides = !collection.sort.isEmpty() || !collection.groupBy.isEmpty();
    if (!overrides && !m_viewOverridden)
        return;
    // Moving between collections starts from the list's own view, not the previous collection's.
    const ListSettings &base = m_viewOverridden ? m_held->list : workingSettings();
    const QString sort = collection.sort.isEmpty() ? base.sort : collection.sort;
    const bool descending = collection.sort.isEmpty() ? base.descending : collection.sortDescending;
    const QString groupBy = collection.groupBy.isEmpty() ? base.groupBy : collection.groupBy;
    if (overrides && !m_viewOverridden) {
        m_held->list.sort = m_sort;
        m_held->list.descending = m_sortDescending;
        m_held->list.groupBy = m_groupBy;
    }
    m_viewOverridden = overrides;
    applyView(sort, descending, groupBy);
}

void Library::applyView(const QString &sort, bool descending, const QString &groupBy)
{
    const bool sortDiffers = sort != m_sort || descending != m_sortDescending;
    const bool groupDiffers = groupBy != m_groupBy;
    m_sort = sort;
    m_sortDescending = descending;
    m_groupBy = groupBy;
    if (sortDiffers) {
        if (achievementsWanted())
            readAchievements();
        updateOrder();
        emit sortChanged();
    }
    if (groupDiffers)
        emit groupByChanged();
}

void Library::leaveCollection(bool rebuildNow)
{
    if (!m_held)
        return;
    const HeldFilters held = *m_held;
    if (m_viewOverridden)
        applyView(held.list.sort, held.list.descending, held.list.groupBy);
    m_viewOverridden = false;
    m_held.reset();
    m_matcher.reset();
    m_collectionId.clear();
    const bool platformsDiffer = m_showHidden != held.list.showHidden;
    m_installedOnly = held.list.installedOnly;
    m_favoritesOnly = held.list.favoritesOnly;
    m_tagFilter = held.list.tags;
    m_tagMatchAll = held.list.matchAllTags;
    m_statusFilter = held.list.statuses;
    m_steamFilter = held.steam;
    m_yearFrom = held.list.yearFrom;
    m_yearTo = held.list.yearTo;
    m_minRating = held.list.minRating;
    m_playTimeFilter = held.list.playTimes;
    m_lastPlayedFilter = held.list.lastPlayed;
    m_achievementFilter = held.list.achievements;
    m_showHidden = held.list.showHidden;
    if (platformsDiffer && updatePlatforms())
        emit platformsChanged();
    const QString platform = std::any_of(m_platforms.cbegin(), m_platforms.cend(),
                                         [&](const Platform &p) { return p.id == held.platform; })
        ? held.platform : QString();
    const bool platformChanged = platform != m_platformFilter;
    m_platformFilter = platform;
    if (platformChanged)
        emit platformFilterChanged();
    emit collectionChanged();
    emit filtersChanged();
    if (rebuildNow)
        rebuild();
    m_viewStateTimer.start();
}

QVariantMap Library::collectionStates(const QStringList &ids) const
{
    QVariantMap out;
    if (ids.isEmpty())
        return out;
    const QSet<QString> wanted(ids.cbegin(), ids.cend());
    for (const Collection &c : m_store.collections()) {
        const CollectionMatcher matcher = matcherOf(c);
        int in = 0;
        int total = 0;
        for (const Game &g : m_games) {
            if (!wanted.contains(g.id))
                continue;
            ++total;
            in += matcher.isMember(g) ? 1 : 0;
        }
        if (in > 0)
            out.insert(c.id, in == total ? 1 : 2);
    }
    return out;
}

QString Library::setCollectionMembers(const QStringList &ids, const QString &collectionId, bool member)
{
    const QList<Collection> &all = m_store.collections();
    const auto found = std::find_if(all.cbegin(), all.cend(), [&](const Collection &c) { return c.id == collectionId; });
    if (found == all.cend())
        return tr("That collection no longer exists");
    Collection c = *found;

    int changed = 0;
    for (const QString &id : ids) {
        const auto game = std::find_if(m_games.cbegin(), m_games.cend(), [&](const Game &g) { return g.id == id; });
        if (game == m_games.cend())
            continue;
        // The copies of one game are members together, so the lists drop every one of them.
        QStringList group = game->linked;
        group.append(game->id);
        if (!game->groupId.isEmpty())
            group.append(game->groupId);
        const auto drop = [&](QStringList &list) {
            const qsizetype before = list.size();
            list.removeIf([&](const QString &x) { return group.contains(x); });
            return list.size() != before;
        };
        const auto inNow = [&] { return matcherOf(c).isMember(*game); };
        const bool was = inNow();
        if (member) {
            drop(c.removed);
            if (!inNow())
                c.added.append(game->id);
        } else {
            drop(c.added);
            if (inNow())
                c.removed.append(game->id);
        }
        changed += was != member ? 1 : 0;
    }
    if (c == *found)
        return {};

    const QString games = changed == 1 ? tr("1 game") : tr("%1 games").arg(changed);
    const QString text = member ? tr("%1 added to %2").arg(games, c.name) : tr("%1 removed from %2").arg(games, c.name);
    m_undo.start();
    m_undo.keepCopy(QStringLiteral("library/collections.toml"));
    const QString error = m_store.setCollection(c);
    commitUndo(text);
    return error;
}

QString Library::addCollection(const QString &name, const QStringList &ids)
{
    return makeCollection(name, ids, std::nullopt);
}

namespace {
QString freeCollectionId(const QString &name, const QSet<QString> &taken)
{
    const QString base = GameId::slug(name).isEmpty() ? QStringLiteral("collection") : GameId::slug(name);
    QString id = base;
    for (int n = 2; taken.contains(id); ++n)
        id = QStringLiteral("%1-%2").arg(base).arg(n);
    return id;
}
}

QString Library::makeCollection(const QString &name, const QStringList &ids, const std::optional<CollectionRules> &rules)
{
    const QString trimmed = name.trimmed();
    if (trimmed.isEmpty())
        return tr("A collection needs a name");
    QSet<QString> taken;
    for (const Collection &c : m_store.collections()) {
        if (c.name.compare(trimmed, Qt::CaseInsensitive) == 0)
            return tr("There is already a collection called %1").arg(c.name);
        taken.insert(c.id);
    }
    Collection c;
    c.name = trimmed;
    c.rules = rules;
    c.id = freeCollectionId(trimmed, taken);
    for (const QString &id : ids) {
        if (!c.added.contains(id))
            c.added.append(id);
    }

    m_undo.start();
    m_undo.keepCopy(QStringLiteral("library/collections.toml"));
    const QString error = m_store.setCollection(c);
    const QString games = c.added.size() == 1 ? tr("1 game") : tr("%1 games").arg(c.added.size());
    commitUndo(rules                ? tr("Collection %1 saved from the filters").arg(trimmed)
               : c.added.isEmpty() ? tr("Collection %1 made").arg(trimmed)
                                    : tr("Collection %1 made with %2").arg(trimmed, games));
    return error;
}

QString Library::addCollectionFromFilters(const QString &name)
{
    if (m_held)
        return tr("Leave the open collection first");
    CollectionRules rules;
    rules.platform = m_platformFilter;
    rules.installedOnly = m_installedOnly;
    rules.favoritesOnly = m_favoritesOnly;
    rules.statuses = m_statusFilter;
    rules.tags = effectiveTags();
    rules.matchAllTags = m_tagMatchAll && rules.tags.size() >= 2;
    rules.steamOwnership = m_steamFilter;
    rules.yearFrom = m_yearFrom;
    rules.yearTo = m_yearTo;
    rules.minRating = m_minRating;
    rules.playTimes = m_playTimeFilter;
    rules.lastPlayed = m_lastPlayedFilter;
    rules.achievements = m_achievementFilter;
    if (rules == CollectionRules{})
        return tr("There are no filters to save");
    return makeCollection(name, {}, rules);
}

namespace {
// The dialog's rules map (see collectionDetails); no rules when "enabled" is false.
std::optional<CollectionRules> rulesFromVariant(const QVariantMap &rules)
{
    if (!rules.value(QStringLiteral("enabled")).toBool())
        return std::nullopt;
    CollectionRules r;
    r.platform = rules.value(QStringLiteral("platform")).toString();
    r.installedOnly = rules.value(QStringLiteral("installedOnly")).toBool();
    r.favoritesOnly = rules.value(QStringLiteral("favoritesOnly")).toBool();
    r.statuses = rules.value(QStringLiteral("statuses")).toStringList();
    r.tags = rules.value(QStringLiteral("tags")).toStringList();
    r.matchAllTags = rules.value(QStringLiteral("matchAllTags")).toBool();
    r.steamOwnership = rules.value(QStringLiteral("steam")).toStringList();
    r.yearFrom = rules.value(QStringLiteral("yearFrom")).toInt();
    r.yearTo = rules.value(QStringLiteral("yearTo")).toInt();
    r.minRating = rules.value(QStringLiteral("minRating"), -1).toDouble();
    r.playTimes = rules.value(QStringLiteral("playTimes")).toStringList();
    r.lastPlayed = rules.value(QStringLiteral("lastPlayed")).toString();
    r.achievements = rules.value(QStringLiteral("achievements")).toStringList();
    return r;
}

// A game moved to the other list leaves this one, so it is never in both.
void applyExceptions(Collection &c, const QStringList &dropAdded, const QStringList &dropRemoved,
                     const QStringList &moreAdded, const QStringList &moreRemoved)
{
    c.added.removeIf([&](const QString &x) { return dropAdded.contains(x) || moreRemoved.contains(x); });
    c.removed.removeIf([&](const QString &x) { return dropRemoved.contains(x) || moreAdded.contains(x); });
    for (const QString &id : moreAdded) {
        if (!c.added.contains(id))
            c.added.append(id);
    }
    for (const QString &id : moreRemoved) {
        if (!c.removed.contains(id))
            c.removed.append(id);
    }
}
} // namespace

QVariantList Library::findGames(const QString &search, int limit) const
{
    const QStringList words = SearchText::words(search);
    QList<const Game *> found;
    for (const Game &g : m_games) {
        if (!g.hidden && SearchText::matches(SearchText::fold(g.title), words))
            found.append(&g);
    }
    const QCollator collator = titleCollator();
    std::sort(found.begin(), found.end(),
              [&](const Game *a, const Game *b) { return collator.compare(a->title, b->title) < 0; });
    QVariantList out;
    for (const Game *g : std::as_const(found)) {
        if (out.size() >= limit)
            break;
        out.append(QVariantMap{{QStringLiteral("id"), g->id}, {QStringLiteral("title"), g->title},
                               {QStringLiteral("platformName"), g->platformName}});
    }
    return out;
}

QVariantMap Library::collectionDetails(const QString &id) const
{
    const QList<Collection> &all = m_store.collections();
    const auto found = std::find_if(all.cbegin(), all.cend(), [&](const Collection &c) { return c.id == id; });
    if (found == all.cend())
        return {};
    const auto titled = [&](const QStringList &ids) {
        QVariantList out;
        for (const QString &gameId : ids) {
            const auto game = std::find_if(m_games.cbegin(), m_games.cend(), [&](const Game &g) { return g.id == gameId; });
            out.append(QVariantMap{{QStringLiteral("id"), gameId},
                                   {QStringLiteral("title"), game == m_games.cend() ? gameId : game->title}});
        }
        return out;
    };
    const CollectionRules rules = found->rules.value_or(CollectionRules{});
    QStringList deletedTags;
    for (const QString &tag : rules.tags) {
        if (!m_tagNames.contains(tag))
            deletedTags.append(tag);
    }
    return QVariantMap{{QStringLiteral("name"), found->name},
                       {QStringLiteral("enabled"), found->rules.has_value()},
                       {QStringLiteral("platform"), rules.platform},
                       {QStringLiteral("installedOnly"), rules.installedOnly},
                       {QStringLiteral("favoritesOnly"), rules.favoritesOnly},
                       {QStringLiteral("statuses"), rules.statuses},
                       {QStringLiteral("tags"), rules.tags},
                       {QStringLiteral("deletedTags"), deletedTags},
                       {QStringLiteral("matchAllTags"), rules.matchAllTags},
                       {QStringLiteral("steam"), rules.steamOwnership},
                       {QStringLiteral("yearFrom"), rules.yearFrom},
                       {QStringLiteral("yearTo"), rules.yearTo},
                       {QStringLiteral("minRating"), rules.minRating},
                       {QStringLiteral("playTimes"), rules.playTimes},
                       {QStringLiteral("lastPlayed"), rules.lastPlayed},
                       {QStringLiteral("achievements"), rules.achievements},
                       {QStringLiteral("sort"), found->sort},
                       {QStringLiteral("sortDescending"), found->sortDescending},
                       {QStringLiteral("groupBy"), found->groupBy},
                       {QStringLiteral("added"), titled(found->added)},
                       {QStringLiteral("removed"), titled(found->removed)}};
}

int Library::countCollectionMatches(const QString &id, const QVariantMap &rules, const QStringList &dropAdded,
                                    const QStringList &dropRemoved, const QStringList &moreAdded,
                                    const QStringList &moreRemoved) const
{
    const QList<Collection> &all = m_store.collections();
    const auto found = std::find_if(all.cbegin(), all.cend(), [&](const Collection &c) { return c.id == id; });
    if (found == all.cend())
        return 0;
    Collection c = *found;
    c.rules = rulesFromVariant(rules);
    applyExceptions(c, dropAdded, dropRemoved, moreAdded, moreRemoved);
    const CollectionMatcher matcher = matcherOf(c);
    return int(std::count_if(m_games.cbegin(), m_games.cend(), [&](const Game &g) { return !g.hidden && matcher.isMember(g); }));
}

QString Library::editCollection(const QString &id, const QVariantMap &rules, const QStringList &dropAdded,
                                const QStringList &dropRemoved, const QStringList &moreAdded,
                                const QStringList &moreRemoved)
{
    const QList<Collection> &all = m_store.collections();
    const auto found = std::find_if(all.cbegin(), all.cend(), [&](const Collection &c) { return c.id == id; });
    if (found == all.cend())
        return tr("That collection no longer exists");
    Collection c = *found;
    c.rules = rulesFromVariant(rules);
    const QString sort = rules.value(QStringLiteral("sort")).toString();
    const QString groupBy = rules.value(QStringLiteral("groupBy")).toString();
    c.sort = listSorts().contains(sort) ? sort : QString();
    c.sortDescending = !c.sort.isEmpty() && rules.value(QStringLiteral("sortDescending")).toBool();
    c.groupBy = listGroupings().contains(groupBy) ? groupBy : QString();
    applyExceptions(c, dropAdded, dropRemoved, moreAdded, moreRemoved);
    if (c == *found)
        return {};

    m_undo.start();
    m_undo.keepCopy(QStringLiteral("library/collections.toml"));
    const QString error = m_store.setCollection(c);
    commitUndo(tr("Collection %1 changed").arg(c.name));
    if (error.isEmpty() && id == m_collectionId) {
        applyCollectionView(c);
        rebuild();
    }
    return error;
}

QString Library::renameTagInCollections(const QString &from, const QString &to)
{
    QString firstError;
    const QList<Collection> all = m_store.collections();
    for (Collection c : all) {
        if (!c.rules || !c.rules->tags.contains(from))
            continue;
        const qsizetype at = c.rules->tags.indexOf(from);
        if (c.rules->tags.contains(to))
            c.rules->tags.removeAt(at);
        else
            c.rules->tags.replace(at, to);
        const QString error = m_store.setCollection(c);
        if (firstError.isEmpty())
            firstError = error;
    }
    return firstError;
}

QString Library::renameCollection(const QString &id, const QString &name)
{
    const QString trimmed = name.trimmed();
    if (trimmed.isEmpty())
        return tr("A collection needs a name");
    const QList<Collection> &all = m_store.collections();
    const auto found = std::find_if(all.cbegin(), all.cend(), [&](const Collection &c) { return c.id == id; });
    if (found == all.cend())
        return tr("That collection no longer exists");
    if (found->name == trimmed)
        return {};
    for (const Collection &c : all) {
        if (c.id != id && c.name.compare(trimmed, Qt::CaseInsensitive) == 0)
            return tr("There is already a collection called %1").arg(c.name);
    }
    Collection c = *found;
    c.name = trimmed;
    m_undo.start();
    m_undo.keepCopy(QStringLiteral("library/collections.toml"));
    const QString error = m_store.setCollection(c);
    commitUndo(tr("Collection renamed to %1").arg(trimmed));
    return error;
}

QString Library::deleteCollection(const QString &id)
{
    const QList<Collection> &all = m_store.collections();
    const auto found = std::find_if(all.cbegin(), all.cend(), [&](const Collection &c) { return c.id == id; });
    if (found == all.cend())
        return {};
    const QString name = found->name;
    m_undo.start();
    m_undo.keepCopy(QStringLiteral("library/collections.toml"));
    const QString error = m_store.removeCollection(id);
    commitUndo(tr("Collection %1 deleted").arg(name));
    return error;
}

namespace {
// Steam's app ids and a game's id: a Steam game is steam-<app id>, a non-Steam shortcut (which Steam
// lists by its 32-bit app id, signed or not) is the game whose shortcut that is.
struct SteamAppLookup
{
    QSet<QString> ids;
    QHash<quint32, QString> shortcuts;

    explicit SteamAppLookup(const QList<Game> &games)
    {
        for (const Game &g : games) {
            ids.insert(g.id);
            if (g.shortcut != 0)
                shortcuts.insert(g.shortcut, g.id);
        }
    }

    // The id a collection stores, empty when Steam's id can't name a game; `inLibrary` says whether the game is here.
    QString resolve(qint64 appId, bool *inLibrary) const
    {
        if (appId > 0 && appId < (qint64(1) << 31)) {
            const QString id = GameId::forSteamApp(appId);
            *inLibrary = ids.contains(id);
            return id;
        }
        const auto found = shortcuts.constFind(quint32(appId));
        *inLibrary = found != shortcuts.cend();
        return *inLibrary ? found.value() : QString();
    }
};

bool onlyDigits(const QString &text)
{
    return !text.isEmpty() && std::all_of(text.cbegin(), text.cend(), [](QChar c) { return c.isDigit() && c.unicode() < 128; });
}
}

QVariantList Library::steamCollectionAccounts() const
{
    const SteamInstall &install = m_store.snapshot().steam.install;
    if (!m_steamSettings.enabled || !install.found())
        return {};
    QStringList ids;
    if (!install.userId.isEmpty())
        ids.append(install.userId);
    for (const SteamUser &user : install.users) {
        if (!ids.contains(user.accountId))
            ids.append(user.accountId);
    }
    QVariantList out;
    for (const QString &id : std::as_const(ids)) {
        if (!onlyDigits(id) || !QFileInfo::exists(steamCollectionsPath(install.root, id)))
            continue;
        QString name = id;
        for (const SteamUser &user : install.users) {
            if (user.accountId == id)
                name = !user.personaName.isEmpty() ? user.personaName : !user.accountName.isEmpty() ? user.accountName : id;
        }
        out.append(QVariantMap{{QStringLiteral("id"), id}, {QStringLiteral("name"), name}});
    }
    return out;
}

QVariantMap Library::readSteamCollections(const QString &accountId) const
{
    const SteamInstall &install = m_store.snapshot().steam.install;
    const auto failed = [](const QString &text) { return QVariantMap{{QStringLiteral("error"), text}}; };
    if (!m_steamSettings.enabled || !install.found() || !onlyDigits(accountId))
        return failed(tr("Steam wasn't found"));
    QFile file(steamCollectionsPath(install.root, accountId));
    if (!file.open(QIODevice::ReadOnly))
        return failed(tr("Steam has no collections for this account"));
    const SteamCollections parsed = parseSteamCollections(file.readAll());
    if (!parsed.ok)
        return failed(tr("Steam's collections couldn't be read"));

    QSet<QString> imported;
    for (const Collection &c : m_store.collections()) {
        if (!c.steamId.isEmpty())
            imported.insert(c.steamId);
    }
    const SteamAppLookup lookup(m_games);
    QVariantList list;
    for (const SteamCollection &c : parsed.collections) {
        int missing = 0;
        for (const qint64 appId : c.appIds) {
            bool here = false;
            lookup.resolve(appId, &here);
            missing += here ? 0 : 1;
        }
        list.append(QVariantMap{{QStringLiteral("steamId"), c.steamId},
                                {QStringLiteral("name"), c.name},
                                {QStringLiteral("games"), int(c.appIds.size())},
                                {QStringLiteral("missing"), missing},
                                {QStringLiteral("imported"), imported.contains(c.steamId)}});
    }
    return QVariantMap{{QStringLiteral("error"), QString()},
                       {QStringLiteral("collections"), list},
                       {QStringLiteral("dynamic"), parsed.dynamic},
                       {QStringLiteral("unreadable"), int(parsed.problems.size())}};
}

QVariantMap Library::importSteamCollections(const QString &accountId, const QStringList &steamIds)
{
    const SteamInstall &install = m_store.snapshot().steam.install;
    const auto failed = [](const QString &text) { return QVariantMap{{QStringLiteral("error"), text}}; };
    if (!m_steamSettings.enabled || !install.found() || !onlyDigits(accountId))
        return failed(tr("Steam wasn't found"));
    QFile file(steamCollectionsPath(install.root, accountId));
    if (!file.open(QIODevice::ReadOnly))
        return failed(tr("Steam has no collections for this account"));
    const SteamCollections parsed = parseSteamCollections(file.readAll());
    if (!parsed.ok)
        return failed(tr("Steam's collections couldn't be read"));

    const SteamAppLookup lookup(m_games);
    const QList<Collection> before = m_store.collections();
    QSet<QString> takenIds;
    QSet<QString> takenNames;
    QHash<QString, Collection> byStamp;
    for (const Collection &c : before) {
        takenIds.insert(c.id);
        takenNames.insert(c.name.toLower());
        if (!c.steamId.isEmpty())
            byStamp.insert(c.steamId, c);
    }

    QVariantList added;
    QVariantList updated;
    QSet<qint64> missingApps;
    QList<Collection> writes;
    int unchanged = 0;
    for (const SteamCollection &steam : parsed.collections) {
        if (!steamIds.contains(steam.steamId))
            continue;
        QStringList games;
        int missing = 0;
        for (const qint64 appId : steam.appIds) {
            bool here = false;
            const QString id = lookup.resolve(appId, &here);
            if (!here) {
                ++missing;
                missingApps.insert(appId);
            }
            if (!id.isEmpty() && !games.contains(id))
                games.append(id);
        }
        const auto row = [&](const QString &name) {
            return QVariantMap{{QStringLiteral("name"), name}, {QStringLiteral("games"), int(steam.appIds.size())},
                               {QStringLiteral("missing"), missing}};
        };

        const auto old = byStamp.constFind(steam.steamId);
        if (old != byStamp.cend()) {
            Collection c = old.value();
            c.added = games;
            if (c == old.value()) {
                ++unchanged;
                continue;
            }
            writes.append(c);
            updated.append(row(c.name));
            continue;
        }
        // Steam's name may already be one of the user's: the import never touches those.
        QString name = steam.name;
        for (int n = 1; takenNames.contains(name.toLower()); ++n)
            name = n == 1 ? tr("%1 (Steam)").arg(steam.name) : tr("%1 (Steam %2)").arg(steam.name).arg(n);
        takenNames.insert(name.toLower());
        Collection c;
        c.name = name;
        c.id = freeCollectionId(name, takenIds);
        takenIds.insert(c.id);
        c.steamId = steam.steamId;
        c.added = games;
        writes.append(c);
        added.append(row(name));
    }

    QString error;
    if (!writes.isEmpty()) {
        m_undo.start();
        m_undo.keepCopy(QStringLiteral("library/collections.toml"));
        for (const Collection &c : std::as_const(writes)) {
            error = m_store.setCollection(c);
            if (!error.isEmpty())
                break;
        }
        commitUndo(writes.size() == 1 ? tr("1 collection imported from Steam")
                                      : tr("%1 collections imported from Steam").arg(writes.size()));
    }

    QList<qint64> missingList(missingApps.cbegin(), missingApps.cend());
    std::sort(missingList.begin(), missingList.end());
    QStringList missingNames;
    for (const qint64 appId : std::as_const(missingList))
        missingNames.append(QString::number(appId));
    return QVariantMap{{QStringLiteral("error"), error},
                       {QStringLiteral("added"), added},
                       {QStringLiteral("updated"), updated},
                       {QStringLiteral("unchanged"), unchanged},
                       {QStringLiteral("dynamic"), parsed.dynamic},
                       {QStringLiteral("unreadable"), int(parsed.problems.size())},
                       {QStringLiteral("missingApps"), missingNames}};
}

void Library::filtersEdited()
{
    emit filtersChanged();
    rebuild();
    saveListSettings();
}

QString Library::renameTag(const QString &from, const QString &to)
{
    const QString target = to.trimmed();
    QStringList games;
    for (auto it = m_store.snapshot().content.games.cbegin(); it != m_store.snapshot().content.games.cend(); ++it) {
        if (it->tags.contains(from))
            games.append(it.key());
    }
    m_undo.start();
    m_undo.keepCopy(QStringLiteral("library/tags.toml"));
    m_undo.keepCopy(QStringLiteral("library/collections.toml"));
    for (const QString &id : std::as_const(games))
        m_undo.keepCopy(gameFileOf(id));

    QString error = m_store.renameTag(from, to);
    const bool renamed = error.isEmpty() && from != target;
    // The rename stands even when a collection can't be written: its rule then names a tag that is gone,
    // which matches nothing and says so, and the error tells the user why.
    if (renamed)
        error = renameTagInCollections(from, target);
    const bool undoable = commitUndo(tr("Tag %1 renamed to %2").arg(from, target)) != 0;

    // The filters follow the tag, and follow it back when Undo renames it again.
    const auto follow = [this](const QString &oldName, const QString &newName) {
        const auto rename = [&](QStringList &tags) {
            const qsizetype at = tags.indexOf(oldName);
            if (at < 0)
                return false;
            if (tags.contains(newName))
                tags.removeAt(at);
            else
                tags[at] = newName;
            return true;
        };
        if (m_held && rename(m_held->list.tags))
            saveListSettings();
        if (rename(m_tagFilter)) {
            m_tagNames.insert(newName);
            filtersEdited();
        }
    };
    if (renamed) {
        follow(from, target);
        if (undoable)
            m_afterUndo = [follow, from, target] { follow(target, from); };
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
    if (error.isEmpty() && m_held && m_held->list.tags.removeAll(name) > 0)
        saveListSettings();
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
    if (m_matcher && !m_matcher->isMember(g))
        return false;
    if (m_favoritesOnly && !g.favorite)
        return false;
    if (!m_statusFilter.isEmpty() && !m_statusFilter.contains(statusOf(g)))
        return false;
    if (!m_steamFilter.isEmpty() && !m_steamFilter.contains(g.steamOwnership))
        return false;
    if ((m_yearFrom > 0 || m_yearTo > 0)
        && (g.year <= 0 || (m_yearFrom > 0 && g.year < m_yearFrom) || (m_yearTo > 0 && g.year > m_yearTo)))
        return false;
    if (m_minRating >= 0 && (g.rating < 0 || g.rating < m_minRating))
        return false;
    if (!m_playTimeFilter.isEmpty() && !m_playTimeFilter.contains(playTimeBucket(g.groupPlayMinutes)))
        return false;
    if (!m_lastPlayedFilter.isEmpty()
        && !playedWithin(m_lastPlayedFilter, g.groupLastPlayed, QDateTime::currentDateTimeUtc()))
        return false;
    if (!m_achievementFilter.isEmpty() && !m_achievementFilter.contains(achievementStateOf(g)))
        return false;
    if (tags.isEmpty())
        return true;
    const auto has = [&](const QString &tag) {
        return std::any_of(g.tags.cbegin(), g.tags.cend(), [&](const GameTag &t) { return t.name == tag; });
    };
    return m_tagMatchAll ? std::all_of(tags.cbegin(), tags.cend(), has)
                         : std::any_of(tags.cbegin(), tags.cend(), has);
}

bool Library::collectionsWantAchievements() const
{
    const QList<Collection> &all = m_store.collections();
    return std::any_of(all.cbegin(), all.cend(),
                       [](const Collection &c) { return c.rules && !c.rules->achievements.isEmpty(); });
}

// A game whose count isn't read (yet) has none to earn.
QString Library::achievementStateOf(const Game &g) const
{
    const auto it = m_achievementOf.constFind(g.id);
    if (it == m_achievementOf.cend() || it->total <= 0)
        return QStringLiteral("none");
    if (it->unlocked <= 0)
        return QStringLiteral("not_started");
    return it->unlocked >= it->total ? QStringLiteral("completed") : QStringLiteral("in_progress");
}

void Library::updateAchievementStates()
{
    QHash<QString, AchievementEntry> best;
    const auto groupOf = [](const Game &g) { return g.groupId.isEmpty() ? g.id : g.groupId; };
    for (const Game &g : std::as_const(m_games)) {
        if (g.platform != kSteamPlatform)
            continue;
        const auto cached = m_achievementCache.constFind(GameId::steamAppId(g.id));
        if (cached == m_achievementCache.cend())
            continue;
        AchievementEntry &entry = best[groupOf(g)];
        if (cached->unlocked > entry.unlocked || (cached->unlocked == entry.unlocked && cached->total > entry.total))
            entry = *cached;
    }
    m_achievementOf.clear();
    for (const Game &g : std::as_const(m_games)) {
        const auto found = best.constFind(groupOf(g));
        if (found != best.cend())
            m_achievementOf.insert(g.id, *found);
    }
    emit achievementStatesChanged();
}

QHash<qint64, qint64> Library::achievementStamps() const
{
    QHash<qint64, qint64> stamps;
    for (const Game &g : m_games) {
        if (g.hidden || g.platform != kSteamPlatform || !(g.installed || g.playMinutes > 0 || g.lastPlayed.isValid()))
            continue;
        const qint64 appId = GameId::steamAppId(g.id);
        if (appId > 0)
            stamps.insert(appId, (qint64(g.playMinutes) << 32) | (g.lastPlayed.toSecsSinceEpoch() & 0xffffffff));
    }
    return stamps;
}

// What was read before is used while a game whose play changed is read again.
void Library::readAchievements()
{
    const QHash<qint64, qint64> stamps = achievementStamps();
    QList<QPair<qint64, qint64>> stale;
    for (auto it = stamps.cbegin(); it != stamps.cend(); ++it) {
        const auto cached = m_achievementCache.constFind(it.key());
        if (cached == m_achievementCache.cend() || cached->stamp != it.value())
            stale.append({it.key(), it.value()});
    }
    const SteamInstall install = m_store.snapshot().steam.install;
    const bool readable = !install.userDataDir().isEmpty();
    if (stale.isEmpty() || !readable) {
        m_achievementsRead = true;
        return;
    }
    if (m_achievementsRunning)
        return;
    m_achievementsRunning = true;
    auto *pass = new QFutureWatcher<QHash<qint64, AchievementEntry>>(this);
    connect(pass, &QFutureWatcher<QHash<qint64, AchievementEntry>>::finished, this, [this, pass] {
        const auto read = pass->result();
        for (auto it = read.cbegin(); it != read.cend(); ++it)
            m_achievementCache.insert(it.key(), it.value());
        m_achievementsRunning = false;
        m_achievementsRead = true;
        pass->deleteLater();
        updateAchievementStates();
        if (achievementsWanted()) {
            updateCollections();
            updateOrder();
            rebuild();
        }
        emit overviewAchievementsRead();
    });
    pass->setFuture(QtConcurrent::run([install, stale] {
        const QString language = SteamAchievements::steamLanguage(install, QDir::homePath());
        QHash<qint64, AchievementEntry> read;
        for (const auto &[appId, stamp] : stale) {
            const auto list = SteamAchievements::readAchievements(install, appId, language);
            const int unlocked = int(std::count_if(list.cbegin(), list.cend(), [](const auto &a) { return a.unlocked; }));
            read.insert(appId, {unlocked, int(list.size()), stamp});
        }
        return read;
    }));
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
    if (!m_restorePlatform.isEmpty() && !m_platforms.isEmpty()) {
        const QString wanted = std::exchange(m_restorePlatform, QString());
        QString &target = m_held ? m_held->platform : m_platformFilter;
        if (target.isEmpty()
            && std::any_of(m_platforms.cbegin(), m_platforms.cend(), [&](const Platform &p) { return p.id == wanted; })) {
            target = wanted;
            if (!m_held)
                emit platformFilterChanged();
        }
    }
    return true;
}

void Library::toggleGroup(const QString &group)
{
    if (!m_collapsed.remove(collapseKey(group)))
        m_collapsed.insert(collapseKey(group));
    rebuildGroup(group);
    m_viewStateTimer.start();
}

bool Library::matchesSearch(const QString &text, const QString &search) const
{
    return SearchText::matches(SearchText::fold(text), SearchText::words(search));
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
    m_viewStateTimer.start();

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
    m_viewStateTimer.start();
    return nextGameRow(header, 1);
}

int Library::expandToGame(const QString &id)
{
    if (const int row = rowOfGame(id); row >= 0 || !m_showingGroups || m_collapsed.isEmpty())
        return row;
    bool showingGroups = false;
    int notInstalled = 0;
    for (const Row &row : computeRows(&showingGroups, &notInstalled, true)) {
        if (row.game < 0 || m_games.at(row.game).id != id)
            continue;
        m_collapsed.remove(collapseKey(row.group));
        rebuildGroup(row.group);
        m_viewStateTimer.start();
        return rowOfGame(id);
    }
    return -1;
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

QVariantMap Library::steamLocalArt(qint64 appId) const
{
    QVariantMap out;
    const QHash<QString, QString> found = SteamArt::find(
        m_store.snapshot().steam.install, appId,
        {QStringLiteral("cover"), QStringLiteral("hero"), QStringLiteral("logo")});
    for (auto it = found.cbegin(); it != found.cend(); ++it)
        out.insert(it.key(), QUrl::fromLocalFile(it.value()).toString());
    return out;
}

qint64 Library::steamAppFor(const QString &id) const
{
    return GameId::steamAppId(id);
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

QVariantMap sessionToMap(const PlayHistory::Session &s)
{
    return {{QStringLiteral("end"), s.end.toOffsetFromUtc(s.end.offsetFromUtc()).toString(Qt::ISODate)},
            {QStringLiteral("minutes"), s.minutes},
            {QStringLiteral("merged"), s.merged},
            {QStringLiteral("manual"), s.manual}};
}
}

namespace {
QStringList copiesOf(const Game &g)
{
    return g.linked.isEmpty() ? QStringList{g.id} : g.linked + QStringList{g.id};
}

// The copies' histories as one: each session names its game when there's more than one.
Parsed<PlayHistory::File> readCopies(const QString &dataDir, const QStringList &ids)
{
    if (ids.size() == 1)
        return PlayHistory::read(dataDir, ids.first());
    Parsed<PlayHistory::File> out;
    for (const QString &id : ids) {
        const Parsed<PlayHistory::File> file = PlayHistory::read(dataDir, id);
        for (PlayHistory::Session s : file.value.sessions) {
            s.gameId = id;
            out.value.sessions.append(s);
        }
        out.value.deletedMinutes += file.value.deletedMinutes;
        out.warnings += file.warnings;
    }
    std::stable_sort(out.value.sessions.begin(), out.value.sessions.end(),
                     [](const PlayHistory::Session &a, const PlayHistory::Session &b) { return a.end < b.end; });
    return out;
}
}

QVariantList Library::linkedCopies(const Game &g) const
{
    QVariantList out;
    for (const QString &id : g.linked) {
        if (const Game *copy = game(id))
            out.append(QVariantMap{{QStringLiteral("id"), id},
                                   {QStringLiteral("title"), copy->title},
                                   {QStringLiteral("platformName"), copy->platformName}});
    }
    return out;
}

QVariantMap Library::copyRow(const QString &id) const
{
    const Game *copy = gameIncludingHidden(id);
    if (!copy)
        return {};
    return {{QStringLiteral("id"), copy->id},
            {QStringLiteral("title"), copy->title},
            {QStringLiteral("platform"), copy->platform},
            {QStringLiteral("platformName"), copy->platformName},
            {QStringLiteral("main"), !copy->groupId.isEmpty() && copy->id == copy->groupId},
            {QStringLiteral("hidden"), copy->hidden},
            {QStringLiteral("notInstalled"), !copy->installed},
            {QStringLiteral("coverUrl"), copy->coverUrl},
            {QStringLiteral("playTimeText"), playTimeText(copy->playMinutes)},
            {QStringLiteral("lastPlayedText"), lastPlayedText(copy->lastPlayed)}};
}

QVariantMap Library::copyRows(const QString &id) const
{
    const Game *g = game(id);
    if (!g || g->groupId.isEmpty())
        return {{QStringLiteral("copies"), QVariantList()}, {QStringLiteral("self"), copyRow(id)}};
    QStringList ids = g->linked;
    std::sort(ids.begin(), ids.end(), [this](const QString &a, const QString &b) {
        const Game *ga = gameIncludingHidden(a);
        const Game *gb = gameIncludingHidden(b);
        return QString::localeAwareCompare(ga ? ga->platformName : a, gb ? gb->platformName : b) < 0;
    });
    ids.prepend(id);
    QVariantList copies;
    for (const QString &copyId : std::as_const(ids)) {
        if (gameIncludingHidden(copyId))
            copies.append(copyRow(copyId));
    }
    return {{QStringLiteral("copies"), copies},
            {QStringLiteral("self"), copyRow(id)},
            {QStringLiteral("totalPlayTimeText"), playTimeText(g->groupPlayMinutes)},
            {QStringLiteral("totalLastPlayedText"), lastPlayedText(g->groupLastPlayed)}};
}

// The main copy's title when `other` is in a group `g` isn't part of; empty otherwise.
QString Library::otherGroupTitle(const Game &g, const Game &other) const
{
    if (other.groupId.isEmpty() || other.groupId == g.groupId)
        return {};
    const Game *main = game(other.groupId);
    return main ? main->title : other.groupId;
}

QVariantList Library::sameGameCandidates(const QString &id) const
{
    const Game *g = game(id);
    if (!g)
        return {};
    const LibraryContent &content = m_store.snapshot().content;
    const int igdb = content.games.value(id).igdb;
    const QString title = SearchText::fold(g->title);
    QVariantList out;
    for (int i = 0; i < m_games.size(); ++i) {
        const Game &other = m_games.at(i);
        if (other.id == id || g->linked.contains(other.id))
            continue;
        const int otherIgdb = content.games.value(other.id).igdb;
        if ((igdb > 0 && otherIgdb == igdb) || m_foldedTitles.at(i) == title)
            out.append(QVariantMap{{QStringLiteral("id"), other.id},
                                   {QStringLiteral("title"), other.title},
                                   {QStringLiteral("platformName"), other.platformName},
                                   {QStringLiteral("coverUrl"), other.coverUrl},
                                   {QStringLiteral("sameIgdb"), igdb > 0 && otherIgdb == igdb},
                                   {QStringLiteral("linkedTo"), otherGroupTitle(*g, other)}});
    }
    return out;
}

QString Library::linkCopy(const QString &id, const QString &other)
{
    const Game *g = game(id);
    const Game *o = game(other);
    if (!g || !o || id == other)
        return tr("There's no game with the id %1").arg(!g ? id : other);
    QString error;
    const auto plan = planCopyChanges(id, {QVariantMap{{QStringLiteral("op"), QStringLiteral("link")},
                                                       {QStringLiteral("id"), other}}}, error);
    if (!error.isEmpty())
        return error;
    return writeCopyPlan(plan, tr("Linked %1 and %2").arg(g->title, o->title));
}

QString Library::setMainCopy(const QString &id)
{
    const Game *g = game(id);
    if (!g)
        return tr("There's no game with the id %1").arg(id);
    QString error;
    const auto plan = planCopyChanges(id, {QVariantMap{{QStringLiteral("op"), QStringLiteral("main")},
                                                       {QStringLiteral("id"), id}}}, error);
    if (!error.isEmpty())
        return error;
    return writeCopyPlan(plan, tr("%1 is now the main copy").arg(g->title));
}

QString Library::applyCopyChanges(const QString &gameId, const QVariantList &changes)
{
    const Game *g = game(gameId);
    if (!g)
        return tr("There's no game with the id %1").arg(gameId);
    QString error;
    const auto plan = planCopyChanges(gameId, changes, error);
    if (!error.isEmpty())
        return error;
    // A Save has taken its Undo already (art and values go into it): the link changes join it.
    return writeCopyPlan(plan, tr("Linked copies of %1 changed").arg(g->title),
                         m_undo.pending() && m_undo.subject() == gameId);
}

// The new same_as of every game the changes touch, worked out in memory first so that a refused change writes nothing.
QHash<QString, QString> Library::planCopyChanges(const QString &gameId, const QVariantList &changes, QString &error) const
{
    QHash<QString, QString> before;
    before.reserve(m_games.size());
    for (const Game &g : m_games)
        before.insert(g.id, g.groupId == g.id ? QString() : g.groupId);
    QHash<QString, QString> now = before;

    const auto mainOf = [&](const QString &id) {
        if (!now.value(id).isEmpty())
            return now.value(id);
        for (auto it = now.cbegin(); it != now.cend(); ++it) {
            if (it.value() == id)
                return id;
        }
        return QString();
    };
    const auto titleOf = [&](const QString &id) {
        const Game *g = game(id);
        return g ? g->title : id;
    };

    for (const QVariant &entry : changes) {
        const QVariantMap change = entry.toMap();
        const QString op = change.value(QStringLiteral("op")).toString();
        const QString id = change.value(QStringLiteral("id")).toString();
        if (!now.contains(id) || !now.contains(gameId)) {
            error = tr("There's no game with the id %1").arg(now.contains(id) ? gameId : id);
            return {};
        }
        const QString main = mainOf(id);
        if (op == QLatin1String("link")) {
            const QString own = mainOf(gameId);
            if (id == gameId)
                continue;
            if (!own.isEmpty() && !main.isEmpty()) {
                if (own != main) {
                    error = tr("%1 is already linked to %2; unlink it first").arg(titleOf(id), titleOf(main));
                    return {};
                }
            } else if (!own.isEmpty()) {
                now[id] = own;
            } else if (!main.isEmpty()) {
                now[gameId] = main;
            } else {
                // The Steam copy stays the main one, so a store game never points at a shortcut.
                const Game *g = gameIncludingHidden(gameId);
                const Game *o = gameIncludingHidden(id);
                if (g->platform == kSteamPlatform && o->platform != kSteamPlatform)
                    now[id] = gameId;
                else
                    now[gameId] = id;
            }
        } else if (op == QLatin1String("unlink")) {
            if (main.isEmpty())
                continue;
            if (main == id) {
                for (auto it = now.begin(); it != now.end(); ++it) {
                    if (it.value() == id)
                        it.value().clear();
                }
            } else {
                now[id].clear();
            }
        } else if (op == QLatin1String("main")) {
            if (main.isEmpty()) {
                error = tr("%1 isn't linked to another copy").arg(titleOf(id));
                return {};
            }
            if (main == id)
                continue;
            for (auto it = now.begin(); it != now.end(); ++it) {
                if (it.value() == main)
                    it.value() = id;
            }
            now[main] = id;
            now[id].clear();
        } else {
            error = tr("Unknown change: %1").arg(op);
            return {};
        }
    }

    QHash<QString, QString> plan;
    for (auto it = now.cbegin(); it != now.cend(); ++it) {
        if (it.value() != before.value(it.key()))
            plan.insert(it.key(), it.value());
    }
    return plan;
}

QString Library::writeCopyPlan(const QHash<QString, QString> &plan, const QString &text, bool joinPendingUndo)
{
    if (plan.isEmpty())
        return {};
    QStringList ids = plan.keys();
    ids.sort();
    if (joinPendingUndo) {
        QStringList errors;
        for (const QString &id : std::as_const(ids))
            m_undo.keepCopy(gameFileOf(id));
        m_store.beginBatch();
        for (const QString &id : std::as_const(ids)) {
            if (const QString error = m_store.setGameValues(id, {{QStringLiteral("same_as"), plan.value(id)}}); !error.isEmpty())
                errors.append(error);
        }
        m_store.endBatch();
        return errors.join(u'\n');
    }
    return editGames(ids, text, [&](const QString &copy) {
        return m_store.setGameValues(copy, {{QStringLiteral("same_as"), plan.value(copy)}});
    });
}

QString Library::unlinkCopy(const QString &id)
{
    const Game *g = game(id);
    if (!g || g->groupId.isEmpty())
        return {};
    // Unlinking the main copy breaks up the whole group.
    const QStringList ids = g->groupId == id ? g->linked : QStringList{id};
    return editGames(ids, tr("Unlinked %1").arg(g->title), [&](const QString &copy) {
        return m_store.setGameValues(copy, {{QStringLiteral("same_as"), QString()}});
    });
}

QVariantMap Library::historyFor(const QString &id) const
{
    const auto it = std::find_if(m_games.cbegin(), m_games.cend(), [&](const Game &g) { return g.id == id; });
    if (it == m_games.cend())
        return {};
    const Parsed<PlayHistory::File> file = readCopies(m_store.dataDir(), copiesOf(*it));
    QVariantMap out = PlayHistory::summary(file.value, it->groupPlayMinutes, QDate::currentDate(),
                                           QLocale().firstDayOfWeek(), QTimeZone::systemTimeZone());
    if (!it->linked.isEmpty()) {
        QVariantList sessions = out.value(QStringLiteral("sessions")).toList();
        for (QVariant &s : sessions) {
            QVariantMap map = s.toMap();
            const Game *copy = game(map.value(QStringLiteral("gameId")).toString());
            map.insert(QStringLiteral("source"), copy ? copy->platformName : QString());
            s = map;
        }
        out.insert(QStringLiteral("sessions"), sessions);
    }
    out.insert(QStringLiteral("id"), id);
    out.insert(QStringLiteral("fromSteam"), playedInSteam(*it));
    out.insert(QStringLiteral("problems"), file.warnings);
    return out;
}

namespace {
// About 1 ms of parsing (some 450 sessions); a longer history is read on a worker thread.
constexpr qint64 kPageHistorySyncBytes = 24 * 1024;

PlayHistory::PageStats readPageStats(const QString &dataDir, const QStringList &ids, const QDate &today,
                                     Qt::DayOfWeek firstDay)
{
    // LocalTime, not systemTimeZone(): the first systemTimeZone() loads the zone database, about 3 ms.
    return PlayHistory::pageStats(readCopies(dataDir, ids).value, today, firstDay, QTimeZone(QTimeZone::LocalTime));
}

QVariantMap pageHistoryMap(const PlayHistory::PageStats &s)
{
    return {{QStringLiteral("timesPlayed"), s.sessions > 0 ? QLocale().toString(s.sessions) : QString()},
            {QStringLiteral("firstPlayed"),
             s.first.isValid() ? DateFormat::shortDate(s.first.toLocalTime().date()) : QString()},
            {QStringLiteral("recent"), QVariantMap{{QStringLiteral("week"), s.week},
                                                   {QStringLiteral("two_weeks"), s.twoWeeks},
                                                   {QStringLiteral("month"), s.month},
                                                   {QStringLiteral("year"), s.year}}}};
}
}

QVariantMap Library::pageHistory(const QString &id)
{
    if (id.isEmpty())
        return {};
    const QString dataDir = m_store.dataDir();
    const QDate today = QDate::currentDate();
    const Qt::DayOfWeek firstDay = QLocale().firstDayOfWeek();
    const Game *g = gameIncludingHidden(id);
    const QStringList ids = g ? copiesOf(*g) : QStringList{id};
    qint64 bytes = 0;
    for (const QString &copy : ids)
        bytes += QFileInfo(PlayHistory::path(dataDir, copy)).size();
    if (bytes <= kPageHistorySyncBytes)
        return pageHistoryMap(readPageStats(dataDir, ids, today, firstDay));

    auto *watcher = new QFutureWatcher<PlayHistory::PageStats>(this);
    connect(watcher, &QFutureWatcher<PlayHistory::PageStats>::finished, this, [this, watcher, id] {
        emit pageHistoryReady(id, pageHistoryMap(watcher->result()));
        watcher->deleteLater();
    });
    watcher->setFuture(QtConcurrent::run(readPageStats, dataDir, ids, today, firstDay));
    return {{QStringLiteral("pending"), true}};
}

struct MediaFavoriteState
{
    QMutex mutex;
    bool loaded = false;
    QSet<QString> set;
    QString problem; // set when the file couldn't be read: nothing is written over it then
    QString file;
    QString home;

    // Read on first use, on whichever media worker asks first.
    QSet<QString> snapshot()
    {
        const QMutexLocker lock(&mutex);
        load();
        return set;
    }

    void load() // with the mutex held
    {
        if (!loaded) {
            Parsed<QSet<QString>> read = MediaFavorites::read(file, home);
            set = std::move(read.value);
            if (!read.ok)
                problem = QStringLiteral("%1 can't be read, so favorites weren't saved: %2")
                              .arg(QFileInfo(file).fileName(), read.warnings.value(0));
            loaded = true;
        }
    }
};

namespace {
void markFavorites(QVariantList &items, const QSet<QString> &favorites)
{
    for (QVariant &v : items) {
        QVariantMap item = v.toMap();
        item.insert(QStringLiteral("favorite"), favorites.contains(item.value(QStringLiteral("path")).toString()));
        v = item;
    }
}

QVariantMap clipItem(const SteamMedia::Clip &c, const QString &gameId = {})
{
    QVariantMap item{{QStringLiteral("kind"), QStringLiteral("clip")},
                     {QStringLiteral("id"), c.id},
                     {QStringLiteral("path"), c.folder},
                     {QStringLiteral("folder"), c.folder},
                     {QStringLiteral("videoDir"), c.videoDir},
                     {QStringLiteral("thumbnail"),
                      c.thumbnail.isEmpty() ? QString() : QUrl::fromLocalFile(c.thumbnail).toString()},
                     {QStringLiteral("durationMs"), c.durationMs},
                     {QStringLiteral("length"), SteamMedia::lengthText(c.durationMs)},
                     {QStringLiteral("time"), c.time}};
    if (!gameId.isEmpty())
        item.insert(QStringLiteral("gameId"), gameId);
    return item;
}

constexpr int kScreenshotThumbnailWidth = 640;

QString screenshotThumbnailPath(const QString &path)
{
    return QStringLiteral("%1/shotthumbs/%2.jpg").arg(AppPaths::cacheDir(), VideoThumbnails::key(QFileInfo(path)));
}

// Full-size screenshots decode one at a time and fill the Gallery's grid over seconds; a small copy is quick.
void makeScreenshotThumbnails(QList<std::pair<QDateTime, QString>> missing)
{
    std::sort(missing.begin(), missing.end(), [](const auto &a, const auto &b) { return a.first > b.first; });
    for (const auto &[time, path] : std::as_const(missing)) {
        const QString out = screenshotThumbnailPath(path);
        if (QFileInfo::exists(out))
            continue;
        QImageReader reader(path);
        reader.setAutoTransform(true);
        const QSize size = reader.size();
        if (!size.isValid())
            continue;
        if (size.width() > kScreenshotThumbnailWidth)
            reader.setScaledSize(QSize(kScreenshotThumbnailWidth,
                                       qMax(1, qRound(size.height() * double(kScreenshotThumbnailWidth) / size.width()))));
        const QImage image = reader.read();
        if (image.isNull())
            continue;
        QDir().mkpath(QFileInfo(out).absolutePath());
        const QString partial = out + QStringLiteral(".partial");
        if (image.save(partial, "JPG", 85) && !QFile::rename(partial, out))
            QFile::remove(partial);
    }
}

QVariantMap screenshotItem(const QString &id, const QString &path, const QDateTime &time,
                           const QString &gameId = {})
{
    const QString url = QUrl::fromLocalFile(path).toString();
    const QString cached = screenshotThumbnailPath(path);
    QVariantMap item{{QStringLiteral("kind"), QStringLiteral("screenshot")},
                     {QStringLiteral("id"), id},
                     {QStringLiteral("path"), path},
                     {QStringLiteral("folder"), QFileInfo(path).absolutePath()},
                     {QStringLiteral("thumbnail"), QFileInfo::exists(cached) ? QUrl::fromLocalFile(cached).toString() : url},
                     {QStringLiteral("image"), url},
                     {QStringLiteral("time"), time}};
    if (!gameId.isEmpty())
        item.insert(QStringLiteral("gameId"), gameId);
    return item;
}

QVariantMap videoItem(const MediaFolders::File &f, const VideoThumbnails::Cached &cached, const QString &gameId = {})
{
    QVariantMap item{{QStringLiteral("kind"), QStringLiteral("video")},
                     {QStringLiteral("id"), f.path},
                     {QStringLiteral("path"), f.path},
                     {QStringLiteral("folder"), f.folder},
                     {QStringLiteral("file"), QUrl::fromLocalFile(f.path).toString()},
                     {QStringLiteral("thumbnail"),
                      cached.thumbnail.isEmpty() ? QString() : QUrl::fromLocalFile(cached.thumbnail).toString()},
                     {QStringLiteral("length"),
                      cached.durationMs > 0 ? SteamMedia::lengthText(cached.durationMs) : QString()},
                     {QStringLiteral("time"), f.time}};
    if (!gameId.isEmpty())
        item.insert(QStringLiteral("gameId"), gameId);
    return item;
}

// What Steam's own lists show of an app, so a folder that points into Steam's screenshots doesn't show it twice.
struct SteamShown
{
    QSet<QString> files;
    QSet<QString> thumbnailFolders;

    void add(const QString &path)
    {
        const QFileInfo info(path);
        files.insert(info.canonicalFilePath());
        thumbnailFolders.insert(info.dir().canonicalPath() + QStringLiteral("/thumbnails"));
    }
    bool has(const MediaFolders::File &f) const
    {
        return files.contains(f.path) || thumbnailFolders.contains(QFileInfo(f.path).dir().path());
    }
};

SteamShown steamShown(const SteamMedia::AppMedia &media)
{
    SteamShown shown;
    for (const SteamMedia::Screenshot &s : media.screenshots)
        shown.add(s.path);
    for (const SteamMedia::Clip &c : media.clips) {
        if (!c.thumbnail.isEmpty())
            shown.files.insert(QFileInfo(c.thumbnail).canonicalFilePath());
    }
    return shown;
}

QVariantList folderItems(const MediaFolders::Listing &listing, const SteamShown &steam,
                         const VideoThumbnails::Cache &cache, const QString &gameId = {})
{
    QVariantList items;
    for (const MediaFolders::File &f : listing.files) {
        if (steam.has(f))
            continue;
        if (f.video) {
            items.append(videoItem(f, cache.find(f.path), gameId));
            continue;
        }
        QVariantMap item = screenshotItem(f.path, f.path, f.time, gameId);
        item.insert(QStringLiteral("folder"), f.folder);
        items.append(item);
    }
    return items;
}

QString videoThumbnailFolder()
{
    return AppPaths::cacheDir() + QStringLiteral("/videothumbs");
}

// The app ID Steam knows a library game by (a non-Steam shortcut's is made from its shortcut ID); 0 for none.
quint64 steamAppIdOf(const Game &g)
{
    if (g.shortcut)
        return (quint64(g.shortcut) << 32) | 0x02000000;
    return quint64(qMax<qint64>(GameId::steamAppId(g.id), 0));
}

// The app id ProtonDB is asked about: a Steam game's own, or the one a lookup found for any other Windows game
// (not a Linux program or a ROM); 0 for none.
qint64 protonDbAppOf(const Game &g)
{
    if (g.platform == kSteamPlatform)
        return qMax<qint64>(GameId::steamAppId(g.id), 0);
    return g.rom || g.platform == kLinuxPlatform ? 0 : g.steamAppId;
}
}

void Library::media(const QString &id)
{
    struct Result
    {
        QVariantList items;
        bool limited = false;
    };
    using Job = std::function<Result()>;

    const auto jobFor = [this](const QString &copyId) -> Job {
        const Game *g = gameIncludingHidden(copyId);
        if (g && g->rom) {
            const Platform *p = platform(g->platform);
            const QString chosen = Emulators::choose(g->emulator, p ? p->emulators : QStringList(), installs());
            const EmulatorDef *def = Emulators::find(m_emulators, chosen);
            const bool shots = def && (!def->screenshots.isEmpty() || def->screenshotFoldersSet) && !g->romPath.isEmpty();
            const QStringList folders = m_store.snapshot().content.games.value(copyId).mediaFolders;
            if (!shots && folders.isEmpty())
                return {};
            return [emulator = def ? *def : EmulatorDef(), install = installs().value(chosen), game = *g, shots, folders,
                    dataDir = m_store.dataDir()] {
                Result result;
                SteamShown shown;
                if (shots) {
                    const RomInfo info = EmulatorScreenshots::readInfo(game);
                    const QList<PlayHistory::Session> sessions = PlayHistory::read(dataDir, game.id).value.sessions;
                    for (const EmulatorScreenshots::Shot &s :
                         EmulatorScreenshots::list(emulator, install, game, info, sessions, QDir::homePath())) {
                        result.items.append(screenshotItem(s.id, s.path, s.time));
                        shown.add(s.path);
                    }
                }
                if (!folders.isEmpty()) {
                    const MediaFolders::Listing listing = MediaFolders::list(folders, QDir::homePath());
                    result.limited = listing.limited;
                    result.items += folderItems(listing, shown, VideoThumbnails::Cache(videoThumbnailFolder()));
                }
                return result;
            };
        }
        quint64 appId = quint64(qMax<qint64>(GameId::steamAppId(copyId), 0));
        if (g && g->shortcut)
            appId = steamAppIdOf(*g);
        const SteamInstall install = m_store.snapshot().steam.install;
        const bool steam = appId != 0 && !install.userDataDir().isEmpty();
        const QStringList folders = m_store.snapshot().content.games.value(copyId).mediaFolders;
        if (!steam && folders.isEmpty())
            return {};
        return [install, appId, steam, folders] {
            Result result;
            SteamMedia::AppMedia shown;
            if (steam) {
                shown.clips = SteamMedia::listClips(install, appId);
                shown.screenshots = SteamMedia::listScreenshots(install, appId);
                for (const SteamMedia::Clip &c : shown.clips)
                    result.items.append(clipItem(c));
                for (const SteamMedia::Screenshot &s : shown.screenshots)
                    result.items.append(screenshotItem(s.id, s.path, s.time));
            }
            if (!folders.isEmpty()) {
                const MediaFolders::Listing listing = MediaFolders::list(folders, QDir::homePath());
                result.limited = listing.limited;
                result.items += folderItems(listing, steamShown(shown), VideoThumbnails::Cache(videoThumbnailFolder()));
            }
            return result;
        };
    };

    const Game *g = gameIncludingHidden(id);
    QList<Job> jobs;
    for (const QString &copyId : g ? copiesOf(*g) : QStringList{id}) {
        if (Job job = jobFor(copyId))
            jobs.append(std::move(job));
    }
    if (jobs.isEmpty()) {
        QMetaObject::invokeMethod(this, [this, id] { emit mediaReady(id, {}); }, Qt::QueuedConnection);
        return;
    }
    auto *watcher = new QFutureWatcher<Result>(this);
    connect(watcher, &QFutureWatcher<Result>::finished, this, [this, watcher, id] {
        const Result result = watcher->result();
        requestVideoThumbnails(result.items);
        if (result.limited)
            emit mediaLimited(id);
        emit mediaReady(id, result.items);
        watcher->deleteLater();
    });
    watcher->setFuture(QtConcurrent::run([jobs = std::move(jobs), favorites = mediaFavoriteState()] {
        Result merged;
        QSet<QString> seen;
        for (const Job &job : jobs) {
            const Result part = job();
            merged.limited = merged.limited || part.limited;
            for (const QVariant &item : part.items) {
                if (seen.contains(item.toMap().value(QStringLiteral("id")).toString()))
                    continue;
                seen.insert(item.toMap().value(QStringLiteral("id")).toString());
                merged.items.append(item);
            }
        }
        std::stable_sort(merged.items.begin(), merged.items.end(), [](const QVariant &a, const QVariant &b) {
            return a.toMap().value(QStringLiteral("time")).toDateTime()
                   > b.toMap().value(QStringLiteral("time")).toDateTime();
        });
        markFavorites(merged.items, favorites->snapshot());
        return merged;
    }));
}

QSharedPointer<MediaFavoriteState> Library::mediaFavoriteState()
{
    if (!m_mediaFavorites) {
        m_mediaFavorites = QSharedPointer<MediaFavoriteState>::create();
        m_mediaFavorites->file = m_store.dataDir() + QStringLiteral("/library/media.toml");
        m_mediaFavorites->home = QDir::homePath();
    }
    return m_mediaFavorites;
}

bool Library::isMediaFavorite(const QString &path) const
{
    if (!m_mediaFavorites)
        return false;
    const QMutexLocker lock(&m_mediaFavorites->mutex);
    return m_mediaFavorites->loaded && m_mediaFavorites->set.contains(path);
}

QString Library::setMediaFavorite(const QString &path, bool on)
{
    if (path.isEmpty())
        return {};
    const QSharedPointer<MediaFavoriteState> state = mediaFavoriteState();
    {
        const QMutexLocker lock(&state->mutex);
        state->load();
        if (!state->problem.isEmpty())
            return state->problem;
        if (state->set.contains(path) == on)
            return {};
        if (on)
            state->set.insert(path);
        else
            state->set.remove(path);
    }
    ++m_mediaFavoriteRevision;
    emit mediaFavoriteChanged(path, on);
    // Each write takes the set as it is then, so writes that run out of order still end on the latest one.
    QThreadPool::globalInstance()->start([state] {
        const QMutexLocker lock(&state->mutex);
        if (!MediaFavorites::write(state->file, state->set, state->home))
            qCWarning(lcLibrary).noquote() << QStringLiteral("could not write %1").arg(state->file);
    });
    return {};
}

QString Library::mediaKey(const QString &id) const
{
    const Game *g = game(id);
    if (!g || g->groupId.isEmpty())
        return id;
    const auto shown = [this](const Game &copy) { return !copy.hidden || m_showHidden; };
    if (const Game *main = game(g->groupId); main && shown(*main))
        return main->id;
    for (const QString &copyId : copiesOf(*g)) {
        if (const Game *copy = game(copyId); copy && shown(*copy))
            return copy->id;
    }
    return g->groupId;
}

QVariantMap Library::madeVideoThumbnail(const QString &file) const
{
    return m_madeVideoThumbnails.value(file);
}

void Library::requestVideoThumbnails(const QVariantList &items)
{
    for (const QVariant &v : items) {
        const QVariantMap item = v.toMap();
        if (item.value(QStringLiteral("kind")) != QLatin1String("video") || !item.value(QStringLiteral("thumbnail")).toString().isEmpty())
            continue;
        if (!m_videoThumbnails) {
            m_videoThumbnails = new VideoThumbnailMaker(videoThumbnailFolder(), this);
            connect(m_videoThumbnails, &VideoThumbnailMaker::made, this,
                    [this](const QString &path, const QString &thumbnail, qint64 durationMs) {
                        const QString file = QUrl::fromLocalFile(path).toString();
                        const QString url = QUrl::fromLocalFile(thumbnail).toString();
                        const QString length = SteamMedia::lengthText(durationMs);
                        m_madeVideoThumbnails.insert(file, {{QStringLiteral("thumbnail"), url}, {QStringLiteral("length"), length}});
                        emit videoThumbnailReady(file, url, length);
                    });
        }
        m_videoThumbnails->request(item.value(QStringLiteral("id")).toString());
    }
}

void Library::allMedia()
{
    const int generation = ++m_allMediaGeneration;

    struct Group
    {
        EmulatorDef emulator;
        EmulatorInstall install;
        QList<Game> games;
    };
    QHash<QString, Group> groups;
    QHash<quint64, QString> steamGames;
    struct FolderGame
    {
        QString id;
        quint64 appId = 0;
        QStringList folders;
    };
    QList<FolderGame> folderGames;
    for (const Game &g : std::as_const(m_games)) {
        if (!g.rom) {
            const quint64 appId = steamAppIdOf(g);
            if (appId)
                steamGames.insert(appId, g.id);
            if (const QStringList folders = m_store.snapshot().content.games.value(g.id).mediaFolders; !folders.isEmpty())
                folderGames.append({g.id, appId, folders});
            continue;
        }
        if (const QStringList folders = m_store.snapshot().content.games.value(g.id).mediaFolders; !folders.isEmpty())
            folderGames.append({g.id, 0, folders});
        const Platform *p = platform(g.platform);
        const QString chosen = Emulators::choose(g.emulator, p ? p->emulators : QStringList(), installs());
        const EmulatorDef *def = Emulators::find(m_emulators, chosen);
        if (!def || (def->screenshots.isEmpty() && !def->screenshotFoldersSet) || g.romPath.isEmpty())
            continue;
        Group &group = groups[chosen];
        if (group.games.isEmpty()) {
            group.emulator = *def;
            group.install = installs().value(chosen);
        }
        group.games.append(g);
    }

    // The worker outlives a closed library for as long as a listing takes, so it reaches the library through a QPointer.
    // Copies of one game list under one id (mediaKey), and a file two copies both reach shows once.
    QHash<QString, QString> keys;
    for (const Game &g : std::as_const(m_games)) {
        if (!g.groupId.isEmpty())
            keys.insert(g.id, mediaKey(g.id));
    }
    const auto seen = QSharedPointer<QSet<QString>>::create();
    const auto missing = QSharedPointer<QList<std::pair<QDateTime, QString>>>::create();
    const auto deliver = [self = QPointer<Library>(this), generation, keys, seen, missing,
                          favorites = mediaFavoriteState()](QVariantList batch, bool done, QStringList limited = {}) {
        markFavorites(batch, favorites->snapshot());
        for (const QVariant &v : std::as_const(batch)) {
            const QVariantMap item = v.toMap();
            if (item.value(QStringLiteral("kind")) == QLatin1String("screenshot")
                && item.value(QStringLiteral("thumbnail")) == item.value(QStringLiteral("image")))
                missing->append({item.value(QStringLiteral("time")).toDateTime(),
                                 QUrl(item.value(QStringLiteral("image")).toString()).toLocalFile()});
        }
        QMetaObject::invokeMethod(
            self.data(),
            [self, generation, keys, seen, batch = std::move(batch), done, limited = std::move(limited)]() mutable {
                if (!self || generation != self->m_allMediaGeneration)
                    return;
                QVariantList kept;
                for (const QVariant &v : std::as_const(batch)) {
                    QVariantMap item = v.toMap();
                    const QString gameId = item.value(QStringLiteral("gameId")).toString();
                    item.insert(QStringLiteral("gameId"), keys.value(gameId, gameId));
                    if (seen->contains(item.value(QStringLiteral("id")).toString()))
                        continue;
                    seen->insert(item.value(QStringLiteral("id")).toString());
                    kept.append(item);
                }
                self->requestVideoThumbnails(kept);
                for (const QString &id : limited)
                    emit self->mediaLimited(keys.value(id, id));
                emit self->allMediaReady(kept, done);
            },
            Qt::QueuedConnection);
    };

    QThreadPool::globalInstance()->start([deliver, missing, groups = groups.values(), steamGames, folderGames,
                                          install = m_store.snapshot().steam.install, dataDir = m_store.dataDir()] {
        QHash<quint64, SteamMedia::AppMedia> all;
        if (!install.userDataDir().isEmpty() && !steamGames.isEmpty()) {
            QVariantList batch;
            all = SteamMedia::listAll(install);
            for (auto it = all.cbegin(); it != all.cend(); ++it) {
                const QString gameId = steamGames.value(it.key());
                if (gameId.isEmpty())
                    continue;
                for (const SteamMedia::Clip &c : it->clips)
                    batch.append(clipItem(c, gameId));
                for (const SteamMedia::Screenshot &s : it->screenshots)
                    batch.append(screenshotItem(s.id, s.path, s.time, gameId));
            }
            if (!batch.isEmpty())
                deliver(std::move(batch), false);
        }
        if (!folderGames.isEmpty()) {
            const VideoThumbnails::Cache cache(videoThumbnailFolder());
            for (const FolderGame &g : folderGames) {
                const MediaFolders::Listing listing = MediaFolders::list(g.folders, QDir::homePath());
                const QVariantList batch = folderItems(listing, steamShown(all.value(g.appId)), cache, g.id);
                if (!batch.isEmpty())
                    deliver(batch, false, listing.limited ? QStringList{g.id} : QStringList{});
            }
        }
        for (const Group &group : groups) {
            QHash<QString, RomInfo> infos;
            QHash<QString, QList<PlayHistory::Session>> sessions;
            const bool bySession = EmulatorScreenshots::usesSessions(group.emulator, group.install, QDir::homePath());
            for (const Game &g : group.games) {
                infos.insert(g.id, EmulatorScreenshots::readInfoCached(g));
                if (bySession)
                    sessions.insert(g.id, PlayHistory::read(dataDir, g.id).value.sessions);
            }
            QVariantList batch;
            for (const EmulatorScreenshots::Entry &e : EmulatorScreenshots::listAll(
                     group.emulator, group.install, group.games, infos, sessions, QDir::homePath()))
                batch.append(screenshotItem(e.shot.id, e.shot.path, e.shot.time, e.gameId));
            if (!batch.isEmpty())
                deliver(std::move(batch), false);
        }
        deliver({}, true);
        makeScreenshotThumbnails(*missing);
    });
}

QVariantMap Library::mediaGames(const QStringList &ids) const
{
    QVariantMap out;
    for (const QString &id : ids) {
        const Game *found = game(id);
        if (!found || mediaKey(id) != id)
            continue;
        const Game &g = *found;
        const Platform *p = platform(g.platform);
        out.insert(g.id, QVariantMap{{QStringLiteral("title"), g.title},
                                     {QStringLiteral("platform"), g.platform},
                                     {QStringLiteral("platformName"), g.platformName},
                                     {QStringLiteral("platformIcon"), p ? p->icon : QString()}});
    }
    return out;
}

void Library::achievements(const QString &id)
{
    const qint64 appId = GameId::steamAppId(id);
    const SteamInstall install = m_store.snapshot().steam.install;
    if (appId <= 0 || install.userDataDir().isEmpty()) {
        QMetaObject::invokeMethod(this, [this, id] { emit achievementsReady(id, {}); }, Qt::QueuedConnection);
        return;
    }
    auto *watcher = new QFutureWatcher<QVariantList>(this);
    connect(watcher, &QFutureWatcher<QVariantList>::finished, this, [this, watcher, id, appId] {
        const QVariantList items = watcher->result();
        emit achievementsReady(id, items);
        watcher->deleteLater();
        if (items.isEmpty())
            return;
        if (!m_rarityFetcher)
            return;
        m_rarityFetcher(appId, this, [this, id, items](const QHash<QString, double> &percents) {
            if (percents.isEmpty())
                return;
            QVariantList merged;
            for (const QVariant &item : items) {
                QVariantMap m = item.toMap();
                if (const auto it = percents.constFind(m.value(QStringLiteral("apiName")).toString()); it != percents.constEnd())
                    m.insert(QStringLiteral("percent"), *it);
                merged.append(m);
            }
            emit achievementsReady(id, merged);
        });
    });
    watcher->setFuture(QtConcurrent::run([install, appId] {
        const QString language = SteamAchievements::steamLanguage(install, QDir::homePath());
        QVariantList items;
        for (const SteamAchievements::Achievement &a : SteamAchievements::readAchievements(install, appId, language)) {
            items.append(QVariantMap{{QStringLiteral("apiName"), a.apiName},
                                     {QStringLiteral("name"), a.name},
                                     {QStringLiteral("description"), a.description},
                                     {QStringLiteral("hidden"), a.hidden},
                                     {QStringLiteral("unlocked"), a.unlocked},
                                     {QStringLiteral("time"), a.unlockTime},
                                     {QStringLiteral("icon"), a.iconUrl},
                                     {QStringLiteral("iconFallback"), a.iconFallbackUrl}});
        }
        return items;
    }));
}

void Library::prepareClip(const QString &token, const QString &videoDir, const QString &ffmpeg)
{
    const QString folder = AppPaths::cacheDir() + QStringLiteral("/clips");
    SteamMedia::Clip clip;
    clip.videoDir = videoDir;
    auto *watcher = new QFutureWatcher<QPair<QString, QString>>(this);
    connect(watcher, &QFutureWatcher<QPair<QString, QString>>::finished, this, [this, watcher, token] {
        emit clipPrepared(token, watcher->result().first, watcher->result().second);
        watcher->deleteLater();
    });
    watcher->setFuture(QtConcurrent::run([clip, ffmpeg, folder, token] {
        // One clip plays at a time, so what an earlier run (or a crash) left is stale.
        QDir(folder).removeRecursively();
        QDir().mkpath(folder);
        const QString path = folder + u'/' + token + QStringLiteral(".mp4");
        const QString error = SteamMedia::remuxClip(clip, ffmpeg, path);
        return qMakePair(error.isEmpty() ? path : QString(), error);
    }));
}

void Library::discardClip(const QString &path)
{
    if (!path.isEmpty() && path.startsWith(AppPaths::cacheDir() + QStringLiteral("/clips/")))
        QFile::remove(path);
}

bool Library::clipExists(const QString &videoDir) const
{
    return QFileInfo::exists(videoDir + QStringLiteral("/session.mpd"))
           && !QDir(videoDir).entryList({QStringLiteral("chunk-stream0-*.m4s")}, QDir::Files).isEmpty();
}

bool Library::showClipFolder(const QString &folder)
{
    return ::openUrl(QUrl::fromLocalFile(folder));
}

bool Library::showSteamMediaFolder(const QString &id)
{
    quint64 appId = quint64(qMax<qint64>(GameId::steamAppId(id), 0));
    if (const Game *g = game(id); g && g->shortcut)
        appId = steamAppIdOf(*g);
    const QString user = m_store.snapshot().steam.install.userDataDir();
    if (appId == 0 || user.isEmpty())
        return false;
    const QString screenshots = QStringLiteral("%1/760/remote/%2/screenshots").arg(user).arg(appId);
    const QString folder = QFileInfo::exists(screenshots) ? screenshots : user + QStringLiteral("/gamerecordings");
    return QFileInfo::exists(folder) && ::openUrl(QUrl::fromLocalFile(folder));
}

QString Library::shortPlayTime(int minutes) const
{
    return shortPlayTimeText(minutes);
}

QString Library::playTime(int minutes, const QString &format) const
{
    return format == QLatin1String("hours_minutes") ? hoursMinutesText(minutes) : playTimeText(minutes);
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
    const auto shown = [this](const Game &g) { return !g.hidden || m_showHidden; };
    // A group counts while any copy is shown, under its main copy, or the first shown one when that's hidden.
    QHash<QString, const Game *> groupKey;
    for (const Game &g : m_games) {
        if (g.groupId.isEmpty() || groupKey.contains(g.groupId))
            continue;
        const Game *main = game(g.groupId);
        if (main && shown(*main)) {
            groupKey.insert(g.groupId, main);
            continue;
        }
        for (const QString &id : copiesOf(g)) {
            const Game *copy = game(id);
            if (copy && shown(*copy)) {
                groupKey.insert(g.groupId, copy);
                break;
            }
        }
    }
    for (const Game &g : m_games) {
        const Game *key = g.groupId.isEmpty() ? (shown(g) ? &g : nullptr) : groupKey.value(g.groupId);
        if (!key)
            continue;
        in.games.append({key->id, key->title, g.platform, {}, g.groupId.isEmpty() ? QString() : g.id});
        in.gameInfo.insert(key->id, {{QStringLiteral("title"), key->title},
                                     {QStringLiteral("platform"), key->platform},
                                     {QStringLiteral("iconUrl"), key->iconUrl},
                                     {QStringLiteral("hue"), hueOf(key->title)}});
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

void Library::overview()
{
    const QHash<qint64, qint64> stamps = achievementStamps();
    QList<LibraryOverview::Game> games;
    games.reserve(m_games.size());
    const auto &files = m_store.snapshot().content.games;
    const SteamState &steam = m_store.snapshot().steam;
    QHash<QString, QString> toolNames;
    for (const CompatTool &t : steam.tools())
        toolNames.insert(t.name, t.displayName);
    const auto toolName = [&](const QString &name) { return toolNames.value(name, name); };
    const QString defaultTool = steam.compatTools.contains(0) ? toolName(steam.compatTools.value(0)) : QString();
    QList<qint64> installedApps;
    for (const Game &g : m_games) {
        QStringList tags;
        for (const GameTag &tag : g.tags)
            tags.append(tag.name);
        LibraryOverview::Game o{g.id, g.title, g.platform, g.hidden, g.installed, g.status, g.groupId, g.playMinutes,
                                g.lastPlayed, g.groupPlayMinutes, g.groupLastPlayed, g.coverUrl.toString(), g.sizeBytes,
                                g.installDir.isEmpty() ? g.romPath : g.installDir, tags, g.year};
        if (g.hidden) {
            games.append(o);
            continue;
        }
        o.rom = g.rom;
        o.added = g.added;
        if (g.installed && !g.rom) {
            if (g.platform == kLinuxPlatform) {
                o.runner = QStringLiteral("native");
            } else if (g.platform == kProtonPlatform) {
                o.runner = QStringLiteral("proton");
                const QString tool = g.proton.isEmpty() ? steam.compatTools.value(g.shortcut) : g.proton;
                o.tool = tool.isEmpty() ? (defaultTool.isEmpty() ? tr("Steam's default") : defaultTool) : toolName(tool);
            } else if (g.platform == kSteamPlatform) {
                const qint64 appId = GameId::steamAppId(g.id);
                const QString tool = steam.compatTools.value(appId);
                o.runner = tool.isEmpty() ? QStringLiteral("steam") : QStringLiteral("proton");
                o.tool = toolName(tool);
                if (appId > 0)
                    installedApps.append(appId);
            }
        }
        if (g.platform == kSteamPlatform && (g.installed || g.playMinutes > 0 || g.lastPlayed.isValid())) {
            o.steamAppId = GameId::steamAppId(g.id);
            o.protonDbApp = o.steamAppId;
        } else if (g.platform != kSteamPlatform && g.installed && protonDbAppOf(g) > 0) {
            o.protonDbApp = protonDbAppOf(g);
            installedApps.append(o.protonDbApp);
        }
        o.updateWaiting = g.update == GameUpdate::Waiting;
        if (g.rom && g.installed) {
            const Platform *p = platform(g.platform);
            o.emulatorMissing = Emulators::choose(g.emulator, p ? p->emulators : QStringList(), installs()).isEmpty();
        }
        // A game IGDB doesn't know (igdb 0) is left to the user, as Look up missing info leaves it.
        const auto file = files.constFind(g.id);
        const bool lookupable = file == files.cend() || file->igdb != 0;
        const QStringList marked = file == files.cend() ? QStringList() : file->unavailable;
        o.missingArt = lookupable && g.coverUrl.isEmpty() && !marked.contains(QStringLiteral("cover"));
        o.missingInfo = lookupable
            && ((g.description.trimmed().isEmpty() && !marked.contains(QStringLiteral("description")))
                || (g.year <= 0 && !marked.contains(QStringLiteral("year")))
                || (g.rating < 0 && !marked.contains(QStringLiteral("rating"))));
        if (!g.rom && (g.platform == kSteamPlatform || g.platform == kEpicPlatform || g.platform == kProtonPlatform
            || g.platform == kLinuxPlatform))
            o.backupKey = g.groupId.isEmpty() ? g.id : g.groupId;
        if (file != files.cend() && file->noSaves)
            o.backupKey.clear();
        games.append(o);
    }
    const QString backupFolder = m_backupFolder ? m_backupFolder() : QString();
    const QString dataDir = m_store.dataDir();
    LibraryOverview::Sources sources;
    sources.defaultTool = defaultTool;
    if (m_protonTiers)
        sources.protonTiers = m_protonTiers(installedApps);
    if (m_protonUnfresh)
        m_protonPending = m_protonUnfresh(installedApps);

    readAchievements();
    LibraryOverview::AchievementCounts counts;
    for (auto it = stamps.cbegin(); it != stamps.cend(); ++it) {
        const auto cached = m_achievementCache.constFind(it.key());
        if (cached != m_achievementCache.cend())
            counts.insert(it.key(), {cached->unlocked, cached->total});
    }
    const bool readable = !m_store.snapshot().steam.install.userDataDir().isEmpty();
    std::optional<LibraryOverview::AchievementCounts> achievementCounts;
    if (m_achievementsRead)
        achievementCounts = readable ? counts : LibraryOverview::AchievementCounts();

    QVariantList platforms;
    for (const Platform &p : m_platforms)
        platforms.append(QVariantMap{{QStringLiteral("id"), p.id}, {QStringLiteral("name"), p.name}, {QStringLiteral("color"), p.color}});
    auto *watcher = new QFutureWatcher<QVariantMap>(this);
    connect(watcher, &QFutureWatcher<QVariantMap>::finished, this, [this, watcher] {
        emit overviewReady(watcher->result());
        watcher->deleteLater();
    });
    watcher->setFuture(QtConcurrent::run([games = std::move(games), platforms = std::move(platforms), backupFolder,
                                     achievementCounts = std::move(achievementCounts), sources = std::move(sources), dataDir,
                                     pending = m_protonPending]() mutable {
        // A link to another drive (~/Games -> /mnt/ssd/Games) takes space where it leads.
        for (LibraryOverview::Game &g : games) {
            if (g.installed && !g.hidden && !g.path.isEmpty()) {
                const QString real = QFileInfo(g.path).canonicalFilePath();
                g.path = real.isEmpty() ? QDir::cleanPath(g.path) : real;
            }
        }
        QList<LibraryOverview::Drive> drives;
        for (const QStorageInfo &v : QStorageInfo::mountedVolumes()) {
            if (!v.isValid() || !v.isReady() || v.bytesTotal() <= 0)
                continue;
            QString name = v.name();
            if (name.isEmpty()) {
                const QString folder = QFileInfo(v.rootPath()).fileName();
                name = folder.isEmpty() ? Library::tr("System") : folder.left(1).toUpper() + folder.sliced(1);
            }
            drives.append({QString::fromUtf8(v.device()), name, v.rootPath(), v.bytesTotal(), v.bytesAvailable()});
        }

        // One listing per backup folder; no archive is opened. A folder holding an archive is a game (or an emulator)
        // that has been backed up.
        std::optional<QSet<QString>> backedUp;
        QSet<QString> nothingFound;
        if (!backupFolder.isEmpty()) {
            backedUp.emplace();
            LibraryOverview::SaveBackups saves;
            const QDir root(backupFolder);
            for (const QString &name : root.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
                const QFileInfoList archives = QDir(root.filePath(name)).entryInfoList({QStringLiteral("*.tar.zst")}, QDir::Files);
                if (archives.isEmpty()) {
                    if (QFileInfo::exists(root.filePath(name) + u'/' + SavesBackup::kNothingFound))
                        nothingFound.insert(name);
                    continue;
                }
                backedUp->insert(name);
                for (const QFileInfo &archive : archives) {
                    ++saves.archives;
                    saves.bytes += archive.size();
                    if (!saves.last.isValid() || archive.lastModified() > saves.last)
                        saves.last = archive.lastModified();
                }
            }
            sources.saves = saves;
        }

        // A backup that looked and found no saves leaves a marker: such a game isn't one to back up.
        for (LibraryOverview::Game &g : games) {
            if (nothingFound.contains(g.backupKey))
                g.backupKey.clear();
        }

        // Merged entries (several sessions in one) don't count, as in Play statistics.
        for (LibraryOverview::Game &g : games) {
            if (g.hidden || (g.playMinutes <= 0 && !g.lastPlayed.isValid()))
                continue;
            for (const PlayHistory::Session &s : PlayHistory::read(dataDir, g.id).value.sessions) {
                if (!s.merged && s.minutes > g.longestSession) {
                    g.longestSession = s.minutes;
                    g.longestEnd = s.end;
                }
            }
        }
        QVariantMap map = LibraryOverview::toVariantMap(LibraryOverview::overview(games, drives, QDateTime::currentDateTimeUtc(), backedUp, achievementCounts, sources));
        // The dates the cards say, in words, as the lists above get theirs.
        const auto textOf = [&map](const QString &key, const QString &date, const QString &name, auto text) {
            if (!map.contains(key))
                return;
            QVariantMap m = map.value(key).toMap();
            m.insert(name, text(m.value(date)));
            map.insert(key, m);
        };
        textOf(QStringLiteral("saves"), QStringLiteral("last"), QStringLiteral("lastText"),
               [](const QVariant &v) { return lastPlayedText(v.toDateTime()); });
        textOf(QStringLiteral("longest"), QStringLiteral("end"), QStringLiteral("endText"),
               [](const QVariant &v) { return lastPlayedText(v.toDateTime()); });
        textOf(QStringLiteral("waiting"), QStringLiteral("added"), QStringLiteral("waitingText"),
               [](const QVariant &v) { return waitingText(v.toDate()); });
        QVariantList pendingApps;
        for (qint64 appId : std::as_const(pending))
            pendingApps.append(appId);
        map.insert(QStringLiteral("protonDbPending"), pendingApps);
        const auto withDates = [&map](const QString &key, auto text) {
            QVariantList rows = map.value(key).toList();
            for (QVariant &row : rows) {
                QVariantMap m = row.toMap();
                m.insert(QStringLiteral("lastPlayedText"), text(m.value(QStringLiteral("lastPlayed")).toDateTime()));
                row = m;
            }
            map.insert(key, rows);
        };
        withDates(QStringLiteral("continuePlaying"), playedAgoText);
        withDates(QStringLiteral("largest"), playedWhenText);
        QVariantList statusCounts = map.value(QStringLiteral("statusCounts")).toList();
        for (QVariant &row : statusCounts) {
            QVariantMap m = row.toMap();
            m.insert(QStringLiteral("name"), statusName(m.value(QStringLiteral("status")).toString()));
            row = m;
        }
        map.insert(QStringLiteral("statusCounts"), statusCounts);
        map.insert(QStringLiteral("platformList"), platforms);
        return map;
    }));
}

QString Library::sizeText(qint64 bytes, int precision) const
{
    return QLocale().formattedDataSize(qMax<qint64>(bytes, 0), precision, QLocale::DataSizeTraditionalFormat);
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
    return {{QStringLiteral("error"), QString()}, {QStringLiteral("session"), sessionToMap(removed)}};
}

namespace {
// A session put into a game's history and totals: restoring a deleted one, or adding one by hand.
QString putSession(LibraryStore &store, const Game &game, const PlayHistory::Session &session)
{
    const QString dataDir = store.dataDir();
    if (const QString error = PlayHistory::append(dataDir, game.id, session); !error.isEmpty())
        return error;
    if (playedInSteam(game)) {
        const int deleted = PlayHistory::read(dataDir, game.id).value.deletedMinutes;
        return PlayHistory::setDeletedMinutes(dataDir, game.id, qMax(0, deleted - session.minutes));
    }
    QList<std::pair<QString, QVariant>> values{{QStringLiteral("play_minutes"), game.playMinutes + session.minutes}};
    if (!game.lastPlayed.isValid() || session.end > game.lastPlayed)
        values.append({QStringLiteral("last_played"), session.end});
    return store.setGameValues(game.id, values);
}
}

QString Library::restoreSession(const QString &id, const QVariantMap &session)
{
    if (m_store.readOnly())
        return tr("Not saved: %1").arg(m_store.versionStatus().reason);
    const auto it = std::find_if(m_games.cbegin(), m_games.cend(), [&](const Game &g) { return g.id == id; });
    if (it == m_games.cend())
        return tr("There's no game with the id %1").arg(id);
    PlayHistory::Session restored{QDateTime::fromString(session.value(QStringLiteral("end")).toString(), Qt::ISODate),
                                  session.value(QStringLiteral("minutes")).toInt(),
                                  session.value(QStringLiteral("merged")).toBool()};
    restored.manual = !restored.merged && session.value(QStringLiteral("manual")).toBool();
    if (!restored.end.isValid() || restored.minutes < 0)
        return tr("That isn't a session");
    return putSession(m_store, *it, restored);
}

QVariantMap Library::addSession(const QString &id, const QString &startIso, int minutes)
{
    const auto fail = [](const QString &error) { return QVariantMap{{QStringLiteral("error"), error}}; };
    if (m_store.readOnly())
        return fail(tr("Not saved: %1").arg(m_store.versionStatus().reason));
    const auto it = std::find_if(m_games.cbegin(), m_games.cend(), [&](const Game &g) { return g.id == id; });
    if (it == m_games.cend())
        return fail(tr("There's no game with the id %1").arg(id));
    if (playedInSteam(*it))
        return fail(tr("Steam counts play outside Lattis itself, so sessions can't be added to its games"));
    const QDateTime start = QDateTime::fromString(startIso, Qt::ISODate);
    if (!start.isValid())
        return fail(tr("%1 isn't a date and time").arg(startIso));
    if (minutes <= 0)
        return fail(tr("The length must be at least a minute"));

    // The offset in force at the end, as for a session Lattis records, so a DST change in between is counted.
    const QDateTime end = start.toTimeZone(QTimeZone::systemTimeZone()).addSecs(qint64(minutes) * 60);
    if (end > QDateTime::currentDateTime())
        return fail(tr("The session can't end in the future"));
    PlayHistory::Session added;
    added.end = end.toOffsetFromUtc(end.offsetFromUtc());
    added.minutes = minutes;
    added.manual = true;

    const Parsed<PlayHistory::File> file = PlayHistory::read(m_store.dataDir(), id);
    if (PlayHistory::hasEnd(file.value, added.end))
        return fail(tr("Another session of this game ends at the same time"));
    const QList<PlayHistory::Session> overlaps = PlayHistory::overlapping(file.value, added);

    if (const QString error = putSession(m_store, *it, added); !error.isEmpty())
        return fail(error);
    QVariantList overlapping;
    for (const PlayHistory::Session &s : overlaps)
        overlapping.append(sessionToMap(s));
    return {{QStringLiteral("error"), QString()},
            {QStringLiteral("session"), sessionToMap(added)},
            {QStringLiteral("overlaps"), overlapping}};
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

const Game *Library::gameIncludingHidden(const QString &id) const
{
    for (const Game &g : m_games) {
        if (g.id == id)
            return &g;
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
            switch (g.update) {
            case GameUpdate::Downloading: return tr("Steam is installing the game; open its downloads");
            case GameUpdate::Paused: return tr("The install is paused; open Steam's downloads");
            default: return tr("Install through Steam; it asks where");
            }
        }
        switch (g.update) {
        case GameUpdate::Waiting: return tr("Open the game's page in Steam to update it");
        case GameUpdate::Downloading: return tr("Steam is updating the game; open its downloads");
        case GameUpdate::Paused: return tr("The update is paused; open Steam's downloads");
        default: return tr("Launch through Steam");
        }
    }
    if (g.platform == kEpicPlatform) {
        if (!g.installed) {
            switch (g.update) {
            case GameUpdate::Downloading: return tr("Legendary is installing the game");
            case GameUpdate::Queued: return tr("The install waits for another Legendary job");
            default: return tr("Install through Heroic");
            }
        }
        switch (g.update) {
        case GameUpdate::Waiting: return tr("Update the game; Lattis runs Heroic's Legendary");
        case GameUpdate::Downloading:
            return g.verifying ? tr("Legendary is verifying the game's files") : tr("Legendary is updating the game");
        case GameUpdate::Queued:
            return g.verifying ? tr("The verification waits for another Legendary job")
                               : tr("The update waits for another Legendary job");
        default: return tr("Launch through Heroic");
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

QVariantList Library::buildModManagers() const
{
    const auto find = [this](const QString &id) { return game(id); };
    QList<ModManager> managers = m_store.snapshot().content.modManagers.values();
    const QCollator collator = titleCollator();
    std::sort(managers.begin(), managers.end(),
              [&](const ModManager &a, const ModManager &b) { return collator.compare(a.name, b.name) < 0; });
    QHash<QString, QString> toolNames;
    for (const CompatTool &t : m_store.snapshot().steam.tools())
        toolNames.insert(t.name, t.displayName);
    const auto toolName = [&](const ModManager &m) {
        const auto shared = m_store.modManagerPrefix(m.id);
        const QString name = shared ? m_store.snapshot().content.games.value(shared->gameId).proton : m.proton;
        return name.isEmpty() ? tr("Steam's default") : toolNames.value(name, name);
    };
    const QHash<QString, QStringList> &queued = m_store.snapshot().steamQueued;
    QVariantList out;
    for (const ModManager &m : std::as_const(managers)) {
        const QStringList queuedKeys = queued.value(ModManagers::kKeyPrefix + m.id);
        const bool pending = !queuedKeys.isEmpty() && queuedKeys != QStringList{SteamSync::kShortcutIcon};
        const auto prefix = m_store.modManagerPrefix(m.id);
        const Game *sharedWith = prefix ? game(prefix->gameId) : nullptr;
        const QString sharedGame = sharedWith ? sharedWith->title : QString();
        QVariantList games;
        for (const ModManagers::LinkedGame &g : ModManagers::linkedGames(m, find)) {
            games.append(QVariantMap{{QStringLiteral("id"), g.id}, {QStringLiteral("title"), g.title},
                                     {QStringLiteral("missing"), g.missing}});
        }
        const QString icon = ManagerIcon::cached(m_cacheDir, m.id, Emulators::expandHome(m.exe, QDir::homePath()),
                                              m.isProton());
        out.append(QVariantMap{
            {QStringLiteral("id"), m.id},
            {QStringLiteral("name"), m.name},
            {QStringLiteral("icon"), icon.isEmpty() ? QString() : QUrl::fromLocalFile(icon).toString()},
            {QStringLiteral("exe"), m.exe},
            {QStringLiteral("startDir"), m.startDir},
            {QStringLiteral("launchOptions"), m.launchOptions},
            {QStringLiteral("proton"), m.isProton()},
            {QStringLiteral("prefix"), m.prefix},
            {QStringLiteral("protonTool"), m.proton},
            {QStringLiteral("shortcut"), m.shortcut},
            {QStringLiteral("inSteam"), m.inSteam},
            {QStringLiteral("shortcutQueued"), queuedKeys.contains(SteamSync::kAddShortcut)},
            {QStringLiteral("pendingEdits"), pending ? QStringLiteral("steam") : QString()},
            {QStringLiteral("protonName"), m.isProton() ? toolName(m) : QString()},
            {QStringLiteral("sharedGame"), sharedGame},
            {QStringLiteral("games"), games},
        });
    }
    return out;
}

QVariantList Library::modManagersFor(const QString &gameId) const
{
    QVariantList out;
    for (const QVariant &v : m_modManagers) {
        const QVariantMap manager = v.toMap();
        for (const QVariant &g : manager.value(QStringLiteral("games")).toList()) {
            if (g.toMap().value(QStringLiteral("id")).toString() == gameId) {
                out.append(manager);
                break;
            }
        }
    }
    return out;
}

QVariant Library::managerValue(const QString &key, const QVariant &value)
{
    if (key == QLatin1String("games") && value.typeId() == QMetaType::QVariantList)
        return value.toStringList();
    return value;
}

QString Library::setModManagerValues(const QString &id, const QVariantMap &values)
{
    QList<std::pair<QString, QVariant>> list;
    for (auto it = values.cbegin(); it != values.cend(); ++it)
        list.append({it.key(), managerValue(it.key(), it.value())});
    return m_store.setModManagerValues(id, list);
}

QVariantList Library::linkableGames() const
{
    QList<const Game *> games;
    for (const Game &g : m_games) {
        if (!g.hidden && ModManagers::linkable(g.platform))
            games.append(&g);
    }
    const QCollator collator = titleCollator();
    std::sort(games.begin(), games.end(),
              [&](const Game *a, const Game *b) { return collator.compare(a->title, b->title) < 0; });
    QVariantList out;
    for (const Game *g : std::as_const(games)) {
        out.append(QVariantMap{{QStringLiteral("id"), g->id}, {QStringLiteral("title"), g->title},
                               {QStringLiteral("platform"), g->platform},
                               {QStringLiteral("platformName"), g->platformName}});
    }
    return out;
}

QVariantMap Library::addModManager(const QVariantMap &values)
{
    ModManager manager;
    for (auto it = values.cbegin(); it != values.cend(); ++it) {
        QString error;
        const QVariant value = ModManagers::validate(it.key(), managerValue(it.key(), it.value()), &error);
        if (!value.isValid())
            return {{QStringLiteral("error"), error}};
        ModManagers::apply(manager, it.key(), value);
    }
    if (manager.name.trimmed().isEmpty())
        return {{QStringLiteral("error"), tr("A mod manager needs a name")}};
    if (manager.exe.isEmpty())
        return {{QStringLiteral("error"), tr("A mod manager needs a program")}};
    const LibraryStore::AddedModManager added = m_store.addModManager(manager);
    if (!added.error.isEmpty())
        return {{QStringLiteral("error"), added.error}};
    return {{QStringLiteral("id"), added.id}};
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
                                           QStringLiteral("args"), QStringLiteral("screenshot_folders"),
                                           QStringLiteral("saves")};
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
        QStringList screenshotFolders;
        for (const QString &folder : EmulatorScreenshots::folders(e, install, home)) {
            screenshotFolders.append(folder.startsWith(home + u'/') ? u'~' + folder.mid(home.size()) : folder);
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
            {QStringLiteral("screenshotFolders"), screenshotFolders},
            {QStringLiteral("saveFolders"), e.saves.value(install.kind == EmulatorInstall::Kind::Flatpak
                                                              ? QStringLiteral("flatpak")
                                                              : QStringLiteral("native"))},
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
            {QStringLiteral("autoBackup"), written != defined.cend() && written->autoBackup},
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
    static const QStringList presetKeys{QStringLiteral("path"), QStringLiteral("args"),
                                        QStringLiteral("screenshot_folders"), QStringLiteral("saves")};
    static const QStringList userKeys{QStringLiteral("name"), QStringLiteral("platforms"), QStringLiteral("flatpak"),
                                      QStringLiteral("path"), QStringLiteral("extensions"), QStringLiteral("command"),
                                      QStringLiteral("args"), QStringLiteral("screenshot_folders"),
                                      QStringLiteral("saves")};
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
    return gameMap(m_games.at(m_rows.at(row).game));
}

QVariantMap Library::gameWithId(const QString &id) const
{
    const Game *g = game(id);
    return g ? gameMap(*g) : QVariantMap();
}

QVariantMap Library::gameMap(const Game &g) const
{
    const QString pendingName = pendingEditsName(g);
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
        {QStringLiteral("protonDbApp"), protonDbAppOf(g)},
        {QStringLiteral("description"), g.description},
        {QStringLiteral("year"), g.year > 0 ? QString::number(g.year) : QString()},
        {QStringLiteral("sizeText"),
         g.sizeBytes > 0 ? QLocale().formattedDataSize(g.sizeBytes, 1, QLocale::DataSizeTraditionalFormat)
                         : QString()},
        {QStringLiteral("playMinutes"), g.groupPlayMinutes},
        {QStringLiteral("playTimeText"), playTimeText(g.groupPlayMinutes)},
        {QStringLiteral("lastPlayedText"), lastPlayedText(g.groupLastPlayed)},
        {QStringLiteral("linked"), linkedCopies(g)},
        {QStringLiteral("addedText"), g.added.isValid() ? DateFormat::shortDate(g.added) : QString()},
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
        {QStringLiteral("launcher"), g.launcher()},
        {QStringLiteral("steamUpdate"), updateStateName(g.update)},
        {QStringLiteral("steamOwnership"), g.steamOwnership},
        {QStringLiteral("sharedBy"), g.sharedBy},
        {QStringLiteral("steamUninstallable"), g.installed || g.update == GameUpdate::Downloading
                                                   || g.update == GameUpdate::Paused},
        {QStringLiteral("steamUpdateText"), updateText(g)},
        {QStringLiteral("verifying"), g.verifying},
        {QStringLiteral("steamUpdatePercent"), updatePercent(g)},
        {QStringLiteral("rom"), g.rom},
        {QStringLiteral("romPath"), g.romPath},
        {QStringLiteral("gameNotFound"), runsProgram(g.platform) && !g.installed && !g.uninstalled},
        {QStringLiteral("launchTip"), launchTip(g)},
        {QStringLiteral("favorite"), g.favorite},
        {QStringLiteral("hidden"), g.hidden},
        {QStringLiteral("emulator"), g.emulator},
        {QStringLiteral("pendingEdits"), pendingName},
        {QStringLiteral("shortcutQueued"),
         g.pendingEdits == PendingLauncher::Steam && m_store.snapshot().steamQueued.value(g.id).contains(SteamSync::kAddShortcut)},
        {QStringLiteral("tags"), tags},
        {QStringLiteral("hue"), hueOf(g.title)},
        {QStringLiteral("iconUrl"), g.iconUrl},
        {QStringLiteral("coverUrl"), g.coverUrl},
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

QList<Library::Row> Library::computeRows(bool *showingGroups, int *notInstalled, bool expandAll) const
{
    const bool byPlatform = m_groupBy == QLatin1String("platform");
    const bool grouped = !m_flat && m_groupBy != QLatin1String("none") && (!byPlatform || m_platformFilter.isEmpty());
    const QStringList tags = effectiveTags();
    const QDateTime now = QDateTime::currentDateTimeUtc();
    *notInstalled = 0;
    const auto listed = [&](int i) {
        const Game &g = m_games.at(i);
        if (!passesFilters(g, tags)
            || !(m_searchWords.isEmpty() || SearchText::matches(m_foldedTitles.at(i), m_searchWords)))
            return false;
        if (m_installedOnly && !g.installed && g.update == GameUpdate::None) {
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

    const auto groupOf = [&](const Game &g) -> QString {
        if (m_groupBy == QLatin1String("status"))
            return statusOf(g);
        if (m_groupBy == QLatin1String("year"))
            return g.year > 0 ? QString::number(g.year) : QStringLiteral("none");
        if (m_groupBy == QLatin1String("rating"))
            return ratingGroup(g.rating);
        if (m_groupBy == QLatin1String("last_played"))
            return lastPlayedGroup(g.groupLastPlayed, now);
        return g.platform;
    };

    QStringList groups;
    if (byPlatform) {
        for (const Platform &platform : m_platforms)
            groups.append(platform.id);
    }
    QHash<QString, QList<int>> matches;
    for (int i : m_order) {
        const Game &g = m_games.at(i);
        if (byPlatform ? !groups.contains(g.platform) : !m_platformFilter.isEmpty() && g.platform != m_platformFilter)
            continue;
        if (listed(i))
            matches[groupOf(g)].append(i);
    }

    // Newest, highest or most recent first, unless the list is sorted the other way by that field; games with no
    // value stay last.
    const bool reversed = m_sort == m_groupBy && !m_sortDescending;
    if (m_groupBy == QLatin1String("status")) {
        groups = listStatuses();
    } else if (m_groupBy == QLatin1String("year")) {
        QList<int> years;
        for (auto it = matches.cbegin(); it != matches.cend(); ++it) {
            if (it.key() != QLatin1String("none"))
                years.append(it.key().toInt());
        }
        std::sort(years.begin(), years.end(), [&](int a, int b) { return reversed ? a < b : a > b; });
        for (int year : years)
            groups.append(QString::number(year));
        groups.append(QStringLiteral("none"));
    } else if (!byPlatform) {
        QStringList order = m_groupBy == QLatin1String("rating")
            ? QStringList{QStringLiteral("5"), QStringLiteral("4"), QStringLiteral("3"), QStringLiteral("under_3")}
            : QStringList{QStringLiteral("week"), QStringLiteral("month"), QStringLiteral("year"),
                          QStringLiteral("older")};
        if (reversed)
            std::reverse(order.begin(), order.end());
        groups = order;
        groups.append(m_groupBy == QLatin1String("rating") ? QStringLiteral("unrated") : QStringLiteral("never"));
    }

    const bool expanded = expandAll || expandsGroups();
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
        l.steamApp = g.platform == kSteamPlatform ? 0 : qMax(file.steamApp, 0);
        l.unavailable = file.unavailable;
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
    const QString pendingName = pendingEditsName(*g);
    QVariantList tags;
    for (const GameTag &tag : g->tags)
        tags.append(QVariantMap{{QStringLiteral("name"), tag.name}, {QStringLiteral("color"), tag.color}});
    sortTagsByName(tags);
    const auto url = [](const QUrl &u) { return u.isEmpty() ? QString() : u.toString(); };
    const QVariantMap emulators = emulatorChoices(g->platform);
    const ProgramFiles program = m_store.snapshot().programFiles.value(id);
    const bool appImage = g->platform == kLinuxPlatform && program.appImage;
    QString platformIcon;
    bool platformAutoBackup = false;
    for (const Platform &p : m_store.snapshot().content.platforms) {
        if (p.id == g->platform)
            platformAutoBackup = p.autoBackup;
    }
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
        {QStringLiteral("steamApp"), g->platform == kSteamPlatform ? 0 : qMax(file.steamApp, 0)},
        {QStringLiteral("tags"), tags},
        {QStringLiteral("emulator"), file.emulator},
        {QStringLiteral("args"), file.args},
        {QStringLiteral("launchOptions"), g->launchOptions},
        {QStringLiteral("proton"), g->proton},
        {QStringLiteral("protonTools"), steam.value(QStringLiteral("protonTools"))},
        {QStringLiteral("protonDefault"), steam.value(QStringLiteral("protonDefault"))},
        {QStringLiteral("steamPresets"),
         g->platform == kLinuxPlatform ? QVariant(linuxPresets()) : steam.value(QStringLiteral("steamPresets"))},
        {QStringLiteral("pendingEdits"), pendingName},
        {QStringLiteral("shortcutQueued"),
         g->pendingEdits == PendingLauncher::Steam && m_store.snapshot().steamQueued.value(id).contains(SteamSync::kAddShortcut)},
        {QStringLiteral("installPath"), g->installDir},
        {QStringLiteral("exe"), file.exe},
        {QStringLiteral("exePath"), g->exe},
        {QStringLiteral("startDir"), file.startDir},
        {QStringLiteral("installDir"), file.installDir},
        {QStringLiteral("appImage"), appImage},
        {QStringLiteral("updateSource"), file.updateSource},
        {QStringLiteral("updateAsset"), file.updateAsset},
        {QStringLiteral("updatePrereleases"), file.updatePrereleases},
        {QStringLiteral("embeddedSource"), QVariantMap{{QStringLiteral("source"), program.embedded.source},
                                                        {QStringLiteral("asset"), program.embedded.asset},
                                                        {QStringLiteral("prereleases"), program.embedded.prereleases}}},
        {QStringLiteral("savePaths"), file.savePaths},
        {QStringLiteral("autoBackup"), file.autoBackup},
        {QStringLiteral("noSaves"), file.noSaves},
        {QStringLiteral("platformAutoBackup"), platformAutoBackup},
        {QStringLiteral("mediaFolders"), file.mediaFolders},
        {QStringLiteral("uninstalled"), g->uninstalled},
        {QStringLiteral("gameNotFound"), runsProgram(g->platform) && !g->installed && !g->uninstalled},
        {QStringLiteral("rom"), g->rom},
        {QStringLiteral("romKey"), g->romKey},
        {QStringLiteral("romPath"), g->romPath},
        {QStringLiteral("romWritten"), file.rom},
        {QStringLiteral("installed"), g->installed},
        {QStringLiteral("launcher"), g->launcher()},
        {QStringLiteral("heroic"), g->launcher() == QLatin1String("heroic") ? heroicSettingsFor(id) : QVariantMap()},
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
            for (const QString &ext : e->extensionsFor(p.id))
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
    for (auto &[key, value] : pairs) {
        if (key == QLatin1String("media_folders") || key == QLatin1String("save_paths")) {
            QStringList written;
            for (const QString &folder : value.toStringList())
                written.append(m_store.writtenPath(folder));
            value = written;
        }
    }
    const auto rom =std::find_if(pairs.cbegin(), pairs.cend(), [](const auto &p) { return p.first == QLatin1String("rom"); });
    const auto exe = std::find_if(pairs.cbegin(), pairs.cend(), [](const auto &p) { return p.first == QLatin1String("exe"); });
    const bool newRom = rom != pairs.cend() && rom->second.isValid();
    const bool newProgram = exe != pairs.cend() && !exe->second.toString().isEmpty()
        && m_store.snapshot().content.games.value(id).platform == kLinuxPlatform;

    // The editor's Save has taken its Undo already (art and links are in it). A new ROM may merge
    // another game into this one and a new program is made executable, and neither can be put
    // back from the game file alone: that Undo is dropped once the change can't be taken back,
    // not before, so a refused Save leaves the art and the links as they were.
    if (!m_undo.pending() || m_undo.subject() != id) {
        m_undo.start(id);
        m_undo.keepCopy(gameFileOf(id));
    }
    const QString title = game(id) ? game(id)->title : id;
    const auto fail = [this](const QString &error) {
        rollBackEdit();
        return error;
    };

    // A picked ROM (a file: URL or path) goes first: it may merge another game into this one.
    if (newProgram) {
        if (const QString problem = linuxProgramProblem(localPath(exe->second.toString())); !problem.isEmpty())
            return fail(problem);
    }
    if (newRom) {
        if (const QString error = m_store.relocateRom(id, localPath(rom->second.toString())); !error.isEmpty())
            return fail(error);
        pairs.erase(rom);
        m_undo.discard();
    }
    if (const QString error = m_store.setGameValues(id, pairs); !error.isEmpty())
        return fail(error);
    if (newProgram)
        m_undo.discard();
    if (std::any_of(pairs.cbegin(), pairs.cend(), [](const auto &p) { return p.first == QLatin1String("media_folders"); }))
        emit mediaFoldersChanged(id);
    if (newRom || newProgram)
        emit undoOffered(0, newRom ? tr("Saved. The new ROM can't be undone") : tr("Saved. The new program can't be undone"));
    else
        commitUndo(tr("Changes to %1 saved").arg(title));
    return {};
}

// A refused Save puts back what its Undo kept, the art and the links the editor had already
// written included. Nothing kept differs: nothing to put back.
void Library::rollBackEdit()
{
    if (const quint64 token = m_undo.commit(); token != 0)
        undo(token);
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

void Library::setViewStateFile(const QString &file)
{
    m_viewStateFile = file;
    m_viewStateTimer.setSingleShot(true);
    m_viewStateTimer.setInterval(500);
    connect(&m_viewStateTimer, &QTimer::timeout, this, &Library::saveViewState, Qt::UniqueConnection);
    if (QCoreApplication::instance())
        connect(QCoreApplication::instance(), &QCoreApplication::aboutToQuit, this, &Library::saveViewState, Qt::UniqueConnection);

    QFile f(file);
    if (!f.open(QIODevice::ReadOnly))
        return;
    const QJsonObject saved = QJsonDocument::fromJson(f.readAll()).object();
    m_restorePlatform = saved.value(QLatin1String("platform")).toString();
    m_restoreCollection = saved.value(QLatin1String("collection")).toString();
    m_tagsOpen = saved.value(QLatin1String("tagsOpen")).toBool(true);
    for (const QJsonValue &key : saved.value(QLatin1String("collapsed")).toArray())
        m_collapsed.insert(key.toString());
    if (!m_platforms.isEmpty()) {
        const QString wanted = std::exchange(m_restorePlatform, QString());
        if (std::any_of(m_platforms.cbegin(), m_platforms.cend(), [&](const Platform &p) { return p.id == wanted; })) {
            m_platformFilter = wanted;
            emit platformFilterChanged();
        }
    }
    if (!m_restoreCollection.isEmpty() && std::any_of(m_collections.cbegin(), m_collections.cend(), [&](const QVariant &c) {
            return c.toMap().value(QStringLiteral("id")).toString() == m_restoreCollection;
        }))
        enterCollection(std::exchange(m_restoreCollection, QString()), false);
    if (!m_rows.isEmpty())
        rebuild();
}

void Library::saveViewState() const
{
    if (m_viewStateFile.isEmpty())
        return;
    QStringList keys(m_collapsed.cbegin(), m_collapsed.cend());
    keys.sort();
    QJsonArray collapsed;
    for (const QString &key : keys)
        collapsed.append(key);
    QSaveFile f(m_viewStateFile);
    if (!f.open(QIODevice::WriteOnly))
        return;
    // While a collection is open the platform kept is the normal view's: opening starts on All.
    f.write(QJsonDocument(QJsonObject{{QStringLiteral("platform"), m_held ? m_held->platform : m_platformFilter},
                                      {QStringLiteral("collapsed"), collapsed},
                                      {QStringLiteral("collection"), m_collectionId},
                                      {QStringLiteral("tagsOpen"), m_tagsOpen}}).toJson());
    f.commit();
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
    if (!GameId::isValid(id) || !m_store.snapshot().content.games.contains(id))
        return tr("Unknown game");
    // A Proton game's files, prefix and shortcut are Uninstall's, which checks it isn't running.
    if (wipe && m_store.snapshot().content.games.value(id).platform == kProtonPlatform)
        return tr("A Proton game is removed with Uninstall");
    // A game only its file makes has nothing to bring it back, so wiping it deletes it.
    const QString title = game(id) ? game(id)->title : id;
    const bool fileOnly = wipe && !m_store.inIndex(id);
    m_undo.start(id);
    m_undo.keepCopy(gameFileOf(id));
    if (fileOnly)
        m_undo.keepCopy(QStringLiteral("library/history/%1.toml").arg(id));
    // The art goes into the undo folder only once the game file has changed, so a change that
    // fails leaves the art where it is.
    const QString artFolder = QStringLiteral("art/") + id;
    const auto done = [&](const QString &error) {
        if (error.isEmpty())
            commitUndo(wipe ? tr("%1 deleted").arg(title) : tr("%1 is hidden").arg(title));
        else
            m_undo.discard();
        return error;
    };
    if (fileOnly) {
        if (const QString error = m_store.removeGameFile(id, false); !error.isEmpty())
            return done(error);
        m_undo.keepMove(artFolder);
        for (const QString &kind : kArtKinds) {
            if (const QString error = m_store.removeArt(id, kind); !error.isEmpty())
                return done(error);
        }
        // With Undo off nothing moved it, and a delete leaves nothing of the game behind.
        if (!m_undo.enabled())
            QDir(m_store.dataDir() + u'/' + artFolder).removeRecursively();
        return done({});
    }
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
        m_undo.keepMove(artFolder);
        for (const QString &kind : kArtKinds) {
            if (const QString error = m_store.removeArt(id, kind); !error.isEmpty())
                return done(error);
        }
    }
    return done({});
}

bool Library::readsNames(const QString &platform) const
{
    return RomInfoReader::readsPlatform(platform);
}

void Library::updateRomNames(const QString &platformId)
{
    if (m_updatingNames)
        return;
    const auto platform = std::find_if(m_allPlatforms.cbegin(), m_allPlatforms.cend(),
                                       [&](const Platform &p) { return p.id == platformId; });
    if (platform == m_allPlatforms.cend())
        return;

    const LibrarySnapshot &snapshot = m_store.snapshot();
    // A title in a game file is the user's own: only the scanner's index titles are touched.
    QSet<QString> edited;
    for (auto it = snapshot.content.games.cbegin(); it != snapshot.content.games.cend(); ++it) {
        if (!it->title.isEmpty())
            edited.insert(it.key());
    }
    const QList<RomNameJob> jobs = romNameJobs(*platform, m_emulators, snapshot.content.roms.value(platformId),
                                               snapshot.romFiles.value(platformId), edited);
    if (jobs.isEmpty()) {
        emit undoOffered(0, tr("No games to read names from"));
        return;
    }

    m_updatingNames = true;
    emit updatingNamesChanged();
    auto *watcher = new QFutureWatcher<QList<RomNameRead>>(this);
    connect(watcher, &QFutureWatcher<QList<RomNameRead>>::finished, this, [this, watcher, platformId] {
        const QList<RomNameRead> reads = watcher->result();
        watcher->deleteLater();
        m_updatingNames = false;
        emit updatingNamesChanged();

        const RomNamesUpdate update = applyRomNames(m_store.snapshot().content.roms.value(platformId), reads);
        if (update.renamed == 0) {
            // The titles were right already; the entries are now known to come from the files.
            if (update.changed)
                m_store.setRomEntries(platformId, update.entries);
            emit undoOffered(0, tr("No names changed"));
            return;
        }
        m_undo.start();
        m_undo.keepCopy(QStringLiteral("library/roms/%1.toml").arg(platformId));
        if (const QString error = m_store.setRomEntries(platformId, update.entries); !error.isEmpty()) {
            m_undo.discard();
            emit undoOffered(0, error);
            return;
        }
        commitUndo(update.renamed == 1 ? tr("1 game renamed from its file")
                                       : tr("%1 games renamed from their files").arg(update.renamed));
    });
    // The button is the user's own request, so it may download a platform's name database.
    watcher->setFuture(QtConcurrent::run(readRomNames, platformId, jobs, RomDbFetch::options(m_cacheDir, true)));
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

QString Library::trashRefusals(const QStringList &folders, bool permanent) const
{
    QStringList refused;
    if (!permanent) {
        for (const QString &folder : folders) {
            if (!m_canTrash(folder))
                refused.append(m_store.writtenPath(folder));
        }
    }
    return refused.join(QStringLiteral(", "));
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
        // Gone already counts as done: the call is repeated after a refused trash.
        if (programFiles.singleFile ? QFileInfo(dir).isFile() : QFileInfo(dir).isDir())
            folders.append(dir);
        else if (!prefix)
            return withError(tr("%1's files are gone already").arg(game.title));
    }
    if (prefix) {
        const QString dir = protonPrefix(quint32(game.shortcut));
        if (dir.isEmpty())
            return withError(tr("%1 has no Wine prefix").arg(game.title));
        if (const QString why = Uninstall::refusal(dir, places); !why.isEmpty())
            return withError(tr("The Wine prefix can't be deleted: %1").arg(why));
        folders.append(dir);
    }
    // Every folder is asked first, so a refusal leaves them all where they are; then the game.
    if (const QString refused = trashRefusals(folders, permanent); !refused.isEmpty())
        return {{QStringLiteral("trashless"), refused}};
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

QString Library::epicAppName(const QString &id) const
{
    for (const HeroicEntry &entry : m_store.snapshot().content.heroic) {
        if (entry.id == id)
            return entry.appName;
    }
    return {};
}

namespace {
QVariantMap wineMap(const HeroicWine &w)
{
    return {{QStringLiteral("bin"), w.bin}, {QStringLiteral("name"), w.name}, {QStringLiteral("type"), w.type},
            {QStringLiteral("wineserver"), w.wineserver}};
}

QVariantMap heroicDraft(const HeroicGameSettings &s)
{
    return {{QStringLiteral("launcherArgs"), s.launcherArgs},
            {QStringLiteral("showMangohud"), s.showMangohud},
            {QStringLiteral("useGameMode"), s.useGameMode},
            {QStringLiteral("environment"), s.environment.toVariantList()},
            {QStringLiteral("wrappers"), s.wrappers.toVariantList()},
            {QStringLiteral("wine"), wineMap(s.wine)}};
}

HeroicGameSettings heroicFromDraft(const QVariantMap &d)
{
    const QVariantMap wine = d.value(QStringLiteral("wine")).toMap();
    HeroicGameSettings s;
    s.launcherArgs = d.value(QStringLiteral("launcherArgs")).toString();
    s.showMangohud = d.value(QStringLiteral("showMangohud")).toBool();
    s.useGameMode = d.value(QStringLiteral("useGameMode")).toBool();
    s.environment = QJsonArray::fromVariantList(d.value(QStringLiteral("environment")).toList());
    s.wrappers = QJsonArray::fromVariantList(d.value(QStringLiteral("wrappers")).toList());
    s.wine = {wine.value(QStringLiteral("bin")).toString(), wine.value(QStringLiteral("name")).toString(),
              wine.value(QStringLiteral("type")).toString(), wine.value(QStringLiteral("wineserver")).toString()};
    return s;
}
}

// Matched by program, so the stored name may differ from the one Lattis lists it under.
QVariantList Library::wineChoices(const HeroicWine &currentWine) const
{
    const LibrarySnapshot &s = m_store.snapshot();
    QVariantList wines;
    bool current = currentWine.bin.isEmpty();
    if (current)
        wines.append(QVariantMap{{QStringLiteral("label"), tr("Not set")}, {QStringLiteral("wine"), wineMap({})}});
    for (const HeroicWineChoice &choice : HeroicWines::find(s.heroic.install.root, s.steam.toolDirs,
                                                           QStandardPaths::findExecutable(QStringLiteral("wine")))) {
        const bool isCurrent = choice.wine.bin == currentWine.bin;
        current = current || isCurrent;
        wines.append(QVariantMap{{QStringLiteral("label"), choice.source == u"steam" ? tr("%1 (Steam)").arg(choice.wine.name)
                                                                                      : choice.wine.name},
                                 {QStringLiteral("wine"), wineMap(isCurrent ? currentWine : choice.wine)}});
    }
    if (!current) {
        wines.prepend(QVariantMap{{QStringLiteral("label"), tr("%1 (not installed)").arg(currentWine.name)},
                                  {QStringLiteral("wine"), wineMap(currentWine)}});
    }
    return wines;
}

QVariantMap Library::epicInstallWine(const QString &id) const
{
    const LibrarySnapshot &s = m_store.snapshot();
    const QString appName = epicAppName(id);
    HeroicGameSettings settings;
    if (appName.isEmpty() || !s.heroic.install.found() || !HeroicGameConfig::read(s.heroic.install.root, appName, settings))
        return {};
    QString prefixDir = HeroicGameConfig::defaultPrefixDir(s.heroic.install.root);
    if (prefixDir.isEmpty())
        prefixDir = QDir::homePath() + QStringLiteral("/Games/Heroic/Prefixes");
    QString shared = HeroicGameConfig::sharedPrefix(s.heroic.install.root);
    if (shared.isEmpty())
        shared = prefixDir + QStringLiteral("/default");
    return {{QStringLiteral("wines"), wineChoices(settings.wine)}, {QStringLiteral("wine"), wineMap(settings.wine)},
            {QStringLiteral("prefixDir"), prefixDir}, {QStringLiteral("sharedPrefix"), shared},
            {QStringLiteral("lastWine"), m_heroicSettings.epicInstallWine}};
}

QString Library::setEpicInstallSettings(const QString &id, const QVariantMap &wine, const QString &prefix)
{
    const QString root = m_store.snapshot().heroic.install.root;
    const QString appName = epicAppName(id);
    if (appName.isEmpty() || root.isEmpty())
        return {};
    HeroicGameSettings before;
    QString error;
    if (!HeroicGameConfig::read(root, appName, before, &error))
        return error;
    HeroicGameEdits edits;
    const HeroicGameSettings picked = heroicFromDraft({{QStringLiteral("wine"), wine}});
    if (!picked.wine.bin.isEmpty() && picked.wine.bin != before.wine.bin)
        edits.wine = picked.wine;
    if (!prefix.isEmpty())
        edits.winePrefix = prefix;
    return queueHeroicEdits(appName, edits);
}

QString Library::queueHeroicEdits(const QString &appName, const HeroicGameEdits &edits)
{
    if (HeroicGameConfig::isEmpty(edits))
        return {};
    if (m_heroicRunning()) {
        HeroicGameConfig::merge(m_heroicPending[appName], edits);
        saveHeroicPending();
        applyHeroicPending();
        updateHeroicWaiting();
        emit dataChanged(index(0), index(int(m_rows.size()) - 1));
        emit gamesChanged();
        return {};
    }
    return applyHeroicEdits(m_store.snapshot().heroic.install.root, appName, edits);
}

QVariantMap Library::heroicSettingsFor(const QString &id) const
{
    const LibrarySnapshot &s = m_store.snapshot();
    const QString appName = epicAppName(id);
    if (appName.isEmpty() || !s.heroic.install.found())
        return {};
    HeroicGameSettings settings;
    QString error;
    if (!HeroicGameConfig::read(s.heroic.install.root, appName, settings, &error))
        return {{QStringLiteral("error"), error}};
    HeroicGameConfig::apply(settings, m_heroicPending.value(appName));
    const QVariantList wines = wineChoices(settings.wine);

    QVariantList presets;
    for (const SteamPreset &p : m_steamSettings.presets) {
        presets.append(QVariantMap{{QStringLiteral("id"), p.id}, {QStringLiteral("name"), p.name},
                                   {QStringLiteral("adds"), p.describe()}});
    }
    return {{QStringLiteral("settings"), heroicDraft(settings)}, {QStringLiteral("wines"), wines},
            {QStringLiteral("presets"), presets}, {QStringLiteral("error"), QString()}};
}

bool Library::heroicPresetOn(const QVariantMap &settings, const QString &preset) const
{
    const SteamPreset *p = m_steamSettings.preset(preset);
    return p && HeroicPresets::isOn(heroicFromDraft(settings), *p);
}

QVariantMap Library::setHeroicPreset(const QVariantMap &settings, const QString &preset, bool on) const
{
    const SteamPreset *p = m_steamSettings.preset(preset);
    if (!p)
        return settings;
    HeroicGameSettings s = heroicFromDraft(settings);
    if (on)
        HeroicPresets::turnOn(s, *p);
    else
        HeroicPresets::turnOff(s, *p);
    QVariantMap out = heroicDraft(s);
    // The picked Wine is not the presets' to change.
    out.insert(QStringLiteral("wine"), settings.value(QStringLiteral("wine")));
    return out;
}

QString Library::saveHeroicSettings(const QString &id, const QVariantMap &settings)
{
    const QString root = m_store.snapshot().heroic.install.root;
    const QString appName = epicAppName(id);
    if (appName.isEmpty() || root.isEmpty())
        return {};
    HeroicGameSettings before;
    QString error;
    if (!HeroicGameConfig::read(root, appName, before, &error))
        return error;
    // What Properties showed includes what already waits for Heroic.
    HeroicGameConfig::apply(before, m_heroicPending.value(appName));
    const HeroicGameSettings after = heroicFromDraft(settings);

    HeroicGameEdits edits;
    if (after.launcherArgs.trimmed() != before.launcherArgs.trimmed())
        edits.launcherArgs = after.launcherArgs.trimmed();
    if (after.showMangohud != before.showMangohud)
        edits.showMangohud = after.showMangohud;
    if (after.useGameMode != before.useGameMode)
        edits.useGameMode = after.useGameMode;
    if (after.environment != before.environment)
        edits.environment = after.environment;
    if (after.wrappers != before.wrappers)
        edits.wrappers = after.wrappers;
    if (after.wine.bin != before.wine.bin && !after.wine.bin.isEmpty())
        edits.wine = after.wine;
    return queueHeroicEdits(appName, edits);
}

QString Library::applyHeroicEdits(const QString &root, const QString &appName, const HeroicGameEdits &edits)
{
    QString error;
    return HeroicGameConfig::isEmpty(edits) || HeroicGameConfig::write(root, appName, edits, &error) ? QString() : error;
}

QString Library::undoWaitingEdits(const QStringList &steamIds, const QStringList &heroicIds)
{
    bool heroicChanged = false;
    for (const QString &id : heroicIds)
        heroicChanged |= m_heroicPending.remove(epicAppName(id)) > 0;
    if (heroicChanged) {
        if (m_heroicPending.isEmpty())
            m_heroicPendingTimer.stop();
        saveHeroicPending();
        updateHeroicWaiting();
        emit dataChanged(index(0), index(int(m_rows.size()) - 1));
        emit gamesChanged();
    }
    QString error;
    if (!steamIds.isEmpty()) {
        m_store.beginBatch();
        for (const QString &key : steamIds) {
            if (const QString e = m_store.undoSteamQueue(key); error.isEmpty())
                error = e;
        }
        m_store.endBatch();
    }
    return error;
}

QString Library::pendingEditsName(const Game &g) const
{
    if (g.pendingEdits == PendingLauncher::Steam)
        return QStringLiteral("steam");
    if (!m_heroicPending.isEmpty() && g.launcher() == QLatin1String("heroic")
        && m_heroicPending.contains(epicAppName(g.id)))
        return QStringLiteral("heroic");
    return {};
}

void Library::setHeroicPendingFile(const QString &path)
{
    m_heroicPendingFile = path;
    m_heroicPending.clear();
    QFile file(path);
    if (!path.isEmpty() && file.open(QIODevice::ReadOnly)) {
        const QJsonObject all = QJsonDocument::fromJson(file.readAll()).object();
        for (auto it = all.begin(); it != all.end(); ++it) {
            const HeroicGameEdits edits = HeroicGameConfig::editsFromJson(it.value().toObject());
            if (!HeroicGameConfig::isEmpty(edits))
                m_heroicPending.insert(it.key(), edits);
        }
    }
    if (!m_heroicPending.isEmpty())
        applyHeroicPending();
    updateHeroicWaiting();
}

void Library::saveHeroicPending() const
{
    if (m_heroicPendingFile.isEmpty())
        return;
    if (m_heroicPending.isEmpty()) {
        QFile::remove(m_heroicPendingFile);
        return;
    }
    QJsonObject all;
    for (auto it = m_heroicPending.cbegin(); it != m_heroicPending.cend(); ++it)
        all.insert(it.key(), HeroicGameConfig::editsToJson(it.value()));
    QDir().mkpath(QFileInfo(m_heroicPendingFile).absolutePath());
    QSaveFile out(m_heroicPendingFile);
    if (!out.open(QIODevice::WriteOnly) || out.write(QJsonDocument(all).toJson()) < 0 || !out.commit())
        qCWarning(lcLibrary).noquote() << "could not save" << m_heroicPendingFile;
}

// Heroic writes a game's whole config back from memory, so edits made to the file while it runs are lost: they
// wait here, and are written when it has closed.
void Library::applyHeroicPending()
{
    if (m_heroicPending.isEmpty()) {
        m_heroicPendingTimer.stop();
        return;
    }
    const QString root = m_store.snapshot().heroic.install.root;
    if (root.isEmpty())
        return;
    if (m_heroicRunning()) {
        if (!m_heroicPendingTimer.isActive())
            m_heroicPendingTimer.start();
        return;
    }
    m_heroicPendingTimer.stop();
    const QHash<QString, HeroicGameEdits> waiting = std::exchange(m_heroicPending, {});
    for (auto it = waiting.cbegin(); it != waiting.cend(); ++it) {
        if (const QString error = applyHeroicEdits(root, it.key(), it.value()); !error.isEmpty()) {
            qCWarning(lcLibrary).noquote() << QStringLiteral("not applied to Heroic: %1").arg(error);
            m_heroicPending.insert(it.key(), it.value());
            continue;
        }
        qCInfo(lcLibrary).noquote() << QStringLiteral("%1: applied its waiting edits to Heroic").arg(it.key());
    }
    saveHeroicPending();
    updateHeroicWaiting();
    emit dataChanged(index(0), index(int(m_rows.size()) - 1));
    emit gamesChanged();
}

QVariantMap Library::epicUninstallChoices(const QString &id)
{
    const LibrarySnapshot &s = m_store.snapshot();
    const QString appName = epicAppName(id);
    if (appName.isEmpty() || !s.heroic.install.found())
        return {};
    const QString installDir = s.heroic.installed.value(id).installDir;
    const QString prefix = HeroicSaves::ownPrefix(s.heroic.install.root, appName);
    const Uninstall::Places places = uninstallPlaces(id);
    const QString refused = installDir.isEmpty() ? QString() : Uninstall::refusal(installDir, places);
    const QString prefixRefused = prefix.isEmpty() || !QFileInfo(prefix).isDir() ? QString()
                                                                                : Uninstall::refusal(prefix, places);

    const QString files = refused.isEmpty() ? installDir : QString();
    const QString prefixDir = prefixRefused.isEmpty() && QFileInfo(prefix).isDir() ? prefix : QString();
    QtConcurrent::run([files, prefixDir] {
        const auto size = [](const QString &path) {
            return path.isEmpty() ? QString()
                                  : QLocale().formattedDataSize(folderSize(path), 1, QLocale::DataSizeTraditionalFormat);
        };
        return std::pair(size(files), size(prefixDir));
    }).then(this, [this, id](const std::pair<QString, QString> &sizes) {
        emit uninstallSizes(id, sizes.first, sizes.second);
    });
    return {{QStringLiteral("installDir"), installDir.isEmpty() ? QString() : m_store.writtenPath(installDir)},
            {QStringLiteral("installDirRefused"), refused},
            {QStringLiteral("prefix"), prefixDir.isEmpty() ? QString() : m_store.writtenPath(prefixDir)},
            {QStringLiteral("prefixRefused"), prefixRefused}};
}

QString Library::epicInstallDir(const QString &id) const
{
    return m_store.snapshot().heroic.installed.value(id).installDir;
}

void Library::removeEpicLeftovers(const QString &id, const QString &installDir)
{
    if (installDir.isEmpty() || readOnly() || !QFileInfo(installDir).isDir())
        return;
    if (const QString why = Uninstall::refusal(installDir, uninstallPlaces(id)); !why.isEmpty()) {
        qCInfo(lcLibrary).noquote() << QStringLiteral("%1: leftovers kept in %2: %3").arg(id, installDir, why);
        return;
    }
    const Uninstall::Removal r = Uninstall::remove(installDir, true, m_trash);
    if (!r.error.isEmpty())
        qCWarning(lcLibrary).noquote() << QStringLiteral("%1: leftovers not removed: %2").arg(id, r.error);
    else
        qCInfo(lcLibrary).noquote() << QStringLiteral("%1: removed the leftovers in %2").arg(id, installDir);
}

QVariantMap Library::removeEpicPrefix(const QString &id, bool permanent)
{
    const auto withError = [](const QString &error) { return QVariantMap{{QStringLiteral("error"), error}}; };
    if (readOnly())
        return withError(tr("The library is read-only; nothing was deleted"));
    const LibrarySnapshot &s = m_store.snapshot();
    const QString appName = epicAppName(id);
    const QString prefix = appName.isEmpty() ? QString() : HeroicSaves::ownPrefix(s.heroic.install.root, appName);
    if (prefix.isEmpty() || !QFileInfo(prefix).isDir())
        return withError(tr("%1 has no Wine prefix of its own").arg(s.content.games.value(id).title));
    if (const QString why = Uninstall::refusal(prefix, uninstallPlaces(id)); !why.isEmpty())
        return withError(tr("The Wine prefix can't be deleted: %1").arg(why));
    const Uninstall::Removal r = Uninstall::remove(prefix, permanent, m_trash);
    if (r.needsPermanent)
        return {{QStringLiteral("trashless"), m_store.writtenPath(prefix)}};
    if (!r.error.isEmpty()) {
        qCWarning(lcLibrary).noquote() << QStringLiteral("%1: prefix not removed: %2").arg(id, r.error);
        return withError(r.error);
    }
    qCInfo(lcLibrary).noquote() << QStringLiteral("%1: removed %2").arg(id, prefix);
    return {};
}

namespace {
// A manager's folder is its program's; a Flatpak app id or a bare command has none.
QString managerFolder(const ModManager &manager)
{
    if (!QFileInfo(manager.exe).isAbsolute() || !QFileInfo(manager.exe).isFile())
        return {};
    return QFileInfo(manager.exe).absolutePath();
}
}

// Never offered while the manager runs in a game's prefix: that one is the game's.
QVariantMap Library::modManagerUninstallChoices(const QString &id)
{
    const auto managers = m_store.snapshot().content.modManagers;
    const auto it = managers.constFind(id);
    if (it == managers.constEnd())
        return {};
    const bool shortcut = it->isProton() && it->shortcut > 0 && it->inSteam;
    const QString folder = managerFolder(*it);
    const QString prefix = it->isProton() && !m_store.modManagerPrefix(id) ? protonPrefix(it->shortcut) : QString();
    const QString refused = folder.isEmpty() ? QString() : Uninstall::refusal(folder, uninstallPlaces({}));

    const QString files = refused.isEmpty() ? folder : QString();
    const QString key = ModManagers::kKeyPrefix + id;
    QtConcurrent::run([files, prefix] {
        const auto size = [](const QString &path) {
            return path.isEmpty() ? QString()
                                  : QLocale().formattedDataSize(folderSize(path), 1, QLocale::DataSizeTraditionalFormat);
        };
        return std::pair(size(files), size(prefix));
    }).then(this, [this, key](const std::pair<QString, QString> &sizes) {
        emit uninstallSizes(key, sizes.first, sizes.second);
    });
    return {{QStringLiteral("shortcut"), shortcut},
            {QStringLiteral("installDir"), folder.isEmpty() ? QString() : m_store.writtenPath(folder)},
            {QStringLiteral("installDirRefused"), refused},
            {QStringLiteral("prefix"), prefix.isEmpty() ? QString() : m_store.writtenPath(prefix)}};
}

QVariantMap Library::uninstallModManager(const QString &id, bool files, bool prefix, bool shortcut, bool permanent)
{
    const auto withError = [](const QString &error) { return QVariantMap{{QStringLiteral("error"), error}}; };
    const auto managers = m_store.snapshot().content.modManagers;
    const auto it = managers.constFind(id);
    if (it == managers.constEnd())
        return withError(tr("%1 isn't a mod manager").arg(id));
    const ModManager manager = *it;
    if (readOnly())
        return withError(tr("The library is read-only; nothing was deleted"));

    QStringList folders;
    const Uninstall::Places places = uninstallPlaces({});
    if (files) {
        const QString dir = managerFolder(manager);
        if (dir.isEmpty()) {
            // Gone already counts as done: the call is repeated after a refused trash.
            if (!prefix)
                return withError(tr("%1's folder is gone already").arg(manager.name));
        } else {
            if (const QString why = Uninstall::refusal(dir, places); !why.isEmpty())
                return withError(tr("The folder can't be deleted: %1").arg(why));
            folders.append(dir);
        }
    }
    if (prefix) {
        if (m_store.modManagerPrefix(id))
            return withError(tr("%1 runs in a game's prefix; that one isn't deleted here").arg(manager.name));
        const QString dir = manager.isProton() ? protonPrefix(manager.shortcut) : QString();
        if (dir.isEmpty())
            return withError(tr("%1 has no Wine prefix").arg(manager.name));
        if (const QString why = Uninstall::refusal(dir, places); !why.isEmpty())
            return withError(tr("The Wine prefix can't be deleted: %1").arg(why));
        folders.append(dir);
    }
    if (const QString refused = trashRefusals(folders, permanent); !refused.isEmpty())
        return {{QStringLiteral("trashless"), refused}};
    for (const QString &folder : std::as_const(folders)) {
        const Uninstall::Removal r = Uninstall::remove(folder, permanent, m_trash);
        if (r.needsPermanent)
            return {{QStringLiteral("trashless"), m_store.writtenPath(folder)}};
        if (!r.error.isEmpty()) {
            qCWarning(lcLibrary).noquote() << QStringLiteral("%1: not removed: %2").arg(id, r.error);
            return withError(tr("%1. Nothing after it was done.").arg(r.error));
        }
        qCInfo(lcLibrary).noquote() << QStringLiteral("%1: removed %2").arg(id, folder);
    }
    // Its folder goes with the program; the shortcut would have nothing to run.
    ManagerIcon::forget(m_cacheDir, id);
    return withError(m_store.removeModManager(id, manager.isProton() && (shortcut || files)));
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
