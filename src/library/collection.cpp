#include "collection.h"
#include "gamefile.h"
#include "listsettings.h"
#include "suggestion.h"
#include "tomlsection.h"

#include <toml++/toml.hpp>

#include <algorithm>
#include <cmath>

namespace {
const QStringList kKnownKeys = {QStringLiteral("id"),           QStringLiteral("name"),
                                QStringLiteral("steam_id"),     QStringLiteral("rules"),
                                QStringLiteral("platform"),     QStringLiteral("installed_only"),
                                QStringLiteral("favorites_only"), QStringLiteral("statuses"),
                                QStringLiteral("tags"),         QStringLiteral("match_all_tags"),
                                QStringLiteral("steam"),        QStringLiteral("added"),
                                QStringLiteral("removed"),      QStringLiteral("year_from"),
                                QStringLiteral("year_to"),      QStringLiteral("min_rating"),
                                QStringLiteral("play_times"),   QStringLiteral("last_played"),
                                QStringLiteral("achievements"), QStringLiteral("sort"),
                                QStringLiteral("sort_descending"), QStringLiteral("group_by")};

QString where(const QString &displayPath, const toml::node &node)
{
    return QStringLiteral("%1:%2").arg(displayPath).arg(node.source().begin.line);
}

bool readString(const toml::node &node, const QString &key, const QString &displayPath, QString *out,
                QStringList *warnings)
{
    if (const auto *s = node.as_string()) {
        *out = toQString(s->get());
        return true;
    }
    warnings->append(QStringLiteral("%1: %2 must be text in quotes").arg(where(displayPath, node), key));
    return false;
}

bool readBool(const toml::node &node, const QString &key, const QString &displayPath, bool *out,
              QStringList *warnings)
{
    if (const auto *b = node.as_boolean()) {
        *out = b->get();
        return true;
    }
    warnings->append(QStringLiteral("%1: %2 must be true or false").arg(where(displayPath, node), key));
    return false;
}

bool readChoice(const toml::node &node, const QString &key, const QString &displayPath, const QStringList &choices,
                QString *out, QStringList *warnings)
{
    QString value;
    if (!readString(node, key, displayPath, &value, warnings))
        return false;
    if (!choices.contains(value)) {
        warnings->append(QStringLiteral("%1: %2 must be one of %3%4")
                             .arg(where(displayPath, node), key, choices.join(QLatin1String(", ")),
                                  suggestion(value, choices)));
        return false;
    }
    *out = value;
    return true;
}

bool readList(const toml::node &node, const QString &key, const QString &displayPath, QStringList *out,
              QStringList *warnings)
{
    const toml::array *array = node.as_array();
    if (!array) {
        warnings->append(QStringLiteral("%1: %2 must be a list like [\"a\", \"b\"]").arg(where(displayPath, node), key));
        return false;
    }
    out->clear();
    for (const toml::node &item : *array) {
        const auto *s = item.as_string();
        if (!s) {
            warnings->append(QStringLiteral("%1: %2 holds only text in quotes; skipping a value")
                                 .arg(where(displayPath, item), key));
            continue;
        }
        const QString value = toQString(s->get());
        if (!out->contains(value))
            out->append(value);
    }
    return true;
}

bool readYear(const toml::node &node, const QString &key, const QString &displayPath, int *out, QStringList *warnings)
{
    const auto *n = node.as_integer();
    if (n && n->get() >= 1 && n->get() <= 9999) {
        *out = int(n->get());
        return true;
    }
    warnings->append(QStringLiteral("%1: %2 must be a year, like 2015").arg(where(displayPath, node), key));
    return false;
}

bool readRating(const toml::node &node, const QString &key, const QString &displayPath, double *out,
                QStringList *warnings)
{
    const double rating = node.is_integer()        ? double(node.as_integer()->get())
                          : node.is_floating_point() ? node.as_floating_point()->get()
                                                     : -1;
    if (rating >= 0 && rating <= 5) {
        *out = std::round(rating * 2) / 2;
        return true;
    }
    warnings->append(QStringLiteral("%1: %2 must be a number from 0 to 5, like 3.5").arg(where(displayPath, node), key));
    return false;
}

// A list whose values are all from `choices`; others are skipped with a warning.
bool readChoices(const toml::node &node, const QString &key, const QString &displayPath, const QStringList &choices,
                 QStringList *out, QStringList *warnings)
{
    QStringList read;
    if (!readList(node, key, displayPath, &read, warnings))
        return false;
    out->clear();
    for (const QString &value : std::as_const(read)) {
        if (choices.contains(value))
            out->append(value);
        else
            warnings->append(QStringLiteral("%1: unknown value \"%2\" in %3; skipping it")
                                 .arg(where(displayPath, node), value, key));
    }
    return true;
}

QString tomlList(const QStringList &list)
{
    QStringList items;
    for (const QString &s : list)
        items.append(tomlString(s));
    return u'[' + items.join(QLatin1String(", ")) + u']';
}

QString statusOf(const Game &game)
{
    return game.status.isEmpty() ? QStringLiteral("none") : game.status;
}
}

Parsed<QList<Collection>> parseCollections(const QByteArray &bytes, const QString &displayPath)
{
    Parsed<QList<Collection>> out;
    toml::table root;
    try {
        root = toml::parse(std::string_view(bytes.constData(), size_t(bytes.size())));
    } catch (const toml::parse_error &e) {
        out.warnings.append(QStringLiteral("%1:%2: %3")
                                .arg(displayPath)
                                .arg(e.source().begin.line)
                                .arg(toQString(e.description())));
        return out;
    }
    out.ok = true;

    QSet<QString> ids;
    for (auto &&[k, node] : root) {
        const QString key = toQString(k.str());
        if (key != QLatin1String("collection")) {
            out.warnings.append(QStringLiteral("%1: unknown key %2; collections go in [[collection]] blocks%3")
                                    .arg(where(displayPath, node), key, suggestion(key, {QStringLiteral("collection")})));
            continue;
        }
        const toml::array *array = node.as_array();
        if (!array || !array->is_array_of_tables()) {
            out.warnings.append(QStringLiteral("%1: collection must be [[collection]] blocks").arg(where(displayPath, node)));
            continue;
        }
        for (const toml::node &item : *array) {
            const toml::table &entry = *item.as_table();
            Collection collection;
            CollectionRules rules;
            bool hasRules = false;
            bool ruleKeySeen = false;
            for (auto &&[ek, value] : entry) {
                const QString name = toQString(ek.str());
                if (name == QLatin1String("id"))
                    readString(value, name, displayPath, &collection.id, &out.warnings);
                else if (name == QLatin1String("name"))
                    readString(value, name, displayPath, &collection.name, &out.warnings);
                else if (name == QLatin1String("steam_id"))
                    readString(value, name, displayPath, &collection.steamId, &out.warnings);
                else if (name == QLatin1String("sort"))
                    readChoice(value, name, displayPath, listSorts(), &collection.sort, &out.warnings);
                else if (name == QLatin1String("group_by"))
                    readChoice(value, name, displayPath, listGroupings(), &collection.groupBy, &out.warnings);
                else if (name == QLatin1String("sort_descending"))
                    readBool(value, name, displayPath, &collection.sortDescending, &out.warnings);
                else if (name == QLatin1String("rules"))
                    readBool(value, name, displayPath, &hasRules, &out.warnings);
                else if (name == QLatin1String("added"))
                    readList(value, name, displayPath, &collection.added, &out.warnings);
                else if (name == QLatin1String("removed"))
                    readList(value, name, displayPath, &collection.removed, &out.warnings);
                else if (name == QLatin1String("platform"))
                    ruleKeySeen |= readString(value, name, displayPath, &rules.platform, &out.warnings);
                else if (name == QLatin1String("installed_only"))
                    ruleKeySeen |= readBool(value, name, displayPath, &rules.installedOnly, &out.warnings);
                else if (name == QLatin1String("favorites_only"))
                    ruleKeySeen |= readBool(value, name, displayPath, &rules.favoritesOnly, &out.warnings);
                else if (name == QLatin1String("match_all_tags"))
                    ruleKeySeen |= readBool(value, name, displayPath, &rules.matchAllTags, &out.warnings);
                else if (name == QLatin1String("statuses"))
                    ruleKeySeen |= readList(value, name, displayPath, &rules.statuses, &out.warnings);
                else if (name == QLatin1String("tags"))
                    ruleKeySeen |= readList(value, name, displayPath, &rules.tags, &out.warnings);
                else if (name == QLatin1String("steam"))
                    ruleKeySeen |= readList(value, name, displayPath, &rules.steamOwnership, &out.warnings);
                else if (name == QLatin1String("year_from"))
                    ruleKeySeen |= readYear(value, name, displayPath, &rules.yearFrom, &out.warnings);
                else if (name == QLatin1String("year_to"))
                    ruleKeySeen |= readYear(value, name, displayPath, &rules.yearTo, &out.warnings);
                else if (name == QLatin1String("min_rating"))
                    ruleKeySeen |= readRating(value, name, displayPath, &rules.minRating, &out.warnings);
                else if (name == QLatin1String("play_times"))
                    ruleKeySeen |= readChoices(value, name, displayPath, listPlayTimes(), &rules.playTimes, &out.warnings);
                else if (name == QLatin1String("achievements"))
                    ruleKeySeen |= readChoices(value, name, displayPath, listAchievementStates(), &rules.achievements,
                                               &out.warnings);
                else if (name == QLatin1String("last_played")) {
                    QString period;
                    if (readString(value, name, displayPath, &period, &out.warnings)) {
                        if (listLastPlayed().contains(period)) {
                            rules.lastPlayed = period;
                            ruleKeySeen = true;
                        } else {
                            out.warnings.append(QStringLiteral("%1: last_played must be one of %2%3")
                                                    .arg(where(displayPath, value), listLastPlayed().join(QLatin1String(", ")),
                                                         suggestion(period, listLastPlayed())));
                        }
                    }
                } else
                    out.warnings.append(QStringLiteral("%1: unknown key %2%3")
                                            .arg(where(displayPath, value), name, suggestion(name, kKnownKeys)));
            }
            if (collection.id.isEmpty()) {
                out.warnings.append(QStringLiteral("%1: a collection needs an id; skipping it").arg(where(displayPath, item)));
                continue;
            }
            if (ids.contains(collection.id)) {
                out.warnings.append(QStringLiteral("%1: there's already a collection with id %2; skipping this one")
                                        .arg(where(displayPath, item), collection.id));
                continue;
            }
            ids.insert(collection.id);
            if (collection.name.isEmpty())
                collection.name = collection.id;
            if (collection.sort.isEmpty())
                collection.sortDescending = false;
            if (hasRules)
                collection.rules = rules;
            else if (ruleKeySeen)
                out.warnings.append(QStringLiteral("%1: %2 has rule keys without rules = true; ignoring them")
                                        .arg(where(displayPath, item), collection.id));
            out.value.append(collection);
        }
    }
    return out;
}

CollectionValues collectionValues(const Collection &collection)
{
    CollectionValues out;
    const auto set = [&](const QString &key, const QString &value) { out.set.append({key, value}); };
    const auto setOrRemove = [&](const QString &key, bool used, const QString &value) {
        if (used)
            set(key, value);
        else
            out.remove.append(key);
    };
    set(QStringLiteral("id"), tomlString(collection.id));
    set(QStringLiteral("name"), tomlString(collection.name));
    setOrRemove(QStringLiteral("steam_id"), !collection.steamId.isEmpty(), tomlString(collection.steamId));
    const CollectionRules rules = collection.rules.value_or(CollectionRules{});
    const bool has = collection.rules.has_value();
    setOrRemove(QStringLiteral("sort"), !collection.sort.isEmpty(), tomlString(collection.sort));
    setOrRemove(QStringLiteral("sort_descending"), !collection.sort.isEmpty(),
                collection.sortDescending ? QStringLiteral("true") : QStringLiteral("false"));
    setOrRemove(QStringLiteral("group_by"), !collection.groupBy.isEmpty(), tomlString(collection.groupBy));
    setOrRemove(QStringLiteral("rules"), has, QStringLiteral("true"));
    setOrRemove(QStringLiteral("platform"), has && !rules.platform.isEmpty(), tomlString(rules.platform));
    setOrRemove(QStringLiteral("installed_only"), has && rules.installedOnly, QStringLiteral("true"));
    setOrRemove(QStringLiteral("favorites_only"), has && rules.favoritesOnly, QStringLiteral("true"));
    setOrRemove(QStringLiteral("statuses"), has && !rules.statuses.isEmpty(), tomlList(rules.statuses));
    setOrRemove(QStringLiteral("tags"), has && !rules.tags.isEmpty(), tomlList(rules.tags));
    setOrRemove(QStringLiteral("match_all_tags"), has && rules.matchAllTags, QStringLiteral("true"));
    setOrRemove(QStringLiteral("steam"), has && !rules.steamOwnership.isEmpty(), tomlList(rules.steamOwnership));
    setOrRemove(QStringLiteral("year_from"), has && rules.yearFrom > 0, QString::number(rules.yearFrom));
    setOrRemove(QStringLiteral("year_to"), has && rules.yearTo > 0, QString::number(rules.yearTo));
    setOrRemove(QStringLiteral("min_rating"), has && rules.minRating >= 0, QString::number(rules.minRating, 'f', 1));
    setOrRemove(QStringLiteral("play_times"), has && !rules.playTimes.isEmpty(), tomlList(rules.playTimes));
    setOrRemove(QStringLiteral("last_played"), has && !rules.lastPlayed.isEmpty(), tomlString(rules.lastPlayed));
    setOrRemove(QStringLiteral("achievements"), has && !rules.achievements.isEmpty(), tomlList(rules.achievements));
    setOrRemove(QStringLiteral("added"), !collection.added.isEmpty(), tomlList(collection.added));
    setOrRemove(QStringLiteral("removed"), !collection.removed.isEmpty(), tomlList(collection.removed));
    return out;
}

CollectionMatcher::CollectionMatcher(const Collection &collection, const QSet<QString> *knownTags,
                                     AchievementState achievementState)
    : m_hasRules(collection.rules.has_value())
    , m_added(collection.added.cbegin(), collection.added.cend())
    , m_removed(collection.removed.cbegin(), collection.removed.cend())
    , m_achievementState(std::move(achievementState))
{
    if (m_hasRules)
        m_rules = *collection.rules;
    if (m_hasRules && knownTags) {
        const bool hadTags = !m_rules.tags.isEmpty();
        m_rules.tags.removeIf([&](const QString &tag) { return !knownTags->contains(tag); });
        m_tagsGone = hadTags && m_rules.tags.isEmpty();
    }
}

bool CollectionMatcher::isMember(const Game &game) const
{
    // The copies of one game (same_as) are in a collection together: one in the lists decides for all.
    const auto listed = [&](const QSet<QString> &ids) {
        if (ids.isEmpty())
            return false;
        return ids.contains(game.id) || (!game.groupId.isEmpty() && ids.contains(game.groupId))
               || std::any_of(game.linked.cbegin(), game.linked.cend(), [&](const QString &id) { return ids.contains(id); });
    };
    if (listed(m_added))
        return true;
    if (!m_hasRules || m_tagsGone || listed(m_removed))
        return false;

    const CollectionRules &r = m_rules;
    if (!r.platform.isEmpty() && game.platform != r.platform)
        return false;
    if (r.installedOnly && !game.installed)
        return false;
    if (r.favoritesOnly && !game.favorite)
        return false;
    if (!r.statuses.isEmpty() && !r.statuses.contains(statusOf(game)))
        return false;
    if (!r.steamOwnership.isEmpty() && !r.steamOwnership.contains(game.steamOwnership))
        return false;
    if ((r.yearFrom > 0 || r.yearTo > 0)
        && (game.year <= 0 || (r.yearFrom > 0 && game.year < r.yearFrom) || (r.yearTo > 0 && game.year > r.yearTo)))
        return false;
    if (r.minRating >= 0 && (game.rating < 0 || game.rating < r.minRating))
        return false;
    if (!r.playTimes.isEmpty() && !r.playTimes.contains(playTimeBucket(game.groupPlayMinutes)))
        return false;
    if (!r.lastPlayed.isEmpty() && !playedWithin(r.lastPlayed, game.groupLastPlayed, QDateTime::currentDateTimeUtc()))
        return false;
    if (!r.achievements.isEmpty()
        && !r.achievements.contains(m_achievementState ? m_achievementState(game) : QStringLiteral("none")))
        return false;
    if (r.tags.isEmpty())
        return true;
    const auto has = [&](const QString &tag) {
        return std::any_of(game.tags.cbegin(), game.tags.cend(), [&](const GameTag &t) { return t.name == tag; });
    };
    return r.matchAllTags ? std::all_of(r.tags.cbegin(), r.tags.cend(), has)
                          : std::any_of(r.tags.cbegin(), r.tags.cend(), has);
}

bool isMember(const Game &game, const Collection &collection)
{
    return CollectionMatcher(collection).isMember(game);
}
