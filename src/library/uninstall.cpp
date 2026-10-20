#include "uninstall.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>

namespace Uninstall {
namespace {
QString tr(const char *text)
{
    return QCoreApplication::translate("Uninstall", text);
}

QString real(const QString &path)
{
    const QFileInfo info(path);
    const QString canonical = info.canonicalFilePath();
    return canonical.isEmpty() ? QDir::cleanPath(info.absoluteFilePath()) : canonical;
}

bool below(const QString &path, const QString &folder)
{
    return folder == u'/' ? path != folder : path.startsWith(folder + u'/');
}
}

QString refusal(const QString &folder, const Places &places)
{
    if (folder.isEmpty())
        return tr("It has no folder");
    if (QFileInfo(folder).isSymLink())
        return tr("It's a link to another folder");
    const QString f = real(folder);
    const QString home = real(places.home);
    if (f == u'/')
        return tr("That's the root folder");
    if (f == home)
        return tr("That's your home folder");
    for (const QString &dir : places.userDirs) {
        if (!dir.isEmpty() && real(dir) != home && f == real(dir))
            return tr("That's your %1 folder").arg(QFileInfo(f).fileName());
    }
    if (const QString data = real(places.dataDir); f == data || below(data, f))
        return tr("It holds the launcher's library");

    QStringList libraries;
    for (const QString &l : places.steamLibraries)
        libraries.append(real(l));
    const QString steamRoot = places.steamRoot.isEmpty() ? QString() : real(places.steamRoot);
    if (!steamRoot.isEmpty() && !libraries.contains(steamRoot))
        libraries.append(steamRoot);
    bool inLibrary = false;
    for (const QString &library : std::as_const(libraries)) {
        if (f == library || below(library, f))
            return library == steamRoot ? tr("It holds Steam") : tr("It holds a Steam library");
        if (!below(f, library))
            continue;
        // A game's or a prefix's folder in steamapps/ (common/<game>, compatdata/<id>); Steam's
        // own folders beside it only in a library of the user's.
        const QStringList parts = QDir(library).relativeFilePath(f).split(u'/');
        if (parts.first() == QLatin1String("steamapps") ? parts.size() < 3 : library == steamRoot)
            return tr("It's one of Steam's own folders");
        inLibrary = true;
    }

    for (const auto &[title, path] : places.otherGames) {
        if (const QString p = real(path); p == f || below(p, f))
            return tr("It holds %1's files too").arg(title);
    }
    if (!inLibrary && !below(f, home))
        return tr("It's outside your home folder and Steam's libraries");
    return {};
}

bool systemTrash(const QString &path, QString *error)
{
    QFile file(path);
    if (file.moveToTrash())
        return true;
    *error = file.errorString();
    return false;
}

Removal remove(const QString &folder, bool permanent, const Trash &trash)
{
    const QFileInfo info(folder);
    if (!info.exists() && !info.isSymLink())
        return {};
    if (!QFileInfo(info.absolutePath()).isWritable())
        return {tr("%1 can't be moved: its parent folder is read-only").arg(folder)};
    QString why;
    if (trash(folder, &why))
        return {};
    if (!permanent)
        return {why, true};
    if (info.isFile() || info.isSymLink() ? !QFile::remove(folder) : !QDir(folder).removeRecursively())
        return {tr("Could not delete all of %1").arg(folder)};
    return {};
}
}
