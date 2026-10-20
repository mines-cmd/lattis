#pragma once

#include <QByteArray>
#include <QJsonDocument>
#include <QList>
#include <QString>
#include <QStringList>
#include <QUrl>

#include <utility>

// See docs/code/metadata.md.
struct IgdbGame
{
    int id = 0;
    QString name;
    QStringList alternativeNames;
    int year = 0;
    QList<int> platforms;
    QString summary;
    double rating = -1; // members' rating as stars, 0 to 5 to a tenth; -1 = not rated
    QString coverImageId;
    qint64 steamAppId = 0;
    QStringList facets;

    QUrl coverThumbnail() const;
    QUrl coverLarge() const;

    bool operator==(const IgdbGame &) const = default;
};

struct IgdbPlatform
{
    int id = 0;
    QString name;
};

namespace Igdb {
constexpr int kSearchLimit = 10;

// wildcard: IGDB's search only matches whole words, so this matches the text anywhere in the name instead.
QByteArray searchQuery(const QString &title, const QList<int> &platforms, int limit = kSearchLimit, bool wildcard = false);

const QList<IgdbPlatform> &platforms();
QString platformName(int id);

QByteArray gameQuery(int id);

constexpr int kBatchSize = 500;

QByteArray gamesQuery(const QList<int> &ids);
QList<QList<int>> batches(const QList<int> &ids);
// IGDB members' rating (0-100) as stars to a tenth, pulled toward a typical game's the fewer ratings it has;
// -1 below kMinRatings.
constexpr int kMinRatings = 20;
constexpr int kPriorRatings = 30;
constexpr double kTypicalStars = 3.5;
double stars(double userRating, int ratingCount);

QList<IgdbGame> parseGames(const QJsonDocument &json, const QList<int> &platforms, QString *error = nullptr);

int clearMatch(const QList<IgdbGame> &games, const QString &title, qint64 steamAppId = 0);

QStringList fittingTags(const QStringList &facets, const QList<std::pair<QString, QStringList>> &tags,
                        const QStringList &has);
}
