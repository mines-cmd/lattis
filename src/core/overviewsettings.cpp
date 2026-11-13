#include "overviewsettings.h"
#include "suggestion.h"
#include "tomlsection.h"

const QStringList &overviewColumnCards()
{
    static const QStringList cards = {QStringLiteral("storage"),       QStringLiteral("platforms"),
                                      QStringLiteral("statuses"),      QStringLiteral("attention"),
                                      QStringLiteral("backlog"),       QStringLiteral("nearly_done"),
                                      QStringLiteral("tags"),          QStringLiteral("years"),
                                      QStringLiteral("compatibility"), QStringLiteral("saves"),
                                      QStringLiteral("facts")};
    return cards;
}

const QStringList &overviewRowCards()
{
    static const QStringList cards = {QStringLiteral("tiles"), QStringLiteral("continue_playing")};
    return cards;
}

static QStringList allCards()
{
    return overviewRowCards() + overviewColumnCards();
}

// Known names only, each once, in the order given.
static QStringList cleaned(const QStringList &names, const QStringList &allowed)
{
    QStringList out;
    for (const QString &name : names)
        if (allowed.contains(name) && !out.contains(name))
            out.append(name);
    return out;
}

Parsed<OverviewSettings> parseOverviewSettings(const QByteArray &bytes, const QString &displayPath)
{
    const TomlSection section(bytes, displayPath, "overview");
    Parsed<OverviewSettings> out{section.ok, {}, section.warnings};
    if (!section.table)
        return out;

    struct List
    {
        const char *key;
        QStringList *target;
        QStringList allowed;
    };
    OverviewSettings &value = out.value;
    const QStringList everything = allCards();
    const List lists[] = {{"left", &value.left, overviewColumnCards()},
                          {"right", &value.right, overviewColumnCards()},
                          {"hidden", &value.hidden, everything},
                          {"above", &value.above, overviewRowCards()},
                          {"below", &value.below, overviewRowCards()}};
    QStringList known;
    for (const List &list : lists)
        known.append(QString::fromLatin1(list.key));

    bool aboveSet = false;
    for (auto &&[k, node] : *section.table) {
        const QString key = toQString(k.str());
        const List *list = nullptr;
        for (const List &candidate : lists)
            if (key == QLatin1String(candidate.key))
                list = &candidate;
        if (!list) {
            out.warnings.append(
                QStringLiteral("%1: unknown key %2%3").arg(section.where(node), key, suggestion(key, known)));
            continue;
        }
        const auto *array = node.as_array();
        if (!array || (!array->empty() && !array->is_homogeneous(toml::node_type::string))) {
            out.warnings.append(
                QStringLiteral("%1: %2 must be a list of card names, like [\"storage\"]").arg(section.where(node), key));
            continue;
        }
        QStringList names;
        for (const toml::node &item : *array) {
            const QString name = toQString(item.as_string()->get());
            if (!list->allowed.contains(name))
                out.warnings.append(QStringLiteral("%1: unknown card \"%2\" in %3: they're \"%4\"%5")
                                        .arg(section.where(item), name, key,
                                             list->allowed.join(QStringLiteral("\", \"")),
                                             suggestion(name, list->allowed)));
            else if (!names.contains(name))
                names.append(name);
        }
        *list->target = names;
        if (key == QLatin1String("above"))
            aboveSet = true;
    }

    // Without an `above` key a card the user put below isn't also above, so the default doesn't fight `below`.
    if (!aboveSet)
        for (const QString &name : value.below)
            value.above.removeAll(name);
    return out;
}

OverviewLayout resolveOverview(const OverviewSettings &settings)
{
    OverviewLayout out;
    out.hidden = cleaned(settings.hidden, allCards());

    out.above = cleaned(settings.above, overviewRowCards());
    for (const QString &name : cleaned(settings.below, overviewRowCards()))
        if (!out.above.contains(name))
            out.below.append(name);
    for (const QString &name : overviewRowCards())
        if (!out.above.contains(name) && !out.below.contains(name))
            out.above.append(name);

    out.left = cleaned(settings.left, overviewColumnCards());
    for (const QString &name : cleaned(settings.right, overviewColumnCards()))
        if (!out.left.contains(name))
            out.right.append(name);
    if (out.left.isEmpty() && out.right.isEmpty())
        return out;

    out.automatic = false;
    for (const QString &name : overviewColumnCards())
        if (!out.left.contains(name) && !out.right.contains(name))
            (out.right.size() < out.left.size() ? out.right : out.left).append(name);
    return out;
}
