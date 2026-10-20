#include "libraryfiles.h"
#include "tomlsection.h"
#include "gameid.h"
#include "gamefile.h"
#include "romscan.h"
#include "suggestion.h"

#include <QCollator>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QRegularExpression>
#include <QSet>
#include <QtEndian>

#include <toml++/toml.hpp>

#include <algorithm>

namespace {
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

QString where(const QString &displayPath, const toml::node &node)
{
    return QStringLiteral("%1:%2").arg(displayPath).arg(node.source().begin.line);
}

void warnUnknownKey(QStringList *warnings, const QString &at, const QString &key, const QStringList &known)
{
    warnings->append(QStringLiteral("%1: unknown key %2%3").arg(at, key, suggestion(key, known)));
}

QList<const toml::table *> arrayOfTables(const toml::table &root, const char *name, const QString &displayPath,
                                         QStringList *warnings)
{
    QList<const toml::table *> out;
    const QString arrayName = QLatin1String(name);
    for (auto &&[k, node] : root) {
        const QString key = toQString(k.str());
        if (key != arrayName) {
            warnings->append(QStringLiteral("%1: unknown key %2; entries go in [[%3]] blocks%4")
                                 .arg(where(displayPath, node), key, arrayName, suggestion(key, {arrayName})));
            continue;
        }
        const toml::array *array = node.as_array();
        if (!array || !array->is_array_of_tables()) {
            warnings->append(QStringLiteral("%1: %2 must be [[%2]] blocks").arg(where(displayPath, node), arrayName));
            continue;
        }
        for (const toml::node &item : *array)
            out.append(item.as_table());
    }
    return out;
}
}

Parsed<LibraryInfo> parseLibraryInfo(const QByteArray &bytes, const QString &displayPath)
{
    Parsed<LibraryInfo> out;
    toml::table root;
    if (!parseToml(bytes, displayPath, &root, &out.warnings))
        return out;
    out.ok = true;

    bool seen = false;
    for (auto &&[k, node] : root) {
        const QString key = toQString(k.str());
        if (key == QLatin1String("id")) {
            if (const auto *id = node.as_string(); id && !id->get().empty())
                out.value.id = toQString(id->get());
            else
                out.warnings.append(QStringLiteral("%1: id must be text; the app makes a new one")
                                        .arg(where(displayPath, node)));
            continue;
        }
        if (key == QLatin1String("setup")) {
            if (const auto *setup = node.as_string(); setup && setup->get() == "pending")
                out.value.setupPending = true;
            else
                out.warnings.append(QStringLiteral("%1: setup can only be \"pending\"; ignored")
                                        .arg(where(displayPath, node)));
            continue;
        }
        if (key != QLatin1String("version")) {
            warnUnknownKey(&out.warnings, where(displayPath, node), key,
                           {QStringLiteral("version"), QStringLiteral("id"), QStringLiteral("setup")});
            continue;
        }
        seen = true;
        const auto *version = node.as_integer();
        if (!version || version->get() < 1 || version->get() > 1'000'000) {
            out.warnings.append(QStringLiteral("%1: version must be a whole number, 1 or more; reading it as 1")
                                    .arg(where(displayPath, node)));
            continue;
        }
        out.value.version = int(version->get());
    }
    if (!seen)
        out.warnings.append(QStringLiteral("%1: no version; reading it as 1").arg(displayPath));
    return out;
}

Parsed<QList<Platform>> parsePlatforms(const QByteArray &bytes, const QString &displayPath)
{
    Parsed<QList<Platform>> out;
    toml::table root;
    if (!parseToml(bytes, displayPath, &root, &out.warnings))
        return out;
    out.ok = true;

    const QStringList known = {QStringLiteral("id"), QStringLiteral("name"), QStringLiteral("icon"),
                               QStringLiteral("emulators"), QStringLiteral("rom_folders"),
                               QStringLiteral("igdb_platforms"), QStringLiteral("in_rail")};
    QSet<QString> ids;
    for (const toml::table *entry : arrayOfTables(root, "platform", displayPath, &out.warnings)) {
        const QString at = where(displayPath, *entry);
        Platform p;
        p.line = int(entry->source().begin.line);
        for (auto &&[k, node] : *entry) {
            const QString key = toQString(k.str());
            const auto *s = node.as_string();
            if (!known.contains(key)) {
                warnUnknownKey(&out.warnings, where(displayPath, node), key, known);
            } else if (key == QLatin1String("in_rail")) {
                if (const auto *b = node.as_boolean())
                    p.inRail = b->get();
                else
                    out.warnings.append(QStringLiteral("%1: in_rail must be true or false").arg(where(displayPath, node)));
            } else if (key == QLatin1String("igdb_platforms")) {
                QList<int> list;
                bool ok = node.is_array();
                if (ok) {
                    for (const toml::node &item : *node.as_array()) {
                        const auto *i = item.as_integer();
                        ok = ok && i && i->get() > 0 && i->get() <= 2147483647;
                        if (ok)
                            list.append(int(i->get()));
                    }
                }
                if (ok)
                    p.igdbPlatforms = list;
                else
                    out.warnings.append(QStringLiteral("%1: igdb_platforms must be a list of IGDB platform ids, like [130]")
                                            .arg(where(displayPath, node)));
            } else if (key == QLatin1String("emulators") || key == QLatin1String("rom_folders")) {
                QStringList list;
                bool ok = node.is_array();
                if (ok) {
                    for (const toml::node &item : *node.as_array()) {
                        const auto *itemString = item.as_string();
                        ok = ok && itemString;
                        if (itemString && !itemString->get().empty())
                            list.append(toQString(itemString->get()));
                    }
                }
                if (!ok)
                    out.warnings.append(QStringLiteral("%1: %2 must be a list of text in quotes, like [\"a\", \"b\"]")
                                            .arg(where(displayPath, node), key));
                else if (key == QLatin1String("emulators"))
                    p.emulators = list;
                else
                    p.romFolders = list;
            } else if (!s) {
                out.warnings.append(QStringLiteral("%1: %2 must be text in quotes").arg(where(displayPath, node), key));
            } else if (key == QLatin1String("id")) {
                p.id = toQString(s->get());
            } else if (key == QLatin1String("name")) {
                p.name = toQString(s->get());
            } else {
                p.icon = toQString(s->get());
            }
        }
        if (p.id.isEmpty()) {
            out.warnings.append(QStringLiteral("%1: platform without an id; skipped").arg(at));
            continue;
        }
        if (!GameId::isValid(p.id)) {
            out.warnings.append(
                QStringLiteral("%1: platform id \"%2\" may only use lowercase a-z, 0-9 and -; skipped").arg(at, p.id));
            continue;
        }
        if (ids.contains(p.id)) {
            out.warnings.append(QStringLiteral("%1: platform id \"%2\" is used twice; skipped").arg(at, p.id));
            continue;
        }
        ids.insert(p.id);
        if (p.name.isEmpty())
            p.name = p.id;
        out.value.append(p);
    }
    return out;
}

QList<int> igdbPlatformIds(const Platform &platform)
{
    if (!platform.igdbPlatforms.isEmpty())
        return platform.igdbPlatforms;
    // From IGDB's platform list (checked 2026-09). The 3DS includes New Nintendo 3DS exclusives.
    static const QHash<QString, QList<int>> builtIn = {
        {QStringLiteral("switch"), {130}},
        {QStringLiteral("3ds"), {37, 137}},
        {QStringLiteral("steam"), {6}},
        {QStringLiteral("proton"), {6}},
        {QStringLiteral("linux"), {3, 6}}, // Linux, and PC: many games are listed on PC only
        {QStringLiteral("gamecube"), {21}},
        {QStringLiteral("wii"), {5}},
        {QStringLiteral("wiiu"), {41}},
        {QStringLiteral("ps1"), {7}},
        {QStringLiteral("ps2"), {8}},
        {QStringLiteral("ps3"), {9}},
        {QStringLiteral("ps4"), {48}},
        {QStringLiteral("psp"), {38}},
        {QStringLiteral("gba"), {24}},
        {QStringLiteral("n64"), {4}},
        {QStringLiteral("nds"), {20}},
    };
    if (const auto it = builtIn.constFind(platform.icon); it != builtIn.cend())
        return *it;
    return builtIn.value(platform.id);
}

Parsed<QList<TagDef>> parseTags(const QByteArray &bytes, const QString &displayPath)
{
    Parsed<QList<TagDef>> out;
    toml::table root;
    if (!parseToml(bytes, displayPath, &root, &out.warnings))
        return out;
    out.ok = true;

    const QStringList known = {QStringLiteral("name"), QStringLiteral("color"), QStringLiteral("igdb")};
    static const QRegularExpression igdbEntry(QStringLiteral("^(?:(?:genre|theme|mode):[1-9][0-9]{0,8}|keyword:[a-z0-9][a-z0-9-]*)$"));
    QSet<QString> names;
    for (const toml::table *entry : arrayOfTables(root, "tag", displayPath, &out.warnings)) {
        TagDef tag;
        for (auto &&[k, node] : *entry) {
            const QString key = toQString(k.str());
            if (key == QLatin1String("name")) {
                if (const auto *s = node.as_string())
                    tag.name = toQString(s->get());
                else
                    out.warnings.append(QStringLiteral("%1: name must be text in quotes").arg(where(displayPath, node)));
            } else if (key == QLatin1String("color")) {
                const auto *i = node.as_integer();
                if (i && i->get() >= 0 && i->get() < 1000)
                    tag.color = int(i->get());
                else
                    out.warnings.append(
                        QStringLiteral("%1: color must be a whole number, 0 or more (an index into the theme's "
                                       "tags.palette); using 0")
                            .arg(where(displayPath, node)));
            } else if (key == QLatin1String("igdb")) {
                const toml::array *array = node.as_array();
                if (!array) {
                    out.warnings.append(QStringLiteral("%1: igdb must be a list like [\"genre:8\", \"keyword:roguelike\"]")
                                            .arg(where(displayPath, node)));
                    continue;
                }
                tag.igdbGiven = true;
                for (const toml::node &item : *array) {
                    const auto *s = item.as_string();
                    const QString entry = s ? toQString(s->get()) : QString();
                    if (!igdbEntry.match(entry).hasMatch())
                        out.warnings.append(
                            QStringLiteral("%1: igdb entries are \"genre:<id>\", \"theme:<id>\", \"mode:<id>\" or "
                                           "\"keyword:<slug>\"; skipping %2")
                                .arg(where(displayPath, item), s ? tomlString(entry) : QStringLiteral("a value")));
                    else if (!tag.igdb.contains(entry))
                        tag.igdb.append(entry);
                }
            } else {
                warnUnknownKey(&out.warnings, where(displayPath, node), key, known);
            }
        }
        if (tag.name.isEmpty()) {
            out.warnings.append(QStringLiteral("%1: tag without a name; skipped").arg(where(displayPath, *entry)));
            continue;
        }
        if (names.contains(tag.name)) {
            out.warnings.append(
                QStringLiteral("%1: tag \"%2\" is listed twice; skipped").arg(where(displayPath, *entry), tag.name));
            continue;
        }
        names.insert(tag.name);
        out.value.append(tag);
    }
    return out;
}

Parsed<QList<IndexEntry>> parseSteamIndex(const QByteArray &bytes, const QString &displayPath)
{
    Parsed<QList<IndexEntry>> out;
    toml::table root;
    if (!parseToml(bytes, displayPath, &root, &out.warnings))
        return out;
    out.ok = true;

    for (auto &&[k, node] : root) {
        const QString key = toQString(k.str());
        const toml::table *games = node.as_table();
        if (key != QLatin1String("games") || !games) {
            out.warnings.append(QStringLiteral("%1: unknown key %2; games go in the [games] section%3")
                                    .arg(where(displayPath, node), key, suggestion(key, {QStringLiteral("games")})));
            continue;
        }
        for (auto &&[appKey, title] : *games) {
            const QString at = where(displayPath, title);
            bool isNumber = false;
            const qint64 appId = toQString(appKey.str()).toLongLong(&isNumber);
            if (!isNumber || appId <= 0) {
                out.warnings.append(QStringLiteral("%1: \"%2\" is not a Steam app ID (a positive whole number); skipped")
                                        .arg(at, toQString(appKey.str())));
                continue;
            }
            const auto *s = title.as_string();
            if (!s) {
                out.warnings.append(QStringLiteral("%1: the title of %2 must be text in quotes; skipped").arg(at).arg(appId));
                continue;
            }
            out.value.append({appId, toQString(s->get())});
        }
    }
    return out;
}

Parsed<QList<RomEntry>> parseRomIndex(const QByteArray &bytes, const QString &displayPath)
{
    Parsed<QList<RomEntry>> out;
    toml::table root;
    if (!parseToml(bytes, displayPath, &root, &out.warnings))
        return out;
    out.ok = true;

    const QStringList known = {QStringLiteral("id"), QStringLiteral("title"), QStringLiteral("title_id"),
                               QStringLiteral("size")};
    QSet<QString> ids;
    for (auto &&[k, node] : root) {
        const QString key = toQString(k.str());
        const toml::table *games = node.as_table();
        if (key != QLatin1String("games") || !games) {
            out.warnings.append(QStringLiteral("%1: unknown key %2; games go in the [games] section%3")
                                    .arg(where(displayPath, node), key, suggestion(key, {QStringLiteral("games")})));
            continue;
        }
        for (auto &&[pathKey, value] : *games) {
            const QString at = where(displayPath, value);
            RomEntry entry;
            entry.path = toQString(pathKey.str());
            const toml::table *fields = value.as_table();
            if (!fields) {
                out.warnings.append(QStringLiteral("%1: \"%2\" must be { id = \"…\", title = \"…\" }; skipped")
                                        .arg(at, entry.path));
                continue;
            }
            for (auto &&[fk, fv] : *fields) {
                const QString field = toQString(fk.str());
                const auto *s = fv.as_string();
                if (!known.contains(field)) {
                    warnUnknownKey(&out.warnings, at, field, known);
                } else if (field == QLatin1String("size")) {
                    // Written by the scan; a wrong one is only a missed rename match.
                    if (const auto *n = fv.as_integer(); n && n->get() >= 0)
                        entry.size = n->get();
                    else
                        out.warnings.append(QStringLiteral("%1: size must be a number of bytes").arg(at));
                } else if (!s) {
                    out.warnings.append(QStringLiteral("%1: %2 must be text in quotes").arg(at, field));
                } else if (field == QLatin1String("id")) {
                    entry.id = toQString(s->get());
                } else if (field == QLatin1String("title")) {
                    entry.title = toQString(s->get());
                } else {
                    entry.titleId = toQString(s->get()).toUpper();
                }
            }
            if (!GameId::isValid(entry.id)) {
                out.warnings.append(
                    entry.id.isEmpty()
                        ? QStringLiteral("%1: \"%2\" has no id; skipped").arg(at, entry.path)
                        : QStringLiteral("%1: id \"%2\" may only use lowercase a-z, 0-9 and -; skipped").arg(at, entry.id));
                continue;
            }
            if (ids.contains(entry.id)) {
                out.warnings.append(QStringLiteral("%1: id \"%2\" is used twice; skipped").arg(at, entry.id));
                continue;
            }
            ids.insert(entry.id);
            out.value.append(entry);
        }
    }
    return out;
}

QString formatRomIndex(const QString &platformName, QList<RomEntry> entries)
{
    std::sort(entries.begin(), entries.end(), [](const RomEntry &a, const RomEntry &b) { return a.path < b.path; });
    QString out = QStringLiteral(
                      "# %1 ROMs found by the folder scan, as path = { id, title }. Paths are relative to a\n"
                      "# folder in rom_folders. Written by the scan; ids never change. You can fix a title here.\n"
                      "# The size (in bytes) helps the scan recognize a renamed file.\n"
                      "[games]\n")
                      .arg(platformName);
    for (const RomEntry &e : std::as_const(entries)) {
        out += QStringLiteral("%1 = { id = %2, title = %3").arg(tomlString(e.path), tomlString(e.id), tomlString(e.title));
        if (!e.titleId.isEmpty())
            out += QStringLiteral(", title_id = %1").arg(tomlString(e.titleId));
        if (e.size > 0)
            out += QStringLiteral(", size = %1").arg(e.size);
        out += QStringLiteral(" }\n");
    }
    return out;
}

QString resolveRomFolder(const QString &folder, const QString &dataDir, const QString &home)
{
    const QString expanded = Emulators::expandHome(folder, home);
    return QDir::cleanPath(QDir(dataDir).absoluteFilePath(expanded));
}

QHash<QString, RomFile> findRomGameFiles(const QMap<QString, GameFile> &games, const QString &dataDir,
                                         const QString &home)
{
    QHash<QString, RomFile> out;
    for (auto it = games.cbegin(); it != games.cend(); ++it) {
        if (it->rom.isEmpty())
            continue;
        const QString path = resolveRomFolder(it->rom, dataDir, home);
        if (const QFileInfo info(path); info.isFile())
            out.insert(it.key(), {path, info.size()});
    }
    return out;
}

namespace {
QString installDirOf(const QString &exe, const QString &home, const QStringList &generic)
{
    QString dir = QFileInfo(exe).absolutePath();
    while (generic.contains(QFileInfo(dir).fileName().toLower())) {
        const QString parent = QFileInfo(dir).absolutePath();
        if (parent == dir || parent == u'/' || QDir::cleanPath(parent) == QDir::cleanPath(home))
            break;
        dir = parent;
    }
    return dir;
}

const QStringList kProtonGenericFolders = {QStringLiteral("bin"), QStringLiteral("binaries"), QStringLiteral("win64"),
                                           QStringLiteral("win32"), QStringLiteral("x64"), QStringLiteral("x86"),
                                           QStringLiteral("game")};

template <typename T>
T elfValue(const QByteArray &bytes, qsizetype at, bool bigEndian)
{
    if (at < 0 || at + qsizetype(sizeof(T)) > bytes.size())
        return 0;
    return bigEndian ? qFromBigEndian<T>(bytes.constData() + at) : qFromLittleEndian<T>(bytes.constData() + at);
}
}

QString protonInstallDir(const QString &exe, const QString &home)
{
    return installDirOf(exe, home, kProtonGenericFolders);
}

QString linuxInstallDir(const QString &exe, const QString &home)
{
    static const QStringList generic = kProtonGenericFolders
        + QStringList{QStringLiteral("x86_64"), QStringLiteral("linux"), QStringLiteral("linux64"), QStringLiteral("lib")};
    return installDirOf(exe, home, generic);
}

LinuxProgramKind linuxProgramKind(const QString &path)
{
    QFile file(path);
    if (!QFileInfo(path).isFile() || !file.open(QIODevice::ReadOnly))
        return LinuxProgramKind::None;
    const QByteArray head = file.read(64);
    if (head.startsWith("#!"))
        return LinuxProgramKind::Script;
    if (!head.startsWith("\x7f" "ELF") || head.size() < 52)
        return LinuxProgramKind::None;
    // AppImage's runtime marks itself in the ELF header's padding: "AI" and its type.
    if (head.mid(8, 2) == "AI" && (head.at(10) == 1 || head.at(10) == 2))
        return LinuxProgramKind::AppImage;
    const bool is64 = head.at(4) == 2;
    const bool big = head.at(5) == 2;
    const quint16 type = elfValue<quint16>(head, 16, big);
    const quint16 machine = elfValue<quint16>(head, 18, big);
#if defined(Q_PROCESSOR_X86)
    const bool runsHere = machine == 62 || machine == 3; // x86-64, i386
#elif defined(Q_PROCESSOR_ARM)
    const bool runsHere = machine == 183 || machine == 40; // AArch64, ARM
#else
    const bool runsHere = true;
#endif
    if (!runsHere)
        return LinuxProgramKind::None;
    static const QRegularExpression libraryName(QStringLiteral("\\.so(\\.\\d+)*$"));
    if (libraryName.match(QFileInfo(path).fileName()).hasMatch())
        return LinuxProgramKind::SharedLibrary;
    if (type == 2) // ET_EXEC
        return LinuxProgramKind::Elf;
    if (type != 3) // not ET_DYN either: an object file, a core dump
        return LinuxProgramKind::None;
    // ET_DYN is a library, or a program built as position-independent; programs name an
    // interpreter (the dynamic linker) in a PT_INTERP header.
    const quint64 phoff = is64 ? elfValue<quint64>(head, 32, big) : elfValue<quint32>(head, 28, big);
    const quint16 phentsize = elfValue<quint16>(head, is64 ? 54 : 42, big);
    const quint16 phnum = elfValue<quint16>(head, is64 ? 56 : 44, big);
    if (phoff == 0 || phentsize < 4 || phnum == 0 || !file.seek(qint64(phoff)))
        return LinuxProgramKind::SharedLibrary;
    const QByteArray headers = file.read(qint64(phentsize) * qMin<quint16>(phnum, 256));
    for (qsizetype at = 0; at + 4 <= headers.size(); at += phentsize) {
        if (elfValue<quint32>(headers, at, big) == 3) // PT_INTERP
            return LinuxProgramKind::Elf;
    }
    return LinuxProgramKind::SharedLibrary;
}

QHash<QString, ProgramFiles> findProgramFiles(const QMap<QString, GameFile> &games, const QString &dataDir,
                                              const QString &home, const QHash<QString, ProgramFiles> &previous,
                                              bool addUpSizes)
{
    QHash<QString, ProgramFiles> out;
    for (auto it = games.cbegin(); it != games.cend(); ++it) {
        if (!runsProgram(it->platform))
            continue;
        ProgramFiles f;
        if (!it->exe.isEmpty()) {
            f.exe = resolveRomFolder(it->exe, dataDir, home);
            const QFileInfo exe(f.exe);
            f.exeFound = exe.isFile() && (it->platform != kLinuxPlatform || exe.isExecutable());
        }
        if (!it->installDir.isEmpty())
            f.installDir = resolveRomFolder(it->installDir, dataDir, home);
        f.singleFile = f.exeFound
                       && (f.installDir.isEmpty() || QDir::cleanPath(f.installDir) == QFileInfo(f.exe).absolutePath())
                       && linuxProgramKind(f.exe) == LinuxProgramKind::AppImage;
        if (const auto old = previous.constFind(it.key());
            old != previous.cend() && old->files() == f.files() && old->sizeBytes > 0)
            f.sizeBytes = old->sizeBytes;
        else if (addUpSizes && f.singleFile)
            f.sizeBytes = QFileInfo(f.exe).size();
        else if (addUpSizes && !f.installDir.isEmpty())
            f.sizeBytes = folderSize(f.installDir);
        out.insert(it.key(), f);
    }
    return out;
}

namespace {
void checkRomType(const GameFile &f, const QString &file, const Platform &platform, const QList<EmulatorDef> &emulators,
                  QStringList *warnings)
{
    QStringList takes;
    for (const QString &id : platform.emulators) {
        const EmulatorDef *e = Emulators::find(emulators, id);
        for (const QString &ext : e->extensions)
            takes.append(u'.' + ext);
        takes += e->files;
    }
    if (takes.isEmpty())
        return;
    const QFileInfo rom(f.rom);
    if (takes.contains(u'.' + rom.suffix().toLower()) || takes.contains(rom.fileName().toLower()))
        return;
    takes.removeDuplicates();
    warnings->append(QStringLiteral("%1:%2: %3 isn't a file %4's emulators run; they take %5")
                         .arg(file)
                         .arg(f.romLine)
                         .arg(rom.fileName(), platform.name, takes.join(QStringLiteral(", "))));
}
}

QCollator titleCollator()
{
    QCollator collator;
    collator.setCaseSensitivity(Qt::CaseInsensitive);
    collator.setNumericMode(true);
    return collator;
}

bool titleBefore(const QCollator &collator, const Game &a, const Game &b)
{
    const int c = collator.compare(a.title, b.title);
    return c != 0 ? c < 0 : a.id < b.id;
}

BuiltLibrary buildLibrary(const LibraryContent &content, const QHash<QString, ArtSet> &art, const RomFiles &romFiles,
                          const QList<EmulatorEntry> &presets, const QHash<QString, RomFile> &romGameFiles,
                          const SteamState &steam, const QHash<QString, QStringList> &steamQueued,
                          const QHash<QString, ProgramFiles> &programFiles)
{
    BuiltLibrary out;
    out.platforms = content.platforms;
    out.emulators = Emulators::merge(presets, content.emulators, &out.warnings);

    QStringList emulatorIds;
    for (const EmulatorDef &e : std::as_const(out.emulators))
        emulatorIds.append(e.id);
    QHash<QString, QString> platformNames;
    for (Platform &p : out.platforms) {
        platformNames.insert(p.id, p.name);
        QStringList known;
        for (const QString &id : std::as_const(p.emulators)) {
            const EmulatorDef *emulator = Emulators::find(out.emulators, id);
            if (!emulator) {
                out.warnings.append(QStringLiteral("library/platforms.toml:%1: unknown emulator \"%2\" for %3%4")
                                        .arg(p.line)
                                        .arg(id, p.name, suggestion(id, emulatorIds)));
                continue;
            }
            if (!emulator->platforms.isEmpty() && !emulator->platforms.contains(p.id))
                out.warnings.append(QStringLiteral("library/platforms.toml:%1: %2 is for %3, not %4")
                                        .arg(p.line)
                                        .arg(emulator->name, emulator->platforms.join(QStringLiteral(", ")), p.id));
            known.append(id);
        }
        p.emulators = known;
    }
    QHash<QString, int> tagColors;
    for (const TagDef &t : content.tags)
        tagColors.insert(t.name, t.color);

    // Steam's apps by game id. An app with manifests in two library folders (a copy left
    // behind by a move) counts as installed if either is.
    QHash<QString, SteamApp> steamApps;
    for (const SteamManifest &m : steam.manifests) {
        if (!m.ok)
            continue;
        SteamApp &app = steamApps[GameId::forSteamApp(m.app.appId)];
        if (!app.installed)
            app = m.app;
    }

    QMap<QString, GameFile> merged;
    QHash<QString, RomFile> romPaths;
    for (const IndexEntry &entry : content.steam) {
        // Recorded before the blocklist knew it (a new Steam runtime, say).
        if (SteamScan::isBlocked(entry.appId, entry.title))
            continue;
        GameFile g;
        g.title = entry.title;
        g.platform = kSteamPlatform;
        merged.insert(GameId::forSteamApp(entry.appId), g);
    }
    for (auto it = content.roms.cbegin(); it != content.roms.cend(); ++it) {
        const QHash<QString, QString> found = romFiles.value(it.key());
        for (const RomEntry &entry : it.value()) {
            if (merged.contains(entry.id)) {
                out.warnings.append(QStringLiteral("library/roms/%1.toml: id \"%2\" is already used by another "
                                                   "game; skipped \"%3\"")
                                        .arg(it.key(), entry.id, entry.path));
                continue;
            }
            GameFile g;
            g.title = entry.title.isEmpty() ? RomName::cleanTitle(QFileInfo(entry.path).fileName()) : entry.title;
            g.platform = it.key();
            merged.insert(entry.id, g);
            const QString folder = found.value(entry.path);
            romPaths.insert(entry.id, folder.isEmpty() ? RomFile() : RomFile{folder + u'/' + entry.path, entry.size});
        }
    }
    for (auto it = content.games.cbegin(); it != content.games.cend(); ++it) {
        GameFile g = it.value();
        if (const auto base = merged.constFind(it.key()); base != merged.cend()) {
            if (g.title.isEmpty())
                g.title = base->title;
            if (g.platform.isEmpty())
                g.platform = base->platform;
        }
        merged.insert(it.key(), g);
        if (!g.rom.isEmpty())
            romPaths.insert(it.key(), romGameFiles.value(it.key()));
    }

    QHash<quint32, qint64> shortcutsLastPlayed;
    for (const SteamShortcut &s : steam.shortcuts)
        shortcutsLastPlayed.insert(s.appId, s.lastPlayTime);
    QStringList protonGames;

    QHash<QString, QStringList> unknownPlatforms;
    QStringList hiddenOnUnknown;
    for (auto it = merged.cbegin(); it != merged.cend(); ++it) {
        const GameFile &f = it.value();
        // A hidden game is built like the others (it can be shown), but its problems aren't
        // reported: the user put it away.
        QStringList ignored;
        QStringList *warnings = f.hidden ? &ignored : &out.warnings;
        const QString file = QStringLiteral("library/games/%1.toml").arg(it.key());

        Game g;
        g.id = it.key();
        g.title = f.title;
        if (g.title.isEmpty()) {
            warnings->append(QStringLiteral("%1: no title; showing the id instead").arg(file));
            g.title = g.id;
        }
        g.platform = f.platform;
        if (g.platform.isEmpty()) {
            warnings->append(QStringLiteral("%1: no platform; add platform = \"<id from platforms.toml>\"").arg(file));
            g.platform = QStringLiteral("unknown");
        }
        if (!platformNames.contains(g.platform)) {
            if (f.hidden)
                hiddenOnUnknown.append(g.platform);
            else
                unknownPlatforms[g.platform].append(file);
        }
        else if (!f.rom.isEmpty())
            checkRomType(f, file, *std::find_if(out.platforms.cbegin(), out.platforms.cend(),
                                                [&](const Platform &p) { return p.id == g.platform; }),
                         out.emulators, warnings);
        g.platformName = platformNames.value(g.platform, g.platform);
        g.description = f.description;
        g.year = f.year;
        g.rating = f.rating;
        g.ratingFromIgdb = f.ratingSource == QLatin1String("igdb");
        g.favorite = f.favorite;
        g.status = f.status;
        g.logoScale = f.logoScale;
        g.logoPosition = f.logoPosition;
        g.logoOffsetX = f.logoOffsetX;
        g.logoOffsetY = f.logoOffsetY;
        g.logoScrim = f.logoScrim;
        g.logoScrimSide = f.logoScrimSide;
        g.logoScrimStrength = f.logoScrimStrength;
        g.playMinutes = f.playMinutes;
        g.lastPlayed = f.lastPlayed;
        g.added = f.added;
        g.emulator = f.emulator;
        if (!g.emulator.isEmpty() && !Emulators::find(out.emulators, g.emulator)) {
            warnings->append(QStringLiteral("%1: unknown emulator \"%2\"%3")
                                    .arg(file, g.emulator, suggestion(g.emulator, emulatorIds)));
            g.emulator.clear();
        }
        g.args = f.args;
        const bool steamOrProton = g.platform == kSteamPlatform || g.platform == kProtonPlatform;
        if (steamOrProton || g.platform == kLinuxPlatform)
            g.launchOptions = f.launchOptions;
        if (steamOrProton)
            g.proton = f.proton;
        const auto ignoredKeys = [&](const QString &platforms, std::initializer_list<std::pair<const char *, bool>> keys) {
            QStringList set;
            for (const auto &[key, isSet] : keys)
                if (isSet)
                    set.append(QLatin1String(key));
            if (!set.isEmpty())
                warnings->append(QStringLiteral("%1: %2 %3 for %4 games; ignored")
                                     .arg(file, set.join(QStringLiteral(", ")),
                                          set.size() == 1 ? QStringLiteral("is") : QStringLiteral("are"), platforms));
        };
        if (!steamOrProton && g.platform != kLinuxPlatform)
            ignoredKeys(QStringLiteral("Steam, Proton and Linux"), {{"launch_options", !f.launchOptions.isEmpty()}});
        if (!steamOrProton)
            ignoredKeys(QStringLiteral("Steam and Proton"),
                    {{"proton", !f.proton.isEmpty()}, {"skip_steam_art", !f.skipSteamArt.isEmpty()}});
        if (!runsProgram(g.platform))
            ignoredKeys(QStringLiteral("Proton and Linux"),
                    {{"exe", !f.exe.isEmpty()}, {"start_dir", !f.startDir.isEmpty()},
                     {"install_dir", !f.installDir.isEmpty()}, {"installed", !f.installed}});
        if (g.platform != kProtonPlatform)
            ignoredKeys(QStringLiteral("Proton"), {{"shortcut", f.shortcut != 0}});
        // A shortcut's icon alone (SteamSync::kShortcutIcon) isn't worth restarting Steam for.
        const QStringList queued = steamQueued.value(g.id);
        g.pendingSteamEdits = queued.size() > int(queued.contains(QStringLiteral("icon")));
        if (const auto rom = romPaths.constFind(g.id); rom != romPaths.cend()) {
            g.rom = true;
            g.romKey = !f.rom.isEmpty();
            g.romPath = rom->path;
            g.installed = !rom->path.isEmpty();
            g.sizeBytes = rom->size;
        } else if (g.platform == kSteamPlatform) {
            // Installed only once Steam says so: without Steam (or with the import off) a
            // Steam game can't launch.
            const qint64 appId = GameId::steamAppId(g.id);
            const SteamApp app = steamApps.value(g.id);
            g.installed = app.installed;
            if (app.installed)
                g.sizeBytes = app.sizeBytes;
            // A first install has no build yet but is still downloading.
            if (app.installed || app.update != SteamUpdate::None) {
                g.installDir = app.installDir;
                g.steamUpdate = app.update;
                g.updateBytes = app.updateBytes;
                g.updatePercent = app.updatePercent;
            }
            if (const auto config = steam.apps.constFind(appId); appId > 0 && config != steam.apps.cend()) {
                g.steamPlayMinutes = config->playMinutes;
                if (config->lastPlayed > 0)
                    g.steamLastPlayed = QDateTime::fromSecsSinceEpoch(config->lastPlayed);
            }
            // Steam's, read-only: it keeps them for every game played on this account.
            g.playMinutes = qMax(g.steamPlayMinutes, 0);
            g.lastPlayed = g.steamLastPlayed;
            if (f.playMinutes > 0 || f.lastPlayed.isValid())
                warnings->append(QStringLiteral("%1: play_minutes and last_played are Steam's for a Steam game; "
                                                "ignored")
                                     .arg(file));
        } else if (g.platform == kProtonPlatform) {
            const ProgramFiles files = programFiles.value(g.id);
            g.exe = files.exe;
            g.installDir = files.installDir;
            g.sizeBytes = files.sizeBytes;
            g.uninstalled = !f.installed;
            g.installed = f.installed && files.exeFound;
            g.shortcut = quint32(f.shortcut);
            if (f.exe.isEmpty())
                warnings->append(QStringLiteral("%1: no exe; add exe = \"<the game's .exe>\"").arg(file));
            if (f.shortcut == 0)
                warnings->append(QStringLiteral("%1: no shortcut; add it from Steam with Settings → Steam → "
                                                "Add existing shortcuts")
                                     .arg(file));
            if (g.shortcut != 0) {
                g.steamPlayMinutes = steam.shortcutConfig(g.shortcut).playMinutes;
                if (const qint64 last = shortcutsLastPlayed.value(g.shortcut); last > 0)
                    g.steamLastPlayed = QDateTime::fromSecsSinceEpoch(last);
            }
            g.playMinutes = qMax(g.steamPlayMinutes, 0);
            g.lastPlayed = g.steamLastPlayed;
            if (f.playMinutes > 0 || f.lastPlayed.isValid())
                warnings->append(QStringLiteral("%1: play_minutes and last_played are Steam's for a Proton game; "
                                                "ignored")
                                     .arg(file));
            if (!f.hidden)
                protonGames.append(file);
        } else if (g.platform == kLinuxPlatform) {
            const ProgramFiles files = programFiles.value(g.id);
            g.exe = files.exe;
            g.installDir = files.installDir;
            g.sizeBytes = files.sizeBytes;
            g.uninstalled = !f.installed;
            g.installed = f.installed && files.exeFound;
            if (f.exe.isEmpty())
                warnings->append(QStringLiteral("%1: no exe; add exe = \"<the game's program>\"").arg(file));
        }
        for (const QString &tag : f.tags)
            g.tags.append({tag, tagColors.value(tag, 0)});

        const ArtSet a = art.value(g.id);
        const auto url = [&a](const QString &path) {
            if (path.isEmpty())
                return QUrl();
            QUrl out = QUrl::fromLocalFile(path);
            if (a.modified > 0)
                out.setQuery(QStringLiteral("v=%1").arg(a.modified));
            return out;
        };
        g.heroUrl = url(a.hero);
        g.logoUrl = url(a.logo);
        g.iconUrl = url(a.icon);
        g.coverUrl = url(a.cover);
        g.hidden = f.hidden;
        (g.hidden ? out.hidden : out.games).append(g);
    }

    if (!protonGames.isEmpty() && !steam.install.found()) {
        const QString more = protonGames.size() > 1
            ? QCoreApplication::translate("Library", " (and %n more game(s))", nullptr, int(protonGames.size() - 1))
            : QString();
        out.warnings.append(QStringLiteral("%1%2: Proton games need Steam, %3")
                                .arg(protonGames.first(), more,
                                     steam.enabled ? QStringLiteral("which isn't found")
                                                   : QStringLiteral("and the Steam import is off in settings.toml")));
    }

    // One warning per unknown platform, not one per game: an imported library can have
    // hundreds of games on a platform that isn't defined yet.
    QStringList unknownIds = unknownPlatforms.keys();
    unknownIds.sort();
    const QStringList knownIds = platformNames.keys();
    for (const QString &id : std::as_const(unknownIds)) {
        const QStringList &files = unknownPlatforms.value(id);
        const QString more = files.size() > 1
            ? QCoreApplication::translate("Library", " (and %n more game(s))", nullptr, int(files.size() - 1))
            : QString();
        out.warnings.append(QStringLiteral("%1%2: platform \"%3\" isn't in library/platforms.toml%4")
                                .arg(files.first(), more, id, suggestion(id, knownIds)));
        out.platforms.append({id, id, {}});
    }
    hiddenOnUnknown.removeDuplicates();
    hiddenOnUnknown.sort();
    for (const QString &id : std::as_const(hiddenOnUnknown)) {
        if (!unknownPlatforms.contains(id))
            out.platforms.append({id, id, {}});
    }

    const QCollator collator = titleCollator();
    const auto byTitle = [&](const Game &a, const Game &b) { return titleBefore(collator, a, b); };
    std::sort(out.games.begin(), out.games.end(), byTitle);
    std::sort(out.hidden.begin(), out.hidden.end(), byTitle);
    return out;
}
