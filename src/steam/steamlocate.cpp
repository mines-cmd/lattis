#include "steamlocate.h"

#include "vdf.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSocketNotifier>

#include <sys/syscall.h>
#include <unistd.h>

namespace {
constexpr quint64 kSteamId64Base = 76561197960265728ULL;

QString canonical(const QString &path)
{
    return QFileInfo(path).canonicalFilePath();
}

bool isSteamFolder(const QString &path)
{
    return !path.isEmpty() && QFileInfo::exists(path + QStringLiteral("/steamapps/libraryfolders.vdf"));
}

QString expandHome(const QString &path, const QString &home)
{
    if (path == u'~')
        return home;
    if (path.startsWith(QStringLiteral("~/")))
        return home + path.mid(1);
    return path;
}

QString processExe(qint64 pid)
{
    QString exe = QFile::symLinkTarget(QStringLiteral("/proc/%1/exe").arg(pid));
    // Steam replaces its binary when it updates itself.
    if (exe.endsWith(QStringLiteral(" (deleted)")))
        exe.chop(10);
    return exe;
}

QString steamExe(const QString &root)
{
    return root + QStringLiteral("/ubuntu12_32/steam");
}
}

QString SteamInstall::userDataDir() const
{
    return userId.isEmpty() ? QString() : root + QStringLiteral("/userdata/") + userId;
}

SteamLocations SteamLocations::system()
{
    return {QDir::homePath()};
}

QString SteamLocations::nativeLink() const
{
    return home + QStringLiteral("/.steam/steam");
}

QString SteamLocations::flatpakRoot() const
{
    return home + QStringLiteral("/.var/app/com.valvesoftware.Steam/.local/share/Steam");
}

namespace SteamLocate {
QString accountId(const QString &steamId64)
{
    bool ok = false;
    const quint64 id = steamId64.toULongLong(&ok);
    if (!ok || id <= kSteamId64Base)
        return {};
    return QString::number(id - kSteamId64Base);
}

QList<SteamUser> readUsers(const QString &root)
{
    QFile file(root + QStringLiteral("/config/loginusers.vdf"));
    if (!file.open(QIODevice::ReadOnly))
        return {};
    const Vdf::Document doc = Vdf::parse(file.readAll());
    if (!doc.ok) {
        qCWarning(lcSteam) << file.fileName() << doc.error;
        return {};
    }
    QList<SteamUser> users;
    const Vdf::Node *list = doc.root.child(u"users");
    if (!list)
        return {};
    for (const Vdf::Node &node : list->children) {
        SteamUser user;
        user.steamId64 = node.key;
        user.accountId = accountId(node.key);
        if (user.accountId.isEmpty() || !node.isBlock)
            continue;
        user.personaName = node.valueOf(u"PersonaName");
        user.accountName = node.valueOf(u"AccountName");
        user.mostRecent = node.valueOf(u"MostRecent") == u'1';
        user.timestamp = node.valueOf(u"Timestamp").toLongLong();
        users.append(user);
    }
    return users;
}

SteamInstall find(const SteamLocations &where, const QString &configuredPath,
                  const QString &configuredUser)
{
    SteamInstall install;
    const QString flatpak = canonical(where.flatpakRoot());
    if (!configuredPath.isEmpty()) {
        const QString root = canonical(expandHome(configuredPath, where.home));
        if (!isSteamFolder(root)) {
            install.pathMissing = true;
            return install;
        }
        install.root = root;
        install.kind = !flatpak.isEmpty() && (root == flatpak || root.startsWith(flatpak + u'/'))
                           ? SteamInstall::Kind::Flatpak
                           : SteamInstall::Kind::Native;
    } else if (const QString native = canonical(where.nativeLink()); isSteamFolder(native)) {
        install.root = native;
        install.kind = SteamInstall::Kind::Native;
    } else if (isSteamFolder(flatpak)) {
        install.root = flatpak;
        install.kind = SteamInstall::Kind::Flatpak;
    } else {
        return install;
    }

    install.users = readUsers(install.root);
    const auto hasUserData = [&](const QString &id) {
        return !id.isEmpty() && QFileInfo(install.root + QStringLiteral("/userdata/") + id).isDir();
    };
    if (!configuredUser.isEmpty()) {
        // The account id, or the SteamID64 people copy from their profile.
        const QString id = accountId(configuredUser).isEmpty() ? configuredUser : accountId(configuredUser);
        if (hasUserData(id)) {
            install.userId = id;
            return install;
        }
        install.userMissing = true;
    }

    const SteamUser *best = nullptr;
    for (const SteamUser &user : std::as_const(install.users)) {
        if (!hasUserData(user.accountId))
            continue;
        if (!best || (user.mostRecent && !best->mostRecent)
            || (user.mostRecent == best->mostRecent && user.timestamp > best->timestamp))
            best = &user;
    }
    if (best) {
        install.userId = best->accountId;
    } else {
        const QStringList ids = QDir(install.root + QStringLiteral("/userdata"))
                                    .entryList(QDir::Dirs | QDir::NoDotAndDotDot);
        QStringList accounts;
        for (const QString &id : ids) {
            if (id != u'0' && id.toULongLong() > 0)
                accounts.append(id);
        }
        if (accounts.size() == 1)
            install.userId = accounts.first();
    }
    return install;
}

qint64 runningPid(const SteamInstall &install, const SteamLocations &where)
{
    if (!install.found())
        return 0;
    const QString exe = canonical(steamExe(install.root));
    if (exe.isEmpty())
        return 0;

    if (install.kind == SteamInstall::Kind::Native) {
        QFile file(where.home + QStringLiteral("/.steam/steam.pid"));
        if (!file.open(QIODevice::ReadOnly))
            return 0;
        const qint64 pid = file.readAll().trimmed().toLongLong();
        return pid > 0 && processExe(pid) == exe ? pid : 0;
    }

    const QStringList pids = QDir(QStringLiteral("/proc")).entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QString &name : pids) {
        bool ok = false;
        const qint64 pid = name.toLongLong(&ok);
        if (ok && processExe(pid) == exe)
            return pid;
    }
    return 0;
}

qint64 gamePid(quint64 appId, const QString &procDir)
{
    if (appId == 0)
        return 0;
    const QByteArray wanted = "AppId=" + QByteArray::number(appId);
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
        // Only up to "--": what follows is the game's own command line.
        const qsizetype end = args.indexOf("--");
        const QList<QByteArray> steams = end < 0 ? args : args.mid(0, end);
        if (steams.contains("SteamLaunch") && steams.contains(wanted))
            return pid;
    }
    return 0;
}
}

SteamExitWatch::SteamExitWatch(QObject *parent)
    : QObject(parent)
{
}

SteamExitWatch::~SteamExitWatch()
{
    stop();
}

bool SteamExitWatch::watch(qint64 pid)
{
    stop();
    m_pidfd = int(::syscall(SYS_pidfd_open, pid_t(pid), 0));
    if (m_pidfd < 0)
        return false;
    m_notifier = new QSocketNotifier(m_pidfd, QSocketNotifier::Read, this);
    connect(m_notifier, &QSocketNotifier::activated, this, [this] {
        stop();
        emit exited();
    });
    return true;
}

void SteamExitWatch::stop()
{
    if (m_notifier) {
        m_notifier->setEnabled(false);
        m_notifier->deleteLater();
        m_notifier = nullptr;
    }
    if (m_pidfd >= 0) {
        ::close(m_pidfd);
        m_pidfd = -1;
    }
}
