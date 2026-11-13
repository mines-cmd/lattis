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
    const Game *active = nullptr; // the installed copy, the main one when several are
    int longest = 0;
    QDateTime longestEnd;
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

// Every installed copy takes its own space, so linked copies are all counted.
void addStorage(Overview &out, const QList<Game> &games, const QList<Drive> &drives)
{
    QList<DriveUse> uses;
    QHash<QString, int> useOfDevice;
    for (const Game &g : games) {
        if (g.hidden || !g.installed)
            continue;
        addBytes(out.platformSizes, g.platform, g.sizeBytes);
        if (g.sizeBytes > 0)
            out.largest.append({g.id, g.title, g.platform, g.coverUrl, g.sizeBytes, g.lastPlayed});

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
    out.largest.resize(qMin<qsizetype>(out.largest.size(), kStorageListLimit));
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

// Facts about a copy (an update, a ROM, an emulator) count per copy, since each is its own thing to fix; facts about the
// game (its art, its info, its saves) count a linked group once, by its shown copy.
void addNextSteps(Overview &out, const QList<Game> &games, const QList<Unit> &units,
                  const std::optional<QSet<QString>> &backedUp)
{
    int neverBackedUp = 0;
    int missingArt = 0;
    int missingInfo = 0;
    for (const Unit &unit : units) {
        const Game &g = *unit.shown;
        if (g.missingArt)
            ++missingArt;
        if (g.missingInfo)
            ++missingInfo;
        const bool played = (unit.grouped ? g.groupPlayMinutes : g.playMinutes) > 0
            || (unit.grouped ? g.groupLastPlayed : g.lastPlayed).isValid();
        if (backedUp && played && unit.installed && !g.backupKey.isEmpty() && !backedUp->contains(g.backupKey))
            ++neverBackedUp;
        if (unit.installed && g.status == QLatin1String("not_started"))
            out.backlog.append({g.id, g.title, g.platform, g.coverUrl, g.year});
    }
    int updates = 0;
    int romsNotFound = 0;
    int emulatorsMissing = 0;
    for (const Game &g : games) {
        if (g.hidden)
            continue;
        if (g.updateWaiting)
            ++updates;
        if (g.rom && !g.installed)
            ++romsNotFound;
        if (g.emulatorMissing)
            ++emulatorsMissing;
    }
    const std::pair<const char *, int> rows[] = {{"neverBackedUp", neverBackedUp}, {"missingArt", missingArt},
                                                 {"missingInfo", missingInfo},     {"updates", updates},
                                                 {"romsNotFound", romsNotFound},   {"emulatorsMissing", emulatorsMissing}};
    for (const auto &[kind, count] : rows) {
        if (count > 0)
            out.attention.append({QLatin1String(kind), count});
    }
    std::sort(out.backlog.begin(), out.backlog.end(), [](const Pick &a, const Pick &b) { return a.id < b.id; });
}

// A linked group counts once, by the copy with the most achievements unlocked, since that is the one played.
Achievements achievementsOf(const QList<Game> &games, const AchievementCounts &counts)
{
    QHash<QString, const Game *> best;
    for (const Game &g : games) {
        if (g.hidden || g.steamAppId <= 0 || counts.value(g.steamAppId).total <= 0)
            continue;
        const AchievementCount c = counts.value(g.steamAppId);
        const QString key = g.groupId.isEmpty() ? g.id : g.groupId;
        const auto it = best.constFind(key);
        if (it == best.cend() || c.unlocked > counts.value((*it)->steamAppId).unlocked
            || (c.unlocked == counts.value((*it)->steamAppId).unlocked && g.id < (*it)->id))
            best.insert(key, &g);
    }

    Achievements out;
    for (const Game *g : std::as_const(best)) {
        const AchievementCount c = counts.value(g->steamAppId);
        ++out.games;
        out.unlocked += c.unlocked;
        out.total += c.total;
        if (c.unlocked >= c.total)
            ++out.perfect;
        else if (c.unlocked * 100 >= c.total * kNearlyDonePercent)
            out.nearlyDone.append({g->id, g->title, g->coverUrl, c.unlocked, c.total});
    }
    // Cross-multiplied, so two fractions compare exactly; ties by title.
    std::sort(out.nearlyDone.begin(), out.nearlyDone.end(), [](const NearlyDone &a, const NearlyDone &b) {
        if (const qint64 byShare = qint64(a.unlocked) * b.total - qint64(b.unlocked) * a.total)
            return byShare > 0;
        if (const int byTitle = a.title.compare(b.title, Qt::CaseInsensitive))
            return byTitle < 0;
        return a.id < b.id;
    });
    out.nearlyDone.resize(qMin<qsizetype>(out.nearlyDone.size(), kNearlyDoneLimit));
    return out;
}

const QStringList kTierOrder{QStringLiteral("native"), QStringLiteral("platinum"), QStringLiteral("gold"),
                             QStringLiteral("silver"), QStringLiteral("bronze"), QStringLiteral("borked"),
                             QStringLiteral("pending")};

// A Steam game with no tool of its own is native when ProtonDB says so, or when Steam has no tool to run it with;
// otherwise a tier ProtonDB knows means it runs under Steam's default tool. No tier yet leaves it uncounted.
void addCompatibility(Overview &out, const QList<Unit> &units, const Sources &sources)
{
    Compatibility &c = out.compatibility;
    QHash<QString, int> tiers;
    QHash<QString, int> tools;
    for (const Unit &unit : units) {
        const Game *g = unit.active;
        if (!g)
            continue;
        QString tier;
        if (g->platform == QLatin1String("steam") && g->steamAppId > 0) {
            ++c.games;
            const auto it = sources.protonTiers.constFind(g->steamAppId);
            if (it == sources.protonTiers.cend())
                ++c.unchecked;
            else if (*it == QLatin1String("none"))
                ++c.noReports;
            else
                ++tiers[kTierOrder.contains(*it) ? *it : QStringLiteral("pending")];
            tier = it == sources.protonTiers.cend() ? QString() : *it;
        }

        QString runner = g->runner;
        QString tool = g->tool;
        if (runner == QLatin1String("steam")) {
            if (tier == QLatin1String("native") || sources.defaultTool.isEmpty()) {
                runner = QStringLiteral("native");
            } else if (!tier.isEmpty()) {
                runner = QStringLiteral("proton");
                tool = sources.defaultTool;
            }
        }
        if (runner == QLatin1String("native")) {
            ++c.native;
        } else if (runner == QLatin1String("proton")) {
            ++c.proton;
            ++tools[tool];
        }
    }
    for (const QString &tier : kTierOrder) {
        if (tiers.value(tier) > 0)
            c.tiers.append({tier, tiers.value(tier)});
    }
    for (auto it = tools.cbegin(); it != tools.cend(); ++it) {
        if (it.value() > c.topToolGames || (it.value() == c.topToolGames && it.key() < c.topTool)) {
            c.topTool = it.key();
            c.topToolGames = it.value();
        }
    }
}

// Coverage is of installed, played games whose saves can be backed up, as Needs attention counts; the archives and their space
// are the whole backup folder's, emulators' included.
void addSaves(Overview &out, const QList<Unit> &units, const std::optional<QSet<QString>> &backedUp,
              const std::optional<SaveBackups> &saves)
{
    if (!saves)
        return;
    SaveCoverage coverage;
    coverage.archives = saves->archives;
    coverage.bytes = saves->bytes;
    coverage.last = saves->last;
    for (const Unit &unit : units) {
        const Game &g = *unit.shown;
        const bool played = (unit.grouped ? g.groupPlayMinutes : g.playMinutes) > 0
            || (unit.grouped ? g.groupLastPlayed : g.lastPlayed).isValid();
        if (!played || !unit.installed || g.backupKey.isEmpty())
            continue;
        ++coverage.played;
        if (backedUp && backedUp->contains(g.backupKey))
            ++coverage.backedUp;
    }
    out.saves = coverage;
}

// The game of a linked group is the shown copy; its sessions are the group's.
void addFacts(Overview &out, const QList<Unit> &units)
{
    for (const Unit &unit : units) {
        const Game &g = *unit.shown;
        if (unit.longest > 0) {
            const bool better = !out.longest || unit.longest > out.longest->minutes
                || (unit.longest == out.longest->minutes && unit.longestEnd > out.longest->end);
            if (better)
                out.longest = LongestSession{g.id, g.title, g.coverUrl, unit.longest, unit.longestEnd};
        }
        if (g.status != QLatin1String("not_started") || !g.added.isValid())
            continue;
        const bool older = !out.waiting || g.added < out.waiting->added
            || (g.added == out.waiting->added && g.title.compare(out.waiting->title, Qt::CaseInsensitive) < 0);
        if (older)
            out.waiting = Waiting{g.id, g.title, g.coverUrl, g.added};
    }
}
} // namespace

Overview overview(const QList<Game> &games, const QList<Drive> &drives, const QDateTime &now,
                  const std::optional<QSet<QString>> &backedUp, const std::optional<AchievementCounts> &achievements, const Sources &sources)
{
    QList<Unit> units;
    QHash<QString, int> groupUnit;
    QSet<QString> platforms;
    for (const Game &g : games) {
        if (g.hidden)
            continue;
        platforms.insert(g.platform);
        if (g.groupId.isEmpty()) {
            units.append({&g, false, g.installed, g.installed ? &g : nullptr, g.longestSession, g.longestEnd});
            continue;
        }
        const auto it = groupUnit.constFind(g.groupId);
        if (it == groupUnit.cend()) {
            groupUnit.insert(g.groupId, int(units.size()));
            units.append({&g, true, g.installed, g.installed ? &g : nullptr, g.longestSession, g.longestEnd});
            continue;
        }
        Unit &unit = units[*it];
        unit.installed = unit.installed || g.installed;
        if (g.installed && (!unit.active || g.id == g.groupId))
            unit.active = &g;
        if (g.longestSession > unit.longest || (g.longestSession == unit.longest && g.longestEnd > unit.longestEnd)) {
            unit.longest = g.longestSession;
            unit.longestEnd = g.longestEnd;
        }
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

    addStorage(out, games, drives);
    addBreakdowns(out, games, units);
    addNextSteps(out, games, units, backedUp);
    addCompatibility(out, units, sources);
    addSaves(out, units, backedUp, sources.saves);
    addFacts(out, units);
    if (achievements)
        out.achievements = achievementsOf(games, *achievements);
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
                                   {QStringLiteral("bytes"), s.bytes},
                                   {QStringLiteral("lastPlayed"), s.lastPlayed}});
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
    QVariantList attention;
    for (const Attention &a : overview.attention)
        attention.append(QVariantMap{{QStringLiteral("kind"), a.kind}, {QStringLiteral("count"), a.count}});
    QVariantList backlog;
    for (const Pick &p : overview.backlog)
        backlog.append(QVariantMap{{QStringLiteral("id"), p.id},
                                   {QStringLiteral("title"), p.title},
                                   {QStringLiteral("platform"), p.platform},
                                   {QStringLiteral("coverUrl"), p.coverUrl},
                                   {QStringLiteral("year"), p.year}});
    QVariantList nearlyDone;
    QVariantMap achievements;
    if (overview.achievements) {
        const Achievements &a = *overview.achievements;
        for (const NearlyDone &n : a.nearlyDone)
            nearlyDone.append(QVariantMap{{QStringLiteral("id"), n.id},
                                          {QStringLiteral("title"), n.title},
                                          {QStringLiteral("coverUrl"), n.coverUrl},
                                          {QStringLiteral("unlocked"), n.unlocked},
                                          {QStringLiteral("total"), n.total}});
        achievements = {{QStringLiteral("games"), a.games},
                        {QStringLiteral("unlocked"), a.unlocked},
                        {QStringLiteral("total"), a.total},
                        {QStringLiteral("perfect"), a.perfect}};
    }
    const Compatibility &comp = overview.compatibility;
    QVariantList tiers;
    for (const TierCount &t : comp.tiers)
        tiers.append(QVariantMap{{QStringLiteral("tier"), t.tier}, {QStringLiteral("count"), t.count}});
    const QVariantMap compatibility{{QStringLiteral("games"), comp.games},
                                    {QStringLiteral("noReports"), comp.noReports},
                                    {QStringLiteral("unchecked"), comp.unchecked},
                                    {QStringLiteral("tiers"), tiers},
                                    {QStringLiteral("native"), comp.native},
                                    {QStringLiteral("proton"), comp.proton},
                                    {QStringLiteral("topTool"), comp.topTool},
                                    {QStringLiteral("topToolGames"), comp.topToolGames}};
    const Totals &t = overview.totals;
    QVariantMap map{{QStringLiteral("compatibility"), compatibility},
            {QStringLiteral("nearlyDone"), nearlyDone}};
    if (overview.achievements)
        map.insert(QStringLiteral("achievements"), achievements);
    if (overview.saves) {
        const SaveCoverage &c = *overview.saves;
        map.insert(QStringLiteral("saves"), QVariantMap{{QStringLiteral("played"), c.played},
                                                        {QStringLiteral("backedUp"), c.backedUp},
                                                        {QStringLiteral("archives"), c.archives},
                                                        {QStringLiteral("bytes"), c.bytes},
                                                        {QStringLiteral("last"), c.last}});
    }
    if (overview.longest) {
        const LongestSession &l = *overview.longest;
        map.insert(QStringLiteral("longest"), QVariantMap{{QStringLiteral("id"), l.id},
                                                          {QStringLiteral("title"), l.title},
                                                          {QStringLiteral("coverUrl"), l.coverUrl},
                                                          {QStringLiteral("minutes"), l.minutes},
                                                          {QStringLiteral("end"), l.end}});
    }
    if (overview.waiting) {
        const Waiting &w = *overview.waiting;
        map.insert(QStringLiteral("waiting"), QVariantMap{{QStringLiteral("id"), w.id},
                                                          {QStringLiteral("title"), w.title},
                                                          {QStringLiteral("coverUrl"), w.coverUrl},
                                                          {QStringLiteral("added"), w.added}});
    }
    map.insert(QVariantMap{{QStringLiteral("attention"), attention},
            {QStringLiteral("backlogPicks"), backlog},
            {QStringLiteral("platformCounts"), platformCounts},
            {QStringLiteral("statusCounts"), statusCounts},
            {QStringLiteral("topTags"), topTags},
            {QStringLiteral("years"), years},
            {QStringLiteral("played"), t.played},
{QStringLiteral("drives"), drives},
            {QStringLiteral("platformSizes"), sizes(overview.platformSizes)},
            {QStringLiteral("largest"), rows(overview.largest)},
            {QStringLiteral("games"), t.games},
            {QStringLiteral("installed"), t.installed},
            {QStringLiteral("platforms"), t.platforms},
            {QStringLiteral("minutes"), t.minutes},
            {QStringLiteral("backlog"), t.backlog},
            {QStringLiteral("continuePlaying"), playing}});
    return map;
}
} // namespace LibraryOverview
