#include "steamart.h"

#include <QDateTime>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QImageReader>
#include <QRegularExpression>
#include <QSet>
#include <QTimeZone>

#include <algorithm>

namespace SteamArt {
namespace {
QString gridName(const QString &kind, qint64 appId)
{
    const QString id = QString::number(appId);
    if (kind == u"cover")
        return id + u'p';
    return id + u'_' + kind;
}

// A library cache file's kind, and how good a match it is (lower is better); -1 if it's
// none of them. The icon is the 32×32 <sha1>.jpg.
std::pair<QString, int> cacheKind(const QString &fileName)
{
    static const QRegularExpression icon(QStringLiteral("^[0-9a-f]{40}\\.jpg$"));
    if (fileName == u"library_hero.jpg")
        return {QStringLiteral("hero"), 0};
    if (fileName == u"library_600x900.jpg")
        return {QStringLiteral("cover"), 0};
    if (fileName == u"library_capsule.jpg") // newer Steam's name for the same cover
        return {QStringLiteral("cover"), 1};
    if (fileName == u"logo.png")
        return {QStringLiteral("logo"), 0};
    if (icon.match(fileName).hasMatch())
        return {QStringLiteral("icon"), 0};
    return {{}, -1};
}
}

QHash<QString, QString> find(const SteamInstall &install, qint64 appId, const QStringList &kinds)
{
    QHash<QString, QString> out;
    if (!install.found() || appId <= 0 || kinds.isEmpty())
        return out;

    if (const QString user = install.userDataDir(); !user.isEmpty()) {
        const QString grid = user + QStringLiteral("/config/grid/");
        if (QFileInfo(grid).isDir()) {
            for (const QString &kind : kinds) {
                for (const char *ext : {".png", ".jpg"}) {
                    const QString path = grid + gridName(kind, appId) + QLatin1String(ext);
                    if (QFileInfo(path).isFile()) {
                        out.insert(kind, path);
                        break;
                    }
                }
            }
        }
    }
    if (out.size() == kinds.size())
        return out;

    struct Best
    {
        QString path;
        int rank = 99;
        qint64 modified = 0;
    };
    QHash<QString, Best> best;
    const QString cache = install.root + QStringLiteral("/appcache/librarycache/") + QString::number(appId);
    QDirIterator it(cache, QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const QFileInfo file = it.nextFileInfo();
        const auto [kind, rank] = cacheKind(file.fileName());
        if (rank < 0 || out.contains(kind) || !kinds.contains(kind))
            continue;
        // Steam keeps older versions in other hashed subfolders; the newest is current.
        const qint64 modified = file.lastModified(QTimeZone::UTC).toMSecsSinceEpoch();
        Best &b = best[kind];
        if (rank < b.rank || (rank == b.rank && modified > b.modified))
            b = {file.absoluteFilePath(), rank, modified};
    }
    for (auto b = best.cbegin(); b != best.cend(); ++b)
        out.insert(b.key(), b->path);
    return out;
}

QString gridDir(const SteamInstall &install)
{
    const QString user = install.userDataDir();
    return user.isEmpty() ? QString() : user + QStringLiteral("/config/grid");
}

QString gridIconPath(const SteamInstall &install, qint64 appId)
{
    const QString grid = gridDir(install);
    return grid.isEmpty() ? QString() : grid + u'/' + gridName(QStringLiteral("icon"), appId) + QStringLiteral(".png");
}

namespace {
struct Stamp
{
    bool exists = false;
    qint64 size = 0;
    qint64 modified = 0;
};

Stamp stampOf(const QString &path)
{
    const QFileInfo info(path);
    if (!info.isFile())
        return {};
    return {true, info.size(), info.lastModified(QTimeZone::UTC).toMSecsSinceEpoch()};
}

QString gridFileName(const QString &kind, qint64 appId, const QString &art)
{
    const QString ext = QFileInfo(art).suffix().toLower();
    const bool jpeg = kind != u"icon" && (ext == u"jpg" || ext == u"jpeg");
    return gridName(kind, appId) + (jpeg ? QStringLiteral(".jpg") : QStringLiteral(".png"));
}

QString writeGridFile(const QString &grid, const QString &name, const QString &art)
{
    const QString target = grid + u'/' + name;
    const QString temp = grid + QStringLiteral("/.") + name + QStringLiteral(".part");
    QFile::remove(temp);
    const QString ext = QFileInfo(art).suffix().toLower();
    if (!name.endsWith(QLatin1String(".png")) || ext == u"png") {
        if (!QFile::copy(art, temp))
            return QStringLiteral("could not copy %1 to %2").arg(art, temp);
    } else {
        const QImage image = QImageReader(art).read();
        if (image.isNull() || !image.save(temp, "PNG")) {
            QFile::remove(temp);
            return QStringLiteral("could not convert %1 to %2").arg(art, temp);
        }
    }
    // A rename never replaces a file: one Steam or the user put there since stays.
    if (!QFile::rename(temp, target)) {
        QFile::remove(temp);
        return QStringLiteral("could not write %1").arg(target);
    }
    return {};
}
}

QStringList writeGrid(const QString &gridDir, const QHash<qint64, QHash<QString, QString>> &art, GridFiles *written)
{
    QStringList log;
    if (gridDir.isEmpty() || (art.isEmpty() && written->isEmpty()))
        return log;
    const QStringList names = QDir(gridDir).entryList(QDir::Files);
    QSet<QString> present(names.cbegin(), names.cend());

    // Deletes one of the launcher's files if it's still as written; false when it isn't (it's
    // the user's now) or can't be deleted. A file already gone is fine.
    const auto removeOwn = [&](const GridFile &f) {
        const QString path = gridDir + u'/' + f.name;
        const Stamp now = stampOf(path);
        if (!now.exists)
            return true;
        if (now.size != f.size || now.modified != f.modified)
            return false;
        if (!QFile::remove(path)) {
            log.append(QStringLiteral("could not delete %1").arg(path));
            return false;
        }
        present.remove(f.name);
        log.append(QStringLiteral("deleted %1").arg(path));
        return true;
    };

    for (auto it = written->begin(); it != written->end();) {
        if (art.contains(it.key())) {
            ++it;
            continue;
        }
        for (const GridFile &f : std::as_const(*it))
            removeOwn(f);
        it = written->erase(it);
    }

    static const QStringList kinds = {QStringLiteral("hero"), QStringLiteral("cover"), QStringLiteral("logo"),
                                      QStringLiteral("icon")};
    static const QStringList exts = {QStringLiteral(".png"), QStringLiteral(".jpg"), QStringLiteral(".jpeg"),
                                     QStringLiteral(".webp")};
    for (auto game = art.cbegin(); game != art.cend(); ++game) {
        const qint64 appId = game.key();
        QHash<QString, GridFile> mine = written->value(appId);
        for (const QString &kind : kinds) {
            const QString source = game->value(kind);
            if (const auto f = mine.constFind(kind); f != mine.cend()) {
                const GridFile own = *f;
                const Stamp now = source.isEmpty() ? Stamp() : stampOf(source);
                // As written, or deleted in Steam: left as it is until the art changes.
                if (now.exists && QFileInfo(source).fileName() == own.art && now.size == own.artSize
                    && now.modified == own.artModified)
                    continue;
                mine.remove(kind);
                if (!removeOwn(own))
                    continue;
            }
            if (source.isEmpty())
                continue;
            const QString base = gridName(kind, appId);
            if (std::any_of(exts.cbegin(), exts.cend(), [&](const QString &ext) { return present.contains(base + ext); }))
                continue;

            const QString name = gridFileName(kind, appId, source);
            QDir().mkpath(gridDir);
            if (const QString error = writeGridFile(gridDir, name, source); !error.isEmpty()) {
                log.append(error);
                continue;
            }
            const Stamp file = stampOf(gridDir + u'/' + name);
            const Stamp from = stampOf(source);
            mine.insert(kind, {name, file.size, file.modified, QFileInfo(source).fileName(), from.size, from.modified});
            present.insert(name);
            log.append(QStringLiteral("wrote %1/%2 from %3").arg(gridDir, name, source));
        }
        if (mine.isEmpty())
            written->remove(appId);
        else
            written->insert(appId, mine);
    }
    return log;
}
}
