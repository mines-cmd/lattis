#include "backupplan.h"
#include "tomlsection.h"
#include "credentials.h"
#include "gamefile.h"
#include "suggestion.h"
#include "tomlpatch.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSet>

#include <toml++/toml.hpp>

#include <algorithm>

namespace BackupPlan {
namespace {
const QString kSection = QStringLiteral("backup");

const QStringList kRegenerated = {QStringLiteral("library/emulators.reference.toml"),
                                  QStringLiteral("themes/colors/reference.toml"),
                                  QStringLiteral("themes/styles/reference.toml")};

Place place(const QString &path)
{
    const QFileInfo info(path);
    return {path, info.exists() || info.isSymLink(), -1};
}

bool inside(const QString &path, const QString &folder)
{
    return path.startsWith(folder + u'/');
}

qint64 sizeOf(const QString &path, const Archive::Skip &skip = {}, const QString &relative = {})
{
    const QFileInfo info(path);
    if (info.isSymLink() || !info.exists())
        return 0;
    if (!info.isDir())
        return info.size();
    qint64 total = 0;
    const QFileInfoList entries =
        QDir(path).entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System);
    for (const QFileInfo &entry : entries) {
        const QString name = relative.isEmpty() ? entry.fileName() : relative + u'/' + entry.fileName();
        if (skip && skip(name))
            continue;
        total += sizeOf(entry.absoluteFilePath(), skip, name);
    }
    return total;
}

void measure(QList<Place> &places)
{
    for (Place &p : places)
        p.bytes = p.exists ? sizeOf(p.path) : 0;
}

qint64 sum(const QList<Place> &places)
{
    qint64 total = 0;
    for (const Place &p : places)
        total += qMax<qint64>(p.bytes, 0);
    return total;
}

QString list(const QStringList &items)
{
    QStringList quoted;
    for (const QString &item : items)
        quoted.append(tomlString(item));
    return u'[' + quoted.join(QStringLiteral(", ")) + u']';
}

QString timestamp(const QDateTime &created)
{
    return created.toOffsetFromUtc(created.offsetFromUtc()).toString(Qt::ISODate);
}

QString firstLine(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return {};
    return QString::fromUtf8(file.readLine()).trimmed();
}
}

Parsed<Settings> parseSettings(const QByteArray &bytes, const QString &displayPath)
{
    Parsed<Settings> out;
    toml::table root;
    try {
        root = toml::parse(std::string_view(bytes.constData(), size_t(bytes.size())));
    } catch (const toml::parse_error &e) {
        out.warnings.append(
            QStringLiteral("%1:%2: %3").arg(displayPath).arg(e.source().begin.line).arg(toQString(e.description())));
        return out;
    }
    out.ok = true;

    const toml::node *section = root.get("backup");
    if (!section)
        return out;
    const auto where = [&](const toml::node &node) {
        return QStringLiteral("%1:%2").arg(displayPath).arg(node.source().begin.line);
    };
    const toml::table *table = section->as_table();
    if (!table) {
        out.warnings.append(QStringLiteral("%1: backup must be a [backup] section").arg(where(*section)));
        return out;
    }

    const QStringList known = {QStringLiteral("credentials"), QStringLiteral("keys"), QStringLiteral("proton")};
    bool *const values[] = {&out.value.credentials, &out.value.keys, &out.value.proton};
    for (auto &&[k, node] : *table) {
        const QString key = toQString(k.str());
        const qsizetype index = known.indexOf(key);
        if (index < 0) {
            out.warnings.append(QStringLiteral("%1: unknown key %2%3").arg(where(node), key, suggestion(key, known)));
        } else if (const auto *b = node.as_boolean()) {
            *values[index] = b->get();
        } else {
            out.warnings.append(QStringLiteral("%1: %2 must be true or false").arg(where(node), key));
        }
    }
    return out;
}

bool saveSetting(const QString &settingsFile, const QString &key, bool value, QString *error)
{
    if (!QFile::exists(settingsFile)) {
        if (value)
            return true;
        if (QFile file(settingsFile); !file.open(QIODevice::WriteOnly)) {
            if (error)
                *error = file.errorString();
            return false;
        }
    }
    return value ? TomlPatch::removeValueInFile(settingsFile, kSection, key, error)
                 : TomlPatch::setValueInFile(settingsFile, kSection, key, QStringLiteral("false"), error);
}

QString tilde(const QString &path, const QString &home)
{
    if (home.isEmpty())
        return path;
    if (path == home)
        return QStringLiteral("~");
    return inside(path, home) ? u'~' + path.mid(home.size()) : path;
}

QString fileName(const QString &kind, const QDate &date)
{
    return QStringLiteral("lattis-%1-%2.tar.zst").arg(kind, date.toString(Qt::ISODate));
}

QStringList LibraryPlan::excluded() const
{
    QStringList out = kRegenerated;
    const QDir dir(dataDir);
    for (const Place &rom : roms)
        out.append(dir.relativeFilePath(rom.path));
    if (!credentials)
        out.append(dir.relativeFilePath(credentialsFile.path));
    out.sort();
    out.removeDuplicates();
    return out;
}

Archive::Skip LibraryPlan::skip() const
{
    const QStringList list = excluded();
    const QSet<QString> set(list.cbegin(), list.cend());
    return [set](const QString &relative) { return set.contains(relative); };
}

qint64 LibraryPlan::bytes() const
{
    if (dataBytes < 0)
        return -1;
    return dataBytes + (credentials ? qMax<qint64>(credentialsFile.bytes, 0) : 0);
}

LibraryPlan planLibrary(const Sources &sources, const Settings &settings)
{
    LibraryPlan plan;
    plan.dataDir = QDir::cleanPath(QFileInfo(sources.dataDir).absoluteFilePath());
    plan.games = int(sources.games.size());
    plan.library = sources.library;
    plan.credentials = settings.credentials;
    plan.credentialsFile = place(CredentialsFile::path(plan.dataDir));

    QStringList folders;
    for (const Platform &platform : sources.platforms) {
        for (const QString &folder : platform.romFolders) {
            const QString path = resolveRomFolder(folder, plan.dataDir, sources.home);
            if (inside(path, plan.dataDir) && !folders.contains(path))
                folders.append(path);
        }
    }
    folders.sort();
    const auto underFolder = [&](const QString &path) {
        return std::any_of(folders.cbegin(), folders.cend(), [&](const QString &f) { return inside(path, f); });
    };
    for (const QString &folder : std::as_const(folders)) {
        if (!underFolder(folder))
            plan.roms.append(place(folder));
    }
    QStringList files;
    for (const Game &game : sources.games) {
        const QString rom = QDir::cleanPath(game.romPath);
        if (game.rom && !rom.isEmpty() && inside(rom, plan.dataDir) && !underFolder(rom) && !files.contains(rom))
            files.append(rom);
    }
    files.sort();
    for (const QString &file : std::as_const(files))
        plan.roms.append(place(file));
    return plan;
}

void measure(LibraryPlan &plan)
{
    LibraryPlan without = plan;
    without.credentials = false;
    plan.dataBytes = sizeOf(plan.dataDir, without.skip());
    plan.credentialsFile.bytes = plan.credentialsFile.exists ? sizeOf(plan.credentialsFile.path) : 0;
    measure(plan.roms);
}

qint64 EmulatorPart::bytes(bool withKeys) const
{
    if (!measured)
        return -1;
    qint64 total = sum(saves) + (withKeys ? sum(keys) : 0);
    for (const BesideRom::GameSaves &game : besideRoms) {
        for (const BesideRom::SaveFile &file : game.files)
            total += file.size;
    }
    return total;
}

bool EmulatorPart::takesAnything(bool withKeys) const
{
    const auto found = [](const QList<Place> &places) {
        return std::any_of(places.cbegin(), places.cend(), [](const Place &p) { return p.exists; });
    };
    return found(saves) || (withKeys && found(keys)) || !besideRoms.isEmpty();
}

qint64 SavesPlan::bytes() const
{
    if (!measured)
        return -1;
    qint64 total = 0;
    for (const EmulatorPart &part : emulators)
        total += part.bytes(keys);
    if (proton) {
        for (const ProtonPart &part : protonGames)
            total += part.saves.bytes;
    }
    return total;
}

SavesPlan planSaves(const Sources &sources, const Settings &settings)
{
    SavesPlan plan;
    plan.home = sources.home;
    plan.library = sources.library;
    plan.keys = settings.keys;
    plan.proton = settings.proton;
    plan.platforms = sources.platforms;
    plan.games = sources.games;
    plan.definitions = sources.emulators;

    QSet<QString> configured;
    for (const Platform &platform : sources.platforms) {
        for (const QString &id : platform.emulators)
            configured.insert(id);
    }
    for (const Game &game : sources.games) {
        if (!game.emulator.isEmpty())
            configured.insert(game.emulator);
    }

    for (const EmulatorDef &def : sources.emulators) {
        if (!configured.contains(def.id))
            continue;
        EmulatorPart part;
        part.id = def.id;
        part.name = def.name.isEmpty() ? def.id : def.name;
        part.besideRomSaves = !def.savesBesideRom.isEmpty();
        const EmulatorInstall install = sources.installs.value(def.id);
        part.describe = install.describe(sources.home);
        if (!install.found()) {
            part.skipped = EmulatorPart::Skipped::NotInstalled;
        } else {
            part.install = install.installType();
            for (const QString &path : Emulators::installPaths(def.saves, install, sources.home))
                part.saves.append(place(path));
            for (const QString &path : Emulators::installPaths(def.keys, install, sources.home))
                part.keys.append(place(path));
            if (part.saves.isEmpty() && !part.besideRomSaves)
                part.skipped = EmulatorPart::Skipped::NoSaves;
        }
        plan.emulators.append(part);
    }

    for (const Game &game : sources.games) {
        if (game.platform != kProtonPlatform)
            continue;
        ProtonPart part;
        part.gameId = game.id;
        part.title = game.title;
        part.saves = ProtonSaves::locate(game, sources.compatdata);
        if (!part.saves.prefix.isEmpty())
            part.proton = firstLine(part.saves.prefix + QStringLiteral("/version"));
        plan.protonGames.append(part);
    }
    return plan;
}

void measure(SavesPlan &plan)
{
    QSet<QString> taken;
    for (EmulatorPart &part : plan.emulators) {
        measure(part.saves);
        measure(part.keys);
        part.besideRoms.clear();
        if (const EmulatorDef *def = Emulators::find(plan.definitions, part.id)) {
            for (const BesideRom::GameSaves &game : BesideRom::find(*def, plan.platforms, plan.games)) {
                if (!taken.contains(game.gameId)) {
                    taken.insert(game.gameId);
                    part.besideRoms.append(game);
                }
            }
        }
        part.measured = true;
    }
    for (ProtonPart &part : plan.protonGames) {
        ProtonSaves::addFiles(part.saves);
        part.measured = true;
    }
    plan.measured = true;
}

QByteArray manifest(const LibraryPlan &plan, const QDateTime &created, const QString &appVersion)
{
    QString out = QStringLiteral(
        "# A Lattis library backup: games, art, themes and settings. Restore it with\n"
        "# Import in the app, or by hand: tar --zstd -xf <this file>, then copy data/ to\n"
        "# ~/.local/share/lattis/.\n");
    out += QStringLiteral("kind = \"library\"\n");
    out += QStringLiteral("format = %1\n").arg(kFormat);
    out += QStringLiteral("app_version = %1\n").arg(tomlString(appVersion));
    out += QStringLiteral("library_version = %1\n").arg(plan.library.version);
    if (!plan.library.id.isEmpty())
        out += QStringLiteral("library_id = %1\n").arg(tomlString(plan.library.id));
    out += QStringLiteral("created = %1\n").arg(timestamp(created));
    out += QStringLiteral("games = %1\n").arg(plan.games);
    if (plan.bytes() >= 0)
        out += QStringLiteral("bytes = %1\n").arg(plan.bytes());
    out += QStringLiteral("credentials = %1\n")
               .arg(plan.credentials && plan.credentialsFile.exists ? QStringLiteral("true") : QStringLiteral("false"));
    return out.toUtf8();
}

QByteArray manifest(const SavesPlan &plan, const QDateTime &created, const QString &appVersion)
{
    const auto boolean = [](bool b) { return b ? QStringLiteral("true") : QStringLiteral("false"); };
    const auto paths = [&](const QList<Place> &places) {
        QStringList out;
        for (const Place &p : places)
            out.append(tilde(p.path, plan.home));
        return out;
    };

    QString out = QStringLiteral(
        "# A Lattis saves backup. Restore it with Import in the app, or by hand:\n"
        "# tar --zstd -xf <this file>, then copy each emulators/<id>/saves/<n>/ to the n-th\n"
        "# folder listed below, and each proton/<game>/ over compatdata/<shortcut>/ in Steam.\n");
    out += QStringLiteral("kind = \"saves\"\n");
    out += QStringLiteral("format = %1\n").arg(kFormat);
    out += QStringLiteral("app_version = %1\n").arg(tomlString(appVersion));
    if (!plan.library.id.isEmpty())
        out += QStringLiteral("library_id = %1   # the library it was made with\n").arg(tomlString(plan.library.id));
    out += QStringLiteral("created = %1\n").arg(timestamp(created));
    out += QStringLiteral("keys = %1\n").arg(boolean(plan.keys));
    out += QStringLiteral("proton = %1\n").arg(boolean(plan.proton));

    for (const EmulatorPart &part : plan.emulators) {
        if (!part.takesAnything(plan.keys))
            continue;
        out += QStringLiteral("\n[[emulator]]\nid = %1\nname = %2\n").arg(tomlString(part.id), tomlString(part.name));
        if (!part.install.isEmpty())
            out += QStringLiteral("install = %1\n").arg(tomlString(part.install));
        if (!part.saves.isEmpty())
            out += QStringLiteral("saves = %1   # where they came from\n").arg(list(paths(part.saves)));
        if (plan.keys && !part.keys.isEmpty())
            out += QStringLiteral("keys = %1\n").arg(list(paths(part.keys)));
        if (!part.besideRoms.isEmpty()) {
            QStringList ids;
            for (const BesideRom::GameSaves &game : part.besideRoms)
                ids.append(game.gameId);
            out += QStringLiteral("beside_roms = %1   # games whose ROMs had saves next to them\n").arg(list(ids));
        }
        if (part.measured)
            out += QStringLiteral("bytes = %1\n").arg(part.bytes(plan.keys));
    }

    if (plan.proton) {
        for (const ProtonPart &part : plan.protonGames) {
            if (part.saves.skipped != ProtonSaves::Skipped::No)
                continue;
            out += QStringLiteral("\n[[game]]\nid = %1\ntitle = %2\n").arg(tomlString(part.gameId), tomlString(part.title));
            out += QStringLiteral("shortcut = %1   # its prefix's folder when it was backed up\n").arg(part.saves.shortcut);
            if (!part.proton.isEmpty())
                out += QStringLiteral("proton = %1   # the prefix's version file\n").arg(tomlString(part.proton));
            if (part.measured)
                out += QStringLiteral("bytes = %1\n").arg(part.saves.bytes);
        }
    }
    return out.toUtf8();
}
}
