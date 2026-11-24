#include "mediafavorites.h"

#include "emulators.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>

#include <toml++/toml.hpp>

#include <algorithm>
#include <sstream>

namespace MediaFavorites {
Parsed<QSet<QString>> read(const QString &path, const QString &home)
{
    Parsed<QSet<QString>> out;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        if (QFileInfo::exists(path))
            out.warnings.append(
                QStringLiteral("Could not read %1: %2").arg(QFileInfo(path).fileName(), file.errorString()));
        else
            out.ok = true;
        return out;
    }
    toml::table root;
    try {
        root = toml::parse(file.readAll().toStdString());
    } catch (const toml::parse_error &e) {
        out.warnings.append(QStringLiteral("%1:%2: %3")
                                .arg(QFileInfo(path).fileName())
                                .arg(e.source().begin.line)
                                .arg(QString::fromUtf8(e.description().data(), qsizetype(e.description().size()))));
        return out;
    }
    out.ok = true;
    const toml::array *list = root["favorites"].as_array();
    if (!list)
        return out;
    for (const toml::node &entry : *list) {
        if (const auto value = entry.value<std::string>(); value && !value->empty())
            out.value.insert(Emulators::expandHome(QString::fromStdString(*value), home));
    }
    return out;
}

bool write(const QString &path, const QSet<QString> &favorites, const QString &home)
{
    QStringList sorted;
    for (const QString &p : favorites) {
        const bool inHome = !home.isEmpty() && (p == home || p.startsWith(home + u'/'));
        sorted.append(inHome ? QStringLiteral("~") + p.mid(home.size()) : p);
    }
    std::sort(sorted.begin(), sorted.end());
    toml::array list;
    for (const QString &p : std::as_const(sorted))
        list.push_back(p.toStdString());
    toml::table root;
    root.insert("favorites", std::move(list));
    std::ostringstream text;
    text << root << '\n';

    QDir().mkpath(QFileInfo(path).absolutePath());
    const QString partial = path + QStringLiteral(".partial");
    QFile file(partial);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return false;
    const std::string bytes = text.str();
    const bool ok = file.write(bytes.data(), qint64(bytes.size())) == qint64(bytes.size());
    file.close();
    if (!ok || !(QFile::remove(path) || !QFileInfo::exists(path)) || !QFile::rename(partial, path)) {
        QFile::remove(partial);
        return false;
    }
    return true;
}
}
