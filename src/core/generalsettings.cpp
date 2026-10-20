#include "generalsettings.h"
#include "tomlsection.h"
#include "suggestion.h"

#include <toml++/toml.hpp>

const QStringList &railButtons()
{
    static const QStringList buttons = {QStringLiteral("add_game"), QStringLiteral("play_statistics"),
                                        QStringLiteral("open_settings")};
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
                                 QStringLiteral("rail_hidden")};
    for (auto &&[k, node] : *table) {
        const QString key = toQString(k.str());
        if (key == known.at(4) || key == known.at(5)) {
            const auto *array = node.as_array();
            if (!array || (!array->empty() && !array->is_homogeneous(toml::node_type::string))) {
                out.warnings.append(QStringLiteral("%1: %2 must be a list, like [\"add_game\"]")
                                        .arg(section.where(node), key));
                continue;
            }
            QStringList &names = key == known.at(4) ? out.value.railHiddenButtons : out.value.railButtonOrder;
            for (const toml::node &item : *array) {
                const QString name = toQString(item.as_string()->get());
                if (!railButtons().contains(name))
                    out.warnings.append(QStringLiteral("%1: unknown rail button \"%2\": they're %3%4")
                                            .arg(section.where(item), name,
                                                 QStringLiteral("\"add_game\", \"play_statistics\", \"open_settings\""),
                                                 suggestion(name, railButtons())));
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
