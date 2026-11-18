#include "heroiclocate.h"

#include "heroicscan.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QSysInfo>
#include <QUrl>

#include <algorithm>

namespace {
const QString kFlatpakId = QStringLiteral("com.heroicgameslauncher.hgl");

QString canonical(const QString &path)
{
    return QFileInfo(path).canonicalFilePath();
}

// Starting Heroic once leaves a root without either file, so a folder alone says nothing.
bool isHeroicFolder(const QString &path)
{
    return !path.isEmpty()
           && (QFileInfo::exists(HeroicScan::libraryPath(path)) || QFileInfo::exists(HeroicScan::installedPath(path)));
}

QString expandHome(const QString &path, const QString &home)
{
    if (path == u'~')
        return home;
    if (path.startsWith(QStringLiteral("~/")))
        return home + path.mid(1);
    return path;
}

HeroicProgram nativeProgram(const HeroicLocations &where)
{
    const QString heroic = QStandardPaths::findExecutable(QStringLiteral("heroic"), where.pathDirs);
    if (heroic.isEmpty())
        return {};
    return {heroic, {QStringLiteral("--no-gui")}, true};
}

HeroicProgram flatpakProgram(const HeroicLocations &where)
{
    const QString flatpak = QStandardPaths::findExecutable(QStringLiteral("flatpak"), where.pathDirs);
    if (flatpak.isEmpty())
        return {};
    for (const QString &dir : where.flatpakDirs) {
        // `current` links to the active branch; an app being uninstalled has none.
        if (QFileInfo::exists(dir + u'/' + kFlatpakId + QStringLiteral("/current")))
            return {flatpak, {QStringLiteral("run"), kFlatpakId, QStringLiteral("--no-gui")}, true};
    }
    return {};
}
}

HeroicLocations HeroicLocations::system()
{
    HeroicLocations where;
    where.home = QDir::homePath();
    where.configHome = qEnvironmentVariable("XDG_CONFIG_HOME", where.home + QStringLiteral("/.config"));
    where.pathDirs = qEnvironmentVariable("PATH").split(u':', Qt::SkipEmptyParts);
    where.flatpakDirs = {
        qEnvironmentVariable("FLATPAK_USER_DIR", where.home + QStringLiteral("/.local/share/flatpak"))
            + QStringLiteral("/app"),
        qEnvironmentVariable("FLATPAK_SYSTEM_DIR", QStringLiteral("/var/lib/flatpak")) + QStringLiteral("/app"),
    };
    return where;
}

QString HeroicLocations::nativeRoot() const
{
    return configHome + QStringLiteral("/heroic");
}

QString HeroicLocations::flatpakRoot() const
{
    return home + QStringLiteral("/.var/app/") + kFlatpakId + QStringLiteral("/config/heroic");
}

namespace HeroicLocate {
HeroicProgram findProgram(HeroicInstall::Kind kind, const HeroicLocations &where)
{
    // The root's own program comes first: each one reads only its own root.
    const bool flatpakFirst = kind == HeroicInstall::Kind::Flatpak;
    HeroicProgram program = flatpakFirst ? flatpakProgram(where) : nativeProgram(where);
    if (!program.found())
        program = flatpakFirst ? nativeProgram(where) : flatpakProgram(where);
    if (!program.found())
        program.program = QStandardPaths::findExecutable(QStringLiteral("xdg-open"), where.pathDirs);
    return program;
}

HeroicInstall find(const HeroicLocations &where, const QString &configuredPath)
{
    HeroicInstall install;
    const QString flatpak = canonical(where.flatpakRoot());
    if (!configuredPath.isEmpty()) {
        const QString root = canonical(expandHome(configuredPath, where.home));
        if (!isHeroicFolder(root)) {
            install.pathMissing = true;
            return install;
        }
        install.root = root;
        install.kind = !flatpak.isEmpty() && (root == flatpak || root.startsWith(flatpak + u'/'))
                           ? HeroicInstall::Kind::Flatpak
                           : HeroicInstall::Kind::Native;
    } else if (const QString native = canonical(where.nativeRoot()); isHeroicFolder(native)) {
        install.root = native;
        install.kind = HeroicInstall::Kind::Native;
    } else if (isHeroicFolder(flatpak)) {
        install.root = flatpak;
        install.kind = HeroicInstall::Kind::Flatpak;
    } else {
        // Without xdg-open, which any desktop has: this is what tells Heroic with no library yet from no Heroic.
        install.program = nativeProgram(where);
        if (!install.program.found())
            install.program = flatpakProgram(where);
        return install;
    }
    install.program = findProgram(install.kind, where);
    return install;
}

QString findLegendary(const HeroicLocations &where)
{
    const QString arch = QSysInfo::currentCpuArchitecture() == QLatin1String("arm64") ? QStringLiteral("arm64")
                                                                                      : QStringLiteral("x64");
    const QString relative = QStringLiteral("/resources/app.asar.unpacked/build/bin/%1/linux/legendary").arg(arch);
    QStringList roots = {QStringLiteral("/usr/lib64/heroic"), QStringLiteral("/usr/lib/heroic"),
                         QStringLiteral("/opt/Heroic")};
    // `heroic` on the PATH is usually a link into the install folder.
    if (const QString heroic = canonical(QStandardPaths::findExecutable(QStringLiteral("heroic"), where.pathDirs));
        !heroic.isEmpty())
        roots.prepend(QFileInfo(heroic).absolutePath());
    for (const QString &root : std::as_const(roots)) {
        if (QFileInfo(root + relative).isExecutable())
            return root + relative;
    }
    return QStandardPaths::findExecutable(QStringLiteral("legendary"), where.pathDirs);
}

QStringList launchArgs(const HeroicProgram &program, const QString &appName)
{
    const QByteArray app = QUrl::toPercentEncoding(appName);
    return program.args
           + QStringList{QStringLiteral("heroic://launch?appName=%1&runner=legendary&gui=false").arg(QString::fromLatin1(app))};
}

QStringList openArgs(const HeroicProgram &program)
{
    // xdg-open needs a URL, and a ping is the one that does nothing but start Heroic or show it.
    if (!program.noGui)
        return {QStringLiteral("heroic://ping")};
    QStringList args = program.args;
    args.removeAll(QStringLiteral("--no-gui"));
    return args;
}

qint64 gamePid(const QString &appName, const QString &procDir)
{
    if (appName.isEmpty())
        return 0;
    const QByteArray wanted = appName.toUtf8();
    const QByteArray epicArg = "-epicapp=" + wanted;
    qint64 found = 0;
    const QStringList pids = QDir(procDir).entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QString &name : pids) {
        bool ok = false;
        const qint64 pid = name.toLongLong(&ok);
        if (!ok)
            continue;
        QFile file(procDir + u'/' + name + QStringLiteral("/cmdline"));
        if (!file.open(QIODevice::ReadOnly))
            continue;
        const QList<QByteArray> args = file.readAll().split('\0');
        // Epic games get -epicapp=<appName> on the command line of every process under the runner. The lowest
        // pid is the one that started the rest, and killing it takes them all.
        if ((args.contains(epicArg) || (args.contains("launch") && args.contains(wanted)))
            && (found == 0 || pid < found))
            found = pid;
    }
    return found;
}

bool running(const QString &procDir)
{
    return !mainPids(procDir, 1).isEmpty();
}

QList<qint64> mainPids(const QString &procDir, int limit)
{
    QList<qint64> out;
    const QStringList pids = QDir(procDir).entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QString &name : pids) {
        bool ok = false;
        const qint64 pid = name.toLongLong(&ok);
        if (!ok)
            continue;
        QFile file(procDir + u'/' + name + QStringLiteral("/cmdline"));
        if (!file.open(QIODevice::ReadOnly))
            continue;
        const QList<QByteArray> args = file.readAll().split('\0');
        if (args.first().isEmpty() || QFileInfo(QString::fromLocal8Bit(args.first())).fileName() != u"heroic")
            continue;
        const bool helper = std::any_of(args.cbegin(), args.cend(),
                                        [](const QByteArray &a) { return a.startsWith("--type="); });
        if (!helper) {
            out.append(pid);
            if (limit > 0 && out.size() >= limit)
                break;
        }
    }
    return out;
}
}
