#include "savesfinder.h"

#include "searchtext.h"

#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSet>
#include <algorithm>

namespace SavesFinder {
namespace {
const QString kWineUser = QStringLiteral("steamuser");

bool hasWildcard(const QString &segment)
{
    return segment.contains(u'*') || segment.contains(u'?') || segment.contains(u'[');
}

bool exists(const QString &path)
{
    const QFileInfo info(path);
    return info.isSymLink() || info.exists();
}

QString expandHome(const QString &path, const QString &home)
{
    if (path == QLatin1String("~"))
        return home;
    if (path.startsWith(QLatin1String("~/")))
        return home + path.mid(1);
    return path;
}

struct Context
{
    const Env &env;
    const Query &query;
    const SavesGame *game;
    bool windows;
    QString drive; // <prefix>/drive_c
    QString user;  // …/users/steamuser

    QStringList values(const QString &name) const
    {
        if (name == QLatin1String("home"))
            return {windows ? user : env.home};
        if (name == QLatin1String("osUserName"))
            return {windows ? kWineUser : env.user};
        if (name == QLatin1String("storeUserId"))
            return {QStringLiteral("*")};
        if (name == QLatin1String("storeGameId"))
            return {query.steamAppId > 0 ? QString::number(query.steamAppId) : QStringLiteral("*")};
        if (name == QLatin1String("base"))
            return query.baseDir.isEmpty() ? QStringList() : QStringList{QDir::cleanPath(query.baseDir)};
        if (name == QLatin1String("root"))
            return env.steamRoot.isEmpty() ? QStringList() : QStringList{env.steamRoot};
        if (name == QLatin1String("game")) {
            QStringList names;
            if (!query.baseDir.isEmpty())
                names << QFileInfo(QDir::cleanPath(query.baseDir)).fileName();
            if (game)
                names << game->installDirs;
            names.removeAll(QString());
            names.removeDuplicates();
            return names;
        }
        if (windows) {
            if (name == QLatin1String("winAppData"))
                return {user + QStringLiteral("/AppData/Roaming")};
            if (name == QLatin1String("winLocalAppData"))
                return {user + QStringLiteral("/AppData/Local")};
            if (name == QLatin1String("winLocalAppDataLow"))
                return {user + QStringLiteral("/AppData/LocalLow")};
            if (name == QLatin1String("winDocuments"))
                return {user + QStringLiteral("/Documents")};
            if (name == QLatin1String("winPublic"))
                return {drive + QStringLiteral("/users/Public")};
            if (name == QLatin1String("winProgramData"))
                return {drive + QStringLiteral("/ProgramData")};
            if (name == QLatin1String("winDir"))
                return {drive + QStringLiteral("/windows")};
            if (name == QLatin1String("winDrive"))
                return {drive};
        } else {
            if (name == QLatin1String("xdgData"))
                return {env.xdgData};
            if (name == QLatin1String("xdgConfig"))
                return {env.xdgConfig};
        }
        return {};
    }
};

// A placeholder with no value here (a Windows folder for a native game) drops the path.
QStringList expand(const QString &path, const Context &ctx)
{
    QStringList results{QString()};
    qsizetype at = 0;
    while (at < path.size()) {
        const qsizetype open = path.indexOf(u'<', at);
        const qsizetype close = open < 0 ? -1 : path.indexOf(u'>', open);
        if (open < 0 || close < 0) {
            for (QString &r : results)
                r += path.mid(at);
            break;
        }
        const QStringList values = ctx.values(path.mid(open + 1, close - open - 1));
        if (values.isEmpty())
            return {};
        QStringList next;
        for (const QString &r : std::as_const(results)) {
            for (const QString &v : values)
                next << r + path.mid(at, open - at) + v;
        }
        results = next;
        at = close + 1;
    }
    return results;
}

bool tooBroad(const QString &path, const Env &env, const Context &ctx)
{
    const QString clean = QDir::cleanPath(path);
    const QStringList roots{QStringLiteral("/"), env.home, env.xdgData, env.xdgConfig, ctx.drive, ctx.user};
    return std::any_of(roots.cbegin(), roots.cend(), [&](const QString &r) { return !r.isEmpty() && clean == r; });
}

QString caseInsensitiveChild(const QString &dir, const QString &name)
{
    const QStringList names = QDir(dir).entryList(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System);
    for (const QString &n : names) {
        if (n.compare(name, Qt::CaseInsensitive) == 0)
            return n;
    }
    return {};
}

// `caseBlind`: a Proton prefix, where Wine finds `my games` for `My Games`.
QStringList expandGlob(const QString &pattern, bool caseBlind)
{
    const QStringList segments = pattern.split(u'/', Qt::SkipEmptyParts);
    QList<QPair<QString, bool>> current{{QStringLiteral("/"), false}}; // path, reached through a glob match that is a link
    for (qsizetype i = 0; i < segments.size(); ++i) {
        const QString &segment = segments.at(i);
        QList<QPair<QString, bool>> next;
        for (const auto &[dir, viaLink] : std::as_const(current)) {
            if (viaLink)
                continue;
            const QString prefix = dir == QLatin1String("/") ? QString() : dir;
            if (segment == QLatin1String("**")) {
                if (!QFileInfo(dir).isDir())
                    continue;
                next.append({dir, false});
                QDirIterator it(dir, QDir::Dirs | QDir::NoDotAndDotDot | QDir::Hidden, QDirIterator::Subdirectories);
                while (it.hasNext()) {
                    const QString sub = it.next();
                    if (!QFileInfo(sub).isSymLink())
                        next.append({sub, false});
                }
                continue;
            }
            if (!hasWildcard(segment)) {
                QString child = prefix + u'/' + segment;
                if (!exists(child) && caseBlind) {
                    const QString found = caseInsensitiveChild(dir, segment);
                    if (found.isEmpty())
                        continue;
                    child = prefix + u'/' + found;
                } else if (!exists(child)) {
                    continue;
                }
                next.append({child, false});
                continue;
            }
            if (!QFileInfo(dir).isDir())
                continue;
            const QRegularExpression re =
                QRegularExpression::fromWildcard(segment, caseBlind ? Qt::CaseInsensitive : Qt::CaseSensitive);
            const QStringList names = QDir(dir).entryList(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System);
            for (const QString &name : names) {
                if (!re.match(name).hasMatch())
                    continue;
                const QString child = prefix + u'/' + name;
                next.append({child, QFileInfo(child).isSymLink() && i + 1 < segments.size()});
            }
        }
        current = next;
        if (current.isEmpty())
            break;
    }
    QStringList out;
    for (const auto &[path, viaLink] : std::as_const(current)) {
        if (path != QLatin1String("/"))
            out << path;
    }
    return out;
}

bool relativeEscape(const QString &path)
{
    return path.split(u'/').contains(QStringLiteral(".."));
}

void addPaths(QStringList &out, const QString &pattern, bool caseBlind)
{
    if (!pattern.startsWith(u'/') || relativeEscape(pattern))
        return;
    out << expandGlob(QDir::cleanPath(pattern), caseBlind);
}

QStringList withoutNested(QStringList paths)
{
    paths.removeDuplicates();
    std::sort(paths.begin(), paths.end());
    QStringList out;
    for (const QString &path : std::as_const(paths)) {
        if (!out.isEmpty() && path.startsWith(out.last() + u'/') && QFileInfo(out.last()).isDir()
            && !QFileInfo(out.last()).isSymLink())
            continue;
        out << path;
    }
    return out;
}
}

Env Env::system()
{
    Env env;
    env.home = QDir::homePath();
    const QString data = qEnvironmentVariable("XDG_DATA_HOME");
    const QString config = qEnvironmentVariable("XDG_CONFIG_HOME");
    env.xdgData = data.startsWith(u'/') ? data : env.home + QStringLiteral("/.local/share");
    env.xdgConfig = config.startsWith(u'/') ? config : env.home + QStringLiteral("/.config");
    env.user = qEnvironmentVariable("USER");
    return env;
}

Matcher::Matcher(const SavesIndex &index) : m_index(index)
{
    for (qsizetype i = 0; i < index.games.size(); ++i) {
        const SavesGame &game = index.games.at(i);
        if (game.steamId > 0)
            m_steam.insert(game.steamId, i);
        m_exact.insert(game.name, i);
        m_names.insert(SearchText::fold(game.name), i);
    }
    // Aliases never take a name from a game of its own.
    for (qsizetype i = 0; i < index.games.size(); ++i) {
        for (const QString &alias : index.games.at(i).aliases) {
            const QString key = SearchText::fold(alias);
            if (!key.isEmpty() && !m_names.contains(key))
                m_names.insert(key, i);
        }
    }
}

const SavesGame *Matcher::find(const Query &query, Match *how) const
{
    const auto done = [&](qsizetype index, Match match) -> const SavesGame * {
        if (how)
            *how = match;
        return &m_index.games.at(index);
    };
    if (how)
        *how = Match::None;
    if (!query.savesGame.isEmpty()) {
        if (const auto it = m_exact.constFind(query.savesGame); it != m_exact.cend())
            return done(*it, Match::Chosen);
        if (const auto it = m_names.constFind(SearchText::fold(query.savesGame)); it != m_names.cend())
            return done(*it, Match::Chosen);
        return nullptr;
    }
    if (query.steamAppId > 0) {
        if (const auto it = m_steam.constFind(query.steamAppId); it != m_steam.cend())
            return done(*it, Match::SteamId);
    }
    const QString title = SearchText::fold(query.title);
    if (!title.isEmpty()) {
        if (const auto it = m_names.constFind(title); it != m_names.cend())
            return done(*it, Match::Title);
    }
    return nullptr;
}

Found find(const Matcher &matcher, const QList<Query> &copies, const Env &env)
{
    Found found;
    if (copies.isEmpty())
        return found;
    found.gameId = copies.first().id;

    QStringList paths;
    for (const Query &query : copies) {
        Match how = Match::None;
        const SavesGame *game = matcher.find(query, &how);
        if (game && found.matched.isEmpty()) {
            found.matched = game->name;
            found.match = how;
        }

        Context ctx{env, query, game, !query.prefix.isEmpty(), {}, {}};
        if (ctx.windows) {
            ctx.drive = QDir::cleanPath(query.prefix) + QStringLiteral("/drive_c");
            ctx.user = ctx.drive + QStringLiteral("/users/") + kWineUser;
        }
        if (game) {
            for (const SavesPath &entry : game->paths) {
                const bool applies = entry.when.isEmpty()
                    || std::any_of(entry.when.cbegin(), entry.when.cend(), [&](const SavesWhen &w) {
                           return w.os.isEmpty() || w.os == (ctx.windows ? QLatin1String("windows") : QLatin1String("linux"));
                       });
                if (!applies)
                    continue;
                for (const QString &path : expand(entry.path, ctx)) {
                    if (!tooBroad(path, env, ctx))
                        addPaths(paths, path, ctx.windows);
                }
            }
        }
        QStringList own = query.savePaths;
        if (!query.saveFolder.isEmpty())
            own.prepend(query.saveFolder);
        for (const QString &path : std::as_const(own)) {
            const QString expanded = expandHome(path, env.home);
            if (!tooBroad(expanded, env, ctx))
                addPaths(paths, expanded, false);
        }
    }
    found.paths = withoutNested(paths);
    return found;
}
}
