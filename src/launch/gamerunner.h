#pragma once

#include "epicinstall.h"
#include "heroiclocate.h"
#include "legendaryjobs.h"
#include "launchsettings.h"
#include "steamlocate.h"

#include <QElapsedTimer>
#include <QFileSystemWatcher>
#include <QHash>
#include <QSet>
#include <QLoggingCategory>
#include <QObject>
#include <QStringList>
#include <QTimer>
#include <QtQml/qqmlregistration.h>

#include <functional>

class DetachedProcess;
struct LaunchCommand;
class Library;
struct Game;
struct ModManager;
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
    Q_PROPERTY(QStringList runningManagers READ runningManagers NOTIFY runningChanged)
    Q_PROPERTY(bool minimizeWhileRunning READ minimizeWhileRunning NOTIFY settingsChanged)
    Q_PROPERTY(int minSessionSeconds READ minSessionSeconds NOTIFY settingsChanged)
    Q_PROPERTY(QStringList problems READ problems NOTIFY settingsChanged)
    Q_PROPERTY(bool closingSteam READ closingSteam NOTIFY closingSteamChanged)
    Q_PROPERTY(bool closingHeroic READ closingHeroic NOTIFY closingHeroicChanged)
    Q_PROPERTY(LegendaryJobs *legendary READ legendary CONSTANT)

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
    Q_INVOKABLE QString saveFolder(const QString &gameId) const;
    Q_INVOKABLE QString openSaveFolder(const QString &gameId);
    Q_INVOKABLE QString shownPath(const QString &path) const;
    Q_INVOKABLE QString openDataFolder();
    Q_INVOKABLE QString openPath(const QString &path);
    Q_INVOKABLE bool folderExists(const QString &path) const;
    Q_INVOKABLE QString openFile(const QString &path);
    Q_INVOKABLE QString problemFile(const QString &message) const;
    Q_INVOKABLE QString uninstall(const QString &gameId);
    // Steam: opens what updates it. Epic: queues `legendary update` (a problem comes as `message`).
    Q_INVOKABLE QString update(const QString &gameId);
    // The titles of the Epic games whose Legendary job runs or waits, for the quit question.
    Q_INVOKABLE QStringList epicJobTitles() const;
    Q_INVOKABLE QString openSteamProperties(const QString &gameId);
    Q_INVOKABLE QString forceClose(const QString &gameId);
    Q_INVOKABLE QVariantMap uninstallProton(const QString &gameId, bool files, bool prefix, bool shortcut,
                                           const QString &action, bool permanent);
    Q_INVOKABLE QVariantMap removeModManager(const QString &id, bool files, bool prefix, bool shortcut,
                                             bool permanent);
    Q_INVOKABLE QString launchModManager(const QString &id);
    Q_INVOKABLE QString forceCloseModManager(const QString &id);
    Q_INVOKABLE QString missingPrefixGame(const QString &managerId) const;
    QStringList runningManagers() const;
    // What a running id is called: a game's title or a manager's name.
    Q_INVOKABLE QString runningTitle(const QString &id) const;
    Q_INVOKABLE QString missingEmulator(const QString &gameId) const;
    Q_INVOKABLE QString closeSteamAndApply();
    // Heroic has no quit command, so it gets SIGTERM; refused while an Epic game runs, which it could take down.
    // Library applies the waiting edits once it's gone.
    Q_INVOKABLE QString closeHeroicAndApply();
    bool closingHeroic() const { return m_heroicCloseTimer.isActive(); }
    Q_INVOKABLE QString openHeroic();
    Q_INVOKABLE bool legendaryFound() const;
    // Heroic's Legendary deletes the files for good; the prefix goes to the trash. The answer comes as
    // epicUninstalled; a result with `trashless` asks to run it again with files off and permanent on.
    Q_INVOKABLE QString uninstallEpic(const QString &gameId, bool files, bool prefix, bool permanent);
    // The Install dialog's start: {appName, folder, sizeState ("known", "checking" or "unknown"), downloadSize,
    // diskSize, platform}. "checking" asks Legendary; the answer comes as epicInstallInfo.
    Q_INVOKABLE QVariantMap epicInstallPlan(const QString &gameId);
    // Bytes free where `folder` is (or would be made); -1 when unknown.
    Q_INVOKABLE double freeSpace(const QString &folder) const;
    Q_INVOKABLE QString sizeText(double bytes) const;
    // Queues `legendary install` into the folder and remembers it for the next install; the error, or empty.
    // `wine` is one of epicInstallWine's `wines[].wine`; `ownPrefix` gives the game a Wine prefix of its own
    // (epicInstallWine's `prefix`) instead of Heroic's shared one. Both only matter for a Windows build.
    Q_INVOKABLE QString installEpic(const QString &gameId, const QString &folder, const QVariantMap &wine = {},
                                    bool ownPrefix = false);
    // The Install dialog's Wine settings: {wines (label, wine), wine (the default one), prefix (the game's own
    // would-be prefix folder)}; empty when Heroic isn't found.
    Q_INVOKABLE QVariantMap epicInstallWine(const QString &gameId) const;
    // Queues `legendary repair` (checks every file, downloads the broken ones); the error, or empty. The result
    // comes as `message`. epicJob is the game's job (appName, kind, percent, state, error), empty when it has none.
    Q_INVOKABLE QString verifyEpic(const QString &gameId);
    Q_INVOKABLE QVariantMap epicJob(const QString &gameId) const;
    void setLegendaryProgram(const QString &program) { m_legendary = program; }
    LegendaryJobs *legendary() { return &m_legendaryJobs; }
    bool closingSteam() const { return m_steamClosing != nullptr; }
    Q_INVOKABLE bool steamRunning() const;
    // Hands a steam:// URL to Steam, bare; the error, or empty.
    QString openSteamUrl(const QString &url) { return runSteam({url}); }

    Q_INVOKABLE QString setLaunchValue(const QString &key, const QString &value);

    QStringList running() const { return m_sessions.keys() + m_steamSessions.keys() + m_heroicSessions.keys(); }
    QString runningIn(const QString &emulatorName) const
    {
        for (auto it = m_sessions.cbegin(); it != m_sessions.cend(); ++it) {
            if (it->emulatorName == emulatorName)
                return it.key();
        }
        return {};
    }
    bool minimizeWhileRunning() const
    {
        return m_settings.whileRunning == LaunchSettings::WhileRunning::Minimize;
    }
    int minSessionSeconds() const { return m_settings.minSessionSeconds; }
    QStringList problems() const { return m_problems; }
    const LaunchSettings &settings() const { return m_settings; }

    static QStringList steamCommand(SteamInstall::Kind kind);
    void setSteamProgram(const QStringList &program) { m_steamProgram = program; }
    void setHeroicProgram(const HeroicProgram &program) { m_heroicProgram = program; }
    // Finds a game's saves (absolute paths) off the main thread and calls back on it.
    using FindSaves = std::function<void(const QString &gameId, std::function<void(const QStringList &paths)> done)>;
    void setFindSaves(FindSaves find) { m_findSaves = std::move(find); }

    // Whether a game's file is being replaced by an update, so it must not start.
    void setUpdating(std::function<bool(const QString &gameId)> updating) { m_updating = std::move(updating); }

    static int sessionMinutes(qint64 elapsedMs);
    static constexpr int kQuickExitSeconds = 10;
    static constexpr int kSteamStartSeconds = 90;
    static constexpr int kSteamGoneSeconds = 5;
    static constexpr int kSteamCloseSeconds = 30;
    static constexpr int kHeroicCloseSeconds = 15;
    void setSteamCloseTimeout(int ms) { m_steamCloseTimer.setInterval(ms); }
    void setSessionPollInterval(int ms) { m_sessionPollMs = ms; }

signals:
    void epicUninstalled(const QString &gameId, const QVariantMap &result);
    void epicInstallInfo(const QString &appName, const QVariantMap &plan);
    void runningChanged();
    void closingSteamChanged();
    void closingHeroicChanged();
    void settingsChanged();
    void sessionStarted(const QString &gameId);
    void sessionEnded(const QString &gameId);
    void message(const QString &text);
    void copyRequested(const QString &text);
    void emulatorExited(const QString &gameId);

private:
    struct Session
    {
        QString emulatorName;
        QString logPath;
        QElapsedTimer timer;
        DetachedProcess *process = nullptr;
        bool forceClosed = false;
    };

    QString chooseEmulator(const QString &gameId, QString *emulatorId) const;
    // The folder of the first own save location that exists.
    QString ownSaveFolder(const QString &gameId) const;
    // The ROM's or emulator's folder, or the prefix's user folder; Linux games have none.
    QString guessedSaveFolder(const QString &gameId) const;
    QString launchProgram(const Game &game);
    QString launchProtonManager(const ModManager &manager, const QString &key);
    // The name of a manager of the game that is running, or empty.
    QString runningManagerFor(const QString &gameId) const;
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

    struct HeroicSession
    {
        QString appName;
        QString root;
        // Heroic's lastPlayed at the launch: it changes when the game closes, however short the session.
        qint64 lastPlayed = 0;
        QElapsedTimer since;
        // Null once it exits, which says nothing while Heroic was already running: it only passed the URL on.
        DetachedProcess *process = nullptr;
        bool forceClosed = false;
    };
    HeroicProgram heroicProgram() const;
    QString runHeroic(const QStringList &args);
    QString launchHeroicGame(const Game &game);
    void onHeroicProcessExited(const QString &gameId, DetachedProcess *process, int exitCode, bool crashed);
    void pollHeroicGames();
    void endHeroicSession(const QString &gameId);
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
    HeroicProgram m_heroicProgram;
    QString m_legendary;
    struct EpicUninstall
    {
        QString appName;
        bool prefix = false;
        bool permanent = false;
    };
    QString legendaryProgram() const;
    void onLegendaryFinished(const LegendaryJobs::Result &result);
    void finishEpicUninstall(const QString &gameId, const EpicUninstall &uninstall);
    QString epicTitle(const QString &appName) const;
    QString updateEpic(const Game &game);
    QVariantMap installPlan(const QString &appName, const EpicInstallInfo *info, const QString &sizeState) const;
    QString installFolder() const;
    void onInfoReady(const QString &appName, const QByteArray &json, const QString &error);
    QHash<QString, EpicInstallInfo> m_epicInfo; // by app name, for this run
    QHash<QString, EpicUninstall> m_epicUninstalling; // by game id
    LegendaryJobs m_legendaryJobs;
    FindSaves m_findSaves;
    std::function<bool(const QString &)> m_updating;
    QHash<QString, HeroicSession> m_heroicSessions;
    QTimer m_heroicPoll;
    QTimer m_heroicCloseTimer;
    QElapsedTimer m_heroicCloseSince;
    QFileSystemWatcher m_watcher;
    QTimer m_reloadTimer;
    int m_sessionPollMs = 2000;
};
