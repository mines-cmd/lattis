#pragma once

#include <QByteArray>
#include <QString>
#include <QStringList>

#include <toml++/toml.hpp>

inline QString toQString(std::string_view s)
{
    return QString::fromUtf8(s.data(), qsizetype(s.size()));
}

// See docs/code/core.md.
class TomlSection
{
public:
    TomlSection(const QByteArray &bytes, const QString &displayPath, std::string_view name);
    // `table` points into the parsed file this object holds.
    TomlSection(const TomlSection &) = delete;
    TomlSection &operator=(const TomlSection &) = delete;

    QString where(const toml::node &node) const;

    bool ok = false;
    QStringList warnings;
    const toml::table *table = nullptr;

private:
    toml::table m_root;
    QString m_displayPath;
};
