#pragma once

#include <QJsonDocument>
#include <QList>
#include <QString>
#include <QStringList>
#include <QUrl>
#include <QUrlQuery>

// See docs/code/metadata.md.
struct SgdbGame
{
    int id = 0;
    QString name;
    int year = 0; // 0 = unknown; can be an early-access date

    bool operator==(const SgdbGame &) const = default;
};

struct SgdbImage
{
    int id = 0;
    QUrl url;
    QUrl thumbnail;
    int width = 0;
    int height = 0;
    QString style;
    QString mime;
    QString author;

    bool operator==(const SgdbImage &) const = default;
};

namespace SteamGridDb {
QStringList styles(const QString &kind);
QString searchPath(const QString &title);
QString steamPath(qint64 appId);
QList<SgdbGame> parseGames(const QJsonDocument &json, QString *error = nullptr);
QString otherNumerals(const QString &title);
int clearMatch(const QList<SgdbGame> &games, const QString &title, int year);
std::pair<QString, QUrlQuery> artRequest(int gameId, const QString &kind, const QString &style, int page);
QList<SgdbImage> parseImages(const QJsonDocument &json, const QString &kind, QString *error = nullptr);
}
