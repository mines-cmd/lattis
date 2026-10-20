#pragma once

#include "game.h"
#include "gameid.h"
#include "herocache.h"
#include "librarystore.h"
#include "libraryundo.h"
#include "listsettings.h"
#include "lookupgame.h"
#include "steamdownloads.h"
#include "uninstall.h"

#include <QAbstractListModel>
#include <QFileSystemWatcher>
#include <QFutureWatcher>
#include <QHash>
#include <QList>
#include <QSet>
#include <QStringList>
#include <QTimer>
#include <QtQml/qqmlregistration.h>

#include <array>

class QJSEngine;
class QQmlEngine;

// See docs/code/library.md.
class Library : public QAbstractListModel
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(QVariantList platforms READ platforms NOTIFY platformsChanged)
    Q_PROPERTY(int gameCount READ gameCount NOTIFY gamesChanged)
    Q_PROPERTY(QVariantList gameCounts READ gameCounts NOTIFY gamesChanged)
    Q_PROPERTY(bool loading READ loading NOTIFY loadingChanged)
    Q_PROPERTY(QStringList problems READ problems NOTIFY problemsChanged)
    Q_PROPERTY(bool readOnly READ readOnly CONSTANT)
    Q_PROPERTY(QString readOnlyNotice READ readOnlyNotice CONSTANT)
    Q_PROPERTY(bool setupPending READ setupPending NOTIFY setupPendingChanged)
    Q_PROPERTY(QString platformFilter READ platformFilter WRITE setPlatformFilter NOTIFY platformFilterChanged)
    Q_PROPERTY(QString search READ search WRITE setSearch NOTIFY searchChanged)
    Q_PROPERTY(QString groupBy READ groupBy WRITE setGroupBy NOTIFY groupByChanged)
    Q_PROPERTY(QString sort READ sort WRITE setSort NOTIFY sortChanged)
    Q_PROPERTY(bool sortDescending READ sortDescending WRITE setSortDescending NOTIFY sortChanged)
    Q_PROPERTY(bool showSortValue READ showSortValue WRITE setShowSortValue NOTIFY showSortValueChanged)
    Q_PROPERTY(bool installedOnly READ installedOnly WRITE setInstalledOnly NOTIFY filtersChanged)
    Q_PROPERTY(bool favoritesOnly READ favoritesOnly WRITE setFavoritesOnly NOTIFY filtersChanged)
    Q_PROPERTY(QStringList tagFilter READ tagFilter WRITE setTagFilter NOTIFY filtersChanged)
    Q_PROPERTY(bool tagMatchAll READ tagMatchAll WRITE setTagMatchAll NOTIFY filtersChanged)
    Q_PROPERTY(QStringList statusFilter READ statusFilter WRITE setStatusFilter NOTIFY filtersChanged)
    Q_PROPERTY(QVariantList statuses READ statuses NOTIFY gamesChanged)
    Q_PROPERTY(bool showHidden READ showHidden WRITE setShowHidden NOTIFY filtersChanged)
    Q_PROPERTY(bool filtersActive READ filtersActive NOTIFY filtersChanged)
    Q_PROPERTY(int notInstalledHidden READ notInstalledHidden NOTIFY notInstalledHiddenChanged)
    Q_PROPERTY(QVariantList emulators READ emulatorList NOTIFY emulatorsChanged)
    Q_PROPERTY(QVariantList platformSettings READ platformSettings NOTIFY platformSettingsChanged)
    Q_PROPERTY(bool scanning READ scanning NOTIFY scanningChanged)
    Q_PROPERTY(QVariantList tags READ tags NOTIFY tagsChanged)
    Q_PROPERTY(QStringList missingDefaultTags READ missingDefaultTags NOTIFY tagsChanged)
    Q_PROPERTY(QVariantList hiddenGames READ hiddenGames NOTIFY hiddenGamesChanged)
    Q_PROPERTY(QVariantMap steam READ steam NOTIFY steamChanged)
    Q_PROPERTY(QVariantList steamWaiting READ steamWaiting NOTIFY steamWaitingChanged)
    Q_PROPERTY(QVariantMap download READ download NOTIFY downloadChanged)
    Q_PROPERTY(QVariantMap groupChange READ groupChange NOTIFY groupChangeChanged)

public:
    enum Role {
        IsHeaderRole = Qt::UserRole + 1,
        GameIdRole,
        TitleRole,
        PlatformRole,
        GroupRole,
        CountRole,
        CollapsedRole,
        InstalledRole,
        HueRole,
        IconUrlRole,
        HiddenRole,
        UpdatingRole,
        SortValueRole,
    };

    Library(const QString &dataDir, const QString &cacheDir, NewLibrary newLibrary = NewLibrary::Plain,
            QObject *parent = nullptr);

    static Library *create(QQmlEngine *, QJSEngine *);

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    void startRefresh();
    LibraryStore &store() { return m_store; }
    const LibraryStore &store() const { return m_store; }

    QVariantList platforms() const;
    QVariantList gameCounts() const;
    int gameCount() const { return m_visibleCount; }
    bool loading() const { return m_store.loading(); }
    QStringList problems() const { return m_problems; }
    bool readOnly() const { return m_store.readOnly(); }
    QString readOnlyNotice() const { return m_store.versionStatus().notice; }
    bool setupPending() const { return m_store.setupPending(); }
    Q_INVOKABLE QString finishSetup() { return m_store.finishSetup(); }

    QString platformFilter() const { return m_platformFilter; }
    void setPlatformFilter(const QString &platform);

    QString search() const { return m_search; }
    void setSearch(const QString &search);

    QString groupBy() const { return m_groupBy; }
    void setGroupBy(const QString &groupBy);

    QString sort() const { return m_sort; }
    void setSort(const QString &sort);
    bool sortDescending() const { return m_sortDescending; }
    void setSortDescending(bool descending);
    static const QStringList &sortOrders();

    bool installedOnly() const { return m_installedOnly; }
    void setInstalledOnly(bool on);
    bool favoritesOnly() const { return m_favoritesOnly; }
    void setFavoritesOnly(bool on);
    QStringList tagFilter() const { return m_tagFilter; }
    void setTagFilter(const QStringList &tags);
    bool tagMatchAll() const { return m_tagMatchAll; }
    void setTagMatchAll(bool all);
    QStringList statusFilter() const { return m_statusFilter; }
    void setStatusFilter(const QStringList &statuses);
    QVariantList statuses() const;
    bool showHidden() const { return m_showHidden; }
    void setShowHidden(bool show);
    bool showSortValue() const { return m_showSortValue; }
    void setShowSortValue(bool show);
    bool filtersActive() const;
    int notInstalledHidden() const { return m_notInstalledHidden; }
    Q_INVOKABLE void clearFilters();

    Q_INVOKABLE void toggleGroup(const QString &group);
    Q_INVOKABLE int nextGameRow(int from, int step) const;
    Q_INVOKABLE int rowOfGame(const QString &id) const;
    Q_INVOKABLE int nextGroupRow(int from, int step) const;
    Q_INVOKABLE int collapseGroupAt(int row);
    Q_INVOKABLE int expandGroupNear(int row);
    Q_INVOKABLE QString gameIdAt(int row) const;

    Q_INVOKABLE QString setGameValue(const QString &id, const QString &key, const QVariant &value);
    QString recordPlaySession(const QString &id, int minutes, const QDateTime &endedAt);

    Q_INVOKABLE QVariantMap historyFor(const QString &id) const;
    Q_INVOKABLE void monthStats(int year, int month);
    Q_INVOKABLE void yearStats(int year);
    Q_INVOKABLE QString shortPlayTime(int minutes) const;
    Q_INVOKABLE QVariantMap deleteSession(const QString &id, const QString &end);
    Q_INVOKABLE QString restoreSession(const QString &id, const QVariantMap &session);

    Q_INVOKABLE QVariantMap gameAt(int row) const;

    QVariantList emulatorList() const { return m_emulatorList; }
    Q_INVOKABLE QString setEmulatorValue(const QString &id, const QString &key, const QVariant &value);
    Q_INVOKABLE QString emulatorIdFor(const QString &name) const { return GameId::slug(name); }
    Q_INVOKABLE QString addEmulator(const QString &name, const QString &startFrom = {}, const QString &id = {})
    {
        return m_store.addEmulator(id.isEmpty() ? GameId::slug(name) : id, name, startFrom);
    }
    Q_INVOKABLE QString removeEmulator(const QString &id) { return m_store.removeEmulator(id); }
    Q_INVOKABLE int pinnedGames(const QString &emulatorId) const;
    Q_INVOKABLE QString emulatorsFile() { return m_store.emulatorsFile(); }
    QVariantList platformSettings() const { return m_platformSettings; }
    Q_INVOKABLE QString setPlatformValue(const QString &id, const QString &key, const QVariant &value)
    {
        return m_store.setPlatformValue(id, key, value);
    }
    Q_INVOKABLE QString platformIdFor(const QString &name) const { return GameId::slug(name); }
    Q_INVOKABLE QString addPlatform(const QString &name, const QString &id = {})
    {
        return m_store.addPlatform(id.isEmpty() ? GameId::slug(name) : id, name);
    }
    Q_INVOKABLE QString removePlatform(const QString &id) { return m_store.removePlatform(id); }
    Q_INVOKABLE QString movePlatform(const QString &id, int step);
    Q_INVOKABLE QString movePlatformBefore(const QString &id, const QString &beforeId);

    Q_INVOKABLE QVariantList matchRomFolders(const QString &parentPath) const;

    Q_INVOKABLE QString platformsFile() { return m_store.platformsFile(); }
    bool scanning() const { return m_store.scanning(); }

    Q_INVOKABLE void scanNow() { m_store.scanRoms(); }

    Q_INVOKABLE void copyHeroesNear(int row);

    QVariantMap steam() const;
    QVariantList steamWaiting() const { return m_steamWaiting; }
    QVariantMap download() const { return m_download; }
    QVariantMap groupChange() const { return m_groupChange; }
    Q_INVOKABLE QString setSteamValue(const QString &key, const QVariant &value);
    Q_INVOKABLE QVariantList steamShortcuts() const;
    Q_INVOKABLE QVariantList steamCovers(int limit) const;
    Q_INVOKABLE QString addSteamShortcuts(const QVariantList &appIds);
    Q_INVOKABLE QString setSteamPresetForAll(const QString &preset, bool on);
    Q_INVOKABLE QString undoSteamEdits(const QString &id);
    Q_INVOKABLE QString undoAllSteamEdits();

    Q_INVOKABLE QVariantMap gameForEditing(const QString &id) const;
    Q_INVOKABLE bool steamPresetOn(const QString &launchOptions, const QString &preset) const;
    Q_INVOKABLE QString setSteamPreset(const QString &launchOptions, const QString &preset, bool on) const;
    Q_INVOKABLE QVariantMap emulatorChoices(const QString &platform) const;
    Q_INVOKABLE QVariantList igdbPlatformIds(const QString &platform) const;
    Q_INVOKABLE QVariantMap inspectRom(const QString &file) const;
    Q_INVOKABLE QStringList romNameFilters() const;
    Q_INVOKABLE QVariantMap addGame(const QString &file, const QString &platform, const QString &title,
                                    const QVariantMap &values, const QVariantMap &art);
    Q_INVOKABLE QString saveGame(const QString &id, const QVariantMap &values);
    Q_INVOKABLE QString locateProgram(const QString &id, const QString &file);
    Q_INVOKABLE QString setArt(const QString &id, const QString &kind, const QString &file);
    Q_INVOKABLE QString removeArt(const QString &id, const QString &kind);
    Q_INVOKABLE QString removeGame(const QString &id, bool wipe = false);
    Q_INVOKABLE QString unhideGame(const QString &id);
    Q_INVOKABLE QString setGamesValue(const QStringList &ids, const QString &key, const QVariant &value);
    Q_INVOKABLE QString retagGames(const QStringList &ids, const QStringList &add, const QStringList &remove);

    void setUndoDir(const QString &dir);
    LibraryUndo &undoLog() { return m_undo; }
    Q_INVOKABLE void beginEditUndo(const QString &id);
    Q_INVOKABLE void discardEditUndo();
    Q_INVOKABLE QString undo(quint64 token);

    Q_INVOKABLE QVariantMap uninstallChoices(const QString &id);
    QVariantMap uninstallProtonGame(const QString &id, bool files, bool prefix, bool shortcut,
                                    const QString &action, bool permanent);
    void setTrash(const Uninstall::Trash &trash) { m_trash = trash; }

    QVariantList tags() const { return m_tags; }
    QStringList missingDefaultTags() const { return m_missingDefaultTags; }
    QVariantList hiddenGames() const { return m_hiddenGames; }
    Q_INVOKABLE QString addTag(const QString &name, int color) { return m_store.addTag(name, color); }
    Q_INVOKABLE QString addDefaultTags() { return m_store.addDefaultTags(); }
    Q_INVOKABLE QString setTagColor(const QString &name, int color) { return m_store.setTagColor(name, color); }
    Q_INVOKABLE QString renameTag(const QString &from, const QString &to);
    Q_INVOKABLE QString removeTag(const QString &name);
    Q_INVOKABLE int nextTagColor(int paletteSize) const;
    Q_INVOKABLE QStringList igdbTags(const QStringList &facets, const QStringList &has) const;
    QList<std::pair<QString, QStringList>> tagMappings() const;

    const Game *game(const QString &id) const;

    QList<LookupGame> lookupGames() const;
    QHash<QString, QString> saveGames(const QList<std::pair<QString, QVariantMap>> &games);
    QHash<QString, QString> setArts(const QList<std::array<QString, 3>> &art);
    const Platform *platform(const QString &id) const;
    const QList<EmulatorDef> &emulators() const { return m_emulators; }
    const QList<Game> &allGames() const { return m_games; }
    const QHash<QString, EmulatorInstall> &installs() const { return m_store.snapshot().installs; }

signals:
    void platformFilterChanged();
    void searchChanged();
    void groupByChanged();
    void sortChanged();
    void showSortValueChanged();
    void filtersChanged();
    void notInstalledHiddenChanged();
    void platformsChanged();
    void gamesChanged();
    void loadingChanged();
    void problemsChanged();
    void emulatorsChanged();
    void platformSettingsChanged();
    void scanningChanged();
    void setupPendingChanged();
    void tagsChanged();
    void hiddenGamesChanged();
    void steamChanged();
    void steamWaitingChanged();
    void downloadChanged();
    void groupChangeChanged();
    void uninstallSizes(const QString &id, const QString &installDir, const QString &prefix);
    void reloaded();
    void monthStatsReady(int year, int month, const QVariantMap &stats);
    void yearStatsReady(int year, const QVariantMap &stats);
    void historyChanged(const QStringList &ids);
    // A change the user can undo; token 0 means it can't be, and `text` says so.
    void undoOffered(quint64 token, const QString &text);
    void undoExpired(quint64 token);

private:
    static QString gameFileOf(const QString &id);
    QString editGames(const QStringList &ids, const QString &text, const std::function<QString(const QString &)> &change);
    quint64 commitUndo(const QString &text);
    QString movePlatformWith(const QString &id, const std::function<QString()> &move);
    struct StatsInput;
    StatsInput statsInput() const;
    struct Row
    {
        int game = -1; // -1 = group header
        QString group;
        int count = 0;
    };

    void rebuild();
    void rebuildGroup(const QString &group);
    void updateOrder();
    QVariantMap steamChoices() const;
    QVariantMap inspectProgram(const QString &path) const;
    QVariantMap inspectLinuxProgram(const QString &path, LinuxProgramKind kind) const;
    QString linuxProgramProblem(const QString &path) const;
    QVariantList linuxPresets() const;
    QString protonPrefix(quint32 appId) const;
    Uninstall::Places uninstallPlaces(const QString &id) const;
    bool updatePlatforms();
    void filtersEdited();
    ListSettings listSettings() const;
    void applyListSettings(const ListSettings &settings);
    void loadListSettings();
    void saveListSettings();
    QString settingsFile() const { return m_store.dataDir() + QStringLiteral("/settings.toml"); }
    bool ensureSettingsFile() const;
    QVariantMap buildSteam() const;
    QVariantList buildSteamWaiting() const;
    void updateSteam();
    void updateDownloads();
    bool applyDownloads();
    void updateDownloadBar();
    QStringList effectiveTags() const;
    bool passesFilters(const Game &game, const QStringList &tags) const;
    QString collapseKey(const QString &group) const { return m_groupBy + u'/' + group; }
    bool expandsGroups() const { return !m_search.isEmpty() || filtersActive(); }
    QList<Row> computeRows(bool *showingGroups, int *notInstalled) const;
    void reloadFromStore();
    QUrl heroDisplayUrl(const Game &game) const;
    void makeNextHeroCopy();
    void heroCopyFinished();
    QString rowKey(const Row &row) const;
    bool applyRowChanges(const QList<Row> &rows, const QStringList &oldKeys, const QStringList &newKeys);
    bool applyRowMove(const QList<Row> &rows, const QStringList &oldKeys, const QStringList &newKeys);
    QString launchTip(const Game &game) const;
    QVariantList buildEmulatorList() const;
    QVariantList buildPlatformSettings() const;

    struct PlatformProblem
    {
        QString text;
        QString command;
        bool addToProblems = true;
    };
    QList<PlatformProblem> platformProblems(const Platform &known, const Platform &written,
                                            const QList<EmulatorDef> &emulators) const;

    static inline Library *s_instance = nullptr;

    LibraryStore m_store;
    LibraryUndo m_undo;
    std::function<void()> m_afterUndo;
    Uninstall::Trash m_trash = Uninstall::systemTrash;
    QList<Game> m_games;
    int m_visibleCount = 0;
    QStringList m_foldedTitles;
    QList<int> m_order;
    QList<Row> m_rows;
    QList<Platform> m_platforms;
    QList<Platform> m_allPlatforms;
    QList<EmulatorDef> m_emulators;
    QVariantList m_emulatorList;
    QVariantList m_platformSettings;
    QVariantList m_tags;
    QStringList m_missingDefaultTags;
    QVariantList m_hiddenGames;
    QHash<QString, QList<PlatformProblem>> m_platformProblems;
    QStringList m_problems;
    QSet<QString> m_collapsed;
    QString m_platformFilter;
    QString m_search;
    QStringList m_searchWords;
    QString m_sort = QStringLiteral("title");
    bool m_sortDescending = false;
    bool m_installedOnly = true;
    bool m_favoritesOnly = false;
    QStringList m_tagFilter;
    bool m_tagMatchAll = false;
    QStringList m_statusFilter;
    bool m_showHidden = false;
    bool m_showSortValue = true;
    int m_notInstalledHidden = 0;
    QSet<QString> m_tagNames;
    ListSettings m_savedList;
    QStringList m_listProblems;
    SteamSettings m_steamSettings = parseSteamSettings({}, {}).value;
    mutable QVariantMap m_steam;
    SteamDownloads m_downloads;
    QVariantList m_steamWaiting;
    QVariantMap m_download;
    QFileSystemWatcher m_settingsWatcher;
    QTimer m_settingsTimer;
    QVariantMap m_groupChange;
    QTimer m_groupChangeTimer;
    QString m_groupBy = QStringLiteral("platform");
    bool m_showingGroups = false;

    QString m_cacheDir;
    QHash<QString, QString> m_heroFiles;
    QSet<QString> m_heroSettled;
    QFutureWatcher<HeroCache::Result> m_heroCopy;
    QString m_heroCopyId;
    QString m_heroCopyStem;
    QString m_heroFocus;
    bool m_heroCopiesStarted = false;
    bool m_heroFailureLogged = false;
};
