#pragma once

#include "bulklookup.h"

#include <QHash>
#include <QObject>
#include <QStringList>
#include <QVariantMap>
#include <QtQmlIntegration>

#include <array>
#include <functional>

class MetadataClient;
class QQmlEngine;
class QJSEngine;

// See docs/code/metadata.md.
class SteamArtFetch : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(bool running READ running NOTIFY runningChanged)
    Q_PROPERTY(int progressDone READ progressDone NOTIFY progressChanged)
    Q_PROPERTY(int progressTotal READ progressTotal NOTIFY progressChanged)

public:
    struct Wanted
    {
        QString id;
        qint64 appId = 0;
        QStringList kinds;
    };
    using Source = std::function<QList<Wanted>(const QStringList &ids)>;

    SteamArtFetch(Source source, BulkLookup::ArtWriter art, MetadataClient *client = nullptr, QObject *parent = nullptr);
    ~SteamArtFetch() override;
    static SteamArtFetch *create(QQmlEngine *, QJSEngine *);

    void add(const QStringList &ids);

    bool running() const { return m_active > 0 || !m_waiting.isEmpty(); }
    int progressDone() const { return m_done; }
    int progressTotal() const { return m_total; }

signals:
    void runningChanged();
    void progressChanged();
    void finished(int written);

private:
    struct Job
    {
        Wanted wanted;
        int pending = 0;
        QList<std::array<QString, 3>> files;
    };

    void pump();
    void onFinished(int request, const QVariantMap &result);
    void jobDone(Job *job);
    void flush(bool all);
    MetadataClient *client() const;

    Source m_source;
    BulkLookup::ArtWriter m_writeArt;
    MetadataClient *m_client = nullptr;
    QList<Wanted> m_waiting;
    QHash<QString, Job *> m_jobs;
    // request -> game id (and the kind, for a download)
    QHash<int, QString> m_lookups;
    QHash<int, std::pair<QString, QString>> m_downloads;
    QList<std::array<QString, 3>> m_ready;
    int m_active = 0;
    int m_done = 0;
    int m_total = 0;
    int m_written = 0;
    bool m_connected = false;
    static inline SteamArtFetch *s_instance = nullptr;
};
