#pragma once

#include "appimageinstall.h"

#include <QObject>
#include <QPointer>
#include <QTimer>
#include <QUrl>
#include <QtQmlIntegration>

#include <functional>

class QJSEngine;
class QNetworkAccessManager;
class QNetworkReply;
class QQmlEngine;

// See docs/code/install.md.

namespace AppUpdates {
// Why the running AppImage can't be replaced from inside the app (empty: it can). `applicationsDir` is the folder of
// the menu entries (~/.local/share/applications).
QString replaceBlocker(const QString &appImage, const QString &applicationsDir);

// The name of the manager (Gear Lever, AppImageLauncher) that keeps a menu entry for `appImage`, or "".
QString managedBy(const QString &appImage, const QString &applicationsDir);
}

class AppUpdate : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    // The feature exists in this run: a release AppImage (or the fixture hooks).
    Q_PROPERTY(bool available READ available CONSTANT)
    // "idle", "checking", "downloading", "ready" or "failed".
    Q_PROPERTY(QString state READ state NOTIFY changed)
    // The newest release's version ("1.2") once known and newer than this one, else "".
    Q_PROPERTY(QString latestVersion READ latestVersion NOTIFY changed)
    Q_PROPERTY(bool hasUpdate READ hasUpdate NOTIFY changed)
    // The newer version is the one the user skipped.
    Q_PROPERTY(bool skipped READ skipped NOTIFY changed)
    // Worth showing in the menu and on the logo: a newer version the user hasn't skipped.
    Q_PROPERTY(bool notify READ notify NOTIFY changed)
    Q_PROPERTY(qint64 size READ size NOTIFY changed)
    Q_PROPERTY(QString notes READ notes NOTIFY changed)
    Q_PROPERTY(double progress READ progress NOTIFY changed)
    Q_PROPERTY(QString error READ error NOTIFY changed)
    // The running file can be replaced; false: open releasePage instead.
    Q_PROPERTY(bool canReplace READ canReplace NOTIFY changed)
    Q_PROPERTY(QString replaceBlocker READ replaceBlocker NOTIFY changed)
    Q_PROPERTY(QUrl releasePage READ releasePage NOTIFY changed)
    // The title of the game that keeps Restart from running, or "".
    Q_PROPERTY(QString runningGame READ runningGame NOTIFY changed)
    // The version this start follows an update made from the app to, and its notes; "" otherwise or once dismissed.
    Q_PROPERTY(QString whatsNewVersion READ whatsNewVersion NOTIFY changed)
    Q_PROPERTY(QString whatsNewNotes READ whatsNewNotes NOTIFY changed)

public:
    struct Setup
    {
        QString stateDir;
        QString cacheDir;
        // A release build (-DLATTIS_UPDATES=ON).
        bool enabled = false;
        // The running AppImage ($APPIMAGE); empty: not an AppImage.
        QString appImage;
        // The folder of the menu entries, where Gear Lever and AppImageLauncher leave theirs.
        QString applicationsDir;
        std::function<bool()> autoCheck;
        std::function<QString()> skippedVersion;
        std::function<void(const QString &version)> setSkippedVersion;
        std::function<bool(const QString &path, QString *error)> trash;
        // The title of a game that is running, or empty.
        std::function<QString()> runningGame;
        std::function<bool(QString *error)> restart;
    };
    static void setup(const Setup &setup) { s_setup = setup; }
    static AppUpdate *instance();
    static AppUpdate *create(QQmlEngine *, QJSEngine *) { return instance(); }

    explicit AppUpdate(const Setup &setup, QObject *parent = nullptr);
    ~AppUpdate() override;

    bool available() const { return m_available; }
    QString state() const;
    QString latestVersion() const { return m_hasUpdate ? m_version : QString(); }
    bool hasUpdate() const { return m_hasUpdate; }
    bool skipped() const;
    bool notify() const { return m_hasUpdate && !skipped(); }
    qint64 size() const { return m_asset.size; }
    QString notes() const { return m_release.notes; }
    double progress() const { return m_progress; }
    QString error() const { return m_error; }
    bool canReplace() const { return m_available && m_blocker.isEmpty(); }
    QString replaceBlocker() const { return m_blocker; }
    QUrl releasePage() const;
    QString runningGame() const;
    QString whatsNewVersion() const { return m_whatsNewVersion; }
    QString whatsNewNotes() const { return m_whatsNewNotes; }

    // quiet: a failure is only logged (the daily check). A fresh answer from the last hour is reused.
    Q_INVOKABLE void check(bool quiet = false);
    Q_INVOKABLE void update();
    // Stops a download and deletes what it wrote; also clears a failure.
    Q_INVOKABLE void cancel();
    Q_INVOKABLE void skip();
    Q_INVOKABLE void restart();
    Q_INVOKABLE void dismissWhatsNew();

    // The daily check: waits `delayMs`, checks when the last check is over a day old, and again a day after each.
    void startDailyCheck(int delayMs = 10000);

    // The running file was moved.
    void setAppImage(const QString &appImage);

    void setCurrentVersion(const QString &version);
    // http: and file: URLs, and a release answered from another address (tests, screenshots).
    void setReleaseUrl(const QUrl &url) { m_releaseUrl = url; }
    void setRelaxed(bool relaxed) { m_relaxed = relaxed; }
    void setPace(qint64 bytes, int tickMs);

signals:
    void changed();

private:
    enum class State { Idle, Checking, Downloading, Ready, Failed };

    QNetworkAccessManager *network();
    void gotRelease(quint64 serial, const QByteArray &json, const QString &networkError, bool fromCache, bool quiet);
    void downloadFinished(quint64 serial, const AppImageDownload::Result &result);
    void fail(const QString &error, bool quiet = false);
    void setState(State state);
    void saveLastCheck();
    QString cachedReleasePath() const;
    QString currentVersion() const;
    QString lastCheckPath() const;
    void dailyCheck();
    void saveUpdated();
    void loadWhatsNew();
    QString updatedPath() const;
    QString notesPath() const;

    static Setup s_setup;
    Setup m_setup;
    bool m_available = false;
    QString m_blocker;
    QNetworkAccessManager *m_network = nullptr;
    QUrl m_releaseUrl;
    QString m_currentVersion;
    bool m_relaxed = false;
    qint64 m_paceBytes = 0;
    int m_paceTickMs = 0;

    State m_state = State::Idle;
    double m_progress = -1;
    QString m_error;
    quint64 m_serial = 0;
    Release m_release;
    ReleaseAsset m_asset;
    QString m_version;
    bool m_hasUpdate = false;
    QPointer<QNetworkReply> m_reply;
    QPointer<AppImageDownload> m_download;
    QTimer m_daily;
    QString m_whatsNewVersion;
    QString m_whatsNewNotes;
};
