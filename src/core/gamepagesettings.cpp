#include "gamepagesettings.h"
#include "suggestion.h"
#include "tomlsection.h"

#include <toml++/toml.hpp>

namespace {
const QStringList &fields()
{
    static const QStringList names = {
        QStringLiteral("play_time"),    QStringLiteral("last_played"),      QStringLiteral("release_date"),
        QStringLiteral("platform"),     QStringLiteral("status"),           QStringLiteral("install_size"),
        QStringLiteral("rating"),       QStringLiteral("times_played"),     QStringLiteral("first_played"),
        QStringLiteral("recent_play_time"), QStringLiteral("date_added")};
    return names;
}

QStringList &shownOf(GamePageSettings &s, const QString &place)
{
    return place == GamePage::chips ? s.chips : place == GamePage::buttons ? s.buttons : s.infoRows;
}


QStringList &orderOf(GamePageSettings &s, const QString &place)
{
    return place == GamePage::chips ? s.chipsOrder : place == GamePage::buttons ? s.buttonsOrder : s.infoRowsOrder;
}

const QStringList &orderOf(const GamePageSettings &s, const QString &place)
{
    return place == GamePage::chips ? s.chipsOrder : place == GamePage::buttons ? s.buttonsOrder : s.infoRowsOrder;
}

// Reads a list of names, dropping and reporting the ones that aren't choices or are listed twice.
bool readNames(const toml::node &node, const QString &key, const QString &place, const TomlSection &section,
               QStringList &out, QStringList &warnings)
{
    const auto *array = node.as_array();
    if (!array || (!array->empty() && !array->is_homogeneous(toml::node_type::string))) {
        warnings.append(QStringLiteral("%1: %2 must be a list, like [\"play_time\"]").arg(section.where(node), key));
        return false;
    }
    out.clear();
    const QStringList &choices = gamePageChoices(place);
    for (const toml::node &item : *array) {
        const QString name = toQString(item.as_string()->get());
        if (!choices.contains(name))
            warnings.append(QStringLiteral("%1: unknown name \"%2\" in %3: they're \"%4\"%5")
                                .arg(section.where(item), name, key, choices.join(QStringLiteral("\", \"")),
                                     suggestion(name, choices)));
        else if (!out.contains(name))
            out.append(name);
    }
    return true;
}
}

QStringList GamePageSettings::defaultChips()
{
    return {QStringLiteral("play_time"), QStringLiteral("last_played"), QStringLiteral("steam_waiting"),
            QStringLiteral("steam_update")};
}

QStringList GamePageSettings::defaultButtons()
{
    return {QStringLiteral("favorite"), QStringLiteral("open_platform"), QStringLiteral("folder"),
            QStringLiteral("settings")};
}

QStringList GamePageSettings::defaultInfoRows()
{
    return {QStringLiteral("release_date"), QStringLiteral("rating"), QStringLiteral("status"),
            QStringLiteral("install_size"), QStringLiteral("platform")};
}

const QStringList &gamePageChoices(const QString &place)
{
    // The Steam chips carry their own actions, so they're chips only; the buttons are the action row's.
    static const QStringList chips = fields() + QStringList{QStringLiteral("steam_waiting"), QStringLiteral("steam_update")};
    static const QStringList buttons = {QStringLiteral("favorite"), QStringLiteral("open_platform"),
                                        QStringLiteral("folder"),   QStringLiteral("settings"),
                                        QStringLiteral("save_folder"), QStringLiteral("menu")};
    if (place == GamePage::chips)
        return chips;
    return place == GamePage::buttons ? buttons : fields();
}

const QStringList &recentPeriods()
{
    static const QStringList periods = {QStringLiteral("week"), QStringLiteral("two_weeks"), QStringLiteral("month"),
                                        QStringLiteral("year")};
    return periods;
}

QStringList gamePageOrder(const GamePageSettings &settings, const QString &place)
{
    const QStringList &shown = gamePageShown(settings, place);
    const QStringList &choices = gamePageChoices(place);

    QStringList full = orderOf(settings, place).isEmpty() ? shown : orderOf(settings, place);
    for (const QString &name : choices)
        if (!full.contains(name))
            full.append(name);

    // The listed order of the shown ones wins over where `_order` put them.
    qsizetype next = 0;
    for (QString &name : full)
        if (shown.contains(name))
            name = shown.at(next++);
    return full;
}

Parsed<GamePageSettings> parseGamePageSettings(const QByteArray &bytes, const QString &displayPath)
{
    const TomlSection section(bytes, displayPath, "game_page");
    Parsed<GamePageSettings> out{section.ok, {}, section.warnings};
    if (!section.table)
        return out;

    const QStringList known = {QStringLiteral("chips"),       QStringLiteral("buttons"),
                               QStringLiteral("info_rows"),   QStringLiteral("chips_order"),
                               QStringLiteral("buttons_order"), QStringLiteral("info_rows_order"),
                               QStringLiteral("recent_period"), QStringLiteral("show_description"),
                               QStringLiteral("show_tags")};
    const QStringList places = {GamePage::chips, GamePage::buttons, GamePage::infoRows};
    for (auto &&[k, node] : *section.table) {
        const QString key = toQString(k.str());
        const qsizetype at = known.indexOf(key);
        if (at < 0) {
            out.warnings.append(
                QStringLiteral("%1: unknown key %2%3").arg(section.where(node), key, suggestion(key, known)));
        } else if (at < 6) {
            const QString &place = places.at(at % 3);
            readNames(node, key, place, section, at < 3 ? shownOf(out.value, place) : orderOf(out.value, place),
                      out.warnings);
        } else if (at == 6) {
            const auto *text = node.as_string();
            if (text && recentPeriods().contains(toQString(text->get())))
                out.value.recentPeriod = toQString(text->get());
            else
                out.warnings.append(QStringLiteral("%1: recent_period must be \"%2\"")
                                        .arg(section.where(node), recentPeriods().join(QStringLiteral("\", \""))));
        } else if (const auto *b = node.as_boolean()) {
            (at == 7 ? out.value.showDescription : out.value.showTags) = b->get();
        } else {
            out.warnings.append(QStringLiteral("%1: %2 must be true or false").arg(section.where(node), key));
        }
    }
    return out;
}

bool isGamePagePlace(const QString &place)
{
    return place == GamePage::chips || place == GamePage::buttons || place == GamePage::infoRows;
}

const QStringList &gamePageShown(const GamePageSettings &settings, const QString &place)
{
    return place == GamePage::chips ? settings.chips : place == GamePage::buttons ? settings.buttons : settings.infoRows;
}
