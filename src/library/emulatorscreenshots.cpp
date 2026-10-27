#include "emulatorscreenshots.h"

#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSet>

#include <algorithm>

namespace EmulatorScreenshots {
namespace {
const QString kRomDir = QStringLiteral("{romdir}/");

// What the emulators leave out of a title when they name a file after it.
QString stripped(const QString &title)
{
    QString out;
    for (const QChar c : title) {
        if (!QStringLiteral("/\\:*?\"<>|").contains(c))
            out.append(c);
    }
    return out.simplified();
}

// The library's title and the emulator's can differ in punctuation ("Bros Ultimate" vs "Bros. Ultimate"), so only
// the letters and digits have to match.
QString looseTitle(const QString &title)
{
    QString out;
    bool gap = false;
    for (const QChar c : title) {
        if (c.isLetterOrNumber()) {
            if (gap && !out.isEmpty())
                out += QStringLiteral("[^\\p{L}\\p{N}]*");
            gap = false;
            out += QRegularExpression::escape(QString(c));
        } else {
            gap = true;
        }
    }
    return out;
}

struct Tokens
{
    QString id;
    QString rom;
    QString romDir;
};

// One path component as a regex: * ? and [..] are wildcards, token values are literal.
QRegularExpression componentRegex(const QString &component, const Tokens &tokens, const QString &title)
{
    QString re;
    for (qsizetype i = 0; i < component.size(); ++i) {
        const QChar c = component.at(i);
        if (c == u'{') {
            const qsizetype end = component.indexOf(u'}', i);
            if (end > i) {
                const QString name = component.mid(i + 1, end - i - 1);
                const QString value = name == QLatin1String("serial") ? tokens.id
                                    : name == QLatin1String("title")  ? title
                                    : name == QLatin1String("rom")    ? tokens.rom
                                                                      : QString();
                re += name == QLatin1String("title") ? looseTitle(value) : QRegularExpression::escape(value);
                i = end;
                continue;
            }
        }
        if (c == u'*') {
            re += QStringLiteral(".*");
        } else if (c == u'?') {
            re += u'.';
        } else if (c == u'[') {
            const qsizetype end = component.indexOf(u']', i);
            if (end > i) {
                re += component.mid(i, end - i + 1);
                i = end;
                continue;
            }
            re += QStringLiteral("\\[");
        } else {
            re += QRegularExpression::escape(QString(c));
        }
    }
    return QRegularExpression(QLatin1Char('^') + re + QLatin1Char('$'), QRegularExpression::CaseInsensitiveOption);
}

bool hasWildcard(const QString &component)
{
    return component.contains(u'*') || component.contains(u'?') || component.contains(u'[');
}

QStringList expand(const QString &pattern, const Tokens &tokens, const QString &title)
{
    QString rest = pattern;
    QString base;
    if (rest.startsWith(kRomDir)) {
        base = tokens.romDir;
        rest.remove(0, kRomDir.size());
    } else if (rest.startsWith(u'/')) {
        base = QStringLiteral("/");
        rest.remove(0, 1);
    } else {
        return {};
    }
    const QStringList components = rest.split(u'/', Qt::SkipEmptyParts);
    QStringList current{base};
    for (qsizetype i = 0; i < components.size() && !current.isEmpty(); ++i) {
        const bool last = i == components.size() - 1;
        const QRegularExpression re = componentRegex(components.at(i), tokens, title);
        QStringList next;
        for (const QString &dir : std::as_const(current)) {
            if (!hasWildcard(components.at(i)) && !components.at(i).contains(u'{')) {
                // A literal name that exists as written needs no listing.
                const QString exact = QDir(dir).filePath(components.at(i));
                if (QFileInfo::exists(exact)) {
                    next.append(exact);
                    continue;
                }
            }
            const QDir::Filters kind = last ? QDir::Files : QDir::Dirs | QDir::NoDotAndDotDot;
            for (const QString &name : QDir(dir).entryList(kind | QDir::Readable, QDir::Name)) {
                if (re.match(name).hasMatch())
                    next.append(QDir(dir).filePath(name));
            }
        }
        current = next;
    }
    return current;
}

bool isImage(const QString &path)
{
    static const QStringList kExtensions = {QStringLiteral("png"), QStringLiteral("jpg"), QStringLiteral("jpeg"),
                                            QStringLiteral("webp"), QStringLiteral("bmp")};
    return kExtensions.contains(QFileInfo(path).suffix().toLower());
}

// A pattern's folder (the components before the first token or wildcard) and the rest.
std::pair<QString, QString> splitPattern(const QString &pattern)
{
    const bool besideRom = pattern.startsWith(kRomDir);
    const QStringList components = pattern.mid(besideRom ? kRomDir.size() : 0).split(u'/', Qt::SkipEmptyParts);
    qsizetype i = 0;
    while (i < components.size() - 1 && !hasWildcard(components.at(i)) && !components.at(i).contains(u'{'))
        ++i;
    QString folder = besideRom ? kRomDir.chopped(1) : QString();
    if (i > 0 || !besideRom)
        folder += u'/' + components.mid(0, i).join(u'/');
    return {folder, components.mid(i).join(u'/')};
}

QStringList builtInPatterns(const EmulatorDef &emulator, const EmulatorInstall &install, const QString &home)
{
    QStringList patterns;
    QMap<QString, QStringList> others;
    const QString type = install.found() ? install.installType() : QStringLiteral("native");
    for (const QString &p : emulator.screenshots.value(type)) {
        if (p.startsWith(kRomDir))
            patterns.append(p);
        else
            others[type].append(p);
    }
    if (install.found()) {
        patterns.append(Emulators::installPaths(others, install, home));
    } else {
        for (const QString &p : others.value(type))
            patterns.append(Emulators::expandHome(p, home));
    }
    return patterns;
}

QStringList searchPatterns(const EmulatorDef &emulator, const EmulatorInstall &install, const QString &home)
{
    const QStringList builtIn = builtInPatterns(emulator, install, home);
    if (!emulator.screenshotFoldersSet)
        return builtIn;
    QStringList names;
    for (const QString &p : builtIn) {
        const QString rest = splitPattern(p).second;
        if (!names.contains(rest))
            names.append(rest);
    }
    if (names.isEmpty())
        names.append(QStringLiteral("*"));
    QStringList out;
    for (const QString &folder : emulator.screenshotFolders) {
        const QString base = folder.startsWith(kRomDir.chopped(1)) ? folder : Emulators::expandHome(folder, home);
        for (const QString &name : std::as_const(names))
            out.append(QDir::cleanPath(base) + u'/' + name);
    }
    return out;
}

bool during(const QDateTime &time, const QList<PlayHistory::Session> &sessions)
{
    // Stamps count whole seconds and a session's length whole minutes.
    constexpr qint64 kSlackMs = 60 * 1000;
    const qint64 at = time.toMSecsSinceEpoch();
    return std::any_of(sessions.cbegin(), sessions.cend(), [&](const PlayHistory::Session &s) {
        return s.end.isValid() && at >= s.start().toMSecsSinceEpoch() - kSlackMs
               && at <= s.end.toMSecsSinceEpoch() + kSlackMs;
    });
}
}

RomInfo readInfo(const Game &game)
{
    if (!game.rom || game.romPath.isEmpty())
        return {};
    if (QFileInfo(game.romPath).isDir())
        return RomInfoReader::readFolder(game.romPath, game.platform);
    return RomInfoReader::read(game.romPath, game.platform);
}

QString idOf(const RomInfo &info)
{
    for (const QString &id : {info.serial, info.titleId, info.discId, info.headerName}) {
        if (!id.isEmpty())
            return id;
    }
    return {};
}

QDateTime timeInName(const QString &fileName)
{
    static const QRegularExpression kStamp(
        QStringLiteral("(?<![0-9])(\\d{4})-?(\\d{2})-?(\\d{2})[_-]?(\\d{2})-?(\\d{2})-?(\\d{2})(?![0-9])"));
    const QRegularExpressionMatch m = kStamp.match(fileName);
    if (!m.hasMatch())
        return {};
    const QDate date(m.captured(1).toInt(), m.captured(2).toInt(), m.captured(3).toInt());
    const QTime time(m.captured(4).toInt(), m.captured(5).toInt(), m.captured(6).toInt());
    return date.isValid() && time.isValid() ? QDateTime(date, time) : QDateTime();
}

QStringList folders(const EmulatorDef &emulator, const EmulatorInstall &install, const QString &home)
{
    if (emulator.screenshotFoldersSet)
        return emulator.screenshotFolders;
    QStringList out;
    for (const QString &p : builtInPatterns(emulator, install, home)) {
        const QString folder = splitPattern(p).first;
        if (!out.contains(folder))
            out.append(folder);
    }
    return out;
}

QList<Shot> list(const EmulatorDef &emulator, const EmulatorInstall &install, const Game &game, const RomInfo &info,
                 const QList<PlayHistory::Session> &sessions, const QString &home)
{
    const QStringList patterns = searchPatterns(emulator, install, home);
    if (patterns.isEmpty())
        return {};

    Tokens tokens;
    tokens.id = idOf(info);
    const QFileInfo rom(game.romPath);
    tokens.rom = game.romPath.isEmpty() ? QString() : rom.completeBaseName();
    tokens.romDir = game.romPath.isEmpty() ? QString() : rom.absolutePath();

    // The ROM's own title first: it is what the emulator names files after.
    QStringList titles;
    for (const QString &t : {stripped(info.title), stripped(game.title)}) {
        if (!t.isEmpty() && !titles.contains(t, Qt::CaseInsensitive))
            titles.append(t);
    }

    QList<Shot> out;
    QSet<QString> seen;
    for (const QString &pattern : std::as_const(patterns)) {
        const bool usesTitle = pattern.contains(QLatin1String("{title}"));
        const bool usesId = pattern.contains(QLatin1String("{serial}"));
        const bool usesRom = pattern.contains(QLatin1String("{rom}")) || pattern.startsWith(kRomDir);
        const bool anyToken = usesTitle || usesId || usesRom;
        if ((usesId && tokens.id.isEmpty()) || (usesRom && tokens.rom.isEmpty()) || (usesTitle && titles.isEmpty()))
            continue;

        // No token names the game (Cemu): only the play sessions tell whose a screenshot is.
        const QStringList variants = usesTitle ? titles : QStringList{QString()};
        for (const QString &title : variants) {
            for (const QString &path : expand(pattern, tokens, title)) {
                const QFileInfo file(path);
                if (!isImage(path) || seen.contains(file.absoluteFilePath()))
                    continue;
                seen.insert(file.absoluteFilePath());
                QDateTime time = timeInName(file.fileName());
                if (!time.isValid())
                    time = file.lastModified();
                if (!anyToken && !during(time, sessions))
                    continue;
                out.append({file.fileName(), file.absoluteFilePath(), time});
            }
        }
    }
    std::stable_sort(out.begin(), out.end(), [](const Shot &a, const Shot &b) { return a.time > b.time; });
    return out;
}
}
