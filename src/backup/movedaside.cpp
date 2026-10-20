#include "movedaside.h"
#include "backuparchive.h"

#include <QCoreApplication>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QTimeZone>

#include <toml++/toml.hpp>

#include <sstream>

#include <sys/stat.h>

namespace MovedAside {
namespace {
const QString kAside = QStringLiteral(".before-import-");

bool exists(const QString &path)
{
    struct stat st;
    return ::lstat(QFile::encodeName(path).constData(), &st) == 0;
}

QString toQString(const std::string &s)
{
    return QString::fromStdString(s);
}

QDateTime fromToml(const toml::date_time &dt)
{
    const QDate date(dt.date.year, dt.date.month, dt.date.day);
    const QTime time(dt.time.hour, dt.time.minute, dt.time.second);
    return dt.offset ? QDateTime(date, time, QTimeZone::fromSecondsAheadOfUtc(dt.offset->minutes * 60))
                     : QDateTime(date, time);
}

toml::date_time toToml(const QDateTime &time)
{
    const QDate d = time.date();
    const QTime t = time.time();
    return toml::date_time(toml::date(d.year(), unsigned(d.month()), unsigned(d.day())),
                           toml::time(unsigned(t.hour()), unsigned(t.minute()), unsigned(t.second())),
                           toml::time_offset(0, int16_t(time.offsetFromUtc() / 60)));
}
}

QString file(const QString &stateDir, const QString &dataDirKey)
{
    return QDir(stateDir).filePath(QStringLiteral("imports-%1.toml").arg(dataDirKey));
}

QList<Import> read(const QString &file)
{
    QList<Import> out;
    QFile f(file);
    if (!f.open(QIODevice::ReadOnly))
        return out;
    const QByteArray bytes = f.readAll();
    toml::table root;
    try {
        root = toml::parse(std::string_view(bytes.constData(), size_t(bytes.size())));
    } catch (const toml::parse_error &) {
        qCWarning(lcBackup).noquote() << file << "doesn't parse; what the imports moved aside is forgotten";
        return out;
    }
    const toml::array *imports = root["import"].as_array();
    if (!imports)
        return out;
    for (const toml::node &node : *imports) {
        const toml::table *table = node.as_table();
        if (!table)
            continue;
        Import import;
        if (const auto *s = table->get_as<std::string>("kind"))
            import.kind = toQString(s->get());
        if (const auto *s = table->get_as<std::string>("archive"))
            import.archive = toQString(s->get());
        if (const auto *t = table->get_as<toml::date_time>("time"))
            import.time = fromToml(t->get());
        if (const toml::array *paths = table->get_as<toml::array>("moved_aside")) {
            for (const toml::node &path : *paths) {
                if (const auto *s = path.as_string())
                    import.paths.append(toQString(s->get()));
            }
        }
        if (const auto *b = table->get_as<bool>("notice"))
            import.notice = b->get();
        out.append(import);
    }
    return out;
}

bool write(const QString &file, const QList<Import> &imports, QString *error)
{
    toml::array array;
    for (const Import &import : imports) {
        toml::array paths;
        for (const QString &path : import.paths)
            paths.push_back(path.toStdString());
        array.push_back(toml::table{
            {"kind", import.kind.toStdString()},
            {"archive", import.archive.toStdString()},
            {"time", toToml(import.time)},
            {"moved_aside", std::move(paths)},
            {"notice", import.notice},
        });
    }
    std::ostringstream text;
    text << "# What each Import moved aside, until it's deleted (Settings → Library).\n\n";
    if (!array.empty())
        text << toml::table{{"import", std::move(array)}} << '\n';

    QDir().mkpath(QFileInfo(file).absolutePath());
    QSaveFile out(file);
    const std::string s = text.str();
    if (!out.open(QIODevice::WriteOnly) || out.write(s.data(), qint64(s.size())) < 0 || !out.commit()) {
        if (error)
            *error = out.errorString();
        return false;
    }
    return true;
}

bool record(const QString &file, const Import &import, QString *error)
{
    QList<Import> all = read(file);
    all.append(import);
    return write(file, all, error);
}

QList<Import> present(QList<Import> imports)
{
    QList<Import> out;
    for (Import &import : imports) {
        const bool movedAny = !import.paths.isEmpty();
        import.paths.removeIf([](const QString &path) { return !exists(path); });
        if (import.paths.isEmpty() && (movedAny || !import.notice))
            continue;
        out.append(import);
    }
    return out;
}

qint64 bytes(const QStringList &paths)
{
    qint64 total = 0;
    for (const QString &path : paths) {
        const QFileInfo info(path);
        if (info.isSymLink())
            continue;
        if (!info.isDir()) {
            total += info.size();
            continue;
        }
        QDirIterator it(path, QDir::Files | QDir::Hidden | QDir::System | QDir::NoSymLinks, QDirIterator::Subdirectories);
        while (it.hasNext())
            total += it.nextFileInfo().size();
    }
    return total;
}

bool moveToTrash(const QString &path, QString *error)
{
    QFile file(path);
    if (file.moveToTrash())
        return true;
    *error = file.errorString();
    return false;
}

QString remove(const QStringList &paths, const Trash &trash)
{
    QStringList failed;
    for (const QString &path : paths) {
        if (!exists(path))
            continue;
        QString error;
        if (!trash(path, &error)) {
            failed.append(QCoreApplication::translate("MovedAside", "Couldn't move %1 to the trash: %2").arg(path, error));
            continue;
        }
        // A Proton prefix went from compatdata.before-import-<date>/<shortcut>.
        const QString parent = QFileInfo(path).path();
        if (QFileInfo(parent).fileName().contains(kAside))
            QDir().rmdir(parent);
    }
    return failed.join(u'\n');
}
}
