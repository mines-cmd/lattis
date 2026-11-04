#include "steamstatus.h"

#include "vdf.h"

#include <QCoreApplication>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUrl>

#include <sys/stat.h>

namespace {
constexpr quint64 kSteamId64Base = 76561197960265728ULL;
constexpr int kDebounceMs = 200;
constexpr int kPollMs = 5000;

QString nameOf(int persona)
{
    switch (persona) {
    case 1:
        return QStringLiteral("online");
    case 7:
        return QStringLiteral("invisible");
    // 3 is Away; 2 (Busy), 4 (Snooze), 5 and 6 (looking to trade or play) can be left by older clients
    // or other devices.
    case 2:
    case 3:
    case 4:
    case 5:
    case 6:
        return QStringLiteral("away");
    default:
        return {};
    }
}
}

SteamStatus::Source &SteamStatus::pending()
{
    static Source source;
    return source;
}

SteamStatus *SteamStatus::instance()
{
    static SteamStatus *status = [] {
        auto *created = new SteamStatus(pending(), QCoreApplication::instance());
        created->start();
        return created;
    }();
    return status;
}

SteamStatus::SteamStatus(const Source &source, QObject *parent)
    : QObject(parent)
    , m_source(source)
{
    m_debounce.setSingleShot(true);
    m_debounce.setInterval(kDebounceMs);
    connect(&m_debounce, &QTimer::timeout, this, [this] { refresh(true); });
    // Steam replaces the file on every save, which drops the file watch; the folder's watch sees the
    // new file arrive.
    connect(&m_watcher, &QFileSystemWatcher::fileChanged, this, [this] { m_debounce.start(); });
    connect(&m_watcher, &QFileSystemWatcher::directoryChanged, this, [this] { m_debounce.start(); });
    // Nothing else signals Steam starting or closing, or the account changing.
    m_poll.setInterval(kPollMs);
    connect(&m_poll, &QTimer::timeout, this, [this] { refresh(false); });
}

void SteamStatus::start()
{
    if (m_started)
        return;
    m_started = true;
    refresh(true);
    m_poll.start();
}

void SteamStatus::refresh(bool readFile)
{
    const QString oldStatus = status();
    const QString oldAvatar = m_avatar;
    const bool oldRunning = m_running;

    m_running = m_source.running && m_source.running();
    const SteamInstall install = m_source.install ? m_source.install() : SteamInstall();
    const QString dir = install.found() && !install.userId.isEmpty()
                            ? install.userDataDir() + QStringLiteral("/config")
                            : QString();
    const QString path = dir.isEmpty() ? QString() : dir + QStringLiteral("/localconfig.vdf");
    const QString avatarPath = path.isEmpty() ? QString()
                                              : install.root + QStringLiteral("/config/avatarcache/")
                                                    + QString::number(kSteamId64Base + install.userId.toULongLong())
                                                    + QStringLiteral(".png");
    if (path != m_path || avatarPath != m_avatarPath) {
        if (!m_watcher.files().isEmpty() || !m_watcher.directories().isEmpty())
            m_watcher.removePaths(m_watcher.files() + m_watcher.directories());
        m_path = path;
        m_avatarPath = avatarPath;
        m_account = install.userId;
        readFile = true;
    }
    if (m_path.isEmpty()) {
        m_status.clear();
        m_avatar.clear();
    } else {
        // A folder or file that appeared since the last look has no watch yet, and nothing told us.
        const bool added = watch(m_path) | watch(m_avatarPath);
        if (added)
            readFile = true;
        if (readFile) {
            QFile file(m_path);
            m_status = file.open(QIODevice::ReadOnly) ? parse(file.readAll(), m_account) : QString();
            m_avatar = avatarUrl(m_avatarPath);
        }
    }

    if (oldStatus != status() || oldRunning != m_running || oldAvatar != m_avatar)
        emit changed();
}

QString SteamStatus::avatarUrl(const QString &path)
{
    struct stat st;
    if (::stat(QFile::encodeName(path).constData(), &st) != 0 || !S_ISREG(st.st_mode))
        return {};
    // A replaced picture keeps its name, so the URL carries what QML's image cache can tell it by.
    return QUrl::fromLocalFile(path).toString() + QStringLiteral("?v=%1-%2-%3-%4")
                                                      .arg(quint64(st.st_ino))
                                                      .arg(qint64(st.st_mtim.tv_sec))
                                                      .arg(qint64(st.st_mtim.tv_nsec))
                                                      .arg(qint64(st.st_size));
}

bool SteamStatus::watch(const QString &path)
{
    bool added = false;
    const QString dir = QFileInfo(path).absolutePath();
    if (!m_watcher.directories().contains(dir) && QFileInfo::exists(dir))
        added |= m_watcher.addPath(dir);
    if (!m_watcher.files().contains(path) && QFileInfo::exists(path))
        added |= m_watcher.addPath(path);
    return added;
}

QString SteamStatus::parse(const QByteArray &vdf, const QString &accountId)
{
    const Vdf::Document doc = Vdf::parse(vdf);
    if (!doc.ok)
        return {};
    const Vdf::Node *prefs = doc.root.find({QStringLiteral("UserLocalConfigStore"), QStringLiteral("WebStorage"),
                                            QStringLiteral("FriendStoreLocalPrefs_") + accountId});
    if (!prefs || prefs->isBlock)
        return {};
    const QJsonDocument json = QJsonDocument::fromJson(prefs->value.toUtf8());
    const QJsonValue persona = json.object().value(QLatin1String("ePersonaState"));
    return persona.isDouble() ? nameOf(persona.toInt(-1)) : QString();
}

QString SteamStatus::setStatus(const QString &status)
{
    static const QStringList known = {QStringLiteral("online"), QStringLiteral("away"), QStringLiteral("invisible")};
    if (!known.contains(status))
        return tr("Unknown status");
    return m_source.open ? m_source.open(QStringLiteral("steam://friends/status/") + status) : QString();
}

QString SteamStatus::openFriends()
{
    return m_source.open ? m_source.open(QStringLiteral("steam://open/friends")) : QString();
}
