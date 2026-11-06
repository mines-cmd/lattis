#include "heroiclocate.h"

#include "heroicscan.h"

#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>
#include <QUrl>

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
}
