#pragma once

#include "credentials.h"

#include <QDateTime>
#include <QJsonDocument>
#include <QList>
#include <QLoggingCategory>
#include <QObject>
#include <QPointer>
#include <QTimer>
#include <QUrl>
#include <QUrlQuery>

#include <functional>

class QNetworkAccessManager;
class QNetworkReply;

Q_DECLARE_LOGGING_CATEGORY(lcMetadata)

// See docs/code/metadata.md.
struct MetadataEndpoints
{
    QUrl twitchToken = QUrl(QStringLiteral("https://id.twitch.tv/oauth2/token"));
    QUrl igdb = QUrl(QStringLiteral("https://api.igdb.com/v4"));
    QUrl steamGridDb = QUrl(QStringLiteral("https://www.steamgriddb.com/api/v2"));
    QUrl steamStore = QUrl(QStringLiteral("https://api.steampowered.com"));
    QUrl steamStoreSearch = QUrl(QStringLiteral("https://store.steampowered.com"));
    QUrl steamWeb = QUrl(QStringLiteral("https://api.steampowered.com"));
    QUrl protonDb = QUrl(QStringLiteral("https://www.protondb.com"));
};

struct MetadataReply
{
    QJsonDocument json;
    QString error;
    int status = 0;
    QByteArray body;
    int retryAfterSeconds = -1; // the Retry-After header; -1 when there is none

    bool ok() const { return error.isEmpty(); }
};

class MetadataHttp : public QObject
{
    Q_OBJECT

public:
    using Callback = std::function<void(const MetadataReply &)>;

    MetadataHttp(const QString &cacheDir, MetadataEndpoints endpoints = {}, QObject *parent = nullptr);
    ~MetadataHttp() override;

    void setCredentials(const Credentials &credentials);
    const Credentials &credentials() const { return m_credentials; }

    void igdb(const QString &endpoint, const QByteArray &query, QObject *context, Callback done);
    void steamGridDb(const QString &path, const QUrlQuery &query, QObject *context, Callback done);
    void steamStore(const QString &path, const QUrlQuery &query, QObject *context, Callback done);
    void steamStoreSearch(const QString &path, const QUrlQuery &query, QObject *context, Callback done);
    void steamWeb(const QString &path, const QUrlQuery &query, QObject *context, Callback done);
    void protonDb(const QString &path, QObject *context, Callback done);
    void download(const QUrl &url, const QString &fileName, QObject *context,
                  std::function<void(const QString &path, const QString &error, int status)> done,
                  const QString &folder = QStringLiteral("editor"));
    // `folder` is one of the subfolders each user of the downloads has; empty is the root.
    QString downloadsDir(const QString &folder = {}) const;
    void removeDownloads(const QString &folder = {});

    void testIgdb(QObject *context, std::function<void(const QString &error)> done);
    void testSteamGridDb(QObject *context, std::function<void(const QString &error)> done);

    void setFixturesDir(const QString &dir) { m_fixturesDir = dir; }
    bool usesFixtures() const { return !m_fixturesDir.isEmpty(); }
    void setSteamWebFixturesDir(const QString &dir) { m_steamWebFixturesDir = dir; }
    QUrl placeholder(const QUrl &url, const QString &kind);

    void setTimeout(int ms) { m_timeoutMs = ms; }
    void setIgdbInterval(int ms) { m_igdbIntervalMs = ms; }
    // Tests: scales the wait after a 429 or a 5xx (Retry-After, or 5 s without one).
    void setIgdbRetryScale(double scale) { m_igdbRetryScale = scale; }
    void setFixtureDelay(int ms) { m_fixtureDelayMs = ms; }
    // Tests: the HTTP status a fixture-mode download answers with for a URL (0 = it works).
    void setFixtureDownloadStatus(std::function<int(const QUrl &)> status) { m_fixtureDownloadStatus = std::move(status); }
    void setProtonDbInterval(int ms) { m_protonDbIntervalMs = ms; }

    bool hasNetworkManager() const { return m_network != nullptr; }
    int tokenFetches() const { return m_tokenFetches; }

private:
    enum class Service { Twitch, Igdb, SteamGridDb, Steam, ProtonDb };

    struct Pending
    {
        QString endpoint;
        QByteArray query;
        QPointer<QObject> context;
        Callback done;
        bool retried = false;
        int attempts = 0; // sends so far that ended in a 429 or a 5xx
    };

    QNetworkAccessManager *network();
    // A reply is stopped once its body passes `limit` bytes (0: an API answer's).
    void send(Service service, QNetworkReply *reply, QPointer<QObject> context, Callback done, bool json = true,
              qint64 limit = 0);
    void pumpIgdb();
    void sendIgdb(Pending pending);
    void fetchToken();
    void loadToken();
    void saveToken();
    static QString serviceName(Service service);
    void answerFromFixture(const QString &name, const QByteArray &empty, QObject *context, Callback done,
                           const QString &style = {});
    QString writePlaceholder(const QString &path, const QString &kind, const QString &seed);

    QString m_cacheDir;
    QString m_fixturesDir;
    QString m_steamWebFixturesDir;
    MetadataEndpoints m_endpoints;
    Credentials m_credentials;
    QNetworkAccessManager *m_network = nullptr;
    int m_timeoutMs = 10000;
    int m_igdbIntervalMs = 260; // IGDB allows 4 requests a second
    std::function<int(const QUrl &)> m_fixtureDownloadStatus;
    int m_fixtureDelayMs = 300; // long enough for a screenshot to catch "Looking up…"
    int m_protonDbIntervalMs = 1000; // ProtonDB documents no limit: stay well under one request a second
    QDateTime m_nextProtonDbSend;

    QString m_token;
    QDateTime m_tokenExpires;
    bool m_tokenLoaded = false;
    bool m_fetchingToken = false;
    int m_tokenFetches = 0;
    int m_shortTokenUses = 0; // requests a token too short-lived to keep may still be sent with

    QList<Pending> m_igdbQueue;
    QTimer m_igdbTimer;
    QDateTime m_lastIgdbSend;
    int m_igdbOpen = 0;
    double m_igdbRetryScale = 1.0;
};
