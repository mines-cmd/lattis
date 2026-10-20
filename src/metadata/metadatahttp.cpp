#include "metadatahttp.h"
#include "gameid.h"

#include <QCryptographicHash>
#include <QDir>
#include <QColor>
#include <QFile>
#include <QFileInfo>
#include <QBuffer>
#include <QImage>
#include <QImageReader>
#include <QJsonArray>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QRegularExpression>
#include <QSaveFile>

Q_LOGGING_CATEGORY(lcMetadata, "launcher.metadata")

namespace {
const QString kTokenFile = QStringLiteral("igdb-token");
constexpr int kIgdbMaxOpen = 8; // IGDB's limit on open requests
constexpr int kTokenMarginSeconds = 60;

QByteArray largestAsPng(const QByteArray &data)
{
    QBuffer buffer;
    buffer.setData(data);
    buffer.open(QIODevice::ReadOnly);
    QImageReader reader(&buffer);
    QImage largest;
    for (int i = 0; i < std::max(1, reader.imageCount()); ++i) {
        if (i > 0 && !reader.jumpToImage(i))
            break;
        const QImage image = reader.read();
        if (!image.isNull() && image.width() * image.height() > largest.width() * largest.height())
            largest = image;
    }
    QByteArray out;
    QBuffer png(&out);
    png.open(QIODevice::WriteOnly);
    if (largest.isNull() || !largest.save(&png, "PNG"))
        return {};
    return out;
}
}

MetadataHttp::MetadataHttp(const QString &cacheDir, MetadataEndpoints endpoints, QObject *parent)
    : QObject(parent), m_cacheDir(cacheDir), m_endpoints(std::move(endpoints))
{
    m_igdbTimer.setSingleShot(true);
    connect(&m_igdbTimer, &QTimer::timeout, this, &MetadataHttp::pumpIgdb);
}

MetadataHttp::~MetadataHttp() = default;

void MetadataHttp::setCredentials(const Credentials &credentials)
{
    if (credentials.igdbClientId != m_credentials.igdbClientId
        || credentials.igdbClientSecret != m_credentials.igdbClientSecret) {
        // A token belongs to its client ID; loadToken() checks that the next time.
        m_token.clear();
        m_tokenLoaded = false;
    }
    m_credentials = credentials;
}

QNetworkAccessManager *MetadataHttp::network()
{
    if (!m_network) {
        qCDebug(lcMetadata) << "network manager created";
        m_network = new QNetworkAccessManager(this);
    }
    return m_network;
}

QString MetadataHttp::serviceName(Service service)
{
    return service == Service::SteamGridDb ? QStringLiteral("SteamGridDB") : QStringLiteral("IGDB");
}

void MetadataHttp::send(Service service, QNetworkReply *reply, QPointer<QObject> context, Callback done, bool json)
{
    // Without data for m_timeoutMs, not in total: a large image on a slow line still arrives.
    auto *timer = new QTimer(reply);
    timer->setSingleShot(true);
    connect(timer, &QTimer::timeout, reply, [reply] {
        reply->setProperty("timedOut", true);
        reply->abort();
    });
    connect(reply, &QNetworkReply::downloadProgress, timer, qOverload<>(&QTimer::start));
    timer->start(m_timeoutMs);

    connect(reply, &QNetworkReply::finished, this, [this, service, reply, context, json, done = std::move(done)] {
        reply->deleteLater();
        MetadataReply out;
        out.status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QString name = serviceName(service);
        const QByteArray body = reply->readAll();
        if (reply->property("timedOut").toBool()) {
            out.error = tr("Couldn't reach %1 (no answer in %2 s)").arg(name).arg(m_timeoutMs / 1000.0);
        } else if (out.status == 401 || out.status == 403 || (service == Service::Twitch && out.status == 400)) {
            out.error = tr("%1 didn't accept your keys").arg(name);
        } else if (out.status == 429) {
            out.error = tr("%1 is busy; try again in a minute").arg(name);
        } else if (out.status >= 400) {
            out.error = tr("%1 answered with an error (%2)").arg(name).arg(out.status);
        } else if (reply->error() != QNetworkReply::NoError) {
            out.error = tr("Couldn't reach %1").arg(name);
        } else if (!json) {
            out.body = body;
        } else {
            QJsonParseError parseError;
            out.json = QJsonDocument::fromJson(body, &parseError);
            if (parseError.error != QJsonParseError::NoError)
                out.error = tr("%1 sent an answer the launcher couldn't read").arg(name);
        }
        // The path only: a URL's query could hold a key.
        if (!out.ok())
            qCWarning(lcMetadata).noquote() << reply->url().path() << out.status << reply->errorString() << "→"
                                            << out.error;
        if (context)
            done(out);
    });
}

void MetadataHttp::igdb(const QString &endpoint, const QByteArray &query, QObject *context, Callback done)
{
    if (usesFixtures()) {
        static const QRegularExpression search(QStringLiteral("^search \"([^\"]*)\""));
        static const QRegularExpression byId(QStringLiteral("^where id = (\\d+)"));
        static const QRegularExpression byIds(QStringLiteral("^where id = \\(([\\d,]+)\\)"));
        const QString text = QString::fromUtf8(query);
        QString name;
        if (const auto m = byIds.match(text); m.hasMatch() && endpoint == QLatin1String("games")) {
            QJsonArray games;
            for (const QString &id : m.captured(1).split(u',', Qt::SkipEmptyParts)) {
                QFile file(QDir(m_fixturesDir).filePath(QStringLiteral("igdb-game-%1.json").arg(id)));
                if (file.open(QIODevice::ReadOnly)) {
                    for (const QJsonValue &game : QJsonDocument::fromJson(file.readAll()).array())
                        games.append(game);
                }
            }
            QTimer::singleShot(m_fixtureDelayMs, context,
                               [json = QJsonDocument(games), done = std::move(done)] { done({json, {}, 200}); });
            return;
        }
        if (const auto m = search.match(text); m.hasMatch()) {
            name = QStringLiteral("igdb-search-") + GameId::slug(m.captured(1));
            static const QRegularExpression platforms(QStringLiteral("where platforms = \\(([\\d,]+)\\)"));
            if (const auto p = platforms.match(text); p.hasMatch()) {
                const QString filtered = name + u'-' + p.captured(1).replace(u',', u'-');
                if (QFile::exists(QDir(m_fixturesDir).filePath(filtered + QStringLiteral(".json"))))
                    name = filtered;
            }
        } else if (const auto m = byId.match(text); m.hasMatch())
            name = QStringLiteral("igdb-game-") + m.captured(1);
        answerFromFixture(endpoint == QLatin1String("games") ? name : QString(), "[]", context, std::move(done));
        return;
    }
    m_igdbQueue.append({endpoint, query, context, std::move(done)});
    pumpIgdb();
}

void MetadataHttp::pumpIgdb()
{
    m_igdbQueue.removeIf([](const Pending &p) { return !p.context; });
    if (m_igdbQueue.isEmpty() || m_fetchingToken || m_igdbTimer.isActive())
        return;
    if (!m_credentials.hasIgdb()) {
        const QList<Pending> failed = std::exchange(m_igdbQueue, {});
        for (const Pending &p : failed) {
            QTimer::singleShot(0, p.context, [done = p.done] {
                done({{}, tr("Add your IGDB keys in Settings → Metadata"), 0});
            });
        }
        return;
    }
    if (!m_tokenLoaded)
        loadToken();
    if (m_token.isEmpty() || QDateTime::currentDateTimeUtc().secsTo(m_tokenExpires) < kTokenMarginSeconds) {
        fetchToken();
        return;
    }
    if (m_igdbOpen >= kIgdbMaxOpen)
        return;
    if (m_lastIgdbSend.isValid()) {
        const qint64 wait = m_igdbIntervalMs - m_lastIgdbSend.msecsTo(QDateTime::currentDateTimeUtc());
        if (wait > 0) {
            m_igdbTimer.start(int(wait));
            return;
        }
    }
    sendIgdb(m_igdbQueue.takeFirst());
    if (!m_igdbQueue.isEmpty())
        m_igdbTimer.start(m_igdbIntervalMs);
}

void MetadataHttp::sendIgdb(Pending pending)
{
    m_lastIgdbSend = QDateTime::currentDateTimeUtc();
    ++m_igdbOpen;
    QUrl url = m_endpoints.igdb;
    url.setPath(url.path() + u'/' + pending.endpoint);
    QNetworkRequest request(url);
    request.setRawHeader("Client-ID", m_credentials.igdbClientId.toUtf8());
    request.setRawHeader("Authorization", "Bearer " + m_token.toUtf8());
    request.setHeader(QNetworkRequest::ContentTypeHeader, QByteArrayLiteral("text/plain"));
    const QString sentToken = m_token;
    send(Service::Igdb, network()->post(request, pending.query), this,
         [this, pending, sentToken](const MetadataReply &reply) mutable {
             --m_igdbOpen;
             if (reply.status == 401 && !pending.retried) {
                 // The token expired or was revoked: fetch a new one and try once more.
                 if (m_token == sentToken) {
                     m_token.clear();
                     QFile::remove(QDir(m_cacheDir).filePath(kTokenFile));
                 }
                 pending.retried = true;
                 m_igdbQueue.prepend(pending);
             } else if (pending.context) {
                 pending.done(reply);
             }
             pumpIgdb();
         });
}

void MetadataHttp::fetchToken()
{
    m_fetchingToken = true;
    ++m_tokenFetches;
    QUrlQuery form;
    form.addQueryItem(QStringLiteral("client_id"), m_credentials.igdbClientId);
    form.addQueryItem(QStringLiteral("client_secret"), m_credentials.igdbClientSecret);
    form.addQueryItem(QStringLiteral("grant_type"), QStringLiteral("client_credentials"));
    QNetworkRequest request(m_endpoints.twitchToken);
    request.setHeader(QNetworkRequest::ContentTypeHeader, QByteArrayLiteral("application/x-www-form-urlencoded"));
    // In the body, not the URL, so the secret never shows up in a URL.
    send(Service::Twitch, network()->post(request, form.toString(QUrl::FullyEncoded).toUtf8()), this,
         [this](const MetadataReply &reply) {
             m_fetchingToken = false;
             const QJsonObject o = reply.json.object();
             QString error = reply.error;
             if (reply.ok() && o.value(QLatin1String("access_token")).toString().isEmpty())
                 error = tr("IGDB sent an answer the launcher couldn't read");
             if (!error.isEmpty()) {
                 const QList<Pending> failed = std::exchange(m_igdbQueue, {});
                 for (const Pending &p : failed) {
                     if (p.context)
                         p.done({{}, error, reply.status});
                 }
                 return;
             }
             m_token = o.value(QLatin1String("access_token")).toString();
             m_tokenExpires = QDateTime::currentDateTimeUtc().addSecs(o.value(QLatin1String("expires_in")).toInteger());
             saveToken();
             pumpIgdb();
         });
}

void MetadataHttp::loadToken()
{
    m_tokenLoaded = true;
    QFile file(QDir(m_cacheDir).filePath(kTokenFile));
    if (!file.open(QIODevice::ReadOnly))
        return;
    const QJsonObject o = QJsonDocument::fromJson(file.readAll()).object();
    if (o.value(QLatin1String("client_id")).toString() != m_credentials.igdbClientId)
        return;
    m_token = o.value(QLatin1String("access_token")).toString();
    m_tokenExpires = QDateTime::fromString(o.value(QLatin1String("expires_at")).toString(), Qt::ISODate);
}

void MetadataHttp::saveToken()
{
    QDir().mkpath(m_cacheDir);
    const QString path = QDir(m_cacheDir).filePath(kTokenFile);
    // QSaveFile keeps the permissions of the file it replaces but creates a new one with the umask's (usually
    // 0644), so the token's file exists, owner-only, before it's written.
    constexpr QFile::Permissions ownerOnly = QFile::ReadOwner | QFile::WriteOwner;
    if (QFile created(path); !created.open(QIODevice::WriteOnly | QIODevice::NewOnly, ownerOnly)
        && !QFile::setPermissions(path, ownerOnly))
        return;
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly))
        return;
    const QJsonObject o{{QStringLiteral("client_id"), m_credentials.igdbClientId},
                        {QStringLiteral("access_token"), m_token},
                        {QStringLiteral("expires_at"), m_tokenExpires.toString(Qt::ISODate)}};
    file.write(QJsonDocument(o).toJson(QJsonDocument::Compact));
    file.commit();
}

void MetadataHttp::steamGridDb(const QString &path, const QUrlQuery &query, QObject *context, Callback done)
{
    if (usesFixtures()) {
        static const QRegularExpression search(QStringLiteral("^/search/autocomplete/(.+)$"));
        static const QRegularExpression steam(QStringLiteral("^/games/steam/(\\d+)$"));
        static const QRegularExpression art(QStringLiteral("^/(grids|heroes|logos|icons)/game/(\\d+)$"));
        QString name;
        if (const auto m = search.match(path); m.hasMatch()) {
            name = QStringLiteral("sgdb-search-") + GameId::slug(QUrl::fromPercentEncoding(m.captured(1).toUtf8()));
        } else if (const auto m = steam.match(path); m.hasMatch()) {
            name = QStringLiteral("sgdb-steam-") + m.captured(1);
        } else if (const auto m = art.match(path); m.hasMatch()) {
            const int page = query.queryItemValue(QStringLiteral("page")).toInt();
            name = QStringLiteral("sgdb-%1-%2").arg(m.captured(1), m.captured(2));
            if (page > 0 || QFile::exists(QDir(m_fixturesDir).filePath(name + QStringLiteral("-page0.json"))))
                name += QStringLiteral("-page%1").arg(page);
        }
        answerFromFixture(name, R"({"success": true, "data": []})", context, std::move(done),
                          query.queryItemValue(QStringLiteral("styles")));
        return;
    }
    if (!m_credentials.hasSteamGridDb()) {
        QTimer::singleShot(0, context, [done = std::move(done)] {
            done({{}, tr("Add your SteamGridDB key in Settings → Metadata"), 0});
        });
        return;
    }
    QUrl url = m_endpoints.steamGridDb;
    url.setPath(url.path() + path, QUrl::TolerantMode);
    url.setQuery(query);
    QNetworkRequest request(url);
    request.setRawHeader("Authorization", "Bearer " + m_credentials.steamGridDbKey.toUtf8());
    send(Service::SteamGridDb, network()->get(request), context, std::move(done));
}

void MetadataHttp::answerFromFixture(const QString &name, const QByteArray &empty, QObject *context, Callback done,
                                     const QString &style)
{
    QByteArray body = empty;
    QFile file(QDir(m_fixturesDir).filePath(name + QStringLiteral(".json")));
    if (!name.isEmpty() && file.open(QIODevice::ReadOnly))
        body = file.readAll();
    QJsonDocument json = QJsonDocument::fromJson(body);
    if (!style.isEmpty()) {
        // The server filters by style; so does this.
        QJsonObject o = json.object();
        QJsonArray kept;
        for (const QJsonValue &v : o.value(QLatin1String("data")).toArray()) {
            if (v.toObject().value(QLatin1String("style")).toString() == style)
                kept.append(v);
        }
        o.insert(QStringLiteral("data"), kept);
        json = QJsonDocument(o);
    }
    QTimer::singleShot(m_fixtureDelayMs, context, [json, done = std::move(done)] { done({json, {}, 200}); });
}

QUrl MetadataHttp::placeholder(const QUrl &url, const QString &kind)
{
    const QString name = QString::fromLatin1(QCryptographicHash::hash(url.toEncoded(), QCryptographicHash::Md5).toHex());
    const QString path = QDir(m_cacheDir).filePath(QStringLiteral("fixture-images/%1-%2.png").arg(kind, name));
    if (!QFile::exists(path))
        writePlaceholder(path, kind, url.toString());
    return QUrl::fromLocalFile(path);
}

QString MetadataHttp::writePlaceholder(const QString &path, const QString &kind, const QString &seed)
{
    const QSize size = kind == QLatin1String("hero") ? QSize(960, 310)
        : kind == QLatin1String("logo")            ? QSize(400, 150)
        : kind == QLatin1String("icon")            ? QSize(128, 128)
                                                    : QSize(300, 450);
    const int hue = int(qHash(seed) % 360);
    const QColor from = QColor::fromHsv(hue, 150, 190).toRgb(); // RGB once, not per pixel
    const QColor to = QColor::fromHsv((hue + 40) % 360, 200, 90).toRgb();
    QImage image(size, QImage::Format_ARGB32);
    for (int y = 0; y < size.height(); ++y) {
        auto *line = reinterpret_cast<QRgb *>(image.scanLine(y));
        for (int x = 0; x < size.width(); ++x) {
            const double t = (double(x) / size.width() + double(y) / size.height()) / 2;
            const bool band = kind != QLatin1String("logo") || (y > size.height() / 4 && y < size.height() * 3 / 4);
            line[x] = band ? qRgb(int(from.red() + (to.red() - from.red()) * t),
                                  int(from.green() + (to.green() - from.green()) * t),
                                  int(from.blue() + (to.blue() - from.blue()) * t))
                           : qRgba(0, 0, 0, 0);
        }
    }
    QDir().mkpath(QFileInfo(path).absolutePath());
    // Fast, barely compressed: they are many, small and disposable.
    return image.save(path, "PNG", 100) ? path : QString();
}

QString MetadataHttp::downloadsDir() const
{
    return QDir(m_cacheDir).filePath(QStringLiteral("downloads"));
}

void MetadataHttp::removeDownloads()
{
    QDir(downloadsDir()).removeRecursively();
}

void MetadataHttp::download(const QUrl &url, const QString &fileName, QObject *context,
                            std::function<void(const QString &, const QString &)> done)
{
    // The library takes png, jpg and webp art; anything else (SteamGridDB's .ico icons) is
    // saved as a PNG of its largest image.
    static const QStringList kept = {QStringLiteral("png"), QStringLiteral("jpg"), QStringLiteral("jpeg"),
                                     QStringLiteral("webp")};
    const bool convert = !kept.contains(QFileInfo(fileName).suffix().toLower());
    const QString path = QDir(downloadsDir()).filePath(convert ? QFileInfo(fileName).completeBaseName() + QStringLiteral(".png")
                                                               : fileName);
    if (usesFixtures()) {
        const QString kind = fileName.section(u'-', 0, 0);
        QTimer::singleShot(m_fixtureDelayMs, context, [this, path, kind, url, done = std::move(done)] {
            const QString written = writePlaceholder(path, kind, url.toString());
            done(written, written.isEmpty() ? tr("Couldn't save the image") : QString());
        });
        return;
    }
    send(Service::SteamGridDb, network()->get(QNetworkRequest(url)), context,
         [this, path, convert, done = std::move(done)](const MetadataReply &reply) {
             if (!reply.ok()) {
                 done({}, reply.error);
                 return;
             }
             QDir().mkpath(downloadsDir());
             QByteArray body = reply.body;
             if (convert) {
                 body = largestAsPng(reply.body);
                 if (body.isEmpty()) {
                     done({}, tr("SteamGridDB sent an image the launcher can't read"));
                     return;
                 }
             }
             QSaveFile file(path);
             if (!file.open(QIODevice::WriteOnly) || file.write(body) < 0 || !file.commit()) {
                 qCWarning(lcMetadata) << "couldn't save" << path << file.errorString();
                 done({}, tr("Couldn't save the image"));
                 return;
             }
             done(path, {});
         },
         false);
}

void MetadataHttp::testIgdb(QObject *context, std::function<void(const QString &)> done)
{
    // A fresh token, so a changed secret is checked too.
    m_token.clear();
    m_tokenLoaded = true;
    igdb(QStringLiteral("games"), QByteArrayLiteral("fields id; limit 1;"), context,
         [done = std::move(done)](const MetadataReply &reply) {
             done(reply.ok() && !reply.json.isArray() ? tr("IGDB sent an answer the launcher couldn't read")
                                                      : reply.error);
         });
}

void MetadataHttp::testSteamGridDb(QObject *context, std::function<void(const QString &)> done)
{
    steamGridDb(QStringLiteral("/search/autocomplete/portal"), {}, context,
                [done = std::move(done)](const MetadataReply &reply) {
                    const bool success = reply.json.object().value(QLatin1String("success")).toBool();
                    done(reply.ok() && !success ? tr("SteamGridDB sent an answer the launcher couldn't read")
                                                : reply.error);
                });
}
