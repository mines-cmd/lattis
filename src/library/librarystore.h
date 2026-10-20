#pragma once

#include "librarycache.h"
#include "libraryversion.h"
#include "playhistory.h"
#include "steamsync.h"
#include "tomlpatch.h"

#include <QFileSystemWatcher>
#include <QFutureWatcher>
#include <QLoggingCategory>
#include <QObject>
#include <QSize>
#include <QTimer>

#include <functional>

Q_DECLARE_LOGGING_CATEGORY(lcLibrary)

// See docs/code/library.md.
struct ScanOptions
{
    bool roms = false;
    QList<EmulatorEntry> presets;
    InstallLocations installs;
    SteamLocations steam = SteamLocations::system();
    bool copySteamArt = false;
    QString steamSyncRecord;
    QString libraryId;
    bool setup = false;
    bool steamHistory = false;
    bool steamStartup = false; // no scan has diffed it in this run: changes may span several sessions
};

struct ScanResult
{
    LibrarySnapshot snapshot;
    bool changed = false;
    int parsed = 0;
    qint64 elapsedMs = 0;

    bool installsDetected = false;
    bool romsScanned = false;
    double romsMs = 0;
    QStringList romLog;
    QList<EmulatorDef> emulators;
    QMap<QString, QList<RomEntry>> romWrites;
    QStringList romAdded;

    QList<IndexEntry> steamWrites;
    bool steamChanged = false;
    QStringList steamArtCopied;

    QHash<QString, QList<std::pair<QString, QVariant>>> steamGameWrites;
    std::optional<SteamSync::Record> steamRecord;
    SteamSync::Record steamRecordBefore;
    QStringList steamSyncLog;
    QStringList steamGridLog;

    bool steamDiffed = false;
    QList<PlayHistory::NewSession> historySessions;
    bool historySeenChanged = false;
    PlayHistory::SeenTotals historySeen;
    QStringList historyProblems;
};

struct ArtShrinkJob
{
    QString id;
    QString kind;
    QString path;
    QString temporary;
    qint64 modified = 0;
    QSize before;
    QSize after;
    QString error;
    bool changed = true;
};

// No model access, so it runs on a worker thread.
ScanResult scanLibrary(const QString &dataDir, const LibrarySnapshot &previous, const ScanOptions &options);

enum class NewLibrary { Plain, WithSetup };

class LibraryStore : public QObject
{
    Q_OBJECT

public:
    LibraryStore(const QString &dataDir, const QString &cacheDir, NewLibrary newLibrary = NewLibrary::Plain,
                 QObject *parent = nullptr);
    ~LibraryStore() override;

    QString dataDir() const { return m_dataDir; }
    const LibrarySnapshot &snapshot() const { return m_snapshot; }

    const LibraryVersion::Status &versionStatus() const { return m_versionStatus; }
    bool readOnly() const { return m_versionStatus.readOnly; }

    bool loading() const { return m_loading; }

    void refresh();
    void scanRoms();

    void hold(bool on);
    void forgetCache();

    bool setupPending() const { return m_setupPending; }
    QString finishSetup();

    // The locations and the state dir are set before refresh().
    void setInstallLocations(const InstallLocations &where) { m_installLocations = where; }
    const InstallLocations &installLocations() const { return m_installLocations; }
    void setSteamLocations(const SteamLocations &where) { m_steamLocations = where; }
    const SteamLocations &steamLocations() const { return m_steamLocations; }
    void setStateDir(const QString &stateDir);

    qint64 steamPid() const;
    QString applySteamQueue();
    bool wroteGridArt(qint64 appId, const QString &kind) const
    {
        return m_steamRecord && m_steamRecord->grid.value(appId).contains(kind);
    }
    QString removedShortcutTitle(quint32 appId) const
    {
        return m_steamRecord ? m_steamRecord->removed.value(appId) : QString();
    }

    QString setGameValues(const QString &id, const QList<std::pair<QString, QVariant>> &values);
    void beginBatch() { ++m_batching; }
    void endBatch();

    struct AddedGame
    {
        QString id;
        QString error;
        bool existing = false;
    };
    AddedGame addGame(const QString &romPath, const QString &platform, const QString &title,
                      const QList<std::pair<QString, QVariant>> &values = {});
    struct AddedShortcuts
    {
        QStringList ids;
        QString error;
    };
    AddedShortcuts addShortcuts(const QList<quint32> &appIds);
    AddedGame addProtonGame(const QString &exePath, const QString &title,
                            const QList<std::pair<QString, QVariant>> &values = {});
    AddedGame addLinuxGame(const QString &exePath, const QString &title,
                           const QList<std::pair<QString, QVariant>> &values = {}, bool makeExecutable = false);
    static QString makeExecutable(const QString &path);
    QString gameWithProgram(const QString &exePath) const;
    quint32 shortcutWithProgram(const QString &exePath) const;
    QString writtenPath(const QString &path) const;
    bool inIndex(const QString &id) const;
    QString gameWithRom(const QString &romPath) const;
    QString relocateRom(const QString &id, const QString &romPath);
    void reloadFiles(const QStringList &relatives);
    QString removeGameFile(const QString &id, bool withArt);
    QString removeProtonGame(const QString &id, bool fromSteam = true);
    QString undoSteamQueue(const QString &key);

    QString setArt(const QString &id, const QString &kind, const QString &source);
    QString removeArt(const QString &id, const QString &kind);

    QString addTag(const QString &name, int color);
    QString setTagColor(const QString &name, int color);
    QString renameTag(const QString &from, const QString &to);
    QString removeTag(const QString &name);
    QHash<QString, int> tagUse() const;
    static const QList<TagDef> &defaultTags();
    static QStringList igdbOf(const TagDef &tag);
    QList<TagDef> missingDefaultTags() const;
    QString addDefaultTags();

    QString setEmulatorValue(const QString &id, const QString &key, const QVariant &value);
    QString addEmulator(const QString &id, const QString &name, const QString &startFrom);
    QString removeEmulator(const QString &id);

    QString setPlatformValue(const QString &id, const QString &key, const QVariant &value);
    QString addPlatform(const QString &id, const QString &name);
    QString removePlatform(const QString &id);
    QString movePlatform(const QString &id, int step);
    QString movePlatformBefore(const QString &id, const QString &beforeId);

    QString emulatorsFile();
    QString platformsFile();

    bool scanning() const { return m_scan.isRunning() || m_scanAgain; }

    int filesParsed() const { return m_filesParsed; }

    QStringList artToShrink(const QString &id) const;
    bool shrinkArt(const QString &id, const QString &kind);
    bool shrinking() const { return m_shrink.isRunning(); }

    void waitForIdle();

signals:
    void artShrunk();
    void changed();
    void loadingChanged();
    void scanningChanged();
    void setupPendingChanged();
    void gamesAdded(const QStringList &ids);
    void historyChanged(const QStringList &ids);

private:
    void applySteamHistory(const ScanResult &result);
    void onScanFinished();
    void onShrinkFinished();
    void updateWatches();
    void scheduleCacheSave();
    void saveCache();
    void writeRomIndexes(const QMap<QString, QList<RomEntry>> &indexes);
    void writeSteamIndex(const QList<IndexEntry> &added);
    bool addBuiltInPlatforms();
    void applySteamSync(const ScanResult &result);
    void updateSteamQueue();
    // No changedHere().
    QString writeNewGameFile(const QString &id, const QList<std::pair<QString, QVariant>> &values);
    QString writeRomIndex(const QString &platform, const QList<RomEntry> &entries);
    void changedHere();
    // No changedHere(): callers may write several files.
    QString patchAndLoad(const QString &relative, const QString &header,
                         const std::function<TomlPatch::Result(const QString &)> &patch);
    QStringList retagGames(const QString &name, const std::function<QStringList(QStringList)> &change);
    QString editFile(const QString &relative, const QString &header,
                     const std::function<TomlPatch::Result(const QString &)> &patch);
    QString fileToOpen(const QString &relative, const QString &header);

    QString m_dataDir;
    InstallLocations m_installLocations = InstallLocations::system();
    SteamLocations m_steamLocations = SteamLocations::system();
    QString m_steamSyncRecord;
    QString m_steamBackupDir;
    std::optional<SteamSync::Record> m_steamRecord;
    QTimer m_steamApplyTimer;
    SteamExitWatch m_steamExit;
    bool m_scanRoms = false;
    QStringList m_protonAdded;
    bool m_steamHistoryStarted = false;
    QString m_cachePath;
    LibraryVersion::Status m_versionStatus;
    LibrarySnapshot m_snapshot;
    bool m_loading = false;
    int m_filesParsed = 0;

    QFutureWatcher<ScanResult> m_scan;
    bool m_scanAgain = false;
    // Bumped by every change made on this thread; a scan started before one is outdated.
    quint64 m_generation = 0;
    quint64 m_scanGeneration = 0;
    int m_batching = 0;
    bool m_batchChanged = false;

    QFileSystemWatcher m_watcher;
    QTimer m_watchTimer;
    // Steam's files and settings.toml: Steam writes several files at once, so a longer wait.
    QFileSystemWatcher m_steamWatcher;
    QTimer m_steamTimer;

    QFutureWatcher<ArtShrinkJob> m_shrink;
    QHash<QString, qint64> m_shrinkFailed;
    bool m_shrinkFailureLogged = false;

    QFutureWatcher<bool> m_save;
    QTimer m_saveTimer;
    bool m_saveAgain = false;
    bool m_held = false;
    bool m_refreshOnResume = false;
    bool m_setupPending = false;
};
