#include "listsettings.h"
#include "tomlsection.h"
#include "suggestion.h"

#include <QHash>

#include <toml++/toml.hpp>

namespace {
const QStringList kSorts = {QStringLiteral("title"), QStringLiteral("last_played"), QStringLiteral("play_time")};

bool defaultDescending(const QString &sort)
{
    return sort != QLatin1String("title");
}

QString quoted(const QString &s)
{
    QString escaped = s;
    escaped.replace(u'\\', QLatin1String("\\\\")).replace(u'"', QLatin1String("\\\""));
    return u'"' + escaped + u'"';
}

QString boolText(bool b)
{
    return b ? QStringLiteral("true") : QStringLiteral("false");
}

QString quotedList(const QStringList &list)
{
    QStringList items;
    for (const QString &s : list)
        items.append(quoted(s));
    return items.join(QLatin1String(", "));
}
}

const QStringList &listGroupings()
{
    static const QStringList groupings = {QStringLiteral("platform"), QStringLiteral("status"), QStringLiteral("none")};
    return groupings;
}

const QStringList &listStatuses()
{
    static const QStringList statuses = {QStringLiteral("playing"),   QStringLiteral("not_started"),
                                         QStringLiteral("finished"),  QStringLiteral("completed"),
                                         QStringLiteral("dropped"),   QStringLiteral("none")};
    return statuses;
}

Parsed<ListSettings> parseListSettings(const QByteArray &bytes, const QString &displayPath)
{
    const TomlSection section(bytes, displayPath, "list");
    Parsed<ListSettings> out{section.ok, {}, section.warnings};
    if (!section.table)
        return out;
    const toml::table *table = section.table;

    // The sort first: it sets the default for `descending`.
    if (const toml::node *node = table->get("sort")) {
        const auto *s = node->as_string();
        const QString sort = s ? toQString(s->get()) : QString();
        if (kSorts.contains(sort))
            out.value.sort = sort;
        else
            out.warnings.append(QStringLiteral("%1: sort must be \"title\", \"last_played\" or \"play_time\"%2")
                                    .arg(section.where(*node), s ? suggestion(sort, kSorts) : QString()));
    }
    out.value.descending = defaultDescending(out.value.sort);

    // group_by_platform: the key before group_by; `false` is "none" unless group_by is set.
    bool groupByPlatform = true;
    const QStringList known = {QStringLiteral("sort"),           QStringLiteral("descending"),
                               QStringLiteral("installed_only"), QStringLiteral("favorites_only"),
                               QStringLiteral("tags"),           QStringLiteral("match_all_tags"),
                               QStringLiteral("statuses"),       QStringLiteral("show_hidden"),
                               QStringLiteral("show_sort_value"), QStringLiteral("group_by")};
    const QHash<QString, bool *> bools = {
        {QStringLiteral("descending"), &out.value.descending},
        {QStringLiteral("installed_only"), &out.value.installedOnly},
        {QStringLiteral("favorites_only"), &out.value.favoritesOnly},
        {QStringLiteral("match_all_tags"), &out.value.matchAllTags},
        {QStringLiteral("show_hidden"), &out.value.showHidden},
        {QStringLiteral("show_sort_value"), &out.value.showSortValue},
        {QStringLiteral("group_by_platform"), &groupByPlatform},
    };
    bool groupBySet = false;
    for (auto &&[k, node] : *table) {
        const QString key = toQString(k.str());
        if (key == QLatin1String("sort"))
            continue;
        if (key == QLatin1String("group_by")) {
            const auto *s = node.as_string();
            const QString groupBy = s ? toQString(s->get()) : QString();
            if (listGroupings().contains(groupBy)) {
                out.value.groupBy = groupBy;
                groupBySet = true;
            } else {
                out.warnings.append(QStringLiteral("%1: group_by must be %2%3")
                                        .arg(section.where(node), quotedList(listGroupings()),
                                             s ? suggestion(groupBy, listGroupings()) : QString()));
            }
        } else if (key == QLatin1String("statuses")) {
            const auto *array = node.as_array();
            if (!array || (!array->empty() && !array->is_homogeneous(toml::node_type::string))) {
                out.warnings.append(QStringLiteral("%1: statuses must be a list of statuses, like [\"playing\", \"none\"]")
                                        .arg(section.where(node)));
                continue;
            }
            for (const toml::node &item : *array) {
                const QString status = toQString(item.as_string()->get());
                if (!listStatuses().contains(status))
                    out.warnings.append(QStringLiteral("%1: unknown status \"%2\" in statuses: they're %3%4")
                                            .arg(section.where(item), status, quotedList(listStatuses()),
                                                 suggestion(status, listStatuses())));
                else if (!out.value.statuses.contains(status))
                    out.value.statuses.append(status);
            }
        } else if (bool *flag = bools.value(key)) {
            if (const auto *b = node.as_boolean())
                *flag = b->get();
            else
                out.warnings.append(QStringLiteral("%1: %2 must be true or false").arg(section.where(node), key));
        } else if (key == QLatin1String("tags")) {
            const auto *array = node.as_array();
            const bool strings = array && array->is_homogeneous(toml::node_type::string);
            if (!array || (!array->empty() && !strings)) {
                out.warnings.append(QStringLiteral("%1: tags must be a list of tag names, like [\"co-op\", \"rpg\"]")
                                        .arg(section.where(node)));
                continue;
            }
            for (const toml::node &tag : *array) {
                const QString name = toQString(tag.as_string()->get());
                if (!out.value.tags.contains(name))
                    out.value.tags.append(name);
            }
        } else {
            out.warnings.append(QStringLiteral("%1: unknown key %2%3").arg(section.where(node), key, suggestion(key, known)));
        }
    }
    if (!groupByPlatform && !groupBySet)
        out.value.groupBy = QStringLiteral("none");
    return out;
}

QList<std::pair<QString, QString>> listSettingsChanges(const ListSettings &from, const ListSettings &to)
{
    const ListSettings defaults;
    QList<std::pair<QString, QString>> out;
    const auto add = [&](const char *key, bool changed, bool isDefault, const QString &value) {
        if (changed)
            out.append({QString::fromLatin1(key), isDefault ? QString() : value});
    };
    add("sort", from.sort != to.sort, to.sort == defaults.sort, quoted(to.sort));
    // Written only when it isn't the sort's usual direction; a sort change rechecks it.
    add("descending", from.descending != to.descending || from.sort != to.sort,
        to.descending == defaultDescending(to.sort), boolText(to.descending));
    add("installed_only", from.installedOnly != to.installedOnly, to.installedOnly == defaults.installedOnly,
        boolText(to.installedOnly));
    add("favorites_only", from.favoritesOnly != to.favoritesOnly, to.favoritesOnly == defaults.favoritesOnly,
        boolText(to.favoritesOnly));
    add("tags", from.tags != to.tags, to.tags.isEmpty(), u'[' + quotedList(to.tags) + u']');
    add("match_all_tags", from.matchAllTags != to.matchAllTags, to.matchAllTags == defaults.matchAllTags,
        boolText(to.matchAllTags));
    add("statuses", from.statuses != to.statuses, to.statuses.isEmpty(), u'[' + quotedList(to.statuses) + u']');
    add("show_hidden", from.showHidden != to.showHidden, to.showHidden == defaults.showHidden,
        boolText(to.showHidden));
    add("show_sort_value", from.showSortValue != to.showSortValue, to.showSortValue == defaults.showSortValue,
        boolText(to.showSortValue));
    add("group_by", from.groupBy != to.groupBy, to.groupBy == defaults.groupBy, quoted(to.groupBy));
    // The old key goes with any change of the grouping, so it can't contradict group_by.
    add("group_by_platform", from.groupBy != to.groupBy, true, {});
    return out;
}
