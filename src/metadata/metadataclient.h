#pragma once

#include "credentials.h"
#include "metadatahttp.h"

#include <QHash>
#include <QObject>
#include <QVariantMap>
#include <QtQmlIntegration>

struct IgdbGame;
class QQmlEngine;
class QJSEngine;

// See docs/code/metadata.md.
class MetadataClient : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(Metadata)
    QML_SINGLETON

    Q_PROPERTY(bool hasIgdbKeys READ hasIgdbKeys NOTIFY keysChanged)
    Q_PROPERTY(bool hasSteamGridDbKey READ hasSteamGridDbKey NOTIFY keysChanged)
    Q_PROPERTY(QVariantMap keys READ keys NOTIFY keysChanged)

public:
    MetadataClient(const QString &dataDir, const QString &cacheDir, MetadataEndpoints endpoints = {},
                   QObject *parent = nullptr);
    static MetadataClient *instance();
    static MetadataClient *create(QQmlEngine *, QJSEngine *) { return instance(); }

    MetadataHttp *http() { return &m_http; }

    bool hasIgdbKeys() const;
    bool hasSteamGridDbKey() const;
    QVariantMap keys() const;

    Q_INVOKABLE void reloadKeys();
    Q_INVOKABLE QString setKey(const QString &section, const QString &key, const QString &value);

    Q_INVOKABLE int lookup(const QString &title, const QVariantList &igdbPlatforms, int igdbId = 0,
                           qint64 steamAppId = 0, int limit = 10);
    Q_INVOKABLE QVariantList igdbPlatforms() const;
    int lookupIds(const QHash<int, QList<int>> &platformsById);
    Q_INVOKABLE int findArt(const QString &name, int year, qint64 steamAppId = 0);
    Q_INVOKABLE int searchArtGames(const QString &text);
    Q_INVOKABLE int artChoices(int steamGridDbId, const QString &kind, const QString &style, int page);
    Q_INVOKABLE int download(const QString &url, const QString &kind);
    Q_INVOKABLE int testKeys(const QString &service);
    Q_INVOKABLE void cancel(int request);
    Q_INVOKABLE void discardDownloads() { m_http.removeDownloads(); }

    Q_INVOKABLE QVariantList styles(const QString &kind) const;

signals:
    void finished(int request, const QVariantMap &result);
    void keysChanged();

private:
    // A context for one request: deleting it (cancel) drops the result.
    QObject *start(int *request);
    void lookupQuery(int request, QObject *context, const QString &title, const QList<int> &platforms, int igdbId,
                     qint64 steamAppId, int limit, bool wildcard);
    void finish(int request, QVariantMap result);
    QString imageUrl(const QUrl &url, const QString &kind);
    QVariantMap candidate(const IgdbGame &game);
    void findArtByTitle(int request, QObject *context, const QString &name, int year, const QString &query);

    QString m_dataDir;
    Credentials m_credentials;
    QStringList m_warnings;
    MetadataHttp m_http;
    int m_nextRequest = 1;
    QHash<int, QObject *> m_requests;
    static inline MetadataClient *s_instance = nullptr;
};
