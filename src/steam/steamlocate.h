#pragma once

#include <QList>
#include <QObject>
#include <QString>
#include <QStringList>

class QSocketNotifier;

// See docs/code/steam.md.
struct SteamUser
{
    QString accountId;   // the userdata folder's name: the SteamID64 minus 76561197960265728
    QString steamId64;
    QString personaName;
    QString accountName;
    bool mostRecent = false;
    qint64 timestamp = 0;

    bool operator==(const SteamUser &) const = default;
};

struct SteamInstall
{
    enum class Kind { None, Native, Flatpak };

    Kind kind = Kind::None;
    QString root;
    bool pathMissing = false;
    QList<SteamUser> users;
    QString userId;
    bool userMissing = false;

    bool found() const { return kind != Kind::None; }
    QString userDataDir() const;

    bool operator==(const SteamInstall &) const = default;
};

struct SteamLocations
{
    QString home;
    QStringList systemToolDirs = {QStringLiteral("/usr/share/steam/compatibilitytools.d")};

    static SteamLocations system();
    QString nativeLink() const;
    QString flatpakRoot() const;
};

namespace SteamLocate {
SteamInstall find(const SteamLocations &where, const QString &configuredPath = {},
                  const QString &configuredUser = {});

QList<SteamUser> readUsers(const QString &root);

QString accountId(const QString &steamId64);

qint64 runningPid(const SteamInstall &install, const SteamLocations &where);

qint64 gamePid(quint64 appId,const QString &procDir = QStringLiteral("/proc"));
}

class SteamExitWatch : public QObject
{
    Q_OBJECT

public:
    explicit SteamExitWatch(QObject *parent = nullptr);
    ~SteamExitWatch() override;

    bool watch(qint64 pid); // false if already gone (or pidfds aren't supported)
    bool watching() const { return m_pidfd >= 0; }

signals:
    void exited();

private:
    void stop();

    int m_pidfd = -1;
    QSocketNotifier *m_notifier = nullptr;
};
