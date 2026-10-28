#pragma once

#include <QJsonDocument>
#include <QList>
#include <QString>
#include <QUrl>
#include <QUrlQuery>

// See docs/code/metadata.md.
struct SteamImage
{
    QString kind; // cover, hero or logo
    QUrl url;
    int width = 0;
    int height = 0; // 0 = not known

    bool operator==(const SteamImage &) const = default;
};

struct SteamStoreApp
{
    qint64 id = 0;
    QString name;

    bool operator==(const SteamStoreApp &) const = default;
};

namespace SteamStore {
std::pair<QString, QUrlQuery> itemsRequest(qint64 appId);
QList<SteamImage> parseArt(const QJsonDocument &json, qint64 appId, QString *error = nullptr);
std::pair<QString, QUrlQuery> searchRequest(const QString &title);
QList<SteamStoreApp> parseSearch(const QJsonDocument &json, QString *error = nullptr);
int clearMatch(const QList<SteamStoreApp> &apps, const QString &title);
}
