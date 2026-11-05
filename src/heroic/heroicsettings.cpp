#include "heroicsettings.h"

#include "suggestion.h"
#include "tomlsection.h"

Parsed<HeroicSettings> parseHeroicSettings(const QByteArray &bytes, const QString &displayPath)
{
    const TomlSection section(bytes, displayPath, "heroic");
    Parsed<HeroicSettings> out{section.ok, {}, section.warnings};
    if (!section.table)
        return out;

    const QStringList known = {QStringLiteral("enabled"), QStringLiteral("path"), QStringLiteral("whole_library")};
    for (auto &&[k, node] : *section.table) {
        const QString key = toQString(k.str());
        if (key == u"enabled" || key == u"whole_library") {
            if (const auto *b = node.as_boolean())
                (key == u"enabled" ? out.value.enabled : out.value.wholeLibrary) = b->get();
            else
                out.warnings.append(QStringLiteral("%1: %2 must be true or false").arg(section.where(node), key));
        } else if (key == u"path") {
            if (const auto *s = node.as_string())
                out.value.path = toQString(s->get());
            else
                out.warnings.append(QStringLiteral("%1: path must be text in quotes").arg(section.where(node)));
        } else {
            out.warnings.append(
                QStringLiteral("%1: unknown key %2%3").arg(section.where(node), key, suggestion(key, known)));
        }
    }
    return out;
}
