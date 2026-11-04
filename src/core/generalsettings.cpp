#include "generalsettings.h"
#include "tomlsection.h"
#include "suggestion.h"

#include <toml++/toml.hpp>

const QStringList &railButtons()
{
    static const QStringList buttons = {QStringLiteral("add_game"), QStringLiteral("play_statistics"),
                                        QStringLiteral("gallery"), QStringLiteral("open_settings")};
    return buttons;
}

QStringList orderedRailButtons(const QStringList &order)
{
    QStringList out = order;
    for (const QString &button : railButtons())
        if (!out.contains(button))
            out.append(button);
    return out;
}

const QStringList &toolbarButtons()
{
    static const QStringList buttons = {QStringLiteral("add_game"), QStringLiteral("play_statistics"),
                                        QStringLiteral("gallery"), QStringLiteral("open_settings"),
                                        QStringLiteral("steam_friends")};
    return buttons;
}

QStringList orderedToolbarButtons(const QStringList &order)
{
    QStringList out = order;
    for (const QString &button : toolbarButtons())
        if (!out.contains(button))
            out.append(button);
    return out;
}

Parsed<GeneralSettings> parseGeneralSettings(const QByteArray &bytes, const QString &displayPath)
{
    const TomlSection section(bytes, displayPath, "general");
    Parsed<GeneralSettings> out{section.ok, {}, section.warnings};
    if (!section.table)
        return out;
    const toml::table *table = section.table;

    const QStringList known = {QStringLiteral("start_fullscreen"), QStringLiteral("download_bar_collapsed"),
                                 QStringLiteral("system_locale"), QStringLiteral("reduce_motion"),
                                 QStringLiteral("rail_hidden_buttons"), QStringLiteral("rail_button_order"),
                                 QStringLiteral("rail_hidden"), QStringLiteral("clip_volume"),
                                 QStringLiteral("check_for_updates"), QStringLiteral("skipped_update"),
                                 QStringLiteral("app_menu_entry"), QStringLiteral("toolbar"),
                                 QStringLiteral("toolbar_hidden_buttons"), QStringLiteral("toolbar_button_order")};
    for (auto &&[k, node] : *table) {
        const QString key = toQString(k.str());
        if (key == known.at(9)) {
            if (const auto *s = node.as_string())
                out.value.skippedUpdate = toQString(s->get());
            else
                out.warnings.append(QStringLiteral("%1: %2 must be a version in quotes, like \"1.2\"")
                                        .arg(section.where(node), key));
            continue;
        }
        if (key == known.at(7)) {
            const std::optional<double> volume = node.value<double>();
            if (!volume || *volume < 0 || *volume > 1)
                out.warnings.append(QStringLiteral("%1: %2 must be a number from 0 to 1").arg(section.where(node), key));
            else
                out.value.clipVolume = *volume;
            continue;
        }
        const bool isToolbarList = key == known.at(12) || key == known.at(13);
        if (key == known.at(4) || key == known.at(5) || isToolbarList) {
            const auto *array = node.as_array();
            if (!array || (!array->empty() && !array->is_homogeneous(toml::node_type::string))) {
                out.warnings.append(QStringLiteral("%1: %2 must be a list, like [\"add_game\"]")
                                        .arg(section.where(node), key));
                continue;
            }
            QStringList &names = key == known.at(4)    ? out.value.railHiddenButtons
                                 : key == known.at(5)  ? out.value.railButtonOrder
                                 : key == known.at(12) ? out.value.toolbarHiddenButtons
                                                       : out.value.toolbarButtonOrder;
            const QStringList &valid = isToolbarList ? toolbarButtons() : railButtons();
            for (const toml::node &item : *array) {
                const QString name = toQString(item.as_string()->get());
                if (!valid.contains(name))
                    out.warnings.append(QStringLiteral("%1: unknown %2 button \"%3\": they're %4%5")
                                            .arg(section.where(item),
                                                 isToolbarList ? QStringLiteral("toolbar") : QStringLiteral("rail"), name,
                                                 QStringLiteral("\"add_game\", \"play_statistics\", \"gallery\", \"open_settings\""),
                                                 suggestion(name, valid)));
                else if (!names.contains(name))
                    names.append(name);
            }
            continue;
        }
        bool *target = key == known.at(0)   ? &out.value.startFullscreen
                       : key == known.at(1) ? &out.value.downloadBarCollapsed
                       : key == known.at(2) ? &out.value.systemLocale
                       : key == known.at(3) ? &out.value.reduceMotion
                       : key == known.at(6) ? &out.value.railHidden
                       : key == known.at(8) ? &out.value.checkForUpdates
                       : key == known.at(10) ? &out.value.appMenuEntry
                       : key == known.at(11) ? &out.value.toolbar
                                            : nullptr;
        if (!target) {
            out.warnings.append(
                QStringLiteral("%1: unknown key %2%3").arg(section.where(node), key, suggestion(key, known)));
        } else if (const auto *b = node.as_boolean()) {
            *target = b->get();
        } else {
            out.warnings.append(QStringLiteral("%1: %2 must be true or false").arg(section.where(node), key));
        }
    }
    return out;
}
