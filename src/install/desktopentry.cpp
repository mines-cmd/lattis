#include "desktopentry.h"

#include "launchcommand.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QProcessEnvironment>
#include <QSaveFile>
#include <QTemporaryDir>

namespace DesktopEntry {
namespace {
constexpr int ExtractTimeoutMs = 20000;

QString quoted(const QString &path)
{
    QString out = QStringLiteral("\"");
    for (const QChar c : path) {
        if (c == u'"' || c == u'\\' || c == u'`' || c == u'$')
            out += u'\\';
        out += c;
    }
    return out + u'"';
}

bool extract(const QString &appImage, const QString &pattern, const QString &folder)
{
    QProcess process;
    process.setProcessEnvironment(childEnvironment());
    process.setWorkingDirectory(folder);
    process.start(appImage, {QStringLiteral("--appimage-extract"), pattern});
    if (!process.waitForFinished(ExtractTimeoutMs)) {
        process.kill();
        process.waitForFinished(1000);
        return false;
    }
    return process.exitStatus() == QProcess::NormalExit && process.exitCode() == 0;
}

constexpr QByteArrayView kUserCopyMark = "X-Lattis-Icon-Copy=true";

QString iconNameOf(const QByteArray &source)
{
    for (const QByteArray &line : source.split('\n')) {
        if (line.startsWith("Icon="))
            return QString::fromUtf8(line.mid(5)).trimmed();
    }
    return {};
}

bool writeFile(const QString &path, const QByteArray &bytes, QString *error)
{
    if (!QDir().mkpath(QFileInfo(path).path())) {
        *error = QStringLiteral("couldn't make %1").arg(QFileInfo(path).path());
        return false;
    }
    QSaveFile out(path);
    if (out.open(QIODevice::WriteOnly) && out.write(bytes) == bytes.size() && out.commit())
        return true;
    *error = QStringLiteral("couldn't write %1: %2").arg(path, out.errorString());
    return false;
}

void removeExtractedIcon(const QString &dataDir)
{
    const QDir icons(QDir(dataDir).filePath(QStringLiteral("icons/lattis")));
    for (const QString &name : icons.entryList({QStringLiteral("lattis.*")}, QDir::Files))
        QFile::remove(icons.filePath(name));
}

// Many AppImages keep the root .desktop and .DirIcon as links into usr/, which the first extraction leaves dangling.
QString firstRootFile(const QString &appImage, const QString &work, const QStringList &filters)
{
    const QString root = QDir(work).filePath(QStringLiteral("squashfs-root"));
    const QStringList found = QDir(root).entryList(filters, QDir::Files | QDir::System | QDir::Hidden, QDir::Name);
    if (found.isEmpty())
        return {};
    QString path = QDir(root).filePath(found.first());
    for (int hops = 0; hops < 4; ++hops) {
        const QFileInfo info(path);
        if (!info.isSymLink())
            return info.isFile() ? path : QString();
        const QString next = QDir::cleanPath(info.dir().filePath(info.symLinkTarget()));
        if (!next.startsWith(root + u'/'))
            return {};
        if (!QFileInfo::exists(next) && !extract(appImage, next.mid(root.size() + 1), work))
            return {};
        path = next;
    }
    return {};
}
}

QByteArray rewrite(const QByteArray &source, const QString &appImage, const QString &icon)
{
    QByteArrayList out;
    bool iconDone = false;
    bool inMain = false;
    for (QByteArray line : source.split('\n')) {
        if (line.endsWith('\r'))
            line.chop(1);
        if (line.startsWith('[')) {
            if (inMain && !iconDone && !icon.isEmpty()) {
                out.append("Icon=" + icon.toUtf8());
                iconDone = true;
            }
            inMain = line == "[Desktop Entry]";
        } else if (line.startsWith("TryExec=")) {
            continue;
        } else if (line.startsWith("Exec=")) {
            const QByteArray rest = line.mid(5).trimmed();
            const qsizetype space = rest.startsWith('"') ? rest.indexOf('"', 1) + 1 : rest.indexOf(' ');
            line = "Exec=" + quoted(appImage).toUtf8() + (space > 0 && space < rest.size() ? rest.mid(space) : QByteArray());
        } else if (inMain && line.startsWith("Icon=") && !icon.isEmpty()) {
            line = "Icon=" + icon.toUtf8();
            iconDone = true;
        } else if (line.startsWith("X-AppImage-")) {
            continue;
        }
        out.append(line);
    }
    while (!out.isEmpty() && out.last().isEmpty())
        out.removeLast();
    if (!iconDone && !icon.isEmpty()) {
        const qsizetype group = out.indexOf("[Desktop Entry]");
        if (group >= 0)
            out.insert(group + 1, "Icon=" + icon.toUtf8());
    }
    return out.join('\n') + '\n';
}

Made make(const QString &id, const QString &appImage, const QString &dataDir, QString *error, const QString &icon)
{
    if (dataDir.isEmpty())
        return {};
    QTemporaryDir work;
    if (!work.isValid()) {
        *error = QStringLiteral("no temporary folder");
        return {};
    }
    if (!extract(appImage, QStringLiteral("*.desktop"), work.path())) {
        *error = QStringLiteral("couldn't read the menu entry out of %1").arg(QFileInfo(appImage).fileName());
        return {};
    }
    const QString source = firstRootFile(appImage, work.path(), {QStringLiteral("*.desktop")});
    QFile file(source);
    if (source.isEmpty() || !file.open(QIODevice::ReadOnly)) {
        *error = QStringLiteral("%1 has no .desktop file").arg(QFileInfo(appImage).fileName());
        return {};
    }
    const QByteArray original = file.readAll();

    Made made;
    const QString iconName = iconNameOf(original);
    if (icon.isEmpty() && !iconName.isEmpty() && !QFileInfo(iconName).isAbsolute()) {
        extract(appImage, iconName + QStringLiteral(".*"), work.path());
        QString icon = firstRootFile(appImage, work.path(), {iconName + QStringLiteral(".svg"), iconName + QStringLiteral(".png"),
                                            iconName + QStringLiteral(".xpm")});
        if (icon.isEmpty() && extract(appImage, QStringLiteral(".DirIcon"), work.path()))
            icon = firstRootFile(appImage, work.path(), {QStringLiteral(".DirIcon")});
        if (!icon.isEmpty() && QFileInfo(icon).isFile()) {
            QString suffix = QFileInfo(icon).canonicalFilePath().section(u'.', -1);
            if (suffix.isEmpty())
                suffix = QStringLiteral("png");
            const QString folder = QDir(dataDir).filePath(QStringLiteral("icons/lattis"));
            const QString target = QDir(folder).filePath(id + u'.' + suffix);
            if (QDir().mkpath(folder) && (!QFile::exists(target) || QFile::remove(target)) && QFile::copy(icon, target))
                made.icon = target;
        }
    }

    const QString applications = QDir(dataDir).filePath(QStringLiteral("applications"));
    const QString target = QDir(applications).filePath(QStringLiteral("lattis-%1.desktop").arg(id));
    if (!writeFile(target, rewrite(original, appImage, icon.isEmpty() ? made.icon : icon), error)) {
        remove(made);
        return {};
    }
    made.desktop = target;
    return made;
}

QByteArray withIcon(const QByteArray &source, const QString &icon)
{
    QByteArrayList out;
    bool inMain = false;
    bool iconDone = false;
    for (QByteArray line : source.split('\n')) {
        if (line.endsWith('\r'))
            line.chop(1);
        if (line.startsWith('[')) {
            inMain = line == "[Desktop Entry]";
        } else if (inMain && line.startsWith("Icon=")) {
            if (iconDone)
                continue;
            line = "Icon=" + icon.toUtf8();
            iconDone = true;
        } else if (line == kUserCopyMark) {
            continue;
        }
        out.append(line);
        if (inMain && line == "[Desktop Entry]")
            out.append(kUserCopyMark.toByteArray());
    }
    while (!out.isEmpty() && out.last().isEmpty())
        out.removeLast();
    if (!iconDone) {
        const qsizetype group = out.indexOf("[Desktop Entry]");
        if (group < 0)
            return {};
        out.insert(group + 1, "Icon=" + icon.toUtf8());
    }
    return out.join('\n') + '\n';
}

Made ensureOwn(const QString &appImage, const QString &dataDir, const QString &icon, QString *error)
{
    const QString id = QStringLiteral("lattis");
    const QString path = QDir(dataDir).filePath(QStringLiteral("applications/lattis-%1.desktop").arg(id));
    QFile existing(path);
    if (existing.open(QIODevice::ReadOnly)) {
        const QByteArray bytes = existing.readAll();
        existing.close();
        if (bytes.contains(("Exec=" + quoted(appImage)).toUtf8())) {
            if (icon.isEmpty() || iconNameOf(bytes) == icon)
                return {};
            // Only the icon differs: no need to read the AppImage again.
            if (!writeFile(path, rewrite(bytes, appImage, icon), error))
                return {};
            removeExtractedIcon(dataDir);
            return {path, {}};
        }
    }
    Made made = make(id, appImage, dataDir, error, icon);
    if (!made.desktop.isEmpty() && !icon.isEmpty())
        removeExtractedIcon(dataDir);
    return made;
}

void removeOwn(const QString &dataDir)
{
    QFile::remove(QDir(dataDir).filePath(QStringLiteral("applications/lattis-lattis.desktop")));
    removeExtractedIcon(dataDir);
}

QString userCopyPath(const QString &dataDir)
{
    return QDir(dataDir).filePath(QStringLiteral("applications/lattis.desktop"));
}

bool isUserCopy(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return false;
    for (const QByteArray &line : file.readAll().split('\n')) {
        if (line.trimmed() == kUserCopyMark)
            return true;
    }
    return false;
}

QString systemEntry(const QStringList &dataDirs)
{
    for (const QString &dir : dataDirs) {
        const QString path = QDir(dir).filePath(QStringLiteral("applications/lattis.desktop"));
        if (QFileInfo(path).isFile() && !isUserCopy(path))
            return path;
    }
    return {};
}

bool ensureUserCopy(const QString &systemEntry, const QString &dataDir, const QString &icon, QString *error)
{
    const QString target = userCopyPath(dataDir);
    if (QFileInfo::exists(target) && !isUserCopy(target)) {
        *error = QStringLiteral("%1 wasn't written by Lattis").arg(target);
        return false;
    }
    QFile source(systemEntry);
    if (!source.open(QIODevice::ReadOnly)) {
        *error = QStringLiteral("can't read %1").arg(systemEntry);
        return false;
    }
    const QByteArray bytes = withIcon(source.readAll(), icon);
    if (bytes.isEmpty()) {
        *error = QStringLiteral("%1 has no [Desktop Entry] group").arg(systemEntry);
        return false;
    }
    QFile existing(target);
    if (existing.open(QIODevice::ReadOnly) && existing.readAll() == bytes)
        return true;
    existing.close();
    return writeFile(target, bytes, error);
}

void removeUserCopy(const QString &dataDir)
{
    const QString path = userCopyPath(dataDir);
    if (isUserCopy(path))
        QFile::remove(path);
}

void remove(const Made &made)
{
    if (!made.desktop.isEmpty())
        QFile::remove(made.desktop);
    if (!made.icon.isEmpty())
        QFile::remove(made.icon);
}
}
