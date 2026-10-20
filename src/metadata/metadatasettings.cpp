#include "metadatasettings.h"
#include "tomlsection.h"
#include "suggestion.h"

#include <toml++/toml.hpp>

Parsed<MetadataSettings> parseMetadataSettings(const QByteArray &bytes, const QString &displayPath)
{
    const TomlSection section(bytes, displayPath, "metadata");
    Parsed<MetadataSettings> out{section.ok, {}, section.warnings};
    if (!section.table)
        return out;
    const toml::table *table = section.table;

    const QStringList known = {QStringLiteral("look_up_new_games")};
    for (auto &&[k, node] : *table) {
        const QString key = toQString(k.str());
        if (key == known.at(0)) {
            if (const auto *b = node.as_boolean())
                out.value.lookUpNewGames = b->get();
            else
                out.warnings.append(QStringLiteral("%1: look_up_new_games must be true or false").arg(section.where(node)));
        } else {
            out.warnings.append(QStringLiteral("%1: unknown key %2%3").arg(section.where(node), key, suggestion(key, known)));
        }
    }
    return out;
}
