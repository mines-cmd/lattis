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
        QStringLiteral("recent_play_time"), QStringLiteral("date_added"),
        QStringLiteral("steam_waiting"),   QStringLiteral("steam_update"),     QStringLiteral("steam_family"),
        QStringLiteral("last_backup"),     QStringLiteral("protondb")};
    return names;
}

QStringList &shownOf(GamePageSettings &s, const QString &place)
{
    return place == GamePage::chips     ? s.chips
           : place == GamePage::buttons ? s.buttons
           : place == GamePage::sections ? s.sections
                                         : s.infoRows;
}

QStringList &orderOf(GamePageSettings &s, const QString &place)
{
    return place == GamePage::chips     ? s.chipsOrder
           : place == GamePage::buttons ? s.buttonsOrder
           : place == GamePage::sections ? s.sectionsOrder
                                         : s.infoRowsOrder;
}

const QStringList &orderOf(const GamePageSettings &s, const QString &place)
{
    return orderOf(const_cast<GamePageSettings &>(s), place);
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
            QStringLiteral("steam_update"), QStringLiteral("steam_family")};
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

QStringList GamePageSettings::defaultSections()
{
    return {QStringLiteral("description"), QStringLiteral("clips"), QStringLiteral("tags")};
}

const QStringList &gamePageChoices(const QString &place)
{
    static const QStringList sections = GamePageSettings::defaultSections();
    if (place == GamePage::sections)
        return sections;
    static const QStringList buttons = {QStringLiteral("favorite"), QStringLiteral("open_platform"),
                                        QStringLiteral("folder"),   QStringLiteral("settings"),
                                        QStringLiteral("save_folder"), QStringLiteral("gallery"), QStringLiteral("mod_manager"),
                                        QStringLiteral("menu")};
    return place == GamePage::buttons ? buttons : fields();
}

const QStringList &recentPeriods()
{
    static const QStringList periods = {QStringLiteral("week"), QStringLiteral("two_weeks"), QStringLiteral("month"),
                                        QStringLiteral("year")};
    return periods;
}

const QStringList &playTimeFormats()
{
    static const QStringList formats = {QStringLiteral("hours"), QStringLiteral("hours_minutes")};
    return formats;
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

    const QStringList places = {GamePage::chips, GamePage::buttons, GamePage::infoRows, GamePage::sections};
    QStringList known = places;
    for (const QString &place : places)
        known.append(place + QStringLiteral("_order"));
    const qsizetype listKeys = known.size();
    known += {QStringLiteral("recent_period"), QStringLiteral("play_time_format"),
              QStringLiteral("chips_without_icon"), QStringLiteral("collapsed_sections"),
              QStringLiteral("show_screenshots"), QStringLiteral("show_video"), QStringLiteral("show_achievements")};
    // Before `sections`, each of them had a switch; a false one still hides it when `sections` isn't set.
    const QStringList flags = {QStringLiteral("show_description"), QStringLiteral("show_tags"),
                               QStringLiteral("show_clips")};
    known += flags;
    QStringList hiddenByFlag;
    bool sectionsSet = false;
    for (auto &&[k, node] : *section.table) {
        const QString key = toQString(k.str());
        const qsizetype at = known.indexOf(key);
        if (at < 0) {
            out.warnings.append(
                QStringLiteral("%1: unknown key %2%3").arg(section.where(node), key, suggestion(key, known)));
        } else if (at < listKeys) {
            const QString &place = places.at(at % places.size());
            const bool shown = at < places.size();
            if (readNames(node, key, place, section, shown ? shownOf(out.value, place) : orderOf(out.value, place),
                          out.warnings)
                && shown && place == GamePage::sections)
                sectionsSet = true;
        } else if (key == QLatin1String("chips_without_icon")) {
            readNames(node, key, GamePage::chips, section, out.value.chipsWithoutIcon, out.warnings);
        } else if (key == QLatin1String("collapsed_sections")) {
            readNames(node, key, GamePage::sections, section, out.value.collapsedSections, out.warnings);
        } else if (key == QLatin1String("show_screenshots") || key == QLatin1String("show_video")
                   || key == QLatin1String("show_achievements")) {
            if (const auto *b = node.as_boolean())
                (key == QLatin1String("show_video")          ? out.value.showVideo
                 : key == QLatin1String("show_achievements") ? out.value.showAchievements
                                                             : out.value.showScreenshots) = b->get();
            else
                out.warnings.append(QStringLiteral("%1: %2 must be true or false").arg(section.where(node), key));
        } else if (flags.contains(key)) {
            if (const auto *b = node.as_boolean()) {
                if (!b->get())
                    hiddenByFlag.append(GamePageSettings::defaultSections().at(flags.indexOf(key)));
            } else {
                out.warnings.append(QStringLiteral("%1: %2 must be true or false").arg(section.where(node), key));
            }
        } else {
            const bool period = key == QLatin1String("recent_period");
            const QStringList &choices = period ? recentPeriods() : playTimeFormats();
            const auto *text = node.as_string();
            if (text && choices.contains(toQString(text->get())))
                (period ? out.value.recentPeriod : out.value.playTimeFormat) = toQString(text->get());
            else
                out.warnings.append(QStringLiteral("%1: %2 must be \"%3\"")
                                        .arg(section.where(node), key, choices.join(QStringLiteral("\", \""))));
        }
    }
    if (!sectionsSet)
        for (const QString &name : hiddenByFlag)
            out.value.sections.removeAll(name);
    return out;
}

bool isGamePagePlace(const QString &place)
{
    return place == GamePage::chips || place == GamePage::buttons || place == GamePage::infoRows
           || place == GamePage::sections;
}

const QStringList &gamePageShown(const GamePageSettings &settings, const QString &place)
{
    return shownOf(const_cast<GamePageSettings &>(settings), place);
}
