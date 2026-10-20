#include "bulklookup.h"
#include "igdb.h"
#include "metadataclient.h"
#include "searchtext.h"

#include <QTimer>

#include <algorithm>
#include <cmath>

namespace {
QString category(const QString &missing)
{
    return missing == QLatin1String("description") || missing == QLatin1String("year") || missing == QLatin1String("rating")
        ? missing
        : QStringLiteral("art");
}

QString fieldName(const QString &key)
{
    return key == QLatin1String("description") ? QObject::tr("Description")
         : key == QLatin1String("year")        ? QObject::tr("Year")
                                               : QObject::tr("Rating");
}

QVariant currentValue(const LookupGame &game, const QString &key)
{
    if (key == QLatin1String("year"))
        return game.year;
    if (key == QLatin1String("rating"))
        return game.rating;
    return game.description;
}

QString kindName(const QString &kind)
{
    static const QHash<QString, QString> names = {{QStringLiteral("cover"), QObject::tr("Cover")},
                                                  {QStringLiteral("hero"), QObject::tr("Hero")},
                                                  {QStringLiteral("logo"), QObject::tr("Logo")},
                                                  {QStringLiteral("icon"), QObject::tr("Icon")}};
    return names.value(kind, kind);
}

const QStringList kKinds = {QStringLiteral("cover"), QStringLiteral("hero"), QStringLiteral("logo"), QStringLiteral("icon")};

constexpr int kWriteBatch = 50;
constexpr int kArtBatch = 20;
constexpr int kDownloads = 4;
}

BulkLookup::BulkLookup(QObject *parent)
    : BulkLookup(MetadataClient::instance(), s_source ? s_source : [] { return QList<LookupGame>(); }, s_writeValues,
                 s_writeArt, s_tagMappings, parent)
{
}

void BulkLookup::setLibrary(Source source, ValueWriter values, ArtWriter art, TagMappings tags)
{
    s_source = std::move(source);
    s_writeValues = std::move(values);
    s_writeArt = std::move(art);
    s_tagMappings = std::move(tags);
}

BulkLookup::BulkLookup(MetadataClient *client, Source source, ValueWriter values, ArtWriter art, TagMappings tags,
                       QObject *parent)
    : QObject(parent), m_client(client), m_source(std::move(source)), m_writeValues(std::move(values)),
      m_writeArt(std::move(art)), m_tagMappings(std::move(tags))
{
    connect(m_client, &MetadataClient::finished, this, &BulkLookup::finished);
    connect(this, &BulkLookup::gamesChanged, this, [this] { m_gameModel.setItems(m_shown); });
    connect(this, &BulkLookup::resultsChanged, this, [this] { m_resultModel.setItems(resultList(m_resultFilter)); });
    reload();
}

void BulkLookup::reload()
{
    m_games = m_source();
    for (const LookupGame &g : std::as_const(m_games)) {
        if (!m_seen.contains(g.id)) {
            m_seen.insert(g.id);
            if (!g.missing().isEmpty() && g.igdb != 0)
                m_checked.insert(g.id);
        }
    }
    refilter();
}

bool BulkLookup::listed(const LookupGame &g) const
{
    return m_replaceExisting || m_includeComplete || !g.missing().isEmpty();
}

// The filters that choose games, less those in `skip`. A section's counts skip their own, so each
// choice shows what picking it would list, not what's left once it's picked.
bool BulkLookup::matches(const LookupGame &g, int skip) const
{
    if (!listed(g) || (g.igdb == 0 && !m_includeNotOnIgdb && !(skip & NotOnIgdbSection)))
        return false;
    if (!(skip & PlatformSection) && !m_platform.isEmpty() && g.platform != m_platform)
        return false;
    QStringList wanted = g.missing();
    if (m_replaceExisting)
        wanted += {QStringLiteral("description"), QStringLiteral("year"), QStringLiteral("rating")};
    if (!m_includeComplete
        && std::none_of(wanted.cbegin(), wanted.cend(), [&](const QString &m) { return m_missing.contains(category(m)); }))
        return false;
    return (skip & TagsSection) || m_tags.isEmpty()
        || std::any_of(m_tags.cbegin(), m_tags.cend(), [&](const QString &t) { return g.tags.contains(t); });
}

// The search is left out of the counts: typing would make every choice's number jump.
bool BulkLookup::shown(const LookupGame &g) const
{
    if (!matches(g, 0))
        return false;
    const QStringList words = SearchText::words(m_search);
    return words.isEmpty() || SearchText::matches(SearchText::fold(g.title), words);
}

void BulkLookup::refilter()
{
    m_shown.clear();
    m_platformCounts.clear();
    m_notOnIgdbCount = 0;
    QHash<QString, int> tagCounts;
    for (const LookupGame &g : std::as_const(m_games)) {
        if (matches(g, PlatformSection))
            ++m_platformCounts[g.platform];
        if (matches(g, TagsSection)) {
            for (const QString &t : g.tags)
                ++tagCounts[t];
        }
        if (g.igdb == 0 && matches(g, NotOnIgdbSection))
            ++m_notOnIgdbCount;
        if (!shown(g))
            continue;
        m_shown.append(QVariantMap{{QStringLiteral("id"), g.id},
                                   {QStringLiteral("title"), g.title},
                                   {QStringLiteral("platformName"), g.platformName},
                                   {QStringLiteral("icon"), g.icon.toString()},
                                   {QStringLiteral("missing"), g.missing()},
                                   {QStringLiteral("checked"), m_checked.contains(g.id)}});
    }
    m_tagCounts.clear();
    for (const QString &t : allTags())
        m_tagCounts.insert(t, tagCounts.value(t));
    emit gamesChanged();
}

int BulkLookup::missingCount() const
{
    return int(std::count_if(m_games.cbegin(), m_games.cend(),
                             [](const LookupGame &g) { return g.igdb != 0 && !g.missing().isEmpty(); }));
}

QVariantMap BulkLookup::missingCounts() const
{
    QHash<QString, int> counts;
    for (const LookupGame &g : m_games) {
        if (g.igdb == 0)
            continue;
        QSet<QString> categories;
        for (const QString &m : g.missing())
            categories.insert(category(m));
        for (const QString &c : std::as_const(categories))
            ++counts[c];
    }
    QVariantMap out;
    for (const QString &c : kCategories)
        out.insert(c, counts.value(c));
    return out;
}

int BulkLookup::checkedCount() const
{
    return int(std::count_if(m_shown.cbegin(), m_shown.cend(),
                             [](const QVariant &g) { return g.toMap().value(QStringLiteral("checked")).toBool(); }));
}

QVariantList BulkLookup::platforms() const
{
    QVariantList out;
    QSet<QString> seen;
    for (const LookupGame &g : m_games) {
        if (listed(g) && !seen.contains(g.platform)) {
            seen.insert(g.platform);
            out.append(QVariantMap{{QStringLiteral("id"), g.platform},
                                   {QStringLiteral("name"), g.platformName},
                                   {QStringLiteral("count"), m_platformCounts.value(g.platform)}});
        }
    }
    return out;
}

QStringList BulkLookup::allTags() const
{
    QStringList out;
    for (const LookupGame &g : m_games) {
        if (listed(g))
            out += g.tags;
    }
    out.removeDuplicates();
    out.sort(Qt::CaseInsensitive);
    return out;
}

void BulkLookup::setPlatform(const QString &platform)
{
    if (platform == m_platform)
        return;
    m_platform = platform;
    emit filtersChanged();
    refilter();
}

void BulkLookup::setMissing(const QStringList &missing)
{
    if (missing == m_missing)
        return;
    m_missing = missing;
    emit filtersChanged();
    refilter();
}

void BulkLookup::setTags(const QStringList &tags)
{
    if (tags == m_tags)
        return;
    m_tags = tags;
    emit filtersChanged();
    refilter();
}

void BulkLookup::setSearch(const QString &search)
{
    if (search == m_search)
        return;
    m_search = search;
    emit filtersChanged();
    refilter();
}

void BulkLookup::setIncludeNotOnIgdb(bool include)
{
    if (include == m_includeNotOnIgdb)
        return;
    m_includeNotOnIgdb = include;
    emit filtersChanged();
    refilter();
}

void BulkLookup::setReplaceExisting(bool replace)
{
    if (replace == m_replaceExisting)
        return;
    m_replaceExisting = replace;
    emit filtersChanged();
    refilter();
}

void BulkLookup::setIncludeComplete(bool include)
{
    if (include == m_includeComplete)
        return;
    m_includeComplete = include;
    emit filtersChanged();
    refilter();
}

void BulkLookup::setSuggestTags(bool suggest)
{
    if (suggest == m_suggestTags)
        return;
    m_suggestTags = suggest;
    emit filtersChanged();
}

void BulkLookup::clearFilters()
{
    m_platform.clear();
    m_missing = kCategories;
    m_tags.clear();
    m_search.clear();
    m_includeNotOnIgdb = false;
    emit filtersChanged();
    refilter();
}

void BulkLookup::setChecked(const QString &id, bool checked)
{
    if (checked)
        m_checked.insert(id);
    else
        m_checked.remove(id);
    refilter();
}

void BulkLookup::setAllChecked(bool checked)
{
    for (const QVariant &g : std::as_const(m_shown)) {
        const QString id = g.toMap().value(QStringLiteral("id")).toString();
        if (checked)
            m_checked.insert(id);
        else
            m_checked.remove(id);
    }
    refilter();
}

void BulkLookup::start()
{
    if (running() || m_applying)
        return;
    m_queue.clear();
    if (!m_resultFilter.isEmpty()) {
        m_resultFilter.clear();
        emit resultFilterChanged();
    }
    for (const LookupGame &g : std::as_const(m_games)) {
        if (shown(g) && m_checked.contains(g.id))
            m_queue.append(g);
    }
    for (const LookupGame &g : std::as_const(m_queue))
        m_rows.removeIf([&](const Row &r) { return r.game.id == g.id; });
    m_done = 0;
    m_total = int(m_queue.size());
    m_clock.start();
    emit resultsChanged();
    emit progressChanged();
    prefetch();
    pump();
}

// Fetches the queued games with an IGDB id ahead, a batch per request, so each needs no
// request of its own when its turn comes. A game whose IGDB id is already in the batch for
// other platforms (its year could differ) is fetched on its own.
void BulkLookup::prefetch()
{
    m_prefetched.clear();
    QHash<QString, int> batch;
    QHash<int, QList<int>> platforms;
    const auto send = [&] {
        if (batch.isEmpty())
            return;
        const int request = m_client->lookupIds(platforms);
        m_requests.insert(request, {QString(), Step::Prefetch, {}});
        m_prefetching.insert(request, batch);
        batch.clear();
        platforms.clear();
    };
    for (const LookupGame &g : std::as_const(m_queue)) {
        if (g.igdb <= 0 || (platforms.contains(g.igdb) && platforms.value(g.igdb) != g.igdbPlatforms))
            continue;
        batch.insert(g.id, g.igdb);
        platforms.insert(g.igdb, g.igdbPlatforms);
        if (platforms.size() == Igdb::kBatchSize)
            send();
    }
    send();
}

void BulkLookup::pump()
{
    const auto awaited = [&](const QString &id) {
        return std::any_of(m_prefetching.cbegin(), m_prefetching.cend(), [&](const auto &b) { return b.contains(id); });
    };
    // In order: a game whose batch hasn't come yet holds up the ones after it.
    while (!m_queue.isEmpty() && m_lookups.size() < kInFlight && !awaited(m_queue.first().id)) {
        const LookupGame g = m_queue.takeFirst();
        Row r;
        r.game = g;
        r.searched = g.title;
        r.replace = m_replaceExisting;
        m_rows.append(r);
        m_lookups.insert(g.id);
        lookUp(m_rows.last());
    }
    emit resultsChanged();
    emit progressChanged();
}

BulkLookup::Row *BulkLookup::row(const QString &id)
{
    for (Row &r : m_rows) {
        if (r.game.id == id)
            return &r;
    }
    return nullptr;
}

void BulkLookup::lookUp(Row &r)
{
    r.state = State::Running;
    r.error.clear();
    r.match.clear();
    r.candidates.clear();
    r.fields.clear();
    r.art.clear();
    r.igdb = -1;
    r.steamgriddb = 0;
    QVariantList platforms;
    for (int id : r.game.igdbPlatforms)
        platforms.append(id);
    const int igdb = r.searched == r.game.title && r.game.igdb > 0 ? r.game.igdb : 0;
    r.pending = 1;
    if (igdb > 0 && m_prefetched.contains(r.game.id)) {
        const QVariantMap game = m_prefetched.value(r.game.id);
        const QVariantMap result{{QStringLiteral("candidates"), game.isEmpty() ? QVariantList() : QVariantList{game}},
                                 {QStringLiteral("match"), game.isEmpty() ? -1 : 0},
                                 {QStringLiteral("error"), QString()}};
        const int request = m_nextLocalRequest--;
        m_requests.insert(request, {r.game.id, Step::Lookup, {}});
        QTimer::singleShot(0, this, [this, request, result] { finished(request, result); });
        return;
    }
    m_requests.insert(m_client->lookup(r.searched, platforms, igdb, r.game.steamAppId), {r.game.id, Step::Lookup, {}});
}

void BulkLookup::resolve(Row &r, const QVariantMap &candidate)
{
    r.state = State::Running;
    r.match = candidate;
    r.igdb = candidate.value(QStringLiteral("igdb")).toInt();
    r.fields.clear();
    r.uncheckedFields.clear();
    r.replaces.clear();
    r.tags.clear();
    r.uncheckedTags.clear();
    r.art.clear();
    if (m_suggestTags && m_tagMappings)
        r.tags = Igdb::fittingTags(candidate.value(QStringLiteral("facets")).toStringList(), m_tagMappings(), r.game.tags);
    const QStringList missing = r.game.missing();
    const QString summary = candidate.value(QStringLiteral("summary")).toString();
    const int year = candidate.value(QStringLiteral("year")).toInt();
    // A missing field is filled in, checked; with replaceExisting, the game's own is offered
    // a replacement, unchecked, when the match's differs.
    const auto offer = [&](const QString &key, const QVariant &value, bool differs) {
        if (missing.contains(key)) {
            r.fields.append({key, value});
        } else if (r.replace && differs) {
            r.fields.append({key, value});
            r.replaces.insert(key);
            r.uncheckedFields.insert(key);
        }
    };
    if (!summary.isEmpty())
        offer(QStringLiteral("description"), summary, summary.trimmed() != r.game.description.trimmed());
    if (year > 0)
        offer(QStringLiteral("year"), year, year != r.game.year);
    if (const double rating = candidate.value(QStringLiteral("rating")).toDouble(); rating >= 0)
        // Offered when the same too, to record that it came from IGDB.
        offer(QStringLiteral("rating"), rating, rating != r.game.rating || !r.game.ratingFromIgdb);

    const bool lacksArt = std::any_of(kKinds.cbegin(), kKinds.cend(), [&](const QString &k) { return missing.contains(k); });
    if (!lacksArt || !m_client->hasSteamGridDbKey()) {
        r.state = State::Matched;
        return rowDone(r);
    }
    const qint64 steamAppId = candidate.value(QStringLiteral("steamAppId")).toLongLong();
    r.pending = 1;
    m_requests.insert(m_client->findArt(candidate.value(QStringLiteral("name")).toString(), year,
                                        steamAppId > 0 ? steamAppId : r.game.steamAppId),
                      {r.game.id, Step::FindArt, {}});
}

void BulkLookup::finished(int request, const QVariantMap &result)
{
    const auto it = m_requests.constFind(request);
    if (it == m_requests.cend())
        return;
    const Request req = *it;
    m_requests.erase(it);

    if (req.step == Step::Download) {
        --m_downloading;
        // Counted as each image arrives, not when its batch is written, so the progress keeps moving.
        ++m_done;
        emit progressChanged();
        if (Row *r = row(req.id)) {
            if (!result.value(QStringLiteral("error")).toString().isEmpty())
                r->applyError = result.value(QStringLiteral("error")).toString();
            else
                m_downloaded.append({req.id, req.kind, result.value(QStringLiteral("path")).toString()});
        }
        flushArt(false);
        pumpDownloads();
        return;
    }

    if (req.step == Step::Prefetch) {
        // On an error nothing is kept, and those games are looked up one by one.
        const QHash<QString, int> batch = m_prefetching.take(request);
        if (result.value(QStringLiteral("error")).toString().isEmpty()) {
            const QVariantMap games = result.value(QStringLiteral("games")).toMap();
            for (auto it = batch.cbegin(); it != batch.cend(); ++it)
                m_prefetched.insert(it.key(), games.value(QString::number(it.value())).toMap());
        }
        return pump();
    }

    Row *r = row(req.id);
    if (!r)
        return;
    --r->pending;
    const QString error = result.value(QStringLiteral("error")).toString();
    switch (req.step) {
    case Step::Lookup: {
        if (!error.isEmpty()) {
            r->state = State::Failed;
            r->error = error;
            return rowDone(*r);
        }
        r->candidates = result.value(QStringLiteral("candidates")).toList();
        const int match = result.value(QStringLiteral("match")).toInt();
        if (r->candidates.isEmpty()) {
            r->state = State::NotFound;
            return rowDone(*r);
        }
        if (match < 0) {
            r->state = State::Pick;
            return rowDone(*r);
        }
        return resolve(*r, r->candidates.at(match).toMap());
    }
    case Step::FindArt: {
        r->steamgriddb = result.value(QStringLiteral("steamgriddb")).toInt();
        const QStringList missing = r->game.missing();
        for (const QString &kind : kKinds) {
            if (!missing.contains(kind))
                continue;
            r->art.insert(kind, {{}, {}, false, true});
            if (r->steamgriddb > 0) {
                ++r->pending;
                m_requests.insert(m_client->artChoices(r->steamgriddb, kind, {}, 0), {r->game.id, Step::ArtChoices, kind});
            }
        }
        break;
    }
    case Step::ArtChoices: {
        const QVariantList images = result.value(QStringLiteral("images")).toList();
        if (!images.isEmpty()) {
            const QVariantMap first = images.first().toMap();
            r->art[req.kind] = {first, first, true, false};
        }
        break;
    }
    case Step::Download:
        break;
    }
    if (r->pending == 0) {
        r->state = State::Matched;
        rowDone(*r);
    } else {
        emit resultsChanged();
    }
}

void BulkLookup::rowDone(Row &r)
{
    if (m_lookups.remove(r.game.id))
        ++m_done;
    emit resultsChanged();
    pump();
}

void BulkLookup::cancelRow(const QString &id)
{
    for (auto it = m_requests.begin(); it != m_requests.end();) {
        if (it->id == id && it->step != Step::Download) {
            m_client->cancel(it.key());
            it = m_requests.erase(it);
        } else {
            ++it;
        }
    }
}

void BulkLookup::stop()
{
    if (m_applying) {
        const QString stopped = tr("Stopped before its art was saved");
        for (const std::array<QString, 3> &d : std::as_const(m_toDownload)) {
            if (Row *r = row(d[0]))
                r->applyError = stopped;
        }
        for (auto it = m_requests.begin(); it != m_requests.end();) {
            if (it->step == Step::Download) {
                if (Row *r = row(it->id))
                    r->applyError = stopped;
                m_client->cancel(it.key());
                it = m_requests.erase(it);
            } else {
                ++it;
            }
        }
        m_toDownload.clear();
        m_downloading = 0;
        flushArt(true);
        applyFinished();
        return;
    }
    m_queue.clear();
    for (auto it = m_prefetching.cbegin(); it != m_prefetching.cend(); ++it) {
        m_client->cancel(it.key());
        m_requests.remove(it.key());
    }
    m_prefetching.clear();
    for (const QString &id : std::as_const(m_lookups)) {
        cancelRow(id);
        m_rows.removeIf([&](const Row &r) { return r.game.id == id; });
    }
    m_lookups.clear();
    m_total = m_done;
    emit resultsChanged();
    emit progressChanged();
}

int BulkLookup::secondsLeft() const
{
    if (m_done == 0 || !m_clock.isValid())
        return -1;
    const double perGame = m_clock.elapsed() / 1000.0 / m_done;
    return int(std::ceil(perGame * (m_total - m_done)));
}

void BulkLookup::pick(const QString &id, int candidate)
{
    Row *r = row(id);
    if (!r || candidate < 0 || candidate >= r->candidates.size())
        return;
    cancelRow(id);
    resolve(*r, r->candidates.at(candidate).toMap());
    emit resultsChanged();
}

void BulkLookup::retry(const QString &id, const QString &title)
{
    Row *r = row(id);
    if (!r || title.trimmed().isEmpty())
        return;
    cancelRow(id);
    r->searched = title.trimmed();
    lookUp(*r);
    emit resultsChanged();
}

void BulkLookup::choose(const QString &id, const QVariantMap &candidate, const QString &searched)
{
    Row *r = row(id);
    if (!r || candidate.value(QStringLiteral("igdb")).toInt() <= 0)
        return;
    cancelRow(id);
    r->searched = searched.trimmed().isEmpty() ? r->searched : searched.trimmed();
    r->candidates = {candidate};
    resolve(*r, candidate);
    emit resultsChanged();
}

void BulkLookup::markNotOnIgdb(const QString &id)
{
    if (Row *r = row(id); r && r->state != State::NotOnIgdb) {
        cancelRow(id);
        r->before = r->state;
        r->state = State::NotOnIgdb;
        r->fields.clear();
        r->art.clear();
        r->pending = 0;
        emit resultsChanged();
    }
}

void BulkLookup::unmarkNotOnIgdb(const QString &id)
{
    Row *r = row(id);
    if (!r || r->state != State::NotOnIgdb)
        return;
    if (r->before == State::Matched) {
        resolve(*r, r->match);
    } else if (r->before == State::Running) {
        lookUp(*r);
    } else {
        r->state = r->before;
    }
    emit resultsChanged();
}

void BulkLookup::setResultFilter(const QString &state)
{
    if (state == m_resultFilter)
        return;
    m_resultFilter = state;
    emit resultFilterChanged();
    m_resultModel.setItems(resultList(m_resultFilter));
}

QVariantMap BulkLookup::stateCounts() const
{
    QHash<QString, int> counts;
    for (const Row &r : m_rows)
        ++counts[stateName(r.state)];
    QVariantMap out;
    for (State s : {State::Running, State::Matched, State::Pick, State::NotFound, State::Failed, State::NotOnIgdb})
        out.insert(stateName(s), counts.value(stateName(s)));
    out.insert(QStringLiteral("all"), int(m_rows.size()));
    return out;
}

void BulkLookup::setFieldChecked(const QString &id, const QString &key, bool checked)
{
    if (Row *r = row(id)) {
        if (checked)
            r->uncheckedFields.remove(key);
        else
            r->uncheckedFields.insert(key);
        emit resultsChanged();
    }
}

void BulkLookup::setTagChecked(const QString &id, const QString &name, bool checked)
{
    if (Row *r = row(id)) {
        if (checked)
            r->uncheckedTags.remove(name);
        else
            r->uncheckedTags.insert(name);
        emit resultsChanged();
    }
}

static void checkArt(auto &art, bool checked)
{
    if (checked && art.image.isEmpty())
        art.image = art.byDefault;
    art.checked = checked && !art.image.isEmpty();
}

void BulkLookup::setArtChecked(const QString &id, const QString &kind, bool checked)
{
    Row *r = row(id);
    if (!r || !r->art.contains(kind))
        return;
    checkArt(r->art[kind], checked);
    emit resultsChanged();
}

void BulkLookup::setCategoryChecked(const QString &category, bool checked)
{
    for (Row &r : m_rows) {
        if (category == QLatin1String("tags")) {
            if (checked)
                r.uncheckedTags.clear();
            else
                r.uncheckedTags = QSet<QString>(r.tags.cbegin(), r.tags.cend());
        } else if (category == QLatin1String("art")) {
            for (Art &a : r.art)
                checkArt(a, checked);
        } else if (r.replaces.contains(category)) {
            if (checked)
                r.uncheckedFields.remove(category);
            else
                r.uncheckedFields.insert(category);
        }
    }
    emit resultsChanged();
}

QVariantMap BulkLookup::categories() const
{
    struct Tally
    {
        int total = 0;
        int on = 0;
    };
    QMap<QString, Tally> tally;
    const auto count = [&](const QString &category, bool on) {
        Tally &t = tally[category];
        ++t.total;
        t.on += on;
    };
    for (const Row &r : m_rows) {
        for (const QString &key : r.replaces)
            count(key, !r.uncheckedFields.contains(key));
        for (const QString &tag : r.tags)
            count(QStringLiteral("tags"), !r.uncheckedTags.contains(tag));
        for (const Art &a : r.art) {
            if (!a.none || !a.byDefault.isEmpty())
                count(QStringLiteral("art"), a.checked);
        }
    }
    QVariantMap out;
    for (const QString &key : {QStringLiteral("description"), QStringLiteral("year"), QStringLiteral("rating"),
                               QStringLiteral("tags"), QStringLiteral("art")})
        out.insert(key, QVariantMap{{QStringLiteral("total"), tally[key].total}, {QStringLiteral("on"), tally[key].on}});
    return out;
}

void BulkLookup::setArt(const QString &id, const QString &kind, const QVariantMap &image)
{
    Row *r = row(id);
    if (!r || !r->art.contains(kind))
        return;
    Art &a = r->art[kind];
    a.image = image;
    a.checked = !image.isEmpty();
    emit resultsChanged();
}

int BulkLookup::steamGridDbId(const QString &id) const
{
    for (const Row &r : m_rows) {
        if (r.game.id == id)
            return r.steamgriddb;
    }
    return 0;
}

void BulkLookup::clearResults()
{
    stop();
    m_rows.clear();
    m_done = m_total = 0;
    emit resultsChanged();
    emit progressChanged();
}

QString BulkLookup::stateName(State state)
{
    switch (state) {
    case State::Running:
        return QStringLiteral("running");
    case State::Matched:
        return QStringLiteral("matched");
    case State::Pick:
        return QStringLiteral("pick");
    case State::NotFound:
        return QStringLiteral("notFound");
    case State::Failed:
        return QStringLiteral("error");
    case State::NotOnIgdb:
        return QStringLiteral("notOnIgdb");
    }
    return {};
}

QVariantList BulkLookup::results() const
{
    return resultList({});
}

// Only the rows in `state` are built ("" = all), so a filtered list doesn't pay for the rest.
QVariantList BulkLookup::resultList(const QString &state) const
{
    QVariantList out;
    for (const Row &r : m_rows) {
        if (!state.isEmpty() && stateName(r.state) != state)
            continue;
        QVariantList fields;
        for (const auto &[key, value] : r.fields) {
            fields.append(QVariantMap{{QStringLiteral("key"), key},
                                      {QStringLiteral("name"), fieldName(key)},
                                      {QStringLiteral("value"), value},
                                      {QStringLiteral("checked"), !r.uncheckedFields.contains(key)},
                                      {QStringLiteral("replaces"), r.replaces.contains(key)},
                                      {QStringLiteral("current"), currentValue(r.game, key)}});
        }
        QVariantList tags;
        for (const QString &name : r.tags)
            tags.append(QVariantMap{{QStringLiteral("name"), name}, {QStringLiteral("checked"), !r.uncheckedTags.contains(name)}});
        QVariantList art;
        for (const QString &kind : kKinds) {
            if (!r.art.contains(kind))
                continue;
            const Art &a = r.art.value(kind);
            art.append(QVariantMap{{QStringLiteral("kind"), kind},
                                   {QStringLiteral("name"), kindName(kind)},
                                   {QStringLiteral("image"), a.image.isEmpty() ? QVariant() : QVariant(a.image)},
                                   {QStringLiteral("checked"), a.checked},
                                   {QStringLiteral("changed"), a.image.value(QStringLiteral("id")) != a.byDefault.value(QStringLiteral("id"))},
                                   {QStringLiteral("none"), a.none && a.byDefault.isEmpty()}});
        }
        QVariantList igdbPlatforms;
        for (int p : r.game.igdbPlatforms)
            igdbPlatforms.append(p);
        out.append(QVariantMap{{QStringLiteral("id"), r.game.id},
                               {QStringLiteral("title"), r.game.title},
                               {QStringLiteral("igdbPlatforms"), igdbPlatforms},
                               {QStringLiteral("searched"), r.searched},
                               {QStringLiteral("platformName"), r.game.platformName},
                               {QStringLiteral("icon"), r.game.icon.toString()},
                               {QStringLiteral("state"), stateName(r.state)},
                               {QStringLiteral("error"), r.error},
                               {QStringLiteral("candidates"), r.candidates},
                               {QStringLiteral("match"), r.match},
                               {QStringLiteral("fields"), fields},
                               {QStringLiteral("tags"), tags},
                               {QStringLiteral("art"), art},
                               {QStringLiteral("applyError"), r.applyError}});
    }
    return out;
}

bool BulkLookup::hasArt() const
{
    return std::any_of(m_rows.cbegin(), m_rows.cend(), [](const Row &r) {
        return r.state == State::Matched
            && std::any_of(r.art.cbegin(), r.art.cend(), [](const Art &a) { return !a.none || !a.byDefault.isEmpty(); });
    });
}

int BulkLookup::applyCount() const
{
    return int(std::count_if(m_rows.cbegin(), m_rows.cend(), [](const Row &r) {
        return r.state == State::NotOnIgdb || r.state == State::Matched;
    }));
}

void BulkLookup::apply()
{
    if (m_applying || running())
        return;
    QHash<QString, LookupGame> now;
    for (const LookupGame &g : m_source())
        now.insert(g.id, g);

    QList<std::pair<QString, QVariantMap>> writes;
    m_toDownload.clear();
    m_downloaded.clear();
    m_applyIds.clear();
    for (Row &r : m_rows) {
        if ((r.state != State::Matched && r.state != State::NotOnIgdb) || !now.contains(r.game.id))
            continue;
        const QStringList missing = now.value(r.game.id).missing();
        QVariantMap values;
        if (r.state == State::NotOnIgdb) {
            values.insert(QStringLiteral("igdb"), 0);
        } else {
            const LookupGame &game = now.value(r.game.id);
            for (const auto &[key, value] : r.fields) {
                if (r.uncheckedFields.contains(key))
                    continue;
                // A replacement only over what the game had at the lookup, not a later edit.
                const bool unchanged = currentValue(game, key) == currentValue(r.game, key);
                if (missing.contains(key) || (r.replaces.contains(key) && unchanged)) {
                    values.insert(key, value);
                    if (key == QLatin1String("rating"))
                        values.insert(QStringLiteral("rating_source"), QStringLiteral("igdb"));
                }
            }
            QStringList tags = game.tags;
            for (const QString &name : r.tags) {
                if (!r.uncheckedTags.contains(name) && !tags.contains(name))
                    tags.append(name);
            }
            if (tags.size() > game.tags.size())
                values.insert(QStringLiteral("tags"), tags);
            if (r.igdb > 0)
                values.insert(QStringLiteral("igdb"), r.igdb);
            if (r.steamgriddb > 0)
                values.insert(QStringLiteral("steamgriddb"), r.steamgriddb);
            for (auto it = r.art.cbegin(); it != r.art.cend(); ++it) {
                if (it->checked && !it->image.isEmpty() && missing.contains(it.key()))
                    m_toDownload.append({r.game.id, it.key(), it->image.value(QStringLiteral("url")).toString()});
            }
        }
        r.applyError.clear();
        m_applyIds.insert(r.game.id);
        if (!values.isEmpty())
            writes.append({r.game.id, values});
    }
    if (m_applyIds.isEmpty())
        return;
    m_applying = true;
    m_done = 0;
    m_total = int(writes.size() + m_toDownload.size());
    m_clock.start();
    emit progressChanged();

    for (qsizetype i = 0; i < writes.size(); i += kWriteBatch) {
        const QHash<QString, QString> errors = m_writeValues(writes.mid(i, kWriteBatch));
        for (auto it = errors.cbegin(); it != errors.cend(); ++it) {
            if (Row *r = row(it.key()))
                r->applyError = it.value();
        }
        m_done += int(std::min<qsizetype>(kWriteBatch, writes.size() - i));
    }
    m_toDownload.removeIf([&](const std::array<QString, 3> &d) {
        const Row *r = row(d[0]);
        return !r || !r->applyError.isEmpty();
    });
    m_total = m_done + int(m_toDownload.size());
    emit progressChanged();
    pumpDownloads();
}

void BulkLookup::pumpDownloads()
{
    while (m_applying && m_downloading < kDownloads && !m_toDownload.isEmpty()) {
        const std::array<QString, 3> d = m_toDownload.takeFirst();
        ++m_downloading;
        m_requests.insert(m_client->download(d[2], d[1]), {d[0], Step::Download, d[1]});
    }
    if (m_applying && m_downloading == 0 && m_toDownload.isEmpty()) {
        flushArt(true);
        applyFinished();
    }
}

void BulkLookup::flushArt(bool all)
{
    if (m_downloaded.isEmpty() || (!all && m_downloaded.size() < kArtBatch))
        return;
    const QHash<QString, QString> errors = m_writeArt(m_downloaded);
    for (auto it = errors.cbegin(); it != errors.cend(); ++it) {
        if (Row *r = row(it.key()))
            r->applyError = it.value();
    }
    m_downloaded.clear();
    emit progressChanged();
}

void BulkLookup::applyFinished()
{
    const int before = int(m_rows.size());
    m_rows.removeIf([&](const Row &r) { return m_applyIds.contains(r.game.id) && r.applyError.isEmpty(); });
    const int written = before - int(m_rows.size());
    m_applying = false;
    m_applyIds.clear();
    reload();
    emit resultsChanged();
    emit progressChanged();
    emit applied(written);
}
