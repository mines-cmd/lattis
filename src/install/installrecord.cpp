#include "installrecord.h"
#include "tomlsection.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>

#include <sstream>

namespace InstallRecords {
namespace {
QString text(const toml::table &table, std::string_view key)
{
    const auto *s = table[key].as_string();
    return s ? toQString(s->get()) : QString();
}
}

QString path(const QString &stateDir)
{
    return stateDir + QStringLiteral("/installs.toml");
}

Parsed<Records> parse(const QByteArray &bytes)
{
    Parsed<Records> out;
    toml::table root;
    try {
        root = toml::parse(std::string_view(bytes.constData(), size_t(bytes.size())));
    } catch (const toml::parse_error &e) {
        out.warnings.append(QStringLiteral("installs.toml:%1: %2").arg(e.source().begin.line).arg(toQString(e.description())));
        return out;
    }
    out.ok = true;
    for (auto &&[key, node] : root) {
        const toml::table *table = node.as_table();
        if (!table)
            continue;
        InstallRecord r;
        r.route = text(*table, "route");
        r.file = text(*table, "file");
        r.tag = text(*table, "tag");
        r.sha256 = text(*table, "sha256");
        r.assetId = text(*table, "asset_id");
        r.assetDate = text(*table, "asset_date");
        r.installed = text(*table, "installed");
        r.desktop = text(*table, "desktop");
        r.icon = text(*table, "icon");
        if (r.route.isEmpty() || r.file.isEmpty())
            continue;
        out.value.insert(toQString(key.str()), r);
    }
    return out;
}

Parsed<Records> read(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        Parsed<Records> none;
        none.ok = true;
        return none;
    }
    Parsed<Records> out = parse(file.readAll());
    for (QString &warning : out.warnings)
        warning.replace(QStringLiteral("installs.toml"), path);
    return out;
}

bool write(const QString &path, const Records &records, QString *error)
{
    toml::table root;
    for (auto it = records.cbegin(); it != records.cend(); ++it) {
        toml::table values;
        const auto put = [&](const char *key, const QString &value) {
            if (!value.isEmpty())
                values.insert(key, value.toStdString());
        };
        const InstallRecord &r = it.value();
        put("route", r.route);
        put("file", r.file);
        put("tag", r.tag);
        put("sha256", r.sha256);
        put("asset_id", r.assetId);
        put("asset_date", r.assetDate);
        put("installed", r.installed);
        put("desktop", r.desktop);
        put("icon", r.icon);
        root.insert(it.key().toStdString(), std::move(values));
    }
    std::ostringstream out;
    out << "# What Lattis installed on this computer; written by the app.\n\n" << toml::toml_formatter(root);

    if (!QDir().mkpath(QFileInfo(path).absolutePath())) {
        *error = QStringLiteral("Couldn't make the folder for %1").arg(path);
        return false;
    }
    QSaveFile file(path);
    const std::string bytes = out.str();
    if (!file.open(QIODevice::WriteOnly) || file.write(bytes.data(), qint64(bytes.size())) != qint64(bytes.size())
        || !file.commit()) {
        *error = QStringLiteral("Couldn't write %1: %2").arg(path, file.errorString());
        return false;
    }
    return true;
}

bool stillThere(const InstallRecord &record, const QSet<QString> &userFlatpaks)
{
    if (record.route == QLatin1String("flatpak"))
        return userFlatpaks.contains(record.file);
    return record.route == QLatin1String("appimage") && QFileInfo(record.file).isFile();
}
}
