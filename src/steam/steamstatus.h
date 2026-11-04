#pragma once

#include "steamlocate.h"

#include <QFileSystemWatcher>
#include <QObject>
#include <QString>
#include <QTimer>
#include <QtQml/qqmlregistration.h>

#include <functional>

class QJSEngine;
class QQmlEngine;

// See docs/code/steam.md.
class SteamStatus : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    // "online", "away" or "invisible"; empty while Steam is closed or the file says nothing usable.
    Q_PROPERTY(QString status READ status NOTIFY changed)
    Q_PROPERTY(bool running READ running NOTIFY changed)
    // A file URL of the profile picture with a version query, empty when the file is missing.
    Q_PROPERTY(QString avatar READ avatar NOTIFY changed)

public:
    struct Source
    {
        std::function<SteamInstall()> install;
        std::function<bool()> running;
        // Hands a steam:// URL to Steam; the error, or empty.
        std::function<QString(const QString &url)> open;
    };

    static void setup(const Source &source) { pending() = source; }
    static SteamStatus *instance();
    static SteamStatus *create(QQmlEngine *, QJSEngine *) { return instance(); }

    explicit SteamStatus(const Source &source, QObject *parent = nullptr);

    QString status() const { return m_running ? m_status : QString(); }
    bool running() const { return m_running; }
    QString avatar() const { return m_avatar; }

    // Reads nothing until called; the singleton calls it once it exists, which is after the first frame.
    void start();
    Q_INVOKABLE QString setStatus(const QString &status);
    Q_INVOKABLE QString openFriends();

    // The status name in a localconfig.vdf's text for this account, or empty.
    static QString parse(const QByteArray &vdf, const QString &accountId);

    void setDebounceMs(int ms) { m_debounce.setInterval(ms); }
    void setPollMs(int ms) { m_poll.setInterval(ms); }

signals:
    void changed();

private:
    void refresh(bool readFile);
    bool watch(const QString &path);
    static QString avatarUrl(const QString &path);

    static Source &pending();

    Source m_source;
    QFileSystemWatcher m_watcher;
    QTimer m_debounce;
    QTimer m_poll;
    QString m_path;
    QString m_avatarPath;
    QString m_avatar;
    QString m_account;
    QString m_status;
    bool m_running = false;
    bool m_started = false;
};
