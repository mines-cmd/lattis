#include "romscan.h"
#include "gameid.h"

#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QRegularExpression>

#include <algorithm>
#include <functional>

namespace RomName {
namespace {
bool isHex(QChar c)
{
    return (c >= u'0' && c <= u'9') || (c >= u'a' && c <= u'f') || (c >= u'A' && c <= u'F');
}

QString withoutExtension(const QString &fileName)
{
    const qsizetype dot = fileName.lastIndexOf(u'.');
    return dot > 0 ? fileName.left(dot) : fileName;
}

// Separators left dangling once the groups are gone, as in `Hollow Knight - [v0]`.
bool isDangling(QChar c)
{
    return c == u'-' || c == u',' || c == u'+' || c.isSpace();
}

bool isHexWord(QStringView word)
{
    return word.size() == 16 && std::all_of(word.begin(), word.end(), isHex);
}

// A Switch title ID written without brackets, as scene-style names do
// (`MONSTER.HUNTER.0100770008DD8800.v262144.nsp`). Other bare hex words are left alone:
// they could be part of the name.
bool isBareTitleId(QStringView word)
{
    return isHexWord(word) && word.startsWith(u"01");
}

QList<QStringView> words(QStringView text)
{
    static const QString kSeparators = QStringLiteral("._-[](){}");
    QList<QStringView> out;
    qsizetype start = 0;
    for (qsizetype i = 0; i <= text.size(); ++i) {
        if (i == text.size() || text.at(i).isSpace() || kSeparators.contains(text.at(i))) {
            if (i > start)
                out.append(text.mid(start, i - start));
            start = i + 1;
        }
    }
    return out;
}
}

QString titleId(const QString &fileName)
{
    for (qsizetype i = fileName.indexOf(u'['); i >= 0; i = fileName.indexOf(u'[', i + 1)) {
        if (i + 17 >= fileName.size() || fileName.at(i + 17) != u']')
            continue;
        const QStringView digits = QStringView(fileName).mid(i + 1, 16);
        if (isHexWord(digits))
            return digits.toString().toUpper();
    }
    const QString base = withoutExtension(fileName);
    for (const QStringView word : words(base)) {
        if (isBareTitleId(word))
            return word.toString().toUpper();
    }
    return {};
}

Kind kindOf(const QString &titleId)
{
    if (titleId.size() != 16 || !titleId.startsWith(QLatin1String("01")))
        return Kind::Game;
    if (titleId.endsWith(QLatin1String("000")))
        return Kind::Game;
    if (titleId.endsWith(QLatin1String("800")))
        return Kind::Update;
    return Kind::Dlc;
}

QString cleanTitle(const QString &fileName)
{
    return cleanName(withoutExtension(fileName));
}

QString cleanName(const QString &name)
{
    const QString base = name.normalized(QString::NormalizationForm_C);

    // Scene-style names use dots for spaces; a name with spaces keeps its dots (`Bros.`).
    const bool dotsAreSpaces = !base.contains(u' ') && base.contains(u'.');
    QString out;
    out.reserve(base.size());
    for (qsizetype i = 0; i < base.size(); ++i) {
        const QChar c = base.at(i);
        const QChar close = c == u'[' ? u']' : c == u'(' ? u')' : c == u'{' ? u'}' : QChar();
        if (!close.isNull()) {
            const qsizetype end = base.indexOf(close, i + 1);
            if (end > i) {
                out += u' ';
                i = end;
                continue;
            }
        }
        out += c == u'_' || (dotsAreSpaces && c == u'.') ? QChar(u' ') : c;
    }

    QStringList kept;
    bool afterTitleId = false;
    for (const QString &word : out.split(u' ', Qt::SkipEmptyParts)) {
        if (isBareTitleId(word)) {
            afterTitleId = true;
            continue;
        }
        const bool version = word.size() > 1 && word.at(0) == u'v'
            && std::all_of(word.cbegin() + 1, word.cend(), [](QChar c) { return c.isDigit(); });
        if (afterTitleId && version)
            continue;
        kept.append(word);
    }
    out = kept.join(u' ');
    qsizetype start = 0;
    qsizetype end = out.size();
    while (start < end && isDangling(out.at(start)))
        ++start;
    while (end > start && isDangling(out.at(end - 1)))
        --end;
    out = out.mid(start, end - start);
    return out.isEmpty() ? base.trimmed() : out;
}

namespace {
bool isApostrophe(QChar c)
{
    return c == u'\'' || c == u'’';
}

bool isWordChar(QChar c)
{
    return c.isLetterOrNumber() || c.isMark() || c.isSurrogate() || isApostrophe(c);
}

bool isRomanNumeral(const QString &word)
{
    static const QRegularExpression kRoman(QStringLiteral("^X{0,3}(IX|IV|V?I{0,3})$"),
                                           QRegularExpression::CaseInsensitiveOption);
    return !word.isEmpty() && kRoman.match(word).hasMatch();
}
}

QString fixCase(const QString &title)
{
    const bool hasLower = std::any_of(title.cbegin(), title.cend(), [](QChar c) { return c.isLower(); });
    const bool hasUpper = std::any_of(title.cbegin(), title.cend(), [](QChar c) { return c.isUpper(); });
    if (hasLower == hasUpper)
        return title;

    QString out;
    out.reserve(title.size());
    for (qsizetype i = 0; i < title.size();) {
        if (!isWordChar(title.at(i))) {
            out += title.at(i++);
            continue;
        }
        qsizetype end = i;
        while (end < title.size() && isWordChar(title.at(end)))
            ++end;
        const QString word = title.mid(i, end - i);
        i = end;
        // Words starting with a digit (`3D`, `2nd`) are left as they are.
        if (!word.at(0).isLetter())
            out += word;
        else if (isRomanNumeral(word))
            out += word.toUpper();
        else
            out += word.left(1).toUpper() + word.mid(1).toLower();
    }
    return out;
}

Info parse(const QString &fileName)
{
    Info info;
    info.title = fixCase(cleanTitle(fileName));
    info.titleId = titleId(fileName);
    info.kind = kindOf(info.titleId);
    return info;
}

Info parseFolder(const QString &folderName)
{
    Info info;
    info.title = fixCase(cleanName(folderName));
    return info;
}
}

namespace {
QString plural(int n, const char *one, const char *many)
{
    return QStringLiteral("%1 %2").arg(n).arg(QLatin1String(n == 1 ? one : many));
}

QList<RomFile> listRoms(const QString &folder, const QSet<QString> &extensions, const QSet<QString> &names)
{
    QList<RomFile> out;
    // Without QDir::Hidden, hidden files are skipped; without FollowSymlinks, symlinked
    // folders aren't entered. Hidden folders are checked by hand below.
    QDirIterator it(folder, QDir::Files | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
    const qsizetype prefix = folder.size() + 1;
    while (it.hasNext()) {
        const QFileInfo info = it.nextFileInfo();
        if (!extensions.contains(info.suffix().toLower()) && !names.contains(info.fileName().toLower()))
            continue;
        const QString relative = info.filePath().mid(prefix);
        if (relative.startsWith(u'.') || relative.contains(QLatin1String("/.")))
            continue;
        out.append({relative, info.size()});
    }
    return out;
}

// A folder game's file in an update folder: shadPS4 looks for a game's update next to it,
// in `<game>-UPDATE` or `<game>-patch`.
bool isFolderUpdate(const QString &path, const QSet<QString> &folderGameFiles)
{
    const QFileInfo info(path);
    if (!folderGameFiles.contains(info.fileName().toLower()) || !path.contains(u'/'))
        return false;
    const QString folder = info.dir().dirName();
    return folder.endsWith(QLatin1String("-update"), Qt::CaseInsensitive)
        || folder.endsWith(QLatin1String("-patch"), Qt::CaseInsensitive);
}

bool isFolderGame(const QString &path, const QSet<QString> &folderGameFiles)
{
    return path.contains(u'/') && folderGameFiles.contains(QFileInfo(path).fileName().toLower());
}

RomName::Info infoOf(const QString &path, const QSet<QString> &folderGameFiles)
{
    return isFolderGame(path, folderGameFiles) ? RomName::parseFolder(path.section(u'/', 0, 0))
                                               : RomName::parse(QFileInfo(path).fileName());
}
}

qint64 folderSize(const QString &folder)
{
    qint64 total = 0;
    QDirIterator it(folder, QDir::Files | QDir::Hidden | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
    while (it.hasNext())
        total += it.nextFileInfo().size();
    return total;
}

RomPick inspectRomPick(const QString &path, const QList<Platform> &platforms, const QList<EmulatorDef> &emulators)
{
    const QFileInfo file(path);
    const QString extension = file.suffix().toLower();
    const QString name = file.fileName().toLower();

    RomPick out;
    bool folderGame = false;
    for (const Platform &platform : platforms) {
        for (const QString &id : platform.emulators) {
            const EmulatorDef *e = Emulators::find(emulators, id);
            if (!e)
                continue;
            const bool byName = e->files.contains(name);
            if (byName || (!extension.isEmpty() && e->extensions.contains(extension))) {
                out.platforms.append(platform.id);
                folderGame = folderGame || byName;
                break;
            }
        }
    }

    if (folderGame) {
        // The game is the folder, named after the first folder up that isn't part of the
        // dump's layout (PS3_GAME/USRDIR) or a disc serial (CUSA00900, BLUS30443).
        static const QRegularExpression kLayout(QStringLiteral("^(USRDIR|PS3_GAME|[A-Z]{4}\\d{5}(-UPDATE|-patch)?)$"),
                                                QRegularExpression::CaseInsensitiveOption);
        static const QRegularExpression kUpdateSuffix(QStringLiteral("-(UPDATE|patch)$"),
                                                      QRegularExpression::CaseInsensitiveOption);
        const QStringList folders = file.path().split(u'/', Qt::SkipEmptyParts);
        qsizetype i = folders.size() - 1;
        while (i > 0 && kLayout.match(folders.at(i)).hasMatch())
            --i;
        out.info = RomName::parseFolder(i >= 0 ? QString(folders.at(i)).remove(kUpdateSuffix) : QString());
        if (isFolderUpdate(path, {name}))
            out.problem =
                QStringLiteral("This is an update for %1; install it in the emulator").arg(out.info.title);
        return out;
    }

    out.info = RomName::parse(file.fileName());
    if (out.info.kind == RomName::Kind::Update)
        out.problem = QStringLiteral("This is an update for %1; install it in the emulator").arg(out.info.title);
    else if (out.info.kind == RomName::Kind::Dlc)
        out.problem = QStringLiteral("This is DLC for %1; install it in the emulator").arg(out.info.title);
    return out;
}

RomIndexUpdate updateRomIndex(const QString &platform, const QList<RomEntry> &index, const QStringList &found,
                              QSet<QString> *taken, const QSet<QString> &folderGameFiles,
                              const QHash<QString, qint64> &sizes)
{
    RomIndexUpdate out;
    for (const RomEntry &e : index) {
        if (isFolderUpdate(e.path, folderGameFiles))
            ++out.dropped;
        else
            out.entries.append(e);
    }

    const QSet<QString> foundSet(found.cbegin(), found.cend());
    bool resized = false;
    QList<qsizetype> gone;
    for (qsizetype i = 0; i < out.entries.size(); ++i) {
        RomEntry &e = out.entries[i];
        if (!foundSet.contains(e.path)) {
            gone.append(i);
        } else if (const qint64 size = sizes.value(e.path); size > 0 && size != e.size) {
            e.size = size;
            resized = true;
        }
    }

    QSet<QString> indexed;
    for (const RomEntry &e : index)
        indexed.insert(e.path);
    struct NewFile
    {
        QString path;
        RomName::Info info;
        qint64 size = 0;
        bool matched = false;
    };
    QList<NewFile> newFiles;
    for (const QString &path : found) {
        if (!indexed.contains(path))
            newFiles.append({path, infoOf(path, folderGameFiles), sizes.value(path)});
    }
    std::sort(newFiles.begin(), newFiles.end(), [](const NewFile &a, const NewFile &b) { return a.path < b.path; });
    newFiles.removeIf([&](const NewFile &f) {
        if (f.info.kind == RomName::Kind::Update || isFolderUpdate(f.path, folderGameFiles)) {
            ++out.updates;
            return true;
        }
        if (f.info.kind == RomName::Kind::Dlc) {
            ++out.dlc;
            return true;
        }
        return false;
    });

    const auto takeOver = [&](qsizetype entry, NewFile &file) {
        RomEntry &e = out.entries[entry];
        e.path = file.path;
        if (file.size > 0)
            e.size = file.size;
        gone.removeOne(entry);
        file.matched = true;
        ++out.renamed;
    };

    for (NewFile &file : newFiles) {
        if (file.info.titleId.isEmpty())
            continue;
        const auto entry = std::find_if(gone.cbegin(), gone.cend(),
                                        [&](qsizetype i) { return out.entries.at(i).titleId == file.info.titleId; });
        if (entry != gone.cend())
            takeOver(*entry, file);
    }

    // Without a title ID, a new file takes over an entry whose file is gone when they share
    // a key and nothing else on either side does: a guess is never made.
    const auto matchBy = [&](const std::function<QString(const QString &path, qint64 size)> &key) {
        QHash<QString, QList<qsizetype>> goneByKey;
        for (const qsizetype i : std::as_const(gone)) {
            const RomEntry &e = out.entries.at(i);
            if (const QString k = key(e.path, e.size); !k.isEmpty())
                goneByKey[k].append(i);
        }
        QHash<QString, QList<qsizetype>> newByKey;
        for (qsizetype i = 0; i < newFiles.size(); ++i) {
            const NewFile &f = newFiles.at(i);
            if (const QString k = key(f.path, f.size); !f.matched && !k.isEmpty())
                newByKey[k].append(i);
        }
        for (auto it = newByKey.cbegin(); it != newByKey.cend(); ++it) {
            const QList<qsizetype> entries = goneByKey.value(it.key());
            if (it->size() != 1 || entries.size() != 1)
                continue;
            NewFile &file = newFiles[it->first()];
            const QString &titleId = out.entries.at(entries.first()).titleId;
            if (!titleId.isEmpty() && !file.info.titleId.isEmpty() && titleId != file.info.titleId)
                continue;
            takeOver(entries.first(), file);
        }
    };
    matchBy([&](const QString &path, qint64) { return infoOf(path, folderGameFiles).title.toCaseFolded(); });
    matchBy([](const QString &path, qint64 size) {
        return size > 0 ? QStringLiteral("%1 %2").arg(size).arg(QFileInfo(path).suffix().toLower()) : QString();
    });

    for (const NewFile &file : std::as_const(newFiles)) {
        if (file.matched)
            continue;
        RomEntry entry;
        entry.path = file.path;
        entry.title = file.info.title;
        entry.titleId = file.info.titleId;
        entry.size = file.size;
        entry.id = GameId::forTitle(platform, file.info.title, *taken);
        taken->insert(entry.id);
        out.entries.append(entry);
        out.addedIds.append(entry.id);
        ++out.added;
    }
    out.changed = out.added > 0 || out.renamed > 0 || out.dropped > 0 || resized;
    return out;
}

RomScanResult scanRomFolders(const QList<Platform> &platforms, const QList<EmulatorDef> &emulators,
                             const QMap<QString, QList<RomEntry>> &indexes, QSet<QString> taken,
                             const QString &dataDir, const QString &home, const QSet<QString> &claimed)
{
    RomScanResult out;
    for (const Platform &platform : platforms) {
        if (platform.romFolders.isEmpty())
            continue;
        const QString at = QStringLiteral("library/platforms.toml:%1").arg(platform.line);

        QSet<QString> extensions;
        QSet<QString> names;
        for (const QString &id : platform.emulators) {
            if (const EmulatorDef *e = Emulators::find(emulators, id)) {
                extensions.unite(QSet<QString>(e->extensions.cbegin(), e->extensions.cend()));
                names.unite(QSet<QString>(e->files.cbegin(), e->files.cend()));
            }
        }
        if (extensions.isEmpty() && names.isEmpty()) {
            QStringList candidates;
            for (const EmulatorDef &e : emulators) {
                if (e.platforms.contains(platform.id))
                    candidates.append(QStringLiteral("\"%1\"").arg(e.id));
            }
            out.warnings.append(QStringLiteral("%1: %2 has ROM folders but no emulators, so they aren't scanned%3")
                                    .arg(at, platform.name,
                                         candidates.isEmpty() ? QString()
                                                              : QStringLiteral("; add emulators = [%1]")
                                                                    .arg(candidates.join(QStringLiteral(", ")))));
            continue;
        }

        QHash<QString, QString> files;
        QStringList found;
        QHash<QString, qint64> sizes;
        QSet<QString> folders;
        int foldersScanned = 0;
        for (const QString &written : platform.romFolders) {
            const QString folder = resolveRomFolder(written, dataDir, home);
            if (folders.contains(folder)) {
                out.warnings.append(QStringLiteral("%1: ROM folder %2 is listed twice for %3").arg(at, written, platform.name));
                continue;
            }
            folders.insert(folder);
            if (!QFileInfo(folder).isDir()) {
                out.missingFolders[platform.id].append(written);
                out.warnings.append(QStringLiteral("%1: ROM folder %2 not found; its games show as \"ROM not found\"")
                                        .arg(at, written));
                continue;
            }
            ++foldersScanned;
            for (const auto &[path, size] : listRoms(folder, extensions, names)) {
                if (claimed.contains(folder + u'/' + path))
                    continue;
                if (files.contains(path)) {
                    out.warnings.append(QStringLiteral("%1: %2 is in two ROM folders of %3; using the one in %4")
                                            .arg(at, path, platform.name, files.value(path)));
                    continue;
                }
                files.insert(path, folder);
                found.append(path);
                sizes.insert(path, size);
            }
        }

        // A folder game's size is its whole folder's, which can be thousands of files: it's added
        // up once, when the index has none.
        const QList<RomEntry> index = indexes.value(platform.id);
        QHash<QString, qint64> known;
        for (const RomEntry &e : index)
            known.insert(e.path, e.size);
        for (const QString &path : std::as_const(found)) {
            if (isFolderGame(path, names) && !isFolderUpdate(path, names))
                sizes[path] = known.value(path) > 0 ? known.value(path)
                                                    : folderSize(files.value(path) + u'/' + path.section(u'/', 0, 0));
        }

        const RomIndexUpdate update = updateRomIndex(platform.id, index, found, &taken, names, sizes);
        if (update.changed)
            out.indexes.insert(platform.id, update.entries);
        if (!update.addedIds.isEmpty())
            out.added.insert(platform.id, update.addedIds);
        out.files.insert(platform.id, files);

        int missing = 0;
        for (const RomEntry &e : update.entries)
            missing += files.contains(e.path) ? 0 : 1;
        QString line = QStringLiteral("%1: %2 in %3, %4 new, %5 renamed, %6 not found")
                           .arg(platform.id, plural(int(found.size()), "file", "files"),
                                plural(foldersScanned, "folder", "folders"))
                           .arg(update.added)
                           .arg(update.renamed)
                           .arg(missing);
        if (update.updates > 0 || update.dlc > 0)
            line += QStringLiteral("; skipped %1 and %2")
                        .arg(plural(update.updates, "update", "updates"), plural(update.dlc, "DLC file", "DLC files"));
        out.log.append(line);
    }
    return out;
}
