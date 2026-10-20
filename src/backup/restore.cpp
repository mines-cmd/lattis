#include "restore.h"
#include "tomlsection.h"
#include "backup.h"
#include "credentials.h"
#include "libraryversion.h"

#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QSet>
#include <QTimeZone>
#include <QtConcurrent/QtConcurrentRun>

#include <toml++/toml.hpp>

#include <fcntl.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>

#include <algorithm>
#include <cerrno>
#include <cstring>
#include <memory>

namespace Restore {
namespace {
QString tr(const char *text)
{
    return QCoreApplication::translate("Restore", text);
}

const QString kManifest = QStringLiteral("manifest.toml");

QString string(const toml::table &table, const char *key)
{
    const auto *value = table.get_as<std::string>(key);
    return value ? QString::fromStdString(value->get()) : QString();
}

qint64 integer(const toml::table &table, const char *key, qint64 fallback)
{
    const auto *value = table.get_as<int64_t>(key);
    return value ? value->get() : fallback;
}

bool boolean(const toml::table &table, const char *key)
{
    const auto *value = table.get_as<bool>(key);
    return value && value->get();
}

QStringList strings(const toml::table &table, const char *key)
{
    QStringList out;
    if (const toml::array *array = table.get_as<toml::array>(key)) {
        for (const toml::node &item : *array) {
            if (const auto *s = item.as_string())
                out.append(QString::fromStdString(s->get()));
        }
    }
    return out;
}

QDateTime dateTime(const toml::table &table, const char *key)
{
    const auto *value = table.get_as<toml::date_time>(key);
    if (!value)
        return {};
    const toml::date_time &dt = value->get();
    const QDate date(dt.date.year, dt.date.month, dt.date.day);
    const QTime time(dt.time.hour, dt.time.minute, dt.time.second);
    return dt.offset ? QDateTime(date, time, QTimeZone::fromSecondsAheadOfUtc(dt.offset->minutes * 60))
                     : QDateTime(date, time);
}

bool present(const QString &path)
{
    struct stat st;
    return ::lstat(QFile::encodeName(path).constData(), &st) == 0;
}

QString linkOnTheWay(const QString &root, const QString &relative)
{
    QString path = root;
    const QStringList parts = relative.split(u'/', Qt::SkipEmptyParts);
    for (qsizetype i = 0; i + 1 < parts.size(); ++i) {
        path += u'/' + parts[i];
        if (QFileInfo(path).isSymLink())
            return path;
    }
    return {};
}

// A link to a folder is followed: the saves go where it leads.
QString followed(const QString &path)
{
    const QFileInfo info(path);
    return info.isSymLink() && info.isDir() ? info.canonicalFilePath() : path;
}

QString errnoText()
{
    return QString::fromLocal8Bit(std::strerror(errno));
}

bool rename(const QString &from, const QString &to, QString *error)
{
    const QByteArray a = QFile::encodeName(from);
    const QByteArray b = QFile::encodeName(to);
    int result = ::renameat2(AT_FDCWD, a.constData(), AT_FDCWD, b.constData(), RENAME_NOREPLACE);
    if (result != 0 && (errno == EINVAL || errno == ENOSYS)) {
        // A filesystem without RENAME_NOREPLACE.
        if (present(to)) {
            errno = EEXIST;
        } else {
            result = ::rename(a.constData(), b.constData());
        }
    }
    if (result != 0) {
        *error = tr("Can't move %1 to %2: %3").arg(from, to, errnoText());
        return false;
    }
    return true;
}

bool makePath(const QString &folder, QStringList &created)
{
    QStringList missing;
    for (QString path = QDir::cleanPath(folder); !present(path); path = QFileInfo(path).path()) {
        missing.prepend(path);
        if (path == QFileInfo(path).path())
            break;
    }
    for (const QString &path : std::as_const(missing)) {
        if (!QDir().mkdir(path))
            return false;
        created.append(path);
    }
    return true;
}

bool copyMissing(const QString &from, const QString &to, const std::atomic_bool *cancel, QString *error)
{
    if (cancel && cancel->load()) {
        *error = tr("Cancelled");
        return false;
    }
    const QFileInfo info(from);
    if (info.isSymLink()) {
        if (present(to))
            return true;
        QByteArray target(4096, Qt::Uninitialized);
        const ssize_t length = ::readlink(QFile::encodeName(from).constData(), target.data(), target.size());
        if (length < 0 || length >= target.size()
            || ::symlink(target.left(length).constData(), QFile::encodeName(to).constData()) != 0) {
            *error = tr("Can't copy %1: %2").arg(from, errnoText());
            return false;
        }
        return true;
    }
    if (info.isDir()) {
        if (!present(to) && !QDir().mkdir(to)) {
            *error = tr("Can't create %1").arg(to);
            return false;
        }
        const QStringList names =
            QDir(from).entryList(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System, QDir::Name);
        for (const QString &name : names) {
            if (!copyMissing(from + u'/' + name, to + u'/' + name, cancel, error))
                return false;
        }
        return true;
    }
    if (present(to) || !info.isFile())
        return true;
    if (!QFile::copy(from, to)) {
        *error = tr("Can't copy %1 to %2").arg(from, to);
        return false;
    }
    return true;
}

QString libraryRelative(const QString &dataDir, const QString &path)
{
    return QDir(dataDir).relativeFilePath(path);
}
}

Opened parseManifest(const QByteArray &text, const QString &display)
{
    Opened out;
    const QString notABackup = tr("%1 isn't a Lattis backup.").arg(display);
    toml::table root;
    try {
        root = toml::parse(std::string_view(text.constData(), size_t(text.size())));
    } catch (const toml::parse_error &e) {
        qCInfo(lcBackup).noquote() << display << "manifest.toml:" << e.source().begin.line << toQString(e.description());
        out.error = notABackup;
        return out;
    }

    Manifest m;
    m.kind = string(root, "kind");
    m.format = int(integer(root, "format", 0));
    if ((m.kind != Backup::kLibrary && m.kind != Backup::kSaves) || m.format < 1) {
        out.error = notABackup;
        return out;
    }
    m.appVersion = string(root, "app_version");
    if (m.format > BackupPlan::kFormat) {
        out.error = tr("%1 was made by a newer version of Lattis (%2). Update the app to import it.")
                        .arg(display, m.appVersion);
        return out;
    }
    m.libraryId = string(root, "library_id");
    m.created = dateTime(root, "created");

    if (m.kind == Backup::kLibrary) {
        m.libraryVersion = int(integer(root, "library_version", 1));
        m.games = int(integer(root, "games", -1));
        m.bytes = integer(root, "bytes", -1);
        m.credentials = boolean(root, "credentials");
        if (m.libraryVersion > LibraryVersion::kCurrent)
            out.warning = tr("Its library is from a newer version of Lattis (%1): it opens read-only until "
                             "you update the app.")
                              .arg(m.appVersion);
    } else {
        m.keys = boolean(root, "keys");
        m.proton = boolean(root, "proton");
        if (const toml::array *emulators = root.get_as<toml::array>("emulator")) {
            for (const toml::node &node : *emulators) {
                const toml::table *t = node.as_table();
                if (!t || string(*t, "id").isEmpty())
                    continue;
                m.emulators.append({string(*t, "id"), string(*t, "name"), string(*t, "install"),
                                    strings(*t, "saves"), strings(*t, "keys"), strings(*t, "beside_roms"),
                                    integer(*t, "bytes", -1)});
            }
        }
        if (const toml::array *games = root.get_as<toml::array>("game")) {
            for (const toml::node &node : *games) {
                const toml::table *t = node.as_table();
                if (!t || string(*t, "id").isEmpty())
                    continue;
                m.protonGames.append({string(*t, "id"), string(*t, "title"), quint32(integer(*t, "shortcut", 0)),
                                      string(*t, "proton"), integer(*t, "bytes", -1)});
            }
        }
    }
    out.manifest = m;
    return out;
}

Opened open(const QString &archive)
{
    QString error;
    const std::optional<QByteArray> text = Archive::readFirst(archive, kManifest, &error);
    if (!text) {
        Opened out;
        out.error = error;
        return out;
    }
    return parseManifest(*text, QFileInfo(archive).fileName());
}

bool Part::restorable() const
{
    return problem.isEmpty()
           && std::any_of(targets.cbegin(), targets.cend(), [](const Target &t) { return t.problem.isEmpty(); });
}

qint64 Plan::bytes() const
{
    // An emulator's bytes are for all its parts; each counts once.
    qint64 total = 0;
    QSet<QString> counted;
    for (const Part &part : parts) {
        if (!part.checked || part.bytes < 0 || counted.contains(part.id))
            continue;
        counted.insert(part.id);
        total += part.bytes;
    }
    return total;
}

Plan plan(const QString &archive, const Manifest &manifest, const BackupPlan::Sources &now)
{
    Plan p;
    p.archive = archive;
    p.manifest = manifest;
    p.sameLibrary = !manifest.libraryId.isEmpty() && manifest.libraryId == now.library.id;
    p.dataDir = QDir::cleanPath(QFileInfo(now.dataDir).absoluteFilePath());
    p.games = int(now.games.size());
    p.compatdata = now.compatdata;
    const auto tilde = [&](const QString &path) { return BackupPlan::tilde(path, now.home); };
    const auto findGame = [&](const QString &id) -> const Game * {
        const auto it = std::find_if(now.games.cbegin(), now.games.cend(), [&](const Game &g) { return g.id == id; });
        return it == now.games.cend() ? nullptr : &*it;
    };
    const QString notInLibrary = tr("Not in your library; restore the library first");

    if (manifest.kind == Backup::kLibrary) {
        Part part;
        part.kind = Part::Kind::Library;
        part.name = tr("Library");
        part.bytes = manifest.bytes;
        part.targets.append({QStringLiteral("data"), tilde(p.dataDir), {}, p.dataDir, present(p.dataDir), {}});
        p.parts.append(part);

        const BackupPlan::LibraryPlan current = BackupPlan::planLibrary(now, {});
        for (const BackupPlan::Place &rom : current.roms) {
            if (rom.exists)
                p.carriedOver.append(libraryRelative(p.dataDir, rom.path));
        }
        if (!manifest.credentials && current.credentialsFile.exists)
            p.carriedOver.append(libraryRelative(p.dataDir, current.credentialsFile.path));
    }

    for (const Manifest::Emulator &e : manifest.emulators) {
        const EmulatorDef *def = Emulators::find(now.emulators, e.id);
        const EmulatorInstall install = now.installs.value(e.id);
        const QString name = e.name.isEmpty() ? e.id : e.name;

        const auto places = [&](Part::Kind kind, const QStringList &was) {
            const bool saves = kind == Part::Kind::Saves;
            Part part;
            part.kind = kind;
            part.id = e.id;
            part.name = name;
            part.backedUp = e.install;
            part.bytes = e.bytes;
            QStringList current;
            if (!def) {
                part.problem = tr("%1 isn't set up in the launcher now").arg(name);
            } else if (!install.found()) {
                part.problem = saves ? tr("Install %1 first; its saves stay in the backup").arg(name)
                                     : tr("Install %1 first; its keys stay in the backup").arg(name);
            } else {
                part.install = install.installType();
                current = Emulators::installPaths(saves ? def->saves : def->keys, install, now.home);
                if (current.isEmpty())
                    part.problem = saves ? tr("%1 doesn't say where it keeps its saves").arg(name)
                                         : tr("%1 doesn't say where it keeps its keys").arg(name);
            }
            for (qsizetype i = 0; i < was.size(); ++i) {
                Target t;
                t.source = Backup::placeName(e.id, saves ? QStringLiteral("saves") : QStringLiteral("keys"), int(i + 1));
                t.label = was.at(i);
                if (part.problem.isEmpty() && i < current.size()) {
                    t.label = tilde(current.at(i));
                    t.path = followed(current.at(i));
                    t.exists = present(t.path);
                } else if (part.problem.isEmpty()) {
                    t.problem = saves ? tr("%1 lists fewer save folders now").arg(name)
                                      : tr("%1 lists fewer key folders now").arg(name);
                }
                part.targets.append(t);
            }
            p.parts.append(part);
        };
        if (!e.saves.isEmpty())
            places(Part::Kind::Saves, e.saves);
        if (manifest.keys && !e.keys.isEmpty())
            places(Part::Kind::Keys, e.keys);

        if (!e.besideRoms.isEmpty()) {
            Part part;
            part.kind = Part::Kind::BesideRoms;
            part.id = e.id;
            part.name = name;
            part.bytes = e.bytes;
            for (const QString &gameId : e.besideRoms) {
                Target t;
                t.source = QStringLiteral("emulators/%1/beside-roms/%2").arg(e.id, gameId);
                t.gameId = gameId;
                t.label = gameId;
                if (const Game *game = findGame(gameId)) {
                    t.label = game->title;
                    if (game->rom && !game->romPath.isEmpty() && QFileInfo(game->romPath).isFile())
                        t.path = game->romPath;
                    else
                        t.problem = tr("Its ROM isn't found");
                } else {
                    t.problem = notInLibrary;
                }
                part.targets.append(t);
            }
            p.parts.append(part);
        }
    }

    for (const Manifest::ProtonGame &g : manifest.protonGames) {
        Part part;
        part.kind = Part::Kind::Proton;
        part.id = g.id;
        part.name = g.title.isEmpty() ? g.id : g.title;
        part.bytes = g.bytes;
        Target t;
        t.source = QStringLiteral("proton/") + g.id;
        t.gameId = g.id;
        t.label = part.name;
        const Game *game = findGame(g.id);
        if (!game) {
            part.problem = notInLibrary;
        } else if (game->platform != kProtonPlatform) {
            part.problem = tr("Not a Proton game in your library now");
        } else {
            part.name = t.label = game->title;
            const ProtonSaves::GameSaves located = ProtonSaves::locate(*game, now.compatdata);
            if (located.skipped == ProtonSaves::Skipped::Unlisted) {
                part.problem = tr("Its Proton doesn't list its own files");
            } else if (located.skipped != ProtonSaves::Skipped::No) {
                part.problem = tr("Launch %1 once first; its saves stay in the backup").arg(game->title);
            } else {
                t.path = located.prefix;
                t.exists = true;
            }
        }
        part.targets.append(t);
        p.parts.append(part);
    }

    for (Part &part : p.parts)
        part.checked = part.restorable();
    return p;
}

namespace {
struct Step
{
    Part::Kind kind;
    Target target;
    QString staging;
    QString aside;
    Archive::Seen seen = Archive::Seen::Nothing;
};

QString importName(const QString &stamp)
{
    return QStringLiteral(".import-") + stamp;
}

QString asideName(const QString &stamp)
{
    return QStringLiteral(".before-import-") + stamp;
}

void place(Step &step, const Plan &plan, const QString &stamp)
{
    const QString &path = step.target.path;
    switch (step.kind) {
    case Part::Kind::BesideRoms: {
        const QFileInfo rom(path);
        step.staging = rom.absolutePath() + u'/' + rom.completeBaseName() + importName(stamp);
        step.aside.clear();
        break;
    }
    case Part::Kind::Proton: {
        const QString shortcut = QFileInfo(path).fileName();
        step.staging = plan.compatdata + importName(stamp) + u'/' + shortcut;
        step.aside = plan.compatdata + asideName(stamp) + u'/' + shortcut;
        break;
    }
    default:
        step.staging = path + importName(stamp);
        step.aside = path + asideName(stamp);
        break;
    }
}

struct Undo
{
    QList<std::pair<QString, QString>> moves;
    QStringList staging;
    QStringList created;

    bool move(const QString &from, const QString &to, QString *error)
    {
        if (!rename(from, to, error))
            return false;
        moves.append({from, to});
        return true;
    }

    void run()
    {
        for (auto it = moves.crbegin(); it != moves.crend(); ++it) {
            QString error;
            if (!rename(it->second, it->first, &error))
                qCWarning(lcBackup).noquote() << "Couldn't undo the import:" << error;
        }
        moves.clear();
        for (const QString &folder : std::as_const(staging)) {
            if (present(folder))
                Archive::removeTree(folder);
        }
        for (auto it = created.crbegin(); it != created.crend(); ++it)
            QDir().rmdir(*it);
    }
};
}

Result apply(const Plan &plan, const QDate &date, const Archive::Control &control)
{
    Result result;
    QList<Step> steps;
    for (const Part &part : plan.parts) {
        if (!part.checked || !part.restorable())
            continue;
        for (const Target &target : part.targets) {
            if (target.problem.isEmpty())
                steps.append({part.kind, target, {}, {}, Archive::Seen::Nothing});
        }
    }
    if (steps.isEmpty())
        return result;

    QString stamp;
    for (int n = 1; stamp.isEmpty(); ++n) {
        const QString candidate =
            n == 1 ? date.toString(Qt::ISODate) : QStringLiteral("%1-%2").arg(date.toString(Qt::ISODate)).arg(n);
        bool free = true;
        for (Step &step : steps) {
            place(step, plan, candidate);
            free = free && !present(step.staging) && (step.aside.isEmpty() || !present(step.aside));
        }
        if (free)
            stamp = candidate;
    }

    Undo undo;
    const auto fail = [&](const QString &error) {
        undo.run();
        result.error = error;
        result.movedAside.clear();
        result.restored = 0;
        qCInfo(lcBackup).noquote() << "Import from" << plan.archive << "failed:" << error;
        return result;
    };

    QList<Archive::Destination> destinations;
    for (const Step &step : std::as_const(steps)) {
        if (!makePath(QFileInfo(step.staging).path(), undo.created))
            return fail(tr("Can't create %1").arg(QFileInfo(step.staging).path()));
        // The library's links are the user's, wherever they lead; saves' stay inside their part.
        destinations.append({step.target.source, step.staging, step.kind == Part::Kind::Library});
    }
    const QStringList allowed = plan.manifest.kind == Backup::kLibrary
                                    ? QStringList{kManifest, QStringLiteral("data")}
                                    : QStringList{kManifest, QStringLiteral("emulators"), QStringLiteral("proton")};
    QString error;
    QHash<QString, Archive::Seen> seen;
    if (!Archive::extract(plan.archive, destinations, allowed, &error, control, &seen))
        return fail(error);
    for (const Step &step : std::as_const(steps))
        undo.staging.append(step.staging);

    for (Step &step : steps) {
        step.seen = seen.value(step.target.source);
        if (step.seen == Archive::Seen::Nothing)
            continue;
        if (step.kind == Part::Kind::Proton) {
            const std::optional<ProtonSaves::Prefix> prefix = ProtonSaves::Prefix::read(step.target.path);
            if (!prefix)
                return fail(tr("%1 doesn't list Proton's files now").arg(step.target.path));
            for (const QString &relative : ProtonSaves::carryOver(*prefix)) {
                const QString to = step.staging + u'/' + relative;
                if (!QDir().mkpath(QFileInfo(to).path()))
                    return fail(tr("Can't create %1").arg(QFileInfo(to).path()));
                if (!copyMissing(prefix->path + u'/' + relative, to, control.cancel, &error))
                    return fail(error);
            }
        } else if (step.kind == Part::Kind::Library) {
            const QString credentials = libraryRelative(plan.dataDir, CredentialsFile::path(plan.dataDir));
            if (plan.carriedOver.contains(credentials)
                && !copyMissing(plan.dataDir + u'/' + credentials, step.staging + u'/' + credentials, nullptr, &error))
                return fail(error);
        } else if ((step.kind == Part::Kind::Saves || step.kind == Part::Kind::Keys)
                   && step.seen == Archive::Seen::Entries
                   && QDir(step.staging).entryList(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System)
                              .size() != 1) {
            return fail(tr("%1 doesn't hold a single file for %2").arg(plan.archive, step.target.label));
        }
    }
    if (control.cancel && control.cancel->load())
        return fail(tr("Cancelled"));

    QStringList movedAside;
    QStringList leftovers;
    for (const Step &step : std::as_const(steps)) {
        const QString &path = step.target.path;
        switch (step.kind) {
        case Part::Kind::Library:
            if (step.seen == Archive::Seen::Nothing)
                break;
            // The ROMs kept in the data dir go into the restored one, unless it has something
            // there (credentials.toml is there already, copied).
            for (const QString &relative : plan.carriedOver) {
                const QString from = path + u'/' + relative;
                const QString to = step.staging + u'/' + relative;
                if (!present(from) || present(to))
                    continue;
                // Never through a link the restored library has on the way (it may lead out of
                // it): those stay in the moved-aside one.
                if (const QString link = linkOnTheWay(step.staging, relative); !link.isEmpty()) {
                    qCInfo(lcBackup).noquote() << "Left" << from << "in the old library:" << link << "is a link";
                    continue;
                }
                if (!QDir().mkpath(QFileInfo(to).path()))
                    return fail(tr("Can't create %1").arg(QFileInfo(to).path()));
                if (!undo.move(from, to, &error))
                    return fail(error);
            }
            [[fallthrough]];
        case Part::Kind::Saves:
        case Part::Kind::Keys: {
            if (step.seen == Archive::Seen::Nothing) {
                leftovers.append(step.staging);
                break;
            }
            if (present(path)) {
                if (!undo.move(path, step.aside, &error))
                    return fail(error);
                movedAside.append(step.aside);
            }
            if (step.seen == Archive::Seen::Entries && step.kind != Part::Kind::Library) {
                const QString name = QDir(step.staging)
                                         .entryList(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System)
                                         .constFirst();
                if (!undo.move(step.staging + u'/' + name, path, &error))
                    return fail(error);
                leftovers.append(step.staging);
            } else if (!undo.move(step.staging, path, &error)) {
                return fail(error);
            }
            ++result.restored;
            break;
        }
        case Part::Kind::BesideRoms: {
            const QStringList names =
                QDir(step.staging).entryList(QDir::Files | QDir::Hidden | QDir::System, QDir::Name);
            for (const QString &name : names) {
                const QString to = BesideRom::restorePath(path, name);
                if (to.isEmpty())
                    continue;
                if (present(to)) {
                    const QString aside = to + asideName(stamp);
                    if (!undo.move(to, aside, &error))
                        return fail(error);
                    movedAside.append(aside);
                }
                if (!undo.move(step.staging + u'/' + name, to, &error))
                    return fail(error);
            }
            leftovers.append(step.staging);
            if (!names.isEmpty())
                ++result.restored;
            break;
        }
        case Part::Kind::Proton:
            if (step.seen == Archive::Seen::Nothing) {
                leftovers.append(step.staging);
                break;
            }
            if (!makePath(QFileInfo(step.aside).path(), undo.created))
                return fail(tr("Can't create %1").arg(QFileInfo(step.aside).path()));
            if (!undo.move(path, step.aside, &error) || !undo.move(step.staging, path, &error))
                return fail(error);
            movedAside.append(step.aside);
            ++result.restored;
            break;
        }
    }

    for (const QString &folder : std::as_const(leftovers)) {
        if (present(folder))
            Archive::removeTree(folder);
    }
    for (auto it = undo.created.crbegin(); it != undo.created.crend(); ++it)
        QDir().rmdir(*it);
    result.movedAside = movedAside;
    qCInfo(lcBackup).noquote() << "Imported" << result.restored << "parts from" << plan.archive;
    return result;
}
}

RestoreJob::RestoreJob(std::function<QStringList()> runningGames, QObject *parent)
    : QObject(parent)
    , m_runningGames(std::move(runningGames))
{
    connect(&m_watcher, &QFutureWatcher<Restore::Result>::finished, this, &RestoreJob::onFinished);
}

RestoreJob::~RestoreJob()
{
    m_cancel = true;
    m_watcher.waitForFinished();
}

QString RestoreJob::start(const Restore::Plan &plan)
{
    if (running())
        return tr("An import is running already.");
    if (const QString why = Backup::gamesRunning(m_runningGames ? m_runningGames() : QStringList()); !why.isEmpty())
        return why;
    m_cancel = false;
    m_bytes = 0;
    m_total = plan.bytes();
    m_result = {};

    auto timer = std::make_shared<QElapsedTimer>();
    Archive::Control control;
    control.cancel = &m_cancel;
    control.progress = [this, timer](qint64 bytes) {
        if (timer->isValid() && timer->elapsed() < 100)
            return;
        timer->start();
        QMetaObject::invokeMethod(this, [this, bytes] {
            m_bytes = bytes;
            emit progressChanged();
        }, Qt::QueuedConnection);
    };
    const QDate date = QDate::currentDate();
    m_watcher.setFuture(QtConcurrent::run([plan, date, control] { return Restore::apply(plan, date, control); }));
    emit runningChanged();
    emit progressChanged();
    return {};
}

void RestoreJob::cancel()
{
    m_cancel = true;
}

void RestoreJob::onFinished()
{
    m_result = m_watcher.result();
    if (m_result.error.isEmpty())
        m_bytes = m_total;
    emit runningChanged();
    emit progressChanged();
    emit finished(m_result.error);
}
