#include "desktopentry.h"

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
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    for (const char *name : {"APPIMAGE", "APPDIR", "ARGV0", "OWD"})
        env.remove(QString::fromLatin1(name));
    process.setProcessEnvironment(env);
    process.setWorkingDirectory(folder);
    process.start(appImage, {QStringLiteral("--appimage-extract"), pattern});
    if (!process.waitForFinished(ExtractTimeoutMs)) {
        process.kill();
        process.waitForFinished(1000);
        return false;
    }
    return process.exitStatus() == QProcess::NormalExit && process.exitCode() == 0;
}

QString iconNameOf(const QByteArray &source)
{
    for (const QByteArray &line : source.split('\n')) {
        if (line.startsWith("Icon="))
            return QString::fromUtf8(line.mid(5)).trimmed();
    }
    return {};
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

Made make(const QString &id, const QString &appImage, const QString &dataDir, QString *error)
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
    if (!iconName.isEmpty() && !QFileInfo(iconName).isAbsolute()) {
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
    QSaveFile out(target);
    const QByteArray bytes = rewrite(original, appImage, made.icon);
    if (!QDir().mkpath(applications) || !out.open(QIODevice::WriteOnly) || out.write(bytes) != bytes.size()
        || !out.commit()) {
        *error = QStringLiteral("couldn't write %1: %2").arg(target, out.errorString());
        remove(made);
        return {};
    }
    made.desktop = target;
    return made;
}

void remove(const Made &made)
{
    if (!made.desktop.isEmpty())
        QFile::remove(made.desktop);
    if (!made.icon.isEmpty())
        QFile::remove(made.icon);
}
}
