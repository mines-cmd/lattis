#pragma once

#include "idlistmodel.h"
#include "lookupgame.h"

#include <QElapsedTimer>
#include <QHash>
#include <QMap>
#include <QObject>
#include <QSet>
#include <QVariantList>
#include <QtQmlIntegration>

#include <array>
#include <functional>

class MetadataClient;

// See docs/code/metadata.md.
class BulkLookup : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(BulkLookupModel)

    Q_PROPERTY(QVariantList games READ games NOTIFY gamesChanged)
    Q_PROPERTY(IdListModel *gameModel READ gameModel CONSTANT)
    Q_PROPERTY(int missingCount READ missingCount NOTIFY gamesChanged)
    Q_PROPERTY(QVariantMap missingCounts READ missingCounts NOTIFY gamesChanged)
    Q_PROPERTY(int checkedCount READ checkedCount NOTIFY gamesChanged)
    Q_PROPERTY(QVariantList platforms READ platforms NOTIFY gamesChanged)
    Q_PROPERTY(QStringList allTags READ allTags NOTIFY gamesChanged)
    Q_PROPERTY(QVariantMap tagCounts READ tagCounts NOTIFY gamesChanged)
    Q_PROPERTY(int notOnIgdbCount READ notOnIgdbCount NOTIFY gamesChanged)
    Q_PROPERTY(QString platform READ platform WRITE setPlatform NOTIFY filtersChanged)
    Q_PROPERTY(QStringList missing READ missing WRITE setMissing NOTIFY filtersChanged)
    Q_PROPERTY(QStringList tags READ tags WRITE setTags NOTIFY filtersChanged)
    Q_PROPERTY(QString search READ search WRITE setSearch NOTIFY filtersChanged)
    Q_PROPERTY(bool includeNotOnIgdb READ includeNotOnIgdb WRITE setIncludeNotOnIgdb NOTIFY filtersChanged)
    Q_PROPERTY(bool replaceExisting READ replaceExisting WRITE setReplaceExisting NOTIFY filtersChanged)
    Q_PROPERTY(bool includeComplete READ includeComplete WRITE setIncludeComplete NOTIFY filtersChanged)
    Q_PROPERTY(bool suggestTags READ suggestTags WRITE setSuggestTags NOTIFY filtersChanged)

    Q_PROPERTY(QVariantList results READ results NOTIFY resultsChanged)
    Q_PROPERTY(IdListModel *resultModel READ resultModel CONSTANT)
    Q_PROPERTY(QString resultFilter READ resultFilter WRITE setResultFilter NOTIFY resultFilterChanged)
    Q_PROPERTY(QVariantMap stateCounts READ stateCounts NOTIFY resultsChanged)
    Q_PROPERTY(QVariantMap categories READ categories NOTIFY resultsChanged)
    Q_PROPERTY(bool hasArt READ hasArt NOTIFY resultsChanged)
    Q_PROPERTY(bool running READ running NOTIFY progressChanged)
    Q_PROPERTY(int done READ done NOTIFY progressChanged)
    Q_PROPERTY(int total READ total NOTIFY progressChanged)
    Q_PROPERTY(int secondsLeft READ secondsLeft NOTIFY progressChanged) // -1 = not known yet
    Q_PROPERTY(int applyCount READ applyCount NOTIFY resultsChanged)
    Q_PROPERTY(bool applying READ applying NOTIFY progressChanged)

public:
    using Source = std::function<QList<LookupGame>()>;
    using ValueWriter = std::function<QHash<QString, QString>(const QList<std::pair<QString, QVariantMap>> &)>;
    using ArtWriter = std::function<QHash<QString, QString>(const QList<std::array<QString, 3>> &)>;
    using TagMappings = std::function<QList<std::pair<QString, QStringList>>()>;

    explicit BulkLookup(QObject *parent = nullptr);
    static void setLibrary(Source source, ValueWriter values, ArtWriter art, TagMappings tags);
    BulkLookup(MetadataClient *client, Source source, ValueWriter values, ArtWriter art, TagMappings tags = {},
               QObject *parent = nullptr);

    QVariantList games() const { return m_shown; }
    IdListModel *gameModel() { return &m_gameModel; }
    int missingCount() const;
    QVariantMap missingCounts() const;
    int checkedCount() const;
    QVariantList platforms() const;
    QStringList allTags() const;
    QVariantMap tagCounts() const { return m_tagCounts; }
    int notOnIgdbCount() const { return m_notOnIgdbCount; }
    QString platform() const { return m_platform; }
    void setPlatform(const QString &platform);
    QStringList missing() const { return m_missing; }
    void setMissing(const QStringList &missing);
    QStringList tags() const { return m_tags; }
    void setTags(const QStringList &tags);
    QString search() const { return m_search; }
    void setSearch(const QString &search);
    bool includeNotOnIgdb() const { return m_includeNotOnIgdb; }
    void setIncludeNotOnIgdb(bool include);
    bool replaceExisting() const { return m_replaceExisting; }
    void setReplaceExisting(bool replace);
    bool includeComplete() const { return m_includeComplete; }
    void setIncludeComplete(bool include);
    bool suggestTags() const { return m_suggestTags; }
    void setSuggestTags(bool suggest);

    QVariantList results() const;
    IdListModel *resultModel() { return &m_resultModel; }
    QString resultFilter() const { return m_resultFilter; }
    void setResultFilter(const QString &state);
    QVariantMap stateCounts() const;
    QVariantMap categories() const;
    bool hasArt() const;
    bool running() const { return !m_queue.isEmpty() || !m_lookups.isEmpty() || !m_prefetching.isEmpty(); }
    int done() const { return m_done; }
    int total() const { return m_total; }
    int secondsLeft() const;
    int applyCount() const;
    bool applying() const { return m_applying; }

    Q_INVOKABLE void reload();
    Q_INVOKABLE void setChecked(const QString &id, bool checked);
    Q_INVOKABLE void setAllChecked(bool checked);
    Q_INVOKABLE void clearFilters();

    Q_INVOKABLE void start();
    Q_INVOKABLE void stop();
    Q_INVOKABLE void pick(const QString &id, int candidate);
    Q_INVOKABLE void retry(const QString &id, const QString &title);
    Q_INVOKABLE void choose(const QString &id, const QVariantMap &candidate, const QString &searched);
    Q_INVOKABLE void markNotOnIgdb(const QString &id);
    Q_INVOKABLE void unmarkNotOnIgdb(const QString &id);
    Q_INVOKABLE void setFieldChecked(const QString &id, const QString &key, bool checked);
    Q_INVOKABLE void setTagChecked(const QString &id, const QString &name, bool checked);
    Q_INVOKABLE void setCategoryChecked(const QString &category, bool checked);
    Q_INVOKABLE void setArtChecked(const QString &id, const QString &kind, bool checked);
    Q_INVOKABLE void setArt(const QString &id, const QString &kind, const QVariantMap &image);
    Q_INVOKABLE int steamGridDbId(const QString &id) const;
    Q_INVOKABLE void clearResults();

    Q_INVOKABLE void apply();

    static constexpr int kInFlight = 3;

signals:
    void gamesChanged();
    void filtersChanged();
    void resultsChanged();
    void resultFilterChanged();
    void progressChanged();
    void applied(int written);

private:
    enum class State { Running, Matched, Pick, NotFound, Failed, NotOnIgdb };
    struct Art
    {
        QVariantMap image;
        QVariantMap byDefault;
        bool checked = false;
        bool none = false;
    };
    struct Row
    {
        LookupGame game;
        QString searched;
        State state = State::Running;
        State before = State::Running;
        QString error;
        QVariantList candidates;
        QVariantMap match;
        QList<std::pair<QString, QVariant>> fields;
        QSet<QString> uncheckedFields;
        bool replace = false;
        QSet<QString> replaces;
        QStringList tags;
        QSet<QString> uncheckedTags;
        QMap<QString, Art> art;
        int igdb = -1;
        int steamgriddb = 0;
        int pending = 0;
        QString applyError;
    };
    enum class Step { Lookup, FindArt, ArtChoices, Download, Prefetch };
    struct Request
    {
        QString id;
        Step step;
        QString kind;
    };

    void refilter();
    enum Section { PlatformSection = 1, TagsSection = 2, NotOnIgdbSection = 4 };
    bool listed(const LookupGame &game) const;
    bool matches(const LookupGame &game, int skip) const;
    bool shown(const LookupGame &game) const;
    QVariantList resultList(const QString &state) const;
    static QString stateName(State state);
    Row *row(const QString &id);
    void prefetch();
    void pump();
    void lookUp(Row &row);
    void resolve(Row &row, const QVariantMap &candidate);
    void finished(int request, const QVariantMap &result);
    void rowDone(Row &row);
    void cancelRow(const QString &id);
    void pumpDownloads();
    void flushArt(bool all);
    void applyFinished();

    static inline Source s_source;
    static inline ValueWriter s_writeValues;
    static inline ArtWriter s_writeArt;
    static inline TagMappings s_tagMappings;

    MetadataClient *m_client = nullptr;
    Source m_source;
    ValueWriter m_writeValues;
    ArtWriter m_writeArt;
    TagMappings m_tagMappings;

    QList<LookupGame> m_games;
    QVariantList m_shown;
    QHash<QString, int> m_platformCounts;
    QVariantMap m_tagCounts;
    int m_notOnIgdbCount = 0;
    QString m_resultFilter;
    IdListModel m_gameModel;
    IdListModel m_resultModel;
    // Checked games. A game is checked when it's first seen, if it's missing something and not
    // marked as not on IGDB; games the filters show later (another game's, "Replace existing")
    // start unchecked, so they don't change a selection.
    QSet<QString> m_checked;
    QSet<QString> m_seen;
    QString m_platform;
    static inline const QStringList kCategories = {QStringLiteral("description"), QStringLiteral("year"),
                                                   QStringLiteral("rating"), QStringLiteral("art")};
    QStringList m_missing = kCategories;
    QStringList m_tags;
    QString m_search;
    bool m_includeNotOnIgdb = false;
    bool m_replaceExisting = false;
    bool m_suggestTags = true;
    bool m_includeComplete = false;

    QList<Row> m_rows;
    QList<LookupGame> m_queue;
    QHash<int, Request> m_requests;
    QSet<QString> m_lookups;
    // Fetching games by IGDB id ahead of their turn: the games a request still has to answer,
    // and what came back (a game IGDB didn't send has none).
    QHash<int, QHash<QString, int>> m_prefetching;
    QHash<QString, QVariantMap> m_prefetched;
    int m_nextLocalRequest = -1; // answers from m_prefetched, delivered like the client's
    int m_done = 0;
    int m_total = 0;
    QElapsedTimer m_clock;

    bool m_applying = false;
    QList<std::array<QString, 3>> m_toDownload;
    QList<std::array<QString, 3>> m_downloaded;
    int m_downloading = 0;
    QSet<QString> m_applyIds;
};
