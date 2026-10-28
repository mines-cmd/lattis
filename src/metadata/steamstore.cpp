#include "steamstore.h"
#include "namematch.h"

#include <QJsonArray>
#include <QJsonObject>

namespace {
// The host Steam's own store pages load assets from; cdn.cloudflare.steamstatic.com doesn't serve the
// hashed file names recent apps have.
const QString kAssetBase = QStringLiteral("https://shared.steamstatic.com/store_item_assets/");

struct Asset
{
    const char *largeKey;
    const char *smallKey;
    QString kind;
    int largeWidth;
    int largeHeight;
};

QUrl assetUrl(const QJsonObject &assets, const QString &fileName)
{
    QString format = assets.value(QLatin1String("asset_url_format")).toString();
    if (!format.contains(QLatin1String("${FILENAME}")) || fileName.isEmpty())
        return {};
    return QUrl(kAssetBase + format.replace(QLatin1String("${FILENAME}"), fileName));
}
}

namespace SteamStore {
std::pair<QString, QUrlQuery> itemsRequest(qint64 appId)
{
    const QJsonObject input{
        {QStringLiteral("ids"), QJsonArray{QJsonObject{{QStringLiteral("appid"), appId}}}},
        {QStringLiteral("context"), QJsonObject{{QStringLiteral("language"), QStringLiteral("english")},
                                                {QStringLiteral("country_code"), QStringLiteral("US")}}},
        {QStringLiteral("data_request"), QJsonObject{{QStringLiteral("include_assets"), true}}},
    };
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("input_json"),
                       QString::fromUtf8(QJsonDocument(input).toJson(QJsonDocument::Compact)));
    return {QStringLiteral("/IStoreBrowseService/GetItems/v1"), query};
}

QList<SteamImage> parseArt(const QJsonDocument &json, qint64 appId, QString *error)
{
    const QJsonValue response = json.object().value(QLatin1String("response"));
    if (!response.isObject()) {
        if (error)
            *error = QStringLiteral("Steam sent an answer the launcher couldn't read");
        return {};
    }
    QJsonObject assets;
    for (const QJsonValue &v : response.toObject().value(QLatin1String("store_items")).toArray()) {
        const QJsonObject item = v.toObject();
        if (item.value(QLatin1String("appid")).toInteger() == appId && item.value(QLatin1String("success")).toInt() == 1)
            assets = item.value(QLatin1String("assets")).toObject();
    }

    static const QList<Asset> kinds = {
        {"library_capsule_2x", "library_capsule", QStringLiteral("cover"), 600, 900},
        {"library_hero_2x", "library_hero", QStringLiteral("hero"), 3840, 1240},
    };
    QList<SteamImage> out;
    for (const Asset &kind : kinds) {
        if (const QUrl url = assetUrl(assets, assets.value(QLatin1String(kind.largeKey)).toString()); !url.isEmpty()) {
            out.append({kind.kind, url, kind.largeWidth, kind.largeHeight});
        } else if (const QUrl small = assetUrl(assets, assets.value(QLatin1String(kind.smallKey)).toString());
                   !small.isEmpty()) {
            out.append({kind.kind, small, 0, 0}); // the file's size varies by app
        }
    }
    // The logo isn't among the assets; it sits at a fixed path (404 for apps without one) and is shown
    // only for an app that has library art.
    if (!out.isEmpty()) {
        out.append({QStringLiteral("logo"),
                    QUrl(kAssetBase + QStringLiteral("steam/apps/%1/logo.png").arg(appId)), 640, 0});
    }
    return out;
}

std::pair<QString, QUrlQuery> searchRequest(const QString &title)
{
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("term"), title);
    query.addQueryItem(QStringLiteral("l"), QStringLiteral("english"));
    query.addQueryItem(QStringLiteral("cc"), QStringLiteral("US"));
    return {QStringLiteral("/api/storesearch/"), query};
}

QList<SteamStoreApp> parseSearch(const QJsonDocument &json, QString *error)
{
    const QJsonValue items = json.object().value(QLatin1String("items"));
    if (!items.isArray()) {
        if (error)
            *error = QStringLiteral("Steam sent an answer the launcher couldn't read");
        return {};
    }
    QList<SteamStoreApp> out;
    for (const QJsonValue &v : items.toArray()) {
        const QJsonObject item = v.toObject();
        const qint64 id = item.value(QLatin1String("id")).toInteger();
        // Bundles and packages have no library art.
        if (item.value(QLatin1String("type")).toString() == QLatin1String("app") && id > 0)
            out.append({id, item.value(QLatin1String("name")).toString()});
    }
    return out;
}

int clearMatch(const QList<SteamStoreApp> &apps, const QString &title)
{
    QList<QStringList> names;
    for (const SteamStoreApp &app : apps)
        names.append({app.name});
    return NameMatch::clearMatch(names, title);
}
}
