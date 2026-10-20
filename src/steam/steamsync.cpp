#include "steamsync.h"
#include "tomlsection.h"

#include "binaryvdf.h"
#include "vdf.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>

#include <toml++/toml.hpp>

#include <algorithm>
#include <sstream>

namespace SteamSync {
namespace {
const QString kLaunchOptions = QStringLiteral("launch_options");
const QString kProton = QStringLiteral("proton");

std::string toStd(const QString &s)
{
    return s.toStdString();
}

QString withoutTrailingSlash(QString path)
{
    while (path.size() > 1 && path.endsWith(u'/'))
        path.chop(1);
    return path;
}

std::optional<QString> recorded(const QHash<qint64, QString> &values, qint64 appId)
{
    const auto it = values.constFind(appId);
    return it == values.cend() ? std::nullopt : std::optional<QString>(*it);
}

const QStringList kLocalConfigApps = {QStringLiteral("UserLocalConfigStore"), QStringLiteral("Software"),
                                      QStringLiteral("Valve"), QStringLiteral("Steam"), QStringLiteral("apps")};
const QStringList kCompatToolMapping = {QStringLiteral("InstallConfigStore"), QStringLiteral("Software"),
                                        QStringLiteral("Valve"), QStringLiteral("Steam"),
                                        QStringLiteral("CompatToolMapping")};

QStringList blockOf(const Edit &edit)
{
    return (edit.key == kProton ? kCompatToolMapping : kLocalConfigApps) + QStringList{QString::number(edit.appId)};
}

QString valueIn(const Vdf::Node &root, const Edit &edit)
{
    const Vdf::Node *block = root.find(blockOf(edit));
    if (!block || !block->isBlock)
        return {};
    return block->valueOf(edit.key == kProton ? u"name" : u"LaunchOptions");
}

Vdf::EditResult edited(const QByteArray &text, const Edit &edit)
{
    const QStringList block = blockOf(edit);
    if (edit.key != kProton) {
        const QStringList key = block + QStringList{QStringLiteral("LaunchOptions")};
        return edit.value.isEmpty() ? Vdf::remove(text, key) : Vdf::setValue(text, key, edit.value);
    }
    if (edit.value.isEmpty())
        return Vdf::remove(text, block);
    Vdf::EditResult r = Vdf::setValue(text, block + QStringList{QStringLiteral("name")}, edit.value);
    if (r.ok && !Vdf::parse(r.text).root.find(block + QStringList{QStringLiteral("config")}))
        r = Vdf::setValue(r.text, block + QStringList{QStringLiteral("config")}, QString());
    if (r.ok)
        r = Vdf::setValue(r.text, block + QStringList{QStringLiteral("priority")}, QStringLiteral("250"));
    return r;
}

bool writeSteamFile(const QString &path, const QString &root, const QByteArray &original, bool existed,
                    const QByteArray &text, const QString &backupDir, const std::function<bool()> &steamRunning,
                    ApplyResult *out);

bool applyToFile(const QString &path, const QString &root, const QList<Edit> &edits, const QString &backupDir,
                 const std::function<bool()> &steamRunning, ApplyResult *out)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        out->error = QStringLiteral("could not read %1: %2").arg(path, file.errorString());
        return false;
    }
    const QByteArray original = file.readAll();
    file.close();
    const Vdf::Document doc = Vdf::parse(original);
    if (!doc.ok) {
        out->error = QStringLiteral("%1: %2").arg(path, doc.error);
        return false;
    }

    QByteArray text = original;
    QList<Edit> done;
    for (const Edit &edit : edits) {
        if (valueIn(doc.root, edit) != edit.steam) {
            out->stale.append(edit);
            continue;
        }
        const Vdf::EditResult r = edited(text, edit);
        if (!r.ok) {
            out->error = QStringLiteral("%1: app %2: %3").arg(path).arg(edit.appId).arg(r.error);
            return false;
        }
        text = r.text;
        done.append(edit);
    }
    if (text == original) {
        out->applied += done;
        return true;
    }

    const Vdf::Document check = Vdf::parse(text);
    const bool readsBack = check.ok && std::all_of(done.cbegin(), done.cend(), [&](const Edit &edit) {
        return valueIn(check.root, edit) == edit.value;
    });
    if (!readsBack) {
        out->error = QStringLiteral("%1: the edited file doesn't read back; not written").arg(path);
        return false;
    }
    if (!writeSteamFile(path, root, original, true, text, backupDir, steamRunning, out))
        return false;
    out->applied += done;
    out->log.append(QStringLiteral("wrote %1 value(s) to %2").arg(done.size()).arg(path));
    return true;
}

bool readsBack(const BinaryVdf::Node &root, const ShortcutEdit &edit)
{
    const QList<SteamShortcut> now = SteamShortcuts::read(root);
    const auto it = std::find_if(now.cbegin(), now.cend(),
                                 [&](const SteamShortcut &s) { return s.appId == edit.shortcut.appId; });
    if (edit.kind == ShortcutEdit::Kind::Remove)
        return it == now.cend();
    return it != now.cend() && sameShortcut(*it, edit.shortcut);
}

bool applyToShortcuts(const QString &path, const QString &root, const QList<ShortcutEdit> &edits,
                      const QString &backupDir, const std::function<bool()> &steamRunning, ApplyResult *out)
{
    QFile file(path);
    const bool existed = file.exists();
    QByteArray original;
    BinaryVdf::Document doc;
    if (existed) {
        if (!file.open(QIODevice::ReadOnly)) {
            out->error = QStringLiteral("could not read %1: %2").arg(path, file.errorString());
            return false;
        }
        original = file.readAll();
        file.close();
        doc = BinaryVdf::parse(original);
        if (!doc.ok) {
            out->error = QStringLiteral("%1: %2").arg(path, doc.error);
            return false;
        }
    } else {
        doc.ok = true; // no shortcuts yet: the first one makes the file
    }

    const QList<SteamShortcut> before = SteamShortcuts::read(doc.root);
    QList<ShortcutEdit> done;
    for (const ShortcutEdit &edit : edits) {
        const auto it = std::find_if(before.cbegin(), before.cend(),
                                     [&](const SteamShortcut &s) { return s.appId == edit.shortcut.appId; });
        const bool asScanned = edit.steam ? it != before.cend() && sameShortcut(*it, *edit.steam) : it == before.cend();
        if (!asScanned) {
            out->shortcutsStale.append(edit);
            continue;
        }
        bool ok = false;
        switch (edit.kind) {
        case ShortcutEdit::Kind::Add: ok = SteamShortcuts::add(&doc.root, edit.shortcut); break;
        case ShortcutEdit::Kind::Update: ok = SteamShortcuts::update(&doc.root, edit.shortcut); break;
        case ShortcutEdit::Kind::Remove: ok = SteamShortcuts::remove(&doc.root, edit.shortcut.appId); break;
        }
        if (!ok) {
            out->error = QStringLiteral("%1: shortcut %2 couldn't be changed").arg(path).arg(edit.shortcut.appId);
            return false;
        }
        done.append(edit);
    }
    const QByteArray text = BinaryVdf::write(doc.root);
    if (existed && text == original) {
        out->shortcutsApplied += done;
        return true;
    }

    const BinaryVdf::Document check = BinaryVdf::parse(text);
    if (!check.ok || !std::all_of(done.cbegin(), done.cend(), [&](const ShortcutEdit &e) { return readsBack(check.root, e); })) {
        out->error = QStringLiteral("%1: the edited file doesn't read back; not written").arg(path);
        return false;
    }
    if (!writeSteamFile(path, root, original, existed, text, backupDir, steamRunning, out))
        return false;
    out->shortcutsApplied += done;
    out->log.append(QStringLiteral("wrote %1 shortcut change(s) to %2").arg(done.size()).arg(path));
    return true;
}
}

bool sameShortcut(const SteamShortcut &a, const SteamShortcut &b)
{
    return a.appId == b.appId && a.name == b.name && a.exe == b.exe && a.exeArgs == b.exeArgs
        && withoutTrailingSlash(a.startDir) == withoutTrailingSlash(b.startDir) && a.launchOptions == b.launchOptions
        && a.icon == b.icon;
}

namespace {
bool writeSteamFile(const QString &path, const QString &root, const QByteArray &original, bool existed,
                    const QByteArray &text, const QString &backupDir, const std::function<bool()> &steamRunning,
                    ApplyResult *out)
{
    // Once: the file as it was before the launcher first changed it. config.vdf holds login
    // tokens, so only the user may read the copy.
    const QString backup = backupDir + u'/' + QDir(root).relativeFilePath(path);
    if (existed && !QFileInfo::exists(backup)) {
        QDir().mkpath(QFileInfo(backup).absolutePath());
        QSaveFile copy(backup);
        if (!copy.open(QIODevice::WriteOnly) || !copy.setPermissions(QFile::ReadOwner | QFile::WriteOwner)
            || copy.write(original) < 0 || !copy.commit()) {
            out->error = QStringLiteral("could not back up %1 to %2: %3").arg(path, backup, copy.errorString());
            return false;
        }
        out->log.append(QStringLiteral("backed up %1 to %2").arg(path, backup));
    }

    QSaveFile save(path);
    if (!save.open(QIODevice::WriteOnly) || save.write(text) < 0) {
        out->error = QStringLiteral("could not write %1: %2").arg(path, save.errorString());
        return false;
    }
    // Right before the rename: Steam would overwrite the file when it exits.
    if (steamRunning()) {
        save.cancelWriting();
        out->error = QStringLiteral("Steam started; %1 wasn't written").arg(path);
        return false;
    }
    if (!save.commit()) {
        out->error = QStringLiteral("could not write %1: %2").arg(path, save.errorString());
        return false;
    }
    return true;
}
}

QString recordPath(const QString &stateDir, const QString &dataDirKey)
{
    return stateDir + QStringLiteral("/steam-sync-") + dataDirKey + QStringLiteral(".toml");
}

Record readRecord(const QString &path, const QString &library, QStringList *warnings)
{
    Record out;
    out.library = library;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return out;
    const QByteArray bytes = file.readAll();
    toml::table root;
    try {
        root = toml::parse(std::string_view(bytes.constData(), size_t(bytes.size())));
    } catch (const toml::parse_error &e) {
        warnings->append(QStringLiteral("%1:%2: %3; the launcher's launch options and Proton versions will be "
                                        "applied to Steam again")
                             .arg(path)
                             .arg(e.source().begin.line)
                             .arg(toQString(e.description())));
        return out;
    }
    const auto *id = root["library"].as_string();
    if (library.isEmpty() || !id || toQString(id->get()) != library) {
        warnings->append(QStringLiteral("%1 isn't this library's; comparing with Steam from scratch").arg(path));
        return out;
    }
    if (const toml::table *shortcuts = root["shortcuts"].as_table()) {
        for (auto &&[key, node] : *shortcuts) {
            bool ok = false;
            const qint64 appId = toQString(key.str()).toLongLong(&ok);
            const toml::table *values = node.as_table();
            if (!ok || appId <= 0 || !values)
                continue;
            const auto text = [&](const char *name) {
                const auto *s = values->get_as<std::string>(name);
                return s ? toQString(s->get()) : QString();
            };
            out.shortcuts.insert(appId, {text("title"), text("exe"), text("start_dir"), text("launch_options"),
                                         text("proton")});
        }
    }
    if (const toml::table *removed = root["removed"].as_table()) {
        for (auto &&[key, node] : *removed) {
            bool ok = false;
            const qint64 appId = toQString(key.str()).toLongLong(&ok);
            if (ok && appId > 0)
                out.removed.insert(appId, node.as_string() ? toQString(node.as_string()->get()) : QString());
        }
    }
    if (const toml::table *grid = root["grid"].as_table()) {
        for (auto &&[key, node] : *grid) {
            bool ok = false;
            const qint64 appId = toQString(key.str()).toLongLong(&ok);
            const toml::table *kinds = node.as_table();
            if (!ok || appId <= 0 || !kinds)
                continue;
            for (auto &&[kind, fileNode] : *kinds) {
                const toml::table *f = fileNode.as_table();
                const auto *name = f ? f->get_as<std::string>("file") : nullptr;
                if (!name)
                    continue;
                const auto number = [&](const char *field) -> qint64 {
                    const auto *v = f->get_as<int64_t>(field);
                    return v ? v->get() : 0;
                };
                const auto *art = f->get_as<std::string>("art");
                out.grid[appId].insert(toQString(kind.str()),
                                       {toQString(name->get()), number("size"), number("modified"),
                                        art ? toQString(art->get()) : QString(), number("art_size"),
                                        number("art_modified")});
            }
        }
    }
    const toml::table *games = root["games"].as_table();
    if (!games)
        return out;
    for (auto &&[key, node] : *games) {
        bool ok = false;
        const qint64 appId = toQString(key.str()).toLongLong(&ok);
        const toml::table *values = node.as_table();
        if (!ok || appId <= 0 || !values)
            continue;
        if (const auto *s = values->get_as<std::string>("launch_options"))
            out.launchOptions.insert(appId, toQString(s->get()));
        if (const auto *s = values->get_as<std::string>("proton"))
            out.proton.insert(appId, toQString(s->get()));
    }
    return out;
}

bool writeRecord(const QString &path, const Record &record, const QString &dataDir, QString *error)
{
    QList<qint64> appIds = record.launchOptions.keys() + record.proton.keys();
    std::sort(appIds.begin(), appIds.end());
    appIds.erase(std::unique(appIds.begin(), appIds.end()), appIds.end());

    std::ostringstream text;
    toml::table library;
    library.insert("library", toStd(record.library));
    text << toml::toml_formatter(library, toml::format_flags::none) << "\n\n[games]\n";
    for (const qint64 appId : std::as_const(appIds)) {
        toml::table values;
        values.is_inline(true);
        if (const auto v = recorded(record.launchOptions, appId))
            values.insert("launch_options", toStd(*v));
        if (const auto v = recorded(record.proton, appId))
            values.insert("proton", toStd(*v));
        text << appId << " = " << toml::toml_formatter(values, toml::format_flags::none) << '\n';
    }
    if (!record.shortcuts.isEmpty()) {
        QList<qint64> ids = record.shortcuts.keys();
        std::sort(ids.begin(), ids.end());
        text << "\n[shortcuts]\n";
        for (const qint64 appId : std::as_const(ids)) {
            const ShortcutRecord &s = record.shortcuts[appId];
            toml::table values;
            values.is_inline(true);
            values.insert("title", toStd(s.title));
            values.insert("exe", toStd(s.exe));
            values.insert("start_dir", toStd(s.startDir));
            values.insert("launch_options", toStd(s.launchOptions));
            values.insert("proton", toStd(s.proton));
            text << appId << " = " << toml::toml_formatter(values, toml::format_flags::none) << '\n';
        }
    }
    if (!record.removed.isEmpty()) {
        QList<qint64> ids = record.removed.keys();
        std::sort(ids.begin(), ids.end());
        text << "\n[removed]\n";
        for (const qint64 appId : std::as_const(ids)) {
            const toml::value<std::string> title(toStd(record.removed[appId]));
            text << appId << " = " << toml::toml_formatter(title, toml::format_flags::none) << '\n';
        }
    }
    QList<qint64> gridIds = record.grid.keys();
    std::sort(gridIds.begin(), gridIds.end());
    for (const qint64 appId : std::as_const(gridIds)) {
        const QHash<QString, SteamArt::GridFile> &files = record.grid[appId];
        QStringList kinds = files.keys();
        kinds.sort();
        text << "\n[grid." << appId << "]\n";
        for (const QString &kind : std::as_const(kinds)) {
            const SteamArt::GridFile &f = files[kind];
            toml::table values;
            values.is_inline(true);
            values.insert("file", toStd(f.name));
            values.insert("size", f.size);
            values.insert("modified", f.modified);
            values.insert("art", toStd(f.art));
            values.insert("art_size", f.artSize);
            values.insert("art_modified", f.artModified);
            text << toStd(kind) << " = " << toml::toml_formatter(values, toml::format_flags::none) << '\n';
        }
    }

    const QString header = QStringLiteral(
        "# What Steam had for each game's launch options and Proton version the last time they\n"
        "# matched the game files in %1, and which files in Steam's grid/ are the\n"
        "# launcher's. Written by Lattis to tell which side changed; deleting it\n"
        "# makes the launcher apply its values to Steam again.\n\n")
                               .arg(dataDir);

    QDir().mkpath(QFileInfo(path).absolutePath());
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly) || file.write(header.toUtf8()) < 0
        || file.write(QByteArray::fromStdString(text.str())) < 0 || !file.commit()) {
        *error = QStringLiteral("could not write %1: %2").arg(path, file.errorString());
        return false;
    }
    return true;
}

ShortcutRecord recordOf(const SteamShortcut &shortcut, const QString &proton)
{
    return {shortcut.name, shortcut.exe, shortcut.startDir, shortcut.launchOptions, proton};
}

Side compare(const QString &gameFile, const QString &steam, const std::optional<QString> &record)
{
    if (gameFile == steam)
        return Side::InSync;
    if (!record)
        return gameFile.isEmpty() ? Side::FromSteam : Side::Queued;
    return steam == *record ? Side::Queued : Side::FromSteam;
}

QString ShortcutGame::startFolder() const
{
    return withoutTrailingSlash(startDir.isEmpty() ? QFileInfo(exe).absolutePath() : startDir);
}

namespace {
void syncShortcut(const ShortcutGame &game, const SteamState &steam, Result *out)
{
    const auto s = std::find_if(steam.shortcuts.cbegin(), steam.shortcuts.cend(),
                                [&](const SteamShortcut &c) { return c.appId == game.appId; });
    const bool inSteam = s != steam.shortcuts.cend();
    const auto recordIt = out->record.shortcuts.constFind(game.appId);
    const std::optional<ShortcutRecord> last =
        recordIt == out->record.shortcuts.cend() ? std::nullopt : std::optional<ShortcutRecord>(*recordIt);

    if (!game.installed) {
        if (inSteam)
            out->queued[game.id].append(kRemoveShortcut);
        else
            out->record.shortcuts.remove(game.appId);
        return;
    }
    if (!inSteam) {
        if (!last) {
            out->queued[game.id].append(kAddShortcut);
            return;
        }
        out->log.append(QStringLiteral("%1: its shortcut was deleted in Steam; marked not installed").arg(game.id));
        out->copies[game.id].append({QStringLiteral("installed"), false});
        out->record.shortcuts.remove(game.appId);
        return;
    }

    // Steam's side and the record's, compared as the game file's: paths without a trailing slash.
    ShortcutRecord now = recordOf(*s, steam.compatTools.value(game.appId));
    now.startDir = withoutTrailingSlash(now.startDir);
    ShortcutRecord next = now;
    struct Value
    {
        QString key;
        bool known;
        QString gameFile;
        QString ShortcutRecord::*field;
    };
    const Value values[] = {
        {QStringLiteral("title"), true, game.title, &ShortcutRecord::title},
        {QStringLiteral("exe"), true, game.exe, &ShortcutRecord::exe},
        {QStringLiteral("start_dir"), true, game.startFolder(), &ShortcutRecord::startDir},
        {kLaunchOptions, true, game.launchOptions, &ShortcutRecord::launchOptions},
        {kProton, steam.configOk, game.proton, &ShortcutRecord::proton},
    };
    QString exe = game.exe;
    QList<std::pair<QString, QString>> copied;
    for (const Value &v : values) {
        const QString steamValue = now.*v.field;
        const std::optional<QString> recorded =
            last ? std::optional<QString>(v.key == QLatin1String("start_dir") ? withoutTrailingSlash((*last).*v.field)
                                                                               : (*last).*v.field)
                 : std::nullopt;
        if (!v.known) {
            next.*v.field = recorded.value_or(QString());
            continue;
        }
        switch (compare(v.gameFile, steamValue, recorded)) {
        case Side::InSync:
            break;
        case Side::Queued:
            out->queued[game.id].append(v.key);
            // What Steam has until it's written: a change in Steam before then still wins.
            next.*v.field = recorded.value_or(steamValue);
            break;
        case Side::FromSteam:
            if (recorded && v.gameFile != *recorded)
                out->log.append(QStringLiteral("%1: %2 changed in Steam and here; keeping Steam's \"%3\" over \"%4\"")
                                    .arg(game.id, v.key, steamValue, v.gameFile));
            if (v.key == QLatin1String("exe"))
                exe = steamValue;
            copied.append({v.key, steamValue});
            break;
        }
    }
    for (const auto &[key, value] : std::as_const(copied)) {
        // The program's own folder is the default: no start_dir.
        const bool ownFolder = key == QLatin1String("start_dir") && value == QFileInfo(exe).absolutePath();
        out->copies[game.id].append({key, ownFolder ? QString() : value});
    }
    // The launcher's icon while Steam's has none; cleared once the launcher's file is gone.
    const bool addIcon = s->icon.isEmpty() && !game.icon.isEmpty();
    const bool clearIcon = game.icon.isEmpty() && !s->icon.isEmpty()
        && s->icon == SteamArt::gridIconPath(steam.install, game.appId) && !QFileInfo::exists(s->icon);
    if (addIcon || clearIcon)
        out->queued[game.id].append(kShortcutIcon);
    out->record.shortcuts.insert(game.appId, next);
}
}

QString removedKey(quint32 appId)
{
    return QStringLiteral("removed:%1").arg(appId);
}

quint32 removedAppId(const QString &key)
{
    return key.startsWith(QLatin1String("removed:")) ? key.mid(8).toUInt() : 0;
}

Result sync(const QList<Game> &games, const SteamState &steam, const Record &record,
            const QList<ShortcutGame> &shortcutGames)
{
    Result out;
    out.record = record;
    if (steam.shortcutsOk) {
        for (const ShortcutGame &game : shortcutGames) {
            if (game.appId != 0)
                syncShortcut(game, steam, &out);
        }
        for (auto it = record.removed.cbegin(); it != record.removed.cend(); ++it) {
            const quint32 appId = quint32(it.key());
            const bool inLibrary = std::any_of(shortcutGames.cbegin(), shortcutGames.cend(),
                                               [&](const ShortcutGame &g) { return g.appId == appId; });
            const bool inSteam = std::any_of(steam.shortcuts.cbegin(), steam.shortcuts.cend(),
                                             [&](const SteamShortcut &s) { return s.appId == appId; });
            const bool mapped = steam.configOk && steam.compatTools.contains(appId);
            if (inLibrary || (!inSteam && !mapped)) {
                out.record.removed.remove(appId);
                if (!inLibrary)
                    out.record.shortcuts.remove(appId);
                continue;
            }
            out.queued[removedKey(appId)].append(kRemoveShortcut);
        }
    }
    for (const Game &game : games) {
        if (game.appId <= 0)
            continue;
        struct Value
        {
            const QString &key;
            bool known;
            QString gameFile;
            QString steam;
            QHash<qint64, QString> &record;
        };
        const Value values[] = {
            {kLaunchOptions, steam.localConfigOk, game.launchOptions,
             steam.apps.value(game.appId).launchOptions, out.record.launchOptions},
            {kProton, steam.configOk, game.proton, steam.compatTools.value(game.appId), out.record.proton},
        };
        for (const Value &v : values) {
            if (!v.known)
                continue;
            const std::optional<QString> last = recorded(v.record, game.appId);
            switch (compare(v.gameFile, v.steam, last)) {
            case Side::InSync:
                v.record.insert(game.appId, v.steam);
                break;
            case Side::Queued:
                out.queued[game.id].append(v.key);
                break;
            case Side::FromSteam:
                if (last && v.gameFile != *last)
                    out.log.append(QStringLiteral("%1: %2 changed in Steam and here; keeping Steam's \"%3\" "
                                                  "over \"%4\"")
                                       .arg(game.id, v.key, v.steam, v.gameFile));
                out.copies[game.id].append({v.key, v.steam});
                v.record.insert(game.appId, v.steam);
                break;
            }
        }
    }
    return out;
}

QString withDefaults(const QString &launchOptions, const SteamSettings &settings, bool forLinux)
{
    QString out = launchOptions;
    for (const QString &id : settings.defaultPresets) {
        if (const SteamPreset *preset = settings.preset(id); preset && !(forLinux && preset->protonOnly()))
            out = LaunchOptions::turnOn(out, *preset);
    }
    return out;
}

ApplyResult apply(const SteamState &steam, const QList<Edit> &edits, const QString &backupDir,
                  const std::function<bool()> &steamRunning, const QList<ShortcutEdit> &shortcuts)
{
    ApplyResult out;
    if (edits.isEmpty() && shortcuts.isEmpty())
        return out;
    if (steamRunning()) {
        out.error = QStringLiteral("Steam is running");
        return out;
    }
    if (!shortcuts.isEmpty()) {
        if (!steam.shortcutsOk || steam.shortcutsPath.isEmpty()) {
            out.error = QStringLiteral("%1 wasn't read")
                            .arg(steam.shortcutsPath.isEmpty() ? QStringLiteral("shortcuts.vdf") : steam.shortcutsPath);
            return out;
        }
        if (!applyToShortcuts(steam.shortcutsPath, steam.install.root, shortcuts, backupDir, steamRunning, &out))
            return out;
    }
    QList<Edit> launchOptions;
    QList<Edit> proton;
    for (const Edit &edit : edits) {
        if (edit.appId <= 0)
            continue;
        (edit.key == kProton ? proton : launchOptions).append(edit);
    }
    const QString config = steam.install.root + QStringLiteral("/config/config.vdf");
    const struct
    {
        QString path;
        bool known;
        const QList<Edit> &edits;
    } files[] = {
        {steam.localConfigPath, steam.localConfigOk && !steam.localConfigPath.isEmpty(), launchOptions},
        {config, steam.configOk, proton},
    };
    for (const auto &f : files) {
        if (f.edits.isEmpty())
            continue;
        if (!f.known) {
            out.error = QStringLiteral("%1 wasn't read").arg(f.path.isEmpty() ? QStringLiteral("localconfig.vdf") : f.path);
            return out;
        }
        if (!applyToFile(f.path, steam.install.root, f.edits, backupDir, steamRunning, &out))
            return out;
    }
    return out;
}
}
