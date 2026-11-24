#pragma once

#include "appimageinstall.h"
#include "emulators.h"
#include "flatpakinstall.h"
#include "installrecord.h"

#include <QHash>
#include <QSet>
#include <QObject>
#include <QPointer>
#include <QTimer>
#include <QVariantMap>
#include <QtQmlIntegration>

#include <functional>
#include <optional>

class QJSEngine;
class QNetworkAccessManager;
class QNetworkReply;
class QQmlEngine;

// See docs/code/install.md.
class Installer : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(QVariantMap emulators READ emulators NOTIFY emulatorsChanged)
    // {id, kind, gameId, name, version, from, notes, size, state} of each emulator Lattis installed, and each AppImage
    // game with a release source, that has a newer release. kind is "emulator" or "game"; a game's id is its job key
    // ("game:<gameId>"), which update(), cancel() and retry() take.
    Q_PROPERTY(QVariantList updates READ updates NOTIFY emulatorsChanged)
    // gameId to {state, progress, error, version, updateVersion, upToDate, matched, match, notes, size, source} of each game with
    // a release source that has a job, a known update or was checked.
    Q_PROPERTY(QVariantMap games READ games NOTIFY emulatorsChanged)

public:
    struct GameTarget
    {
        QString id;
        QString title;
        // The AppImage; an update replaces it at this path.
        QString file;
        // The release source, as an emulator's `release`.
        QString source;
        // The release file name pattern, as an emulator's `release_asset`.
        QString asset;
        bool prereleases = false;
    };

    struct Setup
    {
        QString stateDir;
        QString cacheDir;
        std::function<QList<EmulatorDef>()> emulators;
        std::function<InstallLocations()> locations;
        // Asks the library to look for installed emulators again.
        std::function<void()> redetect;
        // The environment flatpak runs with (QProcess::environment() form); empty: the app's own.
        std::function<QStringList()> environment;
        std::function<bool(const QString &path, QString *error)> trash;
        // The title of a game running in the emulator with this name, or empty.
        std::function<QString(const QString &emulatorName)> runningGame;
        // The AppImage games that have a release source (their own, or the AppImage's).
        std::function<QList<GameTarget>()> games;
        // The title of the game with this id if it is running, or empty.
        std::function<QString(const QString &gameId)> gameRunningTitle;
    };
    // Called once at startup; nothing is made until QML uses it.
    static void setup(const Setup &setup) { s_setup = setup; }
    static Installer *instance();
    // True while the game's AppImage is being replaced; false without making the Installer.
    static bool gameUpdating(const QString &gameId);
    static Installer *create(QQmlEngine *, QJSEngine *) { return instance(); }

    explicit Installer(const Setup &setup, QObject *parent = nullptr);
    ~Installer() override;

    QVariantMap emulators() const { return m_emulators; }
    QVariantList updates() const { return m_updateList; }
    QVariantMap games() const { return m_gameRows; }
    static QString gameKey(const QString &gameId) { return QStringLiteral("game:") + gameId; }

    // Checks for updates after delayMs, then once a day, while autoCheck says so.
    void startDailyCheck(std::function<bool()> autoCheck, int delayMs = 10000);

    Q_INVOKABLE void install(const QString &id, const QString &route = QStringLiteral("appimage"));
    // An emulator, or a game by its job key.
    Q_INVOKABLE void update(const QString &id);
    Q_INVOKABLE void uninstall(const QString &id);
    // One emulator's (or a game's, by its job key), or (no id) every AppImage Lattis installed and every game with a
    // release source, quietly.
    Q_INVOKABLE void checkUpdates(const QString &id = {});
    // Like checkUpdates() for every emulator and game, but updatesChecked() tells the end. Returns how many it is
    // looking at.
    Q_INVOKABLE int checkUpdatesAsked();
    Q_INVOKABLE void updateGame(const QString &gameId) { update(gameKey(gameId)); }
    Q_INVOKABLE void checkGame(const QString &gameId) { checkUpdates(gameKey(gameId)); }
    // Properties' Updates card: what is typed in it, before Save, stands in for the game's saved source until
    // forgetGameSource() (the editor closing). checkGameWith() also checks it; setGameSource() only sets it, so
    // updateGame() can follow a Save.
    Q_INVOKABLE void setGameSource(const QString &gameId, const QString &file, const QString &title,
                                   const QString &source, const QString &asset, bool prereleases);
    Q_INVOKABLE void checkGameWith(const QString &gameId, const QString &file, const QString &title,
                                   const QString &source, const QString &asset, bool prereleases);
    Q_INVOKABLE void forgetGameSource(const QString &gameId);
    // "github:owner/repo" or "gitlab:host/group/project" for a pasted repository URL, else empty.
    Q_INVOKABLE static QString sourceFromText(const QString &text);
    // Runs a failed job again.
    Q_INVOKABLE void retry(const QString &id);
    // The user's choice on a job whose release lists no checksum (row field `unchecked`): downloads it unverified, this
    // once.
    Q_INVOKABLE void installAnyway(const QString &id);
    Q_INVOKABLE void cancel(const QString &id);
    Q_INVOKABLE void refresh();
    // Asks the main window to open the Updates dialog on one game (its chip, info row and menu entry), or on every
    // update when gameId is empty.
    // How many AppImage games have a release source to check.
    Q_INVOKABLE int checkableGames() const { return gameTargets().size(); }
    Q_INVOKABLE void review(const QString &gameId = {}) { emit reviewRequested(gameId); }

    void setFixturesDir(const QString &dir) { m_fixturesDir = dir; }
    void setFixtureDelay(int ms) { m_fixtureDelayMs = ms; }
    // 0: the canned download arrives at once.
    void setFixtureTick(int ms) { m_fixtureTickMs = ms; }

signals:
    void emulatorsChanged();
    void reviewRequested(const QString &gameId);
    // Every look checkUpdatesAsked() started is over; failed of them couldn't reach the release.
    void updatesChecked(int checked, int failed);
    // An AppImage update (an emulator's or a game's, by its name) finished; notes is the release's, possibly empty.
    void emulatorUpdated(const QString &name, const QString &version, const QString &notes);
    // Something to tell the user that no card shows (the old build the trash refused, and where it is).
    void message(const QString &text);

private:
    enum class State { Idle, Checking, Downloading, Extracting, Installing, Removing, Failed };
    enum class Kind { Install, Update, Check, Uninstall };

    // What one check or update runs on: an emulator's release, or a game's.
    struct ReleaseTarget
    {
        // The job key: an emulator's id, or "game:<id>".
        QString id;
        QString name;
        QString release;
        QString assets;
        bool prereleases = false;
        QString inside;
        bool game = false;
        QString gameId;
        QString file;
    };

    struct Job
    {
        quint64 serial = 0;
        Kind kind = Kind::Install;
        QString route;
        State state = State::Idle;
        double progress = -1;
        QString error;
        // A failed update check that nobody asked for is only logged.
        bool quiet = false;
        QPointer<QNetworkReply> reply;
        QPointer<AppImageDownload> download;
        QPointer<FlatpakJob> flatpak;
        QString emulatorName;
        Release release;
        ReleaseAsset asset;
        QString folder;
        QString oldFile;
        QString appId;
        // The release lists no checksum: the job is Failed with that text until the user clicks Install anyway.
        bool unchecked = false;
        bool allowUnchecked = false;
        ReleaseTarget target;
    };

    struct Update
    {
        Release release;
        ReleaseAsset asset;
    };

    bool updatingGame(const QString &gameId) const;
    QNetworkAccessManager *network();
    bool busy(const QString &id) const;
    Job &startJob(const ReleaseTarget &target, Kind kind);
    static ReleaseTarget targetOf(const EmulatorDef &emulator);
    static ReleaseTarget targetOf(const GameTarget &game);
    std::optional<InstallRecord> installedByApp(const QString &id) const;
    QString gameRunning(const EmulatorDef &emulator) const;
    QList<GameTarget> gameTargets() const;
    std::optional<GameTarget> findGame(const QString &gameId) const;
    // Why a game's AppImage can't be replaced (empty: it can).
    QString gameBlocker(const GameTarget &game) const;
    void updateGameFile(const GameTarget &game);
    void begin(const ReleaseTarget &target);
    void gotRelease(const ReleaseTarget &target, quint64 serial, const QByteArray &json, const QString &networkError,
                    bool fromCache);
    void checked(const ReleaseTarget &target);
    void checkedGame(const ReleaseTarget &target);
    void gameCheckDone(const ReleaseTarget &target, bool newer);
    void askedDone(const QString &id, bool failed);
    void download(const ReleaseTarget &target);
    void downloadFinished(const QString &id, const AppImageDownload::Result &result);
    void gameDownloadFinished(const QString &key, const AppImageDownload::Result &result);
    void beginFlatpak(const EmulatorDef &emulator, Kind kind, const QString &appId);
    void flatpakFinished(const QString &id, const FlatpakJob::Result &result);
    void removeAppImage(const EmulatorDef &emulator, const InstallRecord &record);
    void saveRecord(const QString &id, const InstallRecord *record);
    void saveGameRecord(const QString &gameId, const GameUpdateRecord &record);
    QString trashFile(const QString &path);
    void trashOld(const QString &old, bool aside);
    void fail(const QString &id, const QString &error);
    void changed();
    void useFixtureAsset(ReleaseAsset *asset) const;
    QString cachedReleasePath(const QString &release) const;
    void rebuild();
    static QString stateName(State state);

    static Setup s_setup;
    Setup m_setup;
    QString m_fixturesDir;
    int m_fixtureDelayMs = 400;
    int m_fixtureTickMs = 60;
    QNetworkAccessManager *m_network = nullptr;
    QString m_flatpakProgram;
    QHash<QString, Job> m_jobs;
    QHash<QString, Update> m_updates;
    // The emulators checkUpdatesAsked() waits for, and how many of them failed.
    QSet<QString> m_asked;
    int m_askedTotal = 0;
    int m_askedFailed = 0;
    // Checked this session and found no newer release.
    QSet<QString> m_upToDate;
    // The release file each game's last check matched, by job key.
    QHash<QString, QString> m_matched;
    QHash<QString, GameTarget> m_gameOverrides;
    InstallRecords::Records m_records;
    GameUpdateRecords::Records m_gameRecords;
    QVariantMap m_emulators;
    QVariantList m_updateList;
    QVariantMap m_gameRows;
    QTimer m_changed;
    QTimer m_daily;
    std::function<bool()> m_autoCheck;
    quint64 m_serial = 0;
};
