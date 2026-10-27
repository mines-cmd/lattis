#include "gallerysettings.h"
#include "suggestion.h"
#include "tomlsection.h"

const QStringList &galleryGroups()
{
    static const QStringList groups = {QStringLiteral("month"), QStringLiteral("game"), QStringLiteral("none")};
    return groups;
}

const QStringList &gallerySorts()
{
    static const QStringList sorts = {QStringLiteral("newest"), QStringLiteral("oldest")};
    return sorts;
}

const QStringList &gallerySections()
{
    static const QStringList sections = {QStringLiteral("type"), QStringLiteral("platform"), QStringLiteral("game"),
                                         QStringLiteral("date")};
    return sections;
}

Parsed<GallerySettings> parseGallerySettings(const QByteArray &bytes, const QString &displayPath)
{
    const TomlSection section(bytes, displayPath, "gallery");
    Parsed<GallerySettings> out{section.ok, {}, section.warnings};
    if (!section.table)
        return out;

    const QStringList known = {QStringLiteral("group"), QStringLiteral("sort"), QStringLiteral("tile_width"),
                               QStringLiteral("folded_sections"), QStringLiteral("captions")};
    for (auto &&[k, node] : *section.table) {
        const QString key = toQString(k.str());
        if (key == known.at(0) || key == known.at(1)) {
            const bool group = key == known.at(0);
            const QStringList &choices = group ? galleryGroups() : gallerySorts();
            const auto *text = node.as_string();
            if (text && choices.contains(toQString(text->get())))
                (group ? out.value.group : out.value.sort) = toQString(text->get());
            else
                out.warnings.append(QStringLiteral("%1: %2 must be \"%3\"")
                                        .arg(section.where(node), key, choices.join(QStringLiteral("\", \""))));
        } else if (key == known.at(2)) {
            const std::optional<int64_t> width = node.value<int64_t>();
            if (width && *width >= 80 && *width <= 1000)
                out.value.tileWidth = int(*width);
            else
                out.warnings.append(
                    QStringLiteral("%1: tile_width must be a whole number from 80 to 1000").arg(section.where(node)));
        } else if (key == known.at(3)) {
            const auto *array = node.as_array();
            if (!array || (!array->empty() && !array->is_homogeneous(toml::node_type::string))) {
                out.warnings.append(
                    QStringLiteral("%1: folded_sections must be a list, like [\"date\"]").arg(section.where(node)));
                continue;
            }
            for (const toml::node &item : *array) {
                const QString name = toQString(item.as_string()->get());
                if (!gallerySections().contains(name))
                    out.warnings.append(QStringLiteral("%1: unknown section \"%2\": they're \"%3\"%4")
                                            .arg(section.where(item), name,
                                                 gallerySections().join(QStringLiteral("\", \"")),
                                                 suggestion(name, gallerySections())));
                else if (!out.value.foldedSections.contains(name))
                    out.value.foldedSections.append(name);
            }
        } else if (key == known.at(4)) {
            if (const auto *flag = node.as_boolean())
                out.value.captions = flag->get();
            else
                out.warnings.append(QStringLiteral("%1: captions must be true or false").arg(section.where(node)));
        } else {
            out.warnings.append(
                QStringLiteral("%1: unknown key %2%3").arg(section.where(node), key, suggestion(key, known)));
        }
    }
    return out;
}
