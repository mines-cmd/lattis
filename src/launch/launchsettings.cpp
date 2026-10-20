#include "launchsettings.h"
#include "tomlsection.h"
#include "suggestion.h"

#include <toml++/toml.hpp>

Parsed<LaunchSettings> parseLaunchSettings(const QByteArray &bytes, const QString &displayPath)
{
    const TomlSection section(bytes, displayPath, "launch");
    Parsed<LaunchSettings> out{section.ok, {}, section.warnings};
    if (!section.table)
        return out;
    const toml::table *table = section.table;

    const QStringList known = {QStringLiteral("while_running"), QStringLiteral("min_session_seconds")};
    const QStringList modes = {QStringLiteral("stay"), QStringLiteral("minimize")};
    for (auto &&[k, node] : *table) {
        const QString key = toQString(k.str());
        if (key == known.at(0)) {
            const auto *s = node.as_string();
            const QString mode = s ? toQString(s->get()) : QString();
            if (mode == modes.at(0)) {
                out.value.whileRunning = LaunchSettings::WhileRunning::Stay;
            } else if (mode == modes.at(1)) {
                out.value.whileRunning = LaunchSettings::WhileRunning::Minimize;
            } else {
                out.warnings.append(QStringLiteral("%1: while_running must be \"stay\" or \"minimize\"%2")
                                        .arg(section.where(node), s ? suggestion(mode, modes) : QString()));
            }
        } else if (key == known.at(1)) {
            const auto *i = node.as_integer();
            if (i && i->get() >= 0 && i->get() <= 3600)
                out.value.minSessionSeconds = int(i->get());
            else
                out.warnings.append(
                    QStringLiteral("%1: min_session_seconds must be a whole number from 0 to 3600").arg(section.where(node)));
        } else {
            out.warnings.append(
                QStringLiteral("%1: unknown key %2%3").arg(section.where(node), key, suggestion(key, known)));
        }
    }
    return out;
}
