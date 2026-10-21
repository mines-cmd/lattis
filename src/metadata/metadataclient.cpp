#include "metadataclient.h"
#include "apppaths.h"
#include "igdb.h"
#include "steamgriddb.h"

#include <QCryptographicHash>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonObject>
#include <QCoreApplication>
#include <QQmlEngine>
#include <QTimer>

#include <algorithm>
#include <memory>

namespace {
QString styleName(const QString &style)
{
    static const QHash<QString, QString> names = {
        {QStringLiteral("alternate"), QObject::tr("Alternate")}, {QStringLiteral("blurred"), QObject::tr("Blurred")},
        {QStringLiteral("white_logo"), QObject::tr("White logo")}, {QStringLiteral("material"), QObject::tr("Material")},
        {QStringLiteral("no_logo"), QObject::tr("No logo")},     {QStringLiteral("official"), QObject::tr("Official")},
        {QStringLiteral("white"), QObject::tr("White")},         {QStringLiteral("black"), QObject::tr("Black")},
        {QStringLiteral("custom"), QObject::tr("Custom")},
    };
    return names.value(style, style);
}

QVariantMap failure(const QString &error)
{
    return {{QStringLiteral("error"), error}};
}
}

MetadataClient::MetadataClient(const QString &dataDir, const QString &cacheDir, MetadataEndpoints endpoints,
                               QObject *parent)
    : QObject(parent), m_dataDir(dataDir), m_http(cacheDir, std::move(endpoints))
{
    m_http.removeDownloads();
    reloadKeys();
}

MetadataClient *MetadataClient::instance()
{
    if (s_instance)
        return s_instance;
    s_instance = new MetadataClient(AppPaths::dataDir(), AppPaths::cacheDir(), {}, QCoreApplication::instance());
    QJSEngine::setObjectOwnership(s_instance, QJSEngine::CppOwnership);
    const QString fixtures = qEnvironmentVariable("LATTIS_METADATA_FIXTURES");
    if (!fixtures.isEmpty()) {
        s_instance->m_http.setFixturesDir(fixtures);
        emit s_instance->keysChanged();
    }
    return s_instance;
}

bool MetadataClient::hasIgdbKeys() const
{
    return m_http.usesFixtures() || m_credentials.hasIgdb();
}

bool MetadataClient::hasSteamGridDbKey() const
{
    return m_http.usesFixtures() || m_credentials.hasSteamGridDb();
}

QVariantMap MetadataClient::keys() const
{
    return {{QStringLiteral("igdbClientId"), m_credentials.igdbClientId},
            {QStringLiteral("igdbClientSecret"), m_credentials.igdbClientSecret},
            {QStringLiteral("steamGridDbKey"), m_credentials.steamGridDbKey}};
}

void MetadataClient::reloadKeys()
{
    const Parsed<Credentials> parsed = CredentialsFile::load(m_dataDir);
    if (parsed.warnings != m_warnings) {
        for (const QString &warning : parsed.warnings)
            qCWarning(lcMetadata).noquote() << warning;
        m_warnings = parsed.warnings;
    }
    if (!parsed.ok || parsed.value == m_credentials)
        return;
    m_credentials = parsed.value;
    m_http.setCredentials(m_credentials);
    emit keysChanged();
}

QString MetadataClient::setKey(const QString &section, const QString &key, const QString &value)
{
    QString error;
    if (!CredentialsFile::setValue(m_dataDir, section, key, value, &error)) {
        qCWarning(lcMetadata).noquote() << "couldn't save credentials.toml:" << error;
        return tr("Couldn't save credentials.toml: %1").arg(error);
    }
    reloadKeys();
    return {};
}

QObject *MetadataClient::start(int *request)
{
    *request = m_nextRequest++;
    auto *context = new QObject(this);
    m_requests.insert(*request, context);
    return context;
}

void MetadataClient::finish(int request, QVariantMap result)
{
    if (QObject *context = m_requests.take(request))
        context->deleteLater();
    if (!result.contains(QStringLiteral("error")))
        result.insert(QStringLiteral("error"), QString());
    emit finished(request, result);
}

void MetadataClient::cancel(int request)
{
    delete m_requests.take(request);
}

QString MetadataClient::imageUrl(const QUrl &url, const QString &kind)
{
    if (url.isEmpty())
        return {};
    return (m_http.usesFixtures() ? m_http.placeholder(url, kind) : url).toString();
}

int MetadataClient::lookup(const QString &title, const QVariantList &igdbPlatforms, int igdbId, qint64 steamAppId,
                           int limit)
{
    int request = 0;
    QObject *context = start(&request);
    QList<int> platforms;
    for (const QVariant &p : igdbPlatforms)
        platforms.append(p.toInt());
    lookupQuery(request, context, title, platforms, igdbId, steamAppId, limit, false);
    return request;
}

void MetadataClient::lookupQuery(int request, QObject *context, const QString &title, const QList<int> &platforms,
                                 int igdbId, qint64 steamAppId, int limit, bool wildcard)
{
    const QByteArray query =
        igdbId > 0 ? Igdb::gameQuery(igdbId) : Igdb::searchQuery(title, platforms, limit, wildcard);
    m_http.igdb(QStringLiteral("games"), query, context,
                [this, request, context, title, platforms, igdbId, steamAppId, limit, wildcard](const MetadataReply &reply) {
                    if (!reply.ok())
                        return finish(request, failure(reply.error));
                    QString error;
                    const QList<IgdbGame> games = Igdb::parseGames(reply.json, platforms, &error);
                    if (!error.isEmpty())
                        return finish(request, failure(error));
                    if (games.isEmpty() && igdbId <= 0 && !wildcard)
                        return lookupQuery(request, context, title, platforms, igdbId, steamAppId, limit, true);
                    QVariantList candidates;
                    for (const IgdbGame &g : games)
                        candidates.append(candidate(g));
                    // A remembered id is the match, whatever its title.
                    const int match = igdbId > 0 ? (games.isEmpty() ? -1 : 0) : Igdb::clearMatch(games, title, steamAppId);
                    finish(request, {{QStringLiteral("candidates"), candidates}, {QStringLiteral("match"), match}});
                });
}

QVariantMap MetadataClient::candidate(const IgdbGame &g)
{
    QStringList names;
    for (int id : g.platforms) {
        if (const QString name = Igdb::platformName(id); !name.isEmpty())
            names.append(name);
    }
    return {
        {QStringLiteral("igdb"), g.id},
        {QStringLiteral("name"), g.name},
        {QStringLiteral("year"), g.year},
        {QStringLiteral("platforms"), names},
        {QStringLiteral("summary"), g.summary},
        {QStringLiteral("rating"), g.rating},
        {QStringLiteral("cover"), imageUrl(g.coverThumbnail(), QStringLiteral("cover"))},
        {QStringLiteral("coverLarge"), imageUrl(g.coverLarge(), QStringLiteral("cover"))},
        {QStringLiteral("steamAppId"), g.steamAppId},
        {QStringLiteral("facets"), g.facets},
    };
}

QVariantList MetadataClient::igdbPlatforms() const
{
    QList<IgdbPlatform> sorted = Igdb::platforms();
    std::sort(sorted.begin(), sorted.end(),
              [](const IgdbPlatform &a, const IgdbPlatform &b) { return a.name.localeAwareCompare(b.name) < 0; });
    QVariantList out;
    for (const IgdbPlatform &p : sorted)
        out.append(QVariantMap{{QStringLiteral("id"), p.id}, {QStringLiteral("name"), p.name}});
    return out;
}

int MetadataClient::lookupIds(const QHash<int, QList<int>> &platformsById)
{
    int request = 0;
    QObject *context = start(&request);
    QList<int> ids = platformsById.keys();
    std::sort(ids.begin(), ids.end());
    const QList<QList<int>> batches = Igdb::batches(ids);
    if (batches.isEmpty()) {
        QTimer::singleShot(0, context, [this, request] { finish(request, {{QStringLiteral("games"), QVariantMap()}}); });
        return request;
    }
    struct State
    {
        QVariantMap games;
        qsizetype left = 0;
    };
    auto state = std::make_shared<State>();
    state->left = batches.size();
    for (const QList<int> &batch : batches) {
        m_http.igdb(QStringLiteral("games"), Igdb::gamesQuery(batch), context,
                    [this, request, state, platformsById](const MetadataReply &reply) {
                        if (!m_requests.contains(request))
                            return; // an earlier batch failed
                        QString error = reply.error;
                        if (error.isEmpty() && !reply.json.isArray())
                            error = tr("IGDB sent an answer the launcher couldn't read");
                        if (!error.isEmpty())
                            return finish(request, failure(error));
                        for (const QJsonValue &v : reply.json.array()) {
                            const int id = v.toObject().value(QLatin1String("id")).toInt();
                            if (!platformsById.contains(id))
                                continue;
                            const QList<IgdbGame> games
                                = Igdb::parseGames(QJsonDocument(QJsonArray{v}), platformsById.value(id));
                            if (!games.isEmpty())
                                state->games.insert(QString::number(id), candidate(games.first()));
                        }
                        if (--state->left == 0)
                            finish(request, {{QStringLiteral("games"), state->games}});
                    });
    }
    return request;
}

int MetadataClient::findArt(const QString &name, int year, qint64 steamAppId)
{
    int request = 0;
    QObject *context = start(&request);
    if (steamAppId <= 0) {
        findArtByTitle(request, context, name, year, name);
        return request;
    }
    m_http.steamGridDb(SteamGridDb::steamPath(steamAppId), {}, context,
                       [this, request, context, name, year](const MetadataReply &reply) {
        const QList<SgdbGame> games = reply.ok() ? SteamGridDb::parseGames(reply.json) : QList<SgdbGame>();
        if (games.isEmpty())
            return findArtByTitle(request, context, name, year, name);
        finish(request, {{QStringLiteral("steamgriddb"), games.first().id},
                         {QStringLiteral("name"), games.first().name},
                         {QStringLiteral("candidates"), QVariantList()}});
    });
    return request;
}

void MetadataClient::findArtByTitle(int request, QObject *context, const QString &name, int year, const QString &query)
{
    m_http.steamGridDb(SteamGridDb::searchPath(query), {}, context,
                       [this, request, context, name, year, query](const MetadataReply &reply) {
        if (!reply.ok())
            return finish(request, failure(reply.error));
        QString error;
        const QList<SgdbGame> games = SteamGridDb::parseGames(reply.json, &error);
        if (!error.isEmpty())
            return finish(request, failure(error));
        // Nothing: SteamGridDB may know "Mortal Shell 2" only as "Mortal Shell II".
        if (const QString other = SteamGridDb::otherNumerals(name); games.isEmpty() && query == name && !other.isEmpty())
            return findArtByTitle(request, context, name, year, other);
        QVariantList candidates;
        for (const SgdbGame &g : games) {
            candidates.append(QVariantMap{{QStringLiteral("steamgriddb"), g.id},
                                          {QStringLiteral("name"), g.name},
                                          {QStringLiteral("year"), g.year}});
        }
        const int match = SteamGridDb::clearMatch(games, name, year);
        finish(request, {{QStringLiteral("steamgriddb"), match >= 0 ? games.at(match).id : 0},
                         {QStringLiteral("name"), match >= 0 ? games.at(match).name : QString()},
                         {QStringLiteral("candidates"), candidates}});
    });
}

int MetadataClient::searchArtGames(const QString &text)
{
    int request = 0;
    QObject *context = start(&request);
    m_http.steamGridDb(SteamGridDb::searchPath(text), {}, context, [this, request](const MetadataReply &reply) {
        if (!reply.ok())
            return finish(request, failure(reply.error));
        QString error;
        const QList<SgdbGame> games = SteamGridDb::parseGames(reply.json, &error);
        if (!error.isEmpty())
            return finish(request, failure(error));
        QVariantList out;
        for (const SgdbGame &g : games) {
            out.append(QVariantMap{{QStringLiteral("steamgriddb"), g.id},
                                   {QStringLiteral("name"), g.name},
                                   {QStringLiteral("year"), g.year}});
        }
        finish(request, {{QStringLiteral("games"), out}});
    });
    return request;
}

int MetadataClient::artChoices(int steamGridDbId, const QString &kind, const QString &style, int page)
{
    int request = 0;
    QObject *context = start(&request);
    const auto [path, query] = SteamGridDb::artRequest(steamGridDbId, kind, style, page);
    m_http.steamGridDb(path, query, context, [this, request, kind](const MetadataReply &reply) {
        if (!reply.ok())
            return finish(request, failure(reply.error));
        QString error;
        const QList<SgdbImage> images = SteamGridDb::parseImages(reply.json, kind, &error);
        if (!error.isEmpty())
            return finish(request, failure(error));
        QVariantList out;
        for (const SgdbImage &image : images) {
            out.append(QVariantMap{
                {QStringLiteral("id"), image.id},
                {QStringLiteral("url"), image.url.toString()},
                {QStringLiteral("thumbnail"), imageUrl(image.thumbnail.isEmpty() ? image.url : image.thumbnail, kind)},
                {QStringLiteral("width"), image.width},
                {QStringLiteral("height"), image.height},
                {QStringLiteral("style"), image.style},
                {QStringLiteral("author"), image.author},
            });
        }
        // The page as sent, before dropping what slipped through the filters, says whether
        // there's more: a page with fewer than the limit (50) is the last. The total can't
        // tell: it counts images the filters drop.
        const QJsonObject page = reply.json.object();
        const bool end = page.value(QLatin1String("data")).toArray().size() < page.value(QLatin1String("limit")).toInt(50);
        finish(request, {{QStringLiteral("images"), out}, {QStringLiteral("end"), end}});
    });
    return request;
}

int MetadataClient::download(const QString &url, const QString &kind)
{
    int request = 0;
    QObject *context = start(&request);
    const QUrl source(url);
    QString extension = QFileInfo(source.path()).suffix().toLower();
    if (extension.isEmpty() || extension.size() > 4)
        extension = QStringLiteral("png");
    const QString name = QStringLiteral("%1-%2.%3").arg(
        kind, QString::fromLatin1(QCryptographicHash::hash(source.toEncoded(), QCryptographicHash::Sha1).toHex().left(16)),
        extension);
    m_http.download(source, name, context, [this, request](const QString &path, const QString &error) {
        if (!error.isEmpty())
            return finish(request, failure(error));
        finish(request, {{QStringLiteral("path"), QUrl::fromLocalFile(path).toString()}});
    });
    return request;
}

int MetadataClient::testKeys(const QString &service)
{
    int request = 0;
    QObject *context = start(&request);
    const auto done = [this, request](const QString &error) { finish(request, failure(error)); };
    if (service == QLatin1String("igdb"))
        m_http.testIgdb(context, done);
    else
        m_http.testSteamGridDb(context, done);
    return request;
}

QVariantList MetadataClient::styles(const QString &kind) const
{
    QVariantList out;
    for (const QString &style : SteamGridDb::styles(kind))
        out.append(QVariantMap{{QStringLiteral("id"), style}, {QStringLiteral("name"), styleName(style)}});
    return out;
}
