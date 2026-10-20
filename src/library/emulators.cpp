#include "emulators.h"
#include "tomlsection.h"
#include "gamefile.h"
#include "gameid.h"
#include "suggestion.h"

#include <QCollator>
#include <QDir>
#include <QStandardPaths>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSaveFile>

#include <toml++/toml.hpp>

#include <algorithm>

namespace EmulatorFields {
namespace {
using enum Type;

constexpr Field kFields[] = {
    {"name", String, "Shown in the UI"},
    {"platforms", StringList, "Platform ids it can run; offered first when adding an emulator to a platform"},
    {"flatpak", String, "Flatpak app id"},
    {"executables", StringList, "Program names to look for on PATH"},
    {"appimage", String, "File name pattern for an AppImage in ~/Applications or ~/AppImages, e.g. "
                         "\"ryujinx*.AppImage\" (case doesn't matter); several separated by ;"},
    {"path", String, "Your install: an absolute path or ~/…; skips detection"},
    {"command", StringList, "The program's arguments, not a shell line: {exe} is the program (or flatpak run "
                            "<app id>), {args} the args list, {rom} the absolute ROM path. \"Open emulator\" leaves out {rom} and the "
                            "arguments between {args} and it"},
    {"args", StringList, "Extra arguments for every game, spliced in at {args}"},
    {"extensions", StringList, "ROM file extensions, lowercase, without the dot"},
    {"files", StringList, "File names that make the folder they're in a game, like \"eboot.bin\" for PS4 games "
                          "(case doesn't matter); the title comes from the game's folder"},
    {"portable", String, "A file or folder next to the program (or the AppImage) that makes the emulator keep its "
                         "data beside it, like \"portable.txt\"; the install is then portable"},
    {"saves", PathTable, "Where saves are, by install type (native, flatpak, portable): a folder or file, or a list; "
                         "native and flatpak ones absolute or ~/…, portable ones relative to the program's folder. "
                         "Export backs them up"},
    {"keys", PathTable, "Key and firmware folders and files by install type, like saves"},
    {"saves_beside_rom", StringList, "Extensions of the saves it writes next to the ROM under the ROM's name, "
                                     "lowercase, without the dot, like [\"sav\", \"ss1\"]. Export backs them up"},
};
}

std::span<const Field> all()
{
    return kFields;
}

const Field *find(const QString &key)
{
    for (const Field &f : kFields) {
        if (key == QLatin1String(f.key))
            return &f;
    }
    return nullptr;
}

QStringList keys()
{
    QStringList out;
    for (const Field &f : kFields)
        out.append(QLatin1String(f.key));
    return out;
}

QString typeName(Type type)
{
    switch (type) {
    case String: return QStringLiteral("string");
    case StringList: return QStringLiteral("list of strings");
    case PathTable: return QStringLiteral("table");
    }
    return {};
}

QStringList installTypes()
{
    return {QStringLiteral("native"), QStringLiteral("flatpak"), QStringLiteral("portable")};
}
}

namespace {
using EmulatorFields::Field;
using EmulatorFields::Type;

const QString kId = QStringLiteral("id");
const QStringList kPlaceholders = {QStringLiteral("{exe}"), QStringLiteral("{args}"), QStringLiteral("{rom}")};

QString where(const QString &displayPath, const toml::node &node)
{
    return QStringLiteral("%1:%2").arg(displayPath).arg(node.source().begin.line);
}

bool parseToml(const QByteArray &bytes, const QString &displayPath, toml::table *table, QStringList *warnings)
{
    try {
        *table = toml::parse(std::string_view(bytes.constData(), size_t(bytes.size())));
        return true;
    } catch (const toml::parse_error &e) {
        warnings->append(QStringLiteral("%1:%2: %3")
                             .arg(displayPath)
                             .arg(e.source().begin.line)
                             .arg(toQString(e.description())));
        return false;
    }
}

// A relative path that stays below its folder: no /, ~ or .. at the start.
bool relativeInside(const QString &path)
{
    if (path.isEmpty() || path.startsWith(u'/') || path.startsWith(u'~'))
        return false;
    return !path.split(u'/').contains(QStringLiteral(".."));
}

QStringList stringList(const toml::node &node, bool *ok)
{
    QStringList out;
    const toml::array *array = node.as_array();
    *ok = array != nullptr;
    if (!array)
        return out;
    for (const toml::node &item : *array) {
        const auto *s = item.as_string();
        if (!s) {
            *ok = false;
            return {};
        }
        out.append(toQString(s->get()));
    }
    return out;
}

QString commandError(const QStringList &command)
{
    if (command.isEmpty() || !command.contains(QStringLiteral("{exe}")))
        return QStringLiteral("command must include \"{exe}\", the program");
    static const QRegularExpression placeholder(QStringLiteral("\\{[^{}]*\\}"));
    for (const QString &arg : command) {
        for (const QRegularExpressionMatch &m : placeholder.globalMatch(arg)) {
            const QString name = m.captured();
            if (!kPlaceholders.contains(name))
                return QStringLiteral("unknown placeholder %1 in command; use {exe}, {args} or {rom}%2")
                    .arg(name, suggestion(name, kPlaceholders));
            // {exe} and {args} stand for several arguments, so they can't be part of one.
            if (name != QLatin1String("{rom}") && arg != name)
                return QStringLiteral("%1 must be an argument of its own in command, like \"%1\"").arg(name);
        }
    }
    return {};
}

QVariant readValue(const toml::node &node, const Field &field, const QString &displayPath, QStringList *warnings)
{
    const QString key = QLatin1String(field.key);
    const QString at = where(displayPath, node);
    const auto fail = [&](const QString &message) {
        warnings->append(QStringLiteral("%1: %2").arg(at, message));
        return QVariant();
    };

    switch (field.type) {
    case Type::String: {
        const auto *s = node.as_string();
        if (!s)
            return fail(QStringLiteral("%1 must be text in quotes").arg(key));
        const QString value = toQString(s->get());
        if (key == QLatin1String("path") && !value.isEmpty() && !value.startsWith(u'/') && value != u'~'
            && !value.startsWith(QLatin1String("~/")))
            return fail(QStringLiteral("path must be absolute, or start with ~/"));
        if (key == QLatin1String("appimage") && value.contains(u'/'))
            return fail(QStringLiteral("appimage is a file name pattern for ~/Applications and ~/AppImages, without folders; "
                                       "use path for an AppImage elsewhere"));
        if (key == QLatin1String("portable") && !relativeInside(value))
            return fail(QStringLiteral("portable is a file or folder next to the program, like \"portable.txt\""));
        return value;
    }
    case Type::StringList: {
        bool ok = false;
        QStringList list = stringList(node, &ok);
        if (!ok)
            return fail(QStringLiteral("%1 must be a list of text in quotes, like [\"a\", \"b\"]").arg(key));
        if (key == QLatin1String("command")) {
            if (const QString error = commandError(list); !error.isEmpty())
                return fail(error);
        } else if (key == QLatin1String("extensions")) {
            bool dotted = false;
            for (QString &ext : list) {
                dotted = dotted || ext.startsWith(u'.');
                while (ext.startsWith(u'.'))
                    ext.remove(0, 1);
                ext = ext.toLower();
            }
            if (dotted)
                warnings->append(QStringLiteral("%1: write extensions without the dot, like \"nsp\"").arg(at));
            list.removeAll(QString());
        } else if (key == QLatin1String("saves_beside_rom")) {
            for (QString &ext : list) {
                while (ext.startsWith(u'.'))
                    ext.remove(0, 1);
                // A restore puts a save back by the part after its last dot.
                if (ext.contains(u'.') || ext.contains(u'/'))
                    return fail(QStringLiteral("saves_beside_rom are extensions without dots, like \"sav\""));
                ext = ext.toLower();
            }
            list.removeAll(QString());
        } else if (key == QLatin1String("files")) {
            for (QString &name : list) {
                if (name.contains(u'/'))
                    return fail(QStringLiteral("files are file names, without folders, like \"eboot.bin\""));
                name = name.toLower();
            }
            list.removeAll(QString());
        }
        return list;
    }
    case Type::PathTable: {
        const toml::table *table = node.as_table();
        if (!table)
            return fail(QStringLiteral("%1 must be a table of folders by install type, like "
                                       "{ native = \"~/.config/app\", flatpak = \"…\" }")
                             .arg(key));
        const QStringList types = EmulatorFields::installTypes();
        QVariantMap out;
        for (auto &&[k, value] : *table) {
            const QString type = toQString(k.str());
            const QString valueAt = where(displayPath, value);
            if (!types.contains(type)) {
                warnings->append(QStringLiteral("%1: unknown install type %2 in %3; use native, flatpak or "
                                                "portable%4")
                                     .arg(valueAt, type, key, suggestion(type, types)));
                continue;
            }
            QStringList paths;
            bool ok = false;
            if (const auto *s = value.as_string())
                paths = {toQString(s->get())};
            else
                paths = stringList(value, &ok);
            if (paths.isEmpty() && !ok) {
                warnings->append(QStringLiteral("%1: %2.%3 must be a path in quotes, or a list of them")
                                     .arg(valueAt, key, type));
                continue;
            }
            const bool portable = type == QLatin1String("portable");
            const auto bad = std::find_if(paths.cbegin(), paths.cend(), [&](const QString &p) {
                return portable ? !relativeInside(p) : !p.startsWith(u'/') && !p.startsWith(QLatin1String("~/"));
            });
            if (bad != paths.cend()) {
                warnings->append(portable ? QStringLiteral("%1: %2.portable paths are relative to the program's "
                                                           "folder, like \"user/saves\"")
                                                .arg(valueAt, key)
                                          : QStringLiteral("%1: %2.%3 paths must be absolute, or start with ~/")
                                                .arg(valueAt, key, type));
                continue;
            }
            out.insert(type, paths);
        }
        return out;
    }
    }
    return {};
}

EmulatorEntry readEntry(const toml::table &table, const QString &at, const QString &displayPath,
                        QStringList *warnings)
{
    EmulatorEntry entry;
    entry.at = at;
    QStringList known = EmulatorFields::keys();
    known.prepend(kId);
    for (auto &&[k, node] : table) {
        const QString key = toQString(k.str());
        const QString keyAt = where(displayPath, node);
        if (key == kId) {
            if (const auto *s = node.as_string())
                entry.id = toQString(s->get());
            else
                warnings->append(QStringLiteral("%1: id must be text in quotes").arg(keyAt));
            continue;
        }
        const Field *field = EmulatorFields::find(key);
        if (!field) {
            warnings->append(QStringLiteral("%1: unknown key %2%3").arg(keyAt, key, suggestion(key, known)));
            continue;
        }
        const QVariant value = readValue(node, *field, displayPath, warnings);
        if (value.isValid())
            entry.values.insert(key, value);
    }
    return entry;
}

bool checkId(const EmulatorEntry &entry, QStringList *warnings)
{
    if (entry.id.isEmpty()) {
        warnings->append(QStringLiteral("%1: emulator without an id; skipped").arg(entry.at));
        return false;
    }
    if (!GameId::isValid(entry.id)) {
        warnings->append(QStringLiteral("%1: emulator id \"%2\" may only use lowercase a-z, 0-9 and -; skipped")
                             .arg(entry.at, entry.id));
        return false;
    }
    return true;
}

QMap<QString, QStringList> pathTable(const QVariant &value)
{
    QMap<QString, QStringList> out;
    const QVariantMap map = value.toMap();
    for (auto it = map.begin(); it != map.end(); ++it)
        out.insert(it.key(), it.value().toStringList());
    return out;
}

EmulatorDef build(const QString &id, const QVariantMap &v)
{
    EmulatorDef def;
    def.id = id;
    def.name = v.value(QStringLiteral("name")).toString();
    def.platforms = v.value(QStringLiteral("platforms")).toStringList();
    def.flatpak = v.value(QStringLiteral("flatpak")).toString();
    def.executables = v.value(QStringLiteral("executables")).toStringList();
    def.appimage = v.value(QStringLiteral("appimage")).toString();
    def.path = v.value(QStringLiteral("path")).toString();
    def.command = v.value(QStringLiteral("command")).toStringList();
    def.args = v.value(QStringLiteral("args")).toStringList();
    def.extensions = v.value(QStringLiteral("extensions")).toStringList();
    def.files = v.value(QStringLiteral("files")).toStringList();
    def.portable = v.value(QStringLiteral("portable")).toString();
    def.saves = pathTable(v.value(QStringLiteral("saves")));
    def.keys = pathTable(v.value(QStringLiteral("keys")));
    def.savesBesideRom = v.value(QStringLiteral("saves_beside_rom")).toStringList();
    return def;
}

// A user emulator may list `extensions = []`, as the app writes one it has just added; the key
// still has to be there, so a block that is only a typo of a built-in's id is caught.
QStringList missingKeys(const EmulatorDef &def)
{
    QStringList missing;
    if (def.name.isEmpty())
        missing.append(QStringLiteral("name"));
    if (def.command.isEmpty())
        missing.append(QStringLiteral("command"));
    if (def.extensions.isEmpty() && def.files.isEmpty() && !def.userKeys.contains(QStringLiteral("extensions"))
        && !def.userKeys.contains(QStringLiteral("files")))
        missing.append(QStringLiteral("extensions or files"));
    return missing;
}

QString joinAnd(const QStringList &items)
{
    if (items.size() < 2)
        return items.join(QString());
    return items.mid(0, items.size() - 1).join(QStringLiteral(", ")) + QStringLiteral(" and ") + items.last();
}

QString formatValue(const EmulatorDef &def, const Field &field)
{
    const QString key = QLatin1String(field.key);
    const auto list = [](const QStringList &items) {
        QStringList quoted;
        for (const QString &s : items)
            quoted.append(tomlString(s));
        return u'[' + quoted.join(QStringLiteral(", ")) + u']';
    };
    const auto table = [&](const QMap<QString, QStringList> &map) {
        QStringList parts;
        for (const QString &type : EmulatorFields::installTypes()) {
            if (!map.contains(type))
                continue;
            const QStringList paths = map.value(type);
            parts.append(QStringLiteral("%1 = %2").arg(type, paths.size() == 1 ? tomlString(paths.first()) : list(paths)));
        }
        return QStringLiteral("{ %1 }").arg(parts.join(QStringLiteral(", ")));
    };

    if (key == QLatin1String("name")) return def.name.isEmpty() ? QString() : tomlString(def.name);
    if (key == QLatin1String("platforms")) return def.platforms.isEmpty() ? QString() : list(def.platforms);
    if (key == QLatin1String("flatpak")) return def.flatpak.isEmpty() ? QString() : tomlString(def.flatpak);
    if (key == QLatin1String("executables")) return def.executables.isEmpty() ? QString() : list(def.executables);
    if (key == QLatin1String("appimage")) return def.appimage.isEmpty() ? QString() : tomlString(def.appimage);
    if (key == QLatin1String("path")) return def.path.isEmpty() ? QString() : tomlString(def.path);
    if (key == QLatin1String("command")) return list(def.command);
    if (key == QLatin1String("args")) return list(def.args);
    if (key == QLatin1String("extensions")) return list(def.extensions);
    if (key == QLatin1String("files")) return def.files.isEmpty() ? QString() : list(def.files);
    if (key == QLatin1String("portable")) return def.portable.isEmpty() ? QString() : tomlString(def.portable);
    if (key == QLatin1String("saves")) return def.saves.isEmpty() ? QString() : table(def.saves);
    if (key == QLatin1String("keys")) return def.keys.isEmpty() ? QString() : table(def.keys);
    if (key == QLatin1String("saves_beside_rom")) return def.savesBesideRom.isEmpty() ? QString() : list(def.savesBesideRom);
    return {};
}

const char kReferenceHeader[] = R"(# Every emulator the app knows, with its full definition: the built-in presets with your
# changes from emulators.toml, then the emulators you define there.
#
# Written by the app on every start; don't edit it, your changes would be lost. To change
# a preset, add an [[emulator]] block with its id to emulators.toml, with just the keys to
# change. To add an emulator, give it a new id there and at least name, command,
# extensions and a way to find it (flatpak, executables, appimage or path).
#
# Keys:
#   id           Required; lowercase a-z, 0-9 and -. Platforms and games refer to it, so
#                don't change it once they do
)";
}

namespace Emulators {
Parsed<QList<EmulatorEntry>> parseFile(const QByteArray &bytes, const QString &displayPath)
{
    Parsed<QList<EmulatorEntry>> out;
    toml::table root;
    if (!parseToml(bytes, displayPath, &root, &out.warnings))
        return out;
    out.ok = true;

    const QString arrayName = QStringLiteral("emulator");
    QSet<QString> ids;
    for (auto &&[k, node] : root) {
        const QString key = toQString(k.str());
        const toml::array *array = node.as_array();
        if (key != arrayName) {
            out.warnings.append(QStringLiteral("%1: unknown key %2; emulators go in [[emulator]] blocks%3")
                                    .arg(where(displayPath, node), key, suggestion(key, {arrayName})));
            continue;
        }
        if (!array || !array->is_array_of_tables()) {
            out.warnings.append(QStringLiteral("%1: emulator must be [[emulator]] blocks").arg(where(displayPath, node)));
            continue;
        }
        for (const toml::node &item : *array) {
            const toml::table &table = *item.as_table();
            EmulatorEntry entry = readEntry(table, where(displayPath, table), displayPath, &out.warnings);
            if (!checkId(entry, &out.warnings))
                continue;
            if (ids.contains(entry.id)) {
                out.warnings.append(
                    QStringLiteral("%1: emulator \"%2\" is listed twice; skipped").arg(entry.at, entry.id));
                continue;
            }
            ids.insert(entry.id);
            out.value.append(entry);
        }
    }
    return out;
}

Parsed<EmulatorEntry> parsePreset(const QByteArray &bytes, const QString &displayPath)
{
    Parsed<EmulatorEntry> out;
    toml::table root;
    if (!parseToml(bytes, displayPath, &root, &out.warnings))
        return out;
    out.value = readEntry(root, displayPath, displayPath, &out.warnings);
    out.ok = checkId(out.value, &out.warnings);
    return out;
}

const QList<EmulatorEntry> &presets()
{
    static const QList<EmulatorEntry> list = [] {
        QList<EmulatorEntry> out;
        const QDir dir(QStringLiteral(":/qt/qml/Launcher/resources/emulators"));
        for (const QString &name : dir.entryList({QStringLiteral("*.toml")}, QDir::Files, QDir::Name)) {
            QFile file(dir.filePath(name));
            if (!file.open(QIODevice::ReadOnly))
                continue;
            const Parsed<EmulatorEntry> preset =
                parsePreset(file.readAll(), QStringLiteral("built-in emulators/%1").arg(name));
            for (const QString &warning : preset.warnings)
                qWarning().noquote() << warning;
            if (preset.ok)
                out.append(preset.value);
        }
        return out;
    }();
    return list;
}

QList<EmulatorDef> merge(const QList<EmulatorEntry> &presets, const QList<EmulatorEntry> &user,
                         QStringList *warnings)
{
    QList<EmulatorDef> out;
    QStringList presetIds;
    QList<EmulatorEntry> sortedPresets = presets;
    std::sort(sortedPresets.begin(), sortedPresets.end(),
              [](const EmulatorEntry &a, const EmulatorEntry &b) { return a.id < b.id; });
    for (const EmulatorEntry &p : sortedPresets)
        presetIds.append(p.id);

    for (const EmulatorEntry &preset : sortedPresets) {
        QVariantMap values = preset.values;
        QStringList userKeys;
        for (const EmulatorEntry &u : user) {
            if (u.id != preset.id)
                continue;
            for (auto it = u.values.begin(); it != u.values.end(); ++it) {
                values.insert(it.key(), it.value());
                userKeys.append(it.key());
            }
        }
        EmulatorDef def = build(preset.id, values);
        def.preset = true;
        def.userKeys = userKeys;
        if (const QStringList missing = missingKeys(def); !missing.isEmpty()) {
            warnings->append(QStringLiteral("%1: emulator \"%2\" has no %3; skipped")
                                 .arg(preset.at, preset.id, joinAnd(missing)));
            continue;
        }
        out.append(def);
    }

    for (const EmulatorEntry &entry : user) {
        if (presetIds.contains(entry.id))
            continue;
        EmulatorDef def = build(entry.id, entry.values);
        def.userKeys = entry.values.keys();
        if (const QStringList missing = missingKeys(def); !missing.isEmpty()) {
            warnings->append(QStringLiteral("%1: emulator \"%2\" isn't built in, so it needs %3; skipped%4")
                                 .arg(entry.at, entry.id, joinAnd(missing), suggestion(entry.id, presetIds)));
            continue;
        }
        out.append(def);
    }
    return out;
}

const EmulatorDef *find(const QList<EmulatorDef> &emulators, const QString &id)
{
    for (const EmulatorDef &e : emulators) {
        if (e.id == id)
            return &e;
    }
    return nullptr;
}

QString referenceText(const QList<EmulatorDef> &emulators)
{
    QString out = QString::fromUtf8(kReferenceHeader);
    for (const Field &f : EmulatorFields::all()) {
        const QString key = QString::fromLatin1(f.key);
        QString line = QStringLiteral("#   %1").arg(key.leftJustified(13));
        const qsizetype indent = 4 + 13;
        if (key.size() >= 13) {
            out += line + u'\n';
            line = u'#' + QString(indent - 1, u' ');
        }
        bool first = true;
        for (const QString &word : QString::fromUtf8(f.description).split(u' ')) {
            if (!first && line.size() + 1 + word.size() > 90) {
                out += line + u'\n';
                line = u'#' + QString(indent - 1, u' ');
                first = true;
            }
            line += (first ? QString() : QStringLiteral(" ")) + word;
            first = false;
        }
        out += line + u'\n';
    }

    for (const EmulatorDef &def : emulators) {
        out += u'\n';
        if (!def.preset)
            out += QStringLiteral("# Defined in emulators.toml.\n");
        else if (def.userKeys.isEmpty())
            out += QStringLiteral("# Built in.\n");
        else
            out += QStringLiteral("# Built in, with changes from emulators.toml (marked).\n");
        out += QStringLiteral("[[emulator]]\nid = %1\n").arg(tomlString(def.id));
        for (const Field &f : EmulatorFields::all()) {
            const QString value = formatValue(def, f);
            if (value.isEmpty())
                continue;
            const QString key = QLatin1String(f.key);
            out += QStringLiteral("%1 = %2").arg(key, value);
            if (def.preset && def.userKeys.contains(key))
                out += QStringLiteral("   # emulators.toml");
            out += u'\n';
        }
    }
    return out;
}

bool writeReference(const QString &dataDir, const QList<EmulatorDef> &emulators)
{
    const QString path = dataDir + QStringLiteral("/library/emulators.reference.toml");
    const QByteArray content = referenceText(emulators).toUtf8();
    if (QFile existing(path); existing.open(QIODevice::ReadOnly) && existing.readAll() == content)
        return true;
    QSaveFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write(content) == content.size() && file.commit();
}

QString expandHome(const QString &path, const QString &home)
{
    if (path == u'~')
        return home;
    if (path.startsWith(QLatin1String("~/")))
        return home + path.mid(1);
    return path;
}
}

QString EmulatorInstall::describe(const QString &home) const
{
    const auto tilde = [&](const QString &path) {
        if (!home.isEmpty() && path.startsWith(home + u'/'))
            return u'~' + path.mid(home.size());
        return path;
    };
    switch (kind) {
    case Kind::None: return QStringLiteral("Not found");
    case Kind::Flatpak: return QStringLiteral("Flatpak %1").arg(program);
    case Kind::Path:
    case Kind::Executable:
    case Kind::AppImage: return portable ? QStringLiteral("%1 (portable)").arg(tilde(program)) : tilde(program);
    }
    return {};
}

QString EmulatorInstall::installType() const
{
    switch (kind) {
    case Kind::None: return {};
    case Kind::Flatpak: return QStringLiteral("flatpak");
    default: return portable ? QStringLiteral("portable") : QStringLiteral("native");
    }
}

InstallLocations InstallLocations::system()
{
    InstallLocations where;
    where.home = QDir::homePath();
    where.pathDirs = qEnvironmentVariable("PATH").split(u':', Qt::SkipEmptyParts);
    where.flatpakDirs = {
        qEnvironmentVariable("FLATPAK_USER_DIR", where.home + QStringLiteral("/.local/share/flatpak"))
            + QStringLiteral("/app"),
        qEnvironmentVariable("FLATPAK_SYSTEM_DIR", QStringLiteral("/var/lib/flatpak")) + QStringLiteral("/app"),
    };
    const auto location = [](QStandardPaths::StandardLocation l) { return QStandardPaths::writableLocation(l); };
    where.xdgDirs = {
        {QStringLiteral("desktop"), location(QStandardPaths::DesktopLocation)},
        {QStringLiteral("documents"), location(QStandardPaths::DocumentsLocation)},
        {QStringLiteral("download"), location(QStandardPaths::DownloadLocation)},
        {QStringLiteral("music"), location(QStandardPaths::MusicLocation)},
        {QStringLiteral("pictures"), location(QStandardPaths::PicturesLocation)},
        {QStringLiteral("videos"), location(QStandardPaths::MoviesLocation)},
        {QStringLiteral("templates"), location(QStandardPaths::TemplatesLocation)},
        {QStringLiteral("public-share"), location(QStandardPaths::PublicShareLocation)},
        {QStringLiteral("config"), location(QStandardPaths::GenericConfigLocation)},
        {QStringLiteral("data"), location(QStandardPaths::GenericDataLocation)},
        {QStringLiteral("cache"), location(QStandardPaths::GenericCacheLocation)},
    };
    where.applicationsDirs = {where.home + QStringLiteral("/Applications"), where.home + QStringLiteral("/AppImages")};
    return where;
}

namespace Emulators {
QSet<QString> installedFlatpaks(const QStringList &flatpakDirs)
{
    QSet<QString> out;
    for (const QString &dirPath : flatpakDirs) {
        const QDir dir(dirPath);
        for (const QString &app : dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
            // `current` links to the active branch; an app being uninstalled has none.
            if (QFileInfo::exists(dir.filePath(app + QStringLiteral("/current"))))
                out.insert(app);
        }
    }
    return out;
}

namespace {
EmulatorInstall portableChecked(EmulatorInstall install, const EmulatorDef &emulator)
{
    if (install.found() && install.kind != EmulatorInstall::Kind::Flatpak && !emulator.portable.isEmpty())
        install.portable = QFileInfo::exists(QFileInfo(install.program).dir().filePath(emulator.portable));
    return install;
}
}

EmulatorInstall detect(const EmulatorDef &emulator, const InstallLocations &where, const QSet<QString> &flatpaks)
{
    using Kind = EmulatorInstall::Kind;
    EmulatorInstall install;

    if (!emulator.path.isEmpty()) {
        const QFileInfo info(expandHome(emulator.path, where.home));
        if (info.isFile()) {
            install.kind = Kind::Path;
            install.program = info.absoluteFilePath();
            return portableChecked(install, emulator);
        }
        install.pathMissing = true;
    }

    if (!emulator.flatpak.isEmpty() && flatpaks.contains(emulator.flatpak)) {
        install.kind = Kind::Flatpak;
        install.program = emulator.flatpak;
        install.filesystems = flatpakFilesystems(emulator.flatpak, where.flatpakDirs);
        return install;
    }

    for (const QString &name : emulator.executables) {
        for (const QString &dir : where.pathDirs) {
            const QFileInfo info(dir + u'/' + name);
            if (info.isFile() && info.isExecutable()) {
                install.kind = Kind::Executable;
                install.program = info.absoluteFilePath();
                return portableChecked(install, emulator);
            }
        }
    }

    for (const QString &dirPath : where.applicationsDirs) {
        if (emulator.appimage.isEmpty())
            break;
        // QDir's name filters ignore case unless asked not to; `;` separates patterns.
        const QDir dir(dirPath);
        QStringList matches = dir.entryList(emulator.appimage.split(u';', Qt::SkipEmptyParts), QDir::Files);
        if (!matches.isEmpty()) {
            QCollator collator;
            collator.setNumericMode(true);
            std::sort(matches.begin(), matches.end(), collator);
            install.kind = Kind::AppImage;
            install.program = dir.absoluteFilePath(matches.last());
            break;
        }
    }
    return portableChecked(install, emulator);
}

QStringList installPaths(const QMap<QString, QStringList> &table, const EmulatorInstall &install, const QString &home)
{
    QStringList out;
    const QString type = install.installType();
    const QDir programDir = QFileInfo(install.program).dir();
    for (const QString &path : table.value(type)) {
        out.append(type == QLatin1String("portable") ? QDir::cleanPath(programDir.filePath(path))
                                                     : expandHome(path, home));
    }
    return out;
}

QString choose(const QString &pinned, const QStringList &preferred, const QHash<QString, EmulatorInstall> &installs)
{
    if (!pinned.isEmpty() && installs.value(pinned).found())
        return pinned;
    for (const QString &id : preferred) {
        if (installs.value(id).found())
            return id;
    }
    return {};
}

namespace {
QStringList contextFilesystems(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return {};
    bool inContext = false;
    while (!file.atEnd()) {
        const QString line = QString::fromUtf8(file.readLine()).trimmed();
        if (line.startsWith(u'[')) {
            inContext = line == QLatin1String("[Context]");
        } else if (inContext && line.startsWith(QLatin1String("filesystems="))) {
            return line.mid(12).split(u';', Qt::SkipEmptyParts);
        }
    }
    return {};
}
}

QStringList flatpakFilesystems(const QString &appId, const QStringList &flatpakDirs)
{
    QStringList grants;
    const auto apply = [&](const QStringList &entries) {
        for (QString entry : entries) {
            const bool negated = entry.startsWith(u'!');
            if (negated)
                entry.remove(0, 1);
            for (const QLatin1String suffix : {QLatin1String(":ro"), QLatin1String(":rw"), QLatin1String(":create")}) {
                if (entry.endsWith(suffix))
                    entry.chop(suffix.size());
            }
            if (negated)
                grants.removeAll(entry);
            else if (!grants.contains(entry))
                grants.append(entry);
        }
    };

    for (const QString &dir : flatpakDirs) {
        const QString metadata = dir + u'/' + appId + QStringLiteral("/current/active/metadata");
        if (QFileInfo::exists(metadata)) {
            apply(contextFilesystems(metadata));
            break;
        }
    }
    for (auto it = flatpakDirs.crbegin(); it != flatpakDirs.crend(); ++it) {
        const QString overrides = QDir::cleanPath(*it + QStringLiteral("/../overrides/"));
        apply(contextFilesystems(overrides + QStringLiteral("/global")));
        apply(contextFilesystems(overrides + u'/' + appId));
    }
    return grants;
}

bool flatpakCanRead(const QStringList &filesystems, const QString &folder, const InstallLocations &where)
{
    const QString path = QDir::cleanPath(folder);
    const auto under = [&](const QString &root) {
        const QString r = QDir::cleanPath(root);
        return !root.isEmpty() && (path == r || r == QLatin1String("/") || path.startsWith(r + u'/'));
    };
    for (const QString &grant : filesystems) {
        if (grant == QLatin1String("host"))
            return true;
        if (grant == QLatin1String("home")) {
            if (under(where.home))
                return true;
        } else if (grant == QLatin1String("~") || grant.startsWith(QLatin1String("~/"))) {
            if (under(expandHome(grant, where.home)))
                return true;
        } else if (grant.startsWith(u'/')) {
            if (under(grant))
                return true;
        } else if (grant.startsWith(QLatin1String("xdg-"))) {
            const QString name = grant.mid(4).section(u'/', 0, 0);
            const QString rest = grant.section(u'/', 1);
            const QString base = where.xdgDirs.value(name);
            if (!base.isEmpty() && under(rest.isEmpty() ? base : base + u'/' + rest))
                return true;
        }
    }
    return false;
}

QHash<QString, EmulatorInstall> detectAll(const QList<EmulatorDef> &emulators, const InstallLocations &where)
{
    const QSet<QString> flatpaks = installedFlatpaks(where.flatpakDirs);
    QHash<QString, EmulatorInstall> out;
    for (const EmulatorDef &e : emulators)
        out.insert(e.id, detect(e, where, flatpaks));
    return out;
}
}
