#include "tomlsection.h"

TomlSection::TomlSection(const QByteArray &bytes, const QString &displayPath, std::string_view name)
    : m_displayPath(displayPath)
{
    try {
        m_root = toml::parse(std::string_view(bytes.constData(), size_t(bytes.size())));
    } catch (const toml::parse_error &e) {
        warnings.append(
            QStringLiteral("%1:%2: %3").arg(displayPath).arg(e.source().begin.line).arg(toQString(e.description())));
        return;
    }
    ok = true;

    const toml::node *section = m_root.get(name);
    if (!section)
        return;
    table = section->as_table();
    if (!table)
        warnings.append(QStringLiteral("%1: %2 must be a [%2] section").arg(where(*section), toQString(name)));
}

QString TomlSection::where(const toml::node &node) const
{
    return QStringLiteral("%1:%2").arg(m_displayPath).arg(node.source().begin.line);
}
