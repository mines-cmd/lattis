#pragma once

#include "launchsettings.h"
#include "steamlocate.h"

#include <QElapsedTimer>
#include <QFileSystemWatcher>
#include <QHash>
#include <QLoggingCategory>
#include <QObject>
#include <QStringList>
#include <QTimer>
#include <QtQml/qqmlregistration.h>

class DetachedProcess;
struct LaunchCommand;
class Library;
struct Game;
class QJSEngine;
class QQmlEngine;

// See docs/code/launch.md.

Q_DECLARE_LOGGING_CATEGORY(lcLaunch)

class GameRunner : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(QStringList running READ running NOTIFY runningChanged)
    Q_PROPERTY(bool minimizeWhileRunning READ minimizeWhileRunning NOTIFY settingsChanged)
    Q_PROPERTY(int minSessionSeconds READ minSessionSeconds NOTIFY settingsChanged)
    Q_PROPERTY(QStringList problems READ problems NOTIFY settingsChanged)
    Q_PROPERTY(bool closingSteam READ closingSteam NOTIFY closingSteamChanged)

public:
    GameRunner(Library *library, const QString &dataDir, const QString &logDir, QObject *parent = nullptr);
    ~GameRunner() override;

    static GameRunner *create(QQmlEngine *, QJSEngine *);

    // Call it after the first frame.
    void start();

    Q_INVOKABLE QString launch(const QString &gameId);
    Q_INVOKABLE QString restartSteamAndLaunch(const QString &gameId);
    Q_INVOKABLE QString openEmulator(const QString &gameId);
    Q_INVOKABLE QString openFolder(const QString &gameId);
    Q_INVOKABLE QString openDataFolder();
    Q_INVOKABLE QString openFile(const QString &path);
    Q_INVOKABLE QString problemFile(const QString &message) const;
    Q_INVOKABLE QString uninstall(const QString &gameId);
    Q_INVOKABLE QString update(const QString &gameId);
    Q_INVOKABLE QString forceClose(const QString &gameId);
    Q_INVOKABLE QVariantMap uninstallProton(const QString &gameId, bool files, bool prefix, bool shortcut,
                                           const QString &action, bool permanent);
    Q_INVOKABLE QString missingEmulator(const QString &gameId) const;
    Q_INVOKABLE QString closeSteamAndApply();
    bool closingSteam() const { return m_steamClosing != nullptr; }
    Q_INVOKABLE bool steamRunning() const;

    Q_INVOKABLE QString setLaunchValue(const QString &key, const QString &value);

    QStringList running() const { return m_sessions.keys() + m_steamSessions.keys(); }
    bool minimizeWhileRunning() const
    {
        return m_settings.whileRunning == LaunchSettings::WhileRunning::Minimize;
    }
    int minSessionSeconds() const { return m_settings.minSessionSeconds; }
    QStringList problems() const { return m_problems; }
    const LaunchSettings &settings() const { return m_settings; }

    static QStringList steamCommand(SteamInstall::Kind kind);
    void setSteamProgram(const QStringList &program) { m_steamProgram = program; }

    static int sessionMinutes(qint64 elapsedMs);
    static constexpr int kQuickExitSeconds = 10;
    static constexpr int kSteamStartSeconds = 90;
    static constexpr int kSteamGoneSeconds = 5;
    static constexpr int kSteamCloseSeconds = 30;
    void setSteamCloseTimeout(int ms) { m_steamCloseTimer.setInterval(ms); }
    void setSessionPollInterval(int ms) { m_sessionPollMs = ms; }

signals:
    void runningChanged();
    void closingSteamChanged();
    void settingsChanged();
    void sessionStarted(const QString &gameId);
    void sessionEnded(const QString &gameId);
    void message(const QString &text);
    void emulatorExited(const QString &gameId);

private:
    struct Session
    {
        QString emulatorName;
        QString logPath;
        QElapsedTimer timer;
        DetachedProcess *process = nullptr;
    };

    QString chooseEmulator(const QString &gameId, QString *emulatorId) const;
    QString launchProgram(const Game &game);
    QString startSession(const QString &gameId, const QString &name, const LaunchCommand &command,
                         const QString &workingDir, bool followSession);
    void onFinished(const QString &gameId, int exitCode, bool crashed);
    QString runSteam(const QStringList &args, bool raiseWindow = false);
    void pollSteamGames();
    void endSteamSession(const QString &gameId);

    struct SteamRef
    {
        quint64 runId = 0;
        quint64 reaperId = 0;
        bool shortcut = false;

        explicit operator bool() const { return runId != 0; }
    };
    static SteamRef steamRefOf(const Game &game);

    struct SteamSession
    {
        SteamRef steam;
        QElapsedTimer since;
        SteamExitWatch *watch = nullptr;
        bool seen = false;
        // Since the watched process exited. Steam can run a step of its own under the same
        // AppId first (a Proton game's install script), so the game is looked for a little longer.
        QElapsedTimer gone;
    };
    void onSteamProcessExited(const QString &gameId);
    QString startSteamGame(const QString &gameId, const SteamRef &steam);
    void addSteamSession(const QString &gameId, const SteamRef &steam);
    QString closeSteamThen(const QString &gameId);
    void onSteamClosed(bool closed);
    void loadSettings();
    QString settingsFile() const { return m_dataDir + QStringLiteral("/settings.toml"); }

    static inline GameRunner *s_instance = nullptr;

    Library *m_library;
    QString m_dataDir;
    QString m_logDir;
    LaunchSettings m_settings;
    QStringList m_problems;
    QHash<QString, Session> m_sessions;
    QStringList m_steamProgram;
    QHash<QString, SteamSession> m_steamSessions;
    QTimer m_steamPoll;
    SteamExitWatch *m_steamClosing = nullptr;
    QTimer m_steamCloseTimer;
    QString m_launchAfterClose;
    QFileSystemWatcher m_watcher;
    QTimer m_reloadTimer;
    int m_sessionPollMs = 2000;
};
