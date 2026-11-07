#include "libraryoverview.h"

#include <QHash>
#include <QSet>

#include <algorithm>

namespace LibraryOverview {
namespace {
// What a game or a group of linked copies counts as: one entry, under its main copy or else the first shown one.
struct Unit
{
    const Game *shown = nullptr;
    bool grouped = false;
    bool installed = false;
};

bool within(const QString &path, const QString &root)
{
    return root == QLatin1String("/") ? path.startsWith(u'/') : path == root || path.startsWith(root + u'/');
}

// The deepest mount holding the path, so /mnt/games wins over /.
const Drive *driveOf(const QString &path, const QList<Drive> &drives)
{
    const Drive *best = nullptr;
    for (const Drive &d : drives) {
        if (within(path, d.root) && (!best || d.root.size() > best->root.size()))
            best = &d;
    }
    return best;
}

void addBytes(QList<PlatformSize> &list, const QString &platform, qint64 bytes)
{
    for (PlatformSize &p : list) {
        if (p.platform == platform) {
            if (bytes > 0)
                p.bytes += bytes;
            else
                ++p.unknown;
            return;
        }
    }
    list.append({platform, qMax<qint64>(bytes, 0), bytes > 0 ? 0 : 1});
}

void sortBySize(QList<PlatformSize> &list)
{
    std::sort(list.begin(), list.end(), [](const PlatformSize &a, const PlatformSize &b) {
        return a.bytes != b.bytes ? a.bytes > b.bytes : a.platform < b.platform;
    });
}

bool bySizeThenTitle(const Sized &a, const Sized &b)
{
    if (a.bytes != b.bytes)
        return a.bytes > b.bytes;
    if (const int byTitle = a.title.compare(b.title, Qt::CaseInsensitive))
        return byTitle < 0;
    return a.id < b.id;
}

// Every installed copy takes its own space, so linked copies are all counted; a linked copy is dormant by the
// group's last play, since playing it anywhere keeps it in use.
void addStorage(Overview &out, const QList<Game> &games, const QList<Drive> &drives, const QDateTime &now)
{
    const QDateTime cutoff = now.addMonths(-kDormantMonths);
    QList<DriveUse> uses;
    QHash<QString, int> useOfDevice;
    for (const Game &g : games) {
        if (g.hidden || !g.installed)
            continue;
        addBytes(out.platformSizes, g.platform, g.sizeBytes);
        const QDateTime lastPlayed = g.groupId.isEmpty() ? g.lastPlayed : g.groupLastPlayed;
        const Sized row{g.id, g.title, g.platform, g.coverUrl, lastPlayed, qMax<qint64>(g.sizeBytes, 0), g.uninstallable};
        if (row.bytes > 0)
            out.largest.append(row);
        if (!lastPlayed.isValid() || lastPlayed < cutoff)
            out.dormant.append(row);

        const Drive *drive = g.path.isEmpty() ? nullptr : driveOf(g.path, drives);
        if (!drive)
            continue;
        const QString key = drive->device.isEmpty() ? drive->root : drive->device;
        auto it = useOfDevice.find(key);
        if (it == useOfDevice.end()) {
            it = useOfDevice.insert(key, int(uses.size()));
            uses.append({*drive, {}, 0, 0, 0});
        }
        DriveUse &use = uses[*it];
        addBytes(use.platforms, g.platform, g.sizeBytes);
        if (g.sizeBytes > 0)
            use.gamesBytes += g.sizeBytes;
        else
            ++use.unknown;
    }

    for (DriveUse &use : uses) {
        sortBySize(use.platforms);
        use.otherBytes = qMax<qint64>(0, use.drive.total - use.drive.free - use.gamesBytes);
    }
    std::stable_sort(uses.begin(), uses.end(), [](const DriveUse &a, const DriveUse &b) { return a.gamesBytes > b.gamesBytes; });
    out.drives = uses;
    sortBySize(out.platformSizes);

    std::sort(out.largest.begin(), out.largest.end(), bySizeThenTitle);
    std::sort(out.dormant.begin(), out.dormant.end(), bySizeThenTitle);
    out.largest.resize(qMin<qsizetype>(out.largest.size(), kStorageListLimit));
    out.dormant.resize(qMin<qsizetype>(out.dormant.size(), kStorageListLimit));
}

// Platforms count each copy where it is; statuses, tags and years count a linked group once, by its shown copy.
void addBreakdowns(Overview &out, const QList<Game> &games, const QList<Unit> &units)
{
    QHash<QString, PlatformCount> byPlatform;
    for (const Game &g : games) {
        if (g.hidden)
            continue;
        PlatformCount &c = byPlatform[g.platform];
        c.platform = g.platform;
        ++c.games;
        if (g.installed)
            ++c.installed;
    }
    out.platformCounts = byPlatform.values();
    std::sort(out.platformCounts.begin(), out.platformCounts.end(), [](const PlatformCount &a, const PlatformCount &b) {
        return a.games != b.games ? a.games > b.games : a.platform < b.platform;
    });

    static const QStringList statusOrder{QStringLiteral("playing"), QStringLiteral("not_started"), QStringLiteral("finished"),
                                         QStringLiteral("completed"), QStringLiteral("dropped"), QStringLiteral("none")};
    QHash<QString, int> statuses;
    QHash<QString, int> tags;
    QHash<int, int> years;
    for (const Unit &unit : units) {
        const Game &g = *unit.shown;
        ++statuses[statusOrder.contains(g.status) ? g.status : QStringLiteral("none")];
        const QStringList unique = QSet<QString>(g.tags.cbegin(), g.tags.cend()).values();
        for (const QString &tag : unique)
            ++tags[tag];
        if (g.year > 0)
            ++years[g.year];
    }
    for (const QString &status : statusOrder) {
        if (statuses.value(status) > 0)
            out.statusCounts.append({status, statuses.value(status)});
    }

    for (auto it = tags.cbegin(); it != tags.cend(); ++it)
        out.topTags.append({it.key(), it.value()});
    std::sort(out.topTags.begin(), out.topTags.end(), [](const TagCount &a, const TagCount &b) {
        if (a.count != b.count)
            return a.count > b.count;
        if (const int byName = a.tag.compare(b.tag, Qt::CaseInsensitive))
            return byName < 0;
        return a.tag < b.tag;
    });
    out.topTags.resize(qMin<qsizetype>(out.topTags.size(), kTopTagLimit));

    if (years.isEmpty())
        return;
    QList<int> keys = years.keys();
    std::sort(keys.begin(), keys.end());
    const int first = *std::min_element(keys.cbegin(), keys.cend());
    const int last = *std::max_element(keys.cbegin(), keys.cend());
    if (last - first < kYearSpanLimit) {
        for (int year = first; year <= last; ++year)
            out.years.append({year, years.value(year)});
    } else {
        for (int year : std::as_const(keys))
            out.years.append({year, years.value(year)});
    }
}
} // namespace

Overview overview(const QList<Game> &games, const QList<Drive> &drives, const QDateTime &now)
{
    QList<Unit> units;
    QHash<QString, int> groupUnit;
    QSet<QString> platforms;
    for (const Game &g : games) {
        if (g.hidden)
            continue;
        platforms.insert(g.platform);
        if (g.groupId.isEmpty()) {
            units.append({&g, false, g.installed});
            continue;
        }
        const auto it = groupUnit.constFind(g.groupId);
        if (it == groupUnit.cend()) {
            groupUnit.insert(g.groupId, int(units.size()));
            units.append({&g, true, g.installed});
            continue;
        }
        Unit &unit = units[*it];
        unit.installed = unit.installed || g.installed;
        if (g.id == g.groupId)
            unit.shown = &g;
    }

    Overview out;
    out.totals.games = int(units.size());
    out.totals.platforms = int(platforms.size());
    for (const Unit &unit : units) {
        const Game &g = *unit.shown;
        if (unit.installed)
            ++out.totals.installed;
        const int minutes = unit.grouped ? g.groupPlayMinutes : g.playMinutes;
        out.totals.minutes += minutes;
        if (minutes > 0 || (unit.grouped ? g.groupLastPlayed : g.lastPlayed).isValid())
            ++out.totals.played;
        if (g.status == QLatin1String("not_started"))
            ++out.totals.backlog;
        const QDateTime lastPlayed = unit.grouped ? g.groupLastPlayed : g.lastPlayed;
        if (unit.installed && lastPlayed.isValid())
            out.continuePlaying.append({g.id, g.title, g.coverUrl, lastPlayed});
    }

    // Ties by title, so the order doesn't depend on the library's.
    std::sort(out.continuePlaying.begin(), out.continuePlaying.end(), [](const Counted &a, const Counted &b) {
        if (a.lastPlayed != b.lastPlayed)
            return a.lastPlayed > b.lastPlayed;
        if (const int byTitle = a.title.compare(b.title, Qt::CaseInsensitive))
            return byTitle < 0;
        return a.id < b.id;
    });
    if (out.continuePlaying.size() > kContinueLimit)
        out.continuePlaying.resize(kContinueLimit);

    addStorage(out, games, drives, now);
    addBreakdowns(out, games, units);
    return out;
}

QVariantMap toVariantMap(const Overview &overview)
{
    QVariantList playing;
    for (const Counted &c : overview.continuePlaying)
        playing.append(QVariantMap{{QStringLiteral("id"), c.id},
                                   {QStringLiteral("title"), c.title},
                                   {QStringLiteral("coverUrl"), c.coverUrl},
                                   {QStringLiteral("lastPlayed"), c.lastPlayed}});
    const auto sizes = [](const QList<PlatformSize> &list) {
        QVariantList out;
        for (const PlatformSize &p : list)
            out.append(QVariantMap{{QStringLiteral("platform"), p.platform},
                                   {QStringLiteral("bytes"), p.bytes},
                                   {QStringLiteral("unknown"), p.unknown}});
        return out;
    };
    const auto rows = [](const QList<Sized> &list) {
        QVariantList out;
        for (const Sized &s : list)
            out.append(QVariantMap{{QStringLiteral("id"), s.id},
                                   {QStringLiteral("title"), s.title},
                                   {QStringLiteral("platform"), s.platform},
                                   {QStringLiteral("coverUrl"), s.coverUrl},
                                   {QStringLiteral("lastPlayed"), s.lastPlayed},
                                   {QStringLiteral("bytes"), s.bytes},
                                   {QStringLiteral("uninstallable"), s.uninstallable}});
        return out;
    };
    QVariantList drives;
    for (const DriveUse &use : overview.drives)
        drives.append(QVariantMap{{QStringLiteral("name"), use.drive.name},
                                  {QStringLiteral("root"), use.drive.root},
                                  {QStringLiteral("total"), use.drive.total},
                                  {QStringLiteral("free"), use.drive.free},
                                  {QStringLiteral("gamesBytes"), use.gamesBytes},
                                  {QStringLiteral("otherBytes"), use.otherBytes},
                                  {QStringLiteral("unknown"), use.unknown},
                                  {QStringLiteral("platforms"), sizes(use.platforms)}});
    QVariantList platformCounts;
    for (const PlatformCount &c : overview.platformCounts)
        platformCounts.append(QVariantMap{{QStringLiteral("platform"), c.platform},
                                          {QStringLiteral("games"), c.games},
                                          {QStringLiteral("installed"), c.installed}});
    QVariantList statusCounts;
    for (const StatusCount &c : overview.statusCounts)
        statusCounts.append(QVariantMap{{QStringLiteral("status"), c.status}, {QStringLiteral("count"), c.count}});
    QVariantList topTags;
    for (const TagCount &c : overview.topTags)
        topTags.append(QVariantMap{{QStringLiteral("tag"), c.tag}, {QStringLiteral("count"), c.count}});
    QVariantList years;
    for (const YearCount &c : overview.years)
        years.append(QVariantMap{{QStringLiteral("year"), c.year}, {QStringLiteral("count"), c.count}});
    const Totals &t = overview.totals;
    return {{QStringLiteral("platformCounts"), platformCounts},
            {QStringLiteral("statusCounts"), statusCounts},
            {QStringLiteral("topTags"), topTags},
            {QStringLiteral("years"), years},
            {QStringLiteral("played"), t.played},
{QStringLiteral("drives"), drives},
            {QStringLiteral("platformSizes"), sizes(overview.platformSizes)},
            {QStringLiteral("largest"), rows(overview.largest)},
            {QStringLiteral("dormant"), rows(overview.dormant)},
            {QStringLiteral("games"), t.games},
            {QStringLiteral("installed"), t.installed},
            {QStringLiteral("platforms"), t.platforms},
            {QStringLiteral("minutes"), t.minutes},
            {QStringLiteral("backlog"), t.backlog},
            {QStringLiteral("continuePlaying"), playing}};
}
} // namespace LibraryOverview
