#pragma once

#include "metadatahttp.h"

#include <QHash>
#include <QObject>
#include <QTimer>
#include <QVariantMap>
#include <QtQmlIntegration>

class QJSEngine;
class QQmlEngine;

// See docs/code/metadata.md.
class ProtonDb : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

public:
    static ProtonDb *instance();
    static ProtonDb *create(QQmlEngine *, QJSEngine *) { return instance(); }

    ProtonDb(MetadataHttp *http, const QString &cacheDir, QObject *parent = nullptr);

    // What is known about the game: state "checking" (nothing yet, a request is due), "ready" (tier, trending,
    // total; possibly stale while a new one loads), "none" (no reports) or "failed" (asked again when selected again, once per run).
    Q_INVOKABLE QVariantMap summary(qint64 appId);

    void setWaitMs(int ms) { m_waitMs = ms; }
    void setNow(qint64 secsSinceEpoch) { m_now = secsSinceEpoch; }

    static constexpr qint64 kFreshSeconds = 7 * 24 * 3600;
    static constexpr qint64 kNoReportsSeconds = 24 * 3600;

signals:
    void changed(qint64 appId);

private:
    struct Entry
    {
        QString tier;
        QString trending;
        int total = 0;
        qint64 fetched = 0;
        bool noReports = false;
    };

    qint64 now() const;
    bool fresh(const Entry &entry) const;
    void load();
    void save() const;
    void schedule(qint64 appId);
    void pump();
    void answered(qint64 appId, const MetadataReply &reply);
    QVariantMap describe(qint64 appId) const;

    MetadataHttp *m_http;
    QString m_cacheDir;
    QHash<qint64, Entry> m_entries;
    QHash<qint64, int> m_failures;
    qint64 m_waiting = 0; // waiting out the 300 ms
    qint64 m_ready = 0; // waited, queued behind the request in flight
    qint64 m_inFlight = 0;
    qint64 m_lastAsked = 0;
    QTimer m_wait;
    int m_waitMs = 300;
    qint64 m_now = 0;
    bool m_loaded = false;
};
