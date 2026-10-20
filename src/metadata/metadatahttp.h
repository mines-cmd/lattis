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
};

struct MetadataReply
{
    QJsonDocument json;
    QString error;
    int status = 0;
    QByteArray body;

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
    void download(const QUrl &url, const QString &fileName, QObject *context,
                  std::function<void(const QString &path, const QString &error)> done);
    QString downloadsDir() const;
    void removeDownloads();

    void testIgdb(QObject *context, std::function<void(const QString &error)> done);
    void testSteamGridDb(QObject *context, std::function<void(const QString &error)> done);

    void setFixturesDir(const QString &dir) { m_fixturesDir = dir; }
    bool usesFixtures() const { return !m_fixturesDir.isEmpty(); }
    QUrl placeholder(const QUrl &url, const QString &kind);

    void setTimeout(int ms) { m_timeoutMs = ms; }
    void setIgdbInterval(int ms) { m_igdbIntervalMs = ms; }
    void setFixtureDelay(int ms) { m_fixtureDelayMs = ms; }

    bool hasNetworkManager() const { return m_network != nullptr; }
    int tokenFetches() const { return m_tokenFetches; }

private:
    enum class Service { Twitch, Igdb, SteamGridDb };

    struct Pending
    {
        QString endpoint;
        QByteArray query;
        QPointer<QObject> context;
        Callback done;
        bool retried = false;
    };

    QNetworkAccessManager *network();
    void send(Service service, QNetworkReply *reply, QPointer<QObject> context, Callback done, bool json = true);
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
    MetadataEndpoints m_endpoints;
    Credentials m_credentials;
    QNetworkAccessManager *m_network = nullptr;
    int m_timeoutMs = 10000;
    int m_igdbIntervalMs = 260; // IGDB allows 4 requests a second
    int m_fixtureDelayMs = 300; // long enough for a screenshot to catch "Looking up…"

    QString m_token;
    QDateTime m_tokenExpires;
    bool m_tokenLoaded = false;
    bool m_fetchingToken = false;
    int m_tokenFetches = 0;

    QList<Pending> m_igdbQueue;
    QTimer m_igdbTimer;
    QDateTime m_lastIgdbSend;
    int m_igdbOpen = 0;
};
