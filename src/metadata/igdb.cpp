#include "igdb.h"
#include "searchtext.h"

#include <QDateTime>
#include <QJsonArray>
#include <QJsonObject>
#include <QSet>
#include <QTimeZone>

#include <algorithm>
#include <cmath>

namespace {
constexpr int kFullRelease = 6; // release_date_statuses
constexpr int kSteamSource = 1; // external_game_sources

const QByteArray kFields = "fields name, alternative_names.name, first_release_date, release_dates.date, "
                           "release_dates.platform, release_dates.status, platforms, summary, rating, rating_count, cover.image_id, "
                           "external_games.uid, external_games.external_game_source, genres, themes, game_modes, "
                           "keywords.slug;";

void appendFacets(QStringList *facets, const QJsonValue &list, QLatin1String prefix)
{
    for (const QJsonValue &v : list.toArray()) {
        const int id = v.isObject() ? v.toObject().value(QLatin1String("id")).toInt() : v.toInt();
        if (id > 0)
            facets->append(prefix + u':' + QString::number(id));
    }
}

int yearOf(qint64 secondsSinceEpoch)
{
    return QDateTime::fromSecsSinceEpoch(secondsSinceEpoch, QTimeZone::UTC).date().year();
}

int yearFor(const QJsonObject &game, const QList<int> &platforms)
{
    qint64 earliest = 0;
    bool found = false;
    for (const QJsonValue &v : game.value(QLatin1String("release_dates")).toArray()) {
        const QJsonObject date = v.toObject();
        const int status = date.value(QLatin1String("status")).toInt(kFullRelease);
        if (!date.contains(QLatin1String("date")) || status != kFullRelease
            || !platforms.contains(date.value(QLatin1String("platform")).toInt()))
            continue;
        const qint64 at = date.value(QLatin1String("date")).toInteger();
        if (!found || at < earliest) {
            earliest = at;
            found = true;
        }
    }
    if (found)
        return yearOf(earliest);
    if (game.contains(QLatin1String("first_release_date")))
        return yearOf(game.value(QLatin1String("first_release_date")).toInteger());
    return 0;
}
}

QUrl IgdbGame::coverThumbnail() const
{
    if (coverImageId.isEmpty())
        return {};
    return QUrl(QStringLiteral("https://images.igdb.com/igdb/image/upload/t_cover_small/%1.jpg").arg(coverImageId));
}

QUrl IgdbGame::coverLarge() const
{
    if (coverImageId.isEmpty())
        return {};
    return QUrl(QStringLiteral("https://images.igdb.com/igdb/image/upload/t_cover_big/%1.jpg").arg(coverImageId));
}

namespace Igdb {
double stars(double userRating, int ratingCount)
{
    if (ratingCount < kMinRatings)
        return -1;
    const double raw = userRating / 20;
    const double shrunk = (raw * ratingCount + kTypicalStars * kPriorRatings) / (ratingCount + kPriorRatings);
    return std::clamp(std::round(shrunk * 10) / 10, 0.0, 5.0);
}

const QList<IgdbPlatform> &platforms()
{
    static const QList<IgdbPlatform> list = {
        {3, QStringLiteral("Linux")},
        {4, QStringLiteral("N64")},
        {5, QStringLiteral("Wii")},
        {6, QStringLiteral("PC")},
        {7, QStringLiteral("PS1")},
        {8, QStringLiteral("PS2")},
        {9, QStringLiteral("PS3")},
        {11, QStringLiteral("Xbox")},
        {12, QStringLiteral("Xbox 360")},
        {13, QStringLiteral("DOS")},
        {14, QStringLiteral("Mac")},
        {18, QStringLiteral("NES")},
        {19, QStringLiteral("SNES")},
        {20, QStringLiteral("DS")},
        {21, QStringLiteral("GameCube")},
        {22, QStringLiteral("Game Boy Color")},
        {23, QStringLiteral("Dreamcast")},
        {24, QStringLiteral("GBA")},
        {29, QStringLiteral("Mega Drive")},
        {30, QStringLiteral("32X")},
        {32, QStringLiteral("Saturn")},
        {33, QStringLiteral("Game Boy")},
        {34, QStringLiteral("Android")},
        {35, QStringLiteral("Game Gear")},
        {37, QStringLiteral("3DS")},
        {38, QStringLiteral("PSP")},
        {39, QStringLiteral("iOS")},
        {41, QStringLiteral("Wii U")},
        {46, QStringLiteral("PS Vita")},
        {48, QStringLiteral("PS4")},
        {49, QStringLiteral("Xbox One")},
        {52, QStringLiteral("Arcade")},
        {59, QStringLiteral("Atari 2600")},
        {64, QStringLiteral("Master System")},
        {78, QStringLiteral("Sega CD")},
        {79, QStringLiteral("Neo Geo MVS")},
        {80, QStringLiteral("Neo Geo AES")},
        {82, QStringLiteral("Web browser")},
        {86, QStringLiteral("PC Engine")},
        {87, QStringLiteral("Virtual Boy")},
        {130, QStringLiteral("Switch")},
        {137, QStringLiteral("New 3DS")},
        {167, QStringLiteral("PS5")},
        {169, QStringLiteral("Xbox Series")},
        {508, QStringLiteral("Switch 2")},
    };
    return list;
}

QString platformName(int id)
{
    for (const IgdbPlatform &p : platforms()) {
        if (p.id == id)
            return p.name;
    }
    return {};
}

QByteArray searchQuery(const QString &title, const QList<int> &platforms, int limit, bool wildcard)
{
    // Apicalypse strings have no escapes worth relying on; quotes and backslashes don't help
    // a search anyway.
    QString search = title;
    search.remove(u'"').remove(u'\\');
    QByteArray where = "version_parent = null & game_type = (0,4,8,9,10,11,12)";
    if (!platforms.isEmpty()) {
        QStringList ids;
        for (int id : platforms)
            ids.append(QString::number(id));
        where = "platforms = (" + ids.join(u',').toLatin1() + ") & " + where;
    }
    if (wildcard) {
        return "where name ~ *\"" + search.simplified().toUtf8() + "\"* & " + where + "; " + kFields + " limit "
               + QByteArray::number(limit) + ';';
    }
    return "search \"" + search.simplified().toUtf8() + "\"; where " + where + "; " + kFields + " limit "
           + QByteArray::number(limit) + ';';
}

QByteArray gameQuery(int id)
{
    return "where id = " + QByteArray::number(id) + "; " + kFields;
}

QByteArray gamesQuery(const QList<int> &ids)
{
    QByteArrayList numbers;
    for (int id : ids)
        numbers.append(QByteArray::number(id));
    return "where id = (" + numbers.join(',') + "); " + kFields + " limit " + QByteArray::number(kBatchSize) + ';';
}

QList<QList<int>> batches(const QList<int> &ids)
{
    QList<QList<int>> out;
    for (qsizetype i = 0; i < ids.size(); i += kBatchSize)
        out.append(ids.mid(i, kBatchSize));
    return out;
}

QList<IgdbGame> parseGames(const QJsonDocument &json, const QList<int> &platforms, QString *error)
{
    if (!json.isArray()) {
        if (error)
            *error = QStringLiteral("IGDB sent an answer the launcher couldn't read");
        return {};
    }
    QList<IgdbGame> out;
    for (const QJsonValue &v : json.array()) {
        const QJsonObject o = v.toObject();
        IgdbGame g;
        g.id = o.value(QLatin1String("id")).toInt();
        g.name = o.value(QLatin1String("name")).toString();
        if (g.id <= 0 || g.name.isEmpty())
            continue;
        for (const QJsonValue &a : o.value(QLatin1String("alternative_names")).toArray())
            g.alternativeNames.append(a.toObject().value(QLatin1String("name")).toString());
        for (const QJsonValue &p : o.value(QLatin1String("platforms")).toArray())
            g.platforms.append(p.toInt());
        g.year = yearFor(o, platforms);
        g.summary = o.value(QLatin1String("summary")).toString().trimmed();
        if (const QJsonValue user = o.value(QLatin1String("rating")); user.isDouble())
            g.rating = stars(user.toDouble(), o.value(QLatin1String("rating_count")).toInt());
        g.coverImageId = o.value(QLatin1String("cover")).toObject().value(QLatin1String("image_id")).toString();
        for (const QJsonValue &e : o.value(QLatin1String("external_games")).toArray()) {
            const QJsonObject external = e.toObject();
            if (external.value(QLatin1String("external_game_source")).toInt() == kSteamSource) {
                g.steamAppId = external.value(QLatin1String("uid")).toString().toLongLong();
                if (g.steamAppId > 0)
                    break;
            }
        }
        appendFacets(&g.facets, o.value(QLatin1String("genres")), QLatin1String("genre"));
        appendFacets(&g.facets, o.value(QLatin1String("themes")), QLatin1String("theme"));
        appendFacets(&g.facets, o.value(QLatin1String("game_modes")), QLatin1String("mode"));
        for (const QJsonValue &k : o.value(QLatin1String("keywords")).toArray()) {
            if (const QString slug = k.toObject().value(QLatin1String("slug")).toString(); !slug.isEmpty())
                g.facets.append(QStringLiteral("keyword:") + slug);
        }
        out.append(g);
    }
    return out;
}

int clearMatch(const QList<IgdbGame> &games, const QString &title, qint64 steamAppId)
{
    const auto only = [](const QList<int> &indexes) { return indexes.size() == 1 ? indexes.first() : -1; };
    // A Steam game's app id is surer than its title, which can differ ("… Intergrade").
    QList<int> steam;
    for (qsizetype i = 0; steamAppId > 0 && i < games.size(); ++i) {
        if (games.at(i).steamAppId == steamAppId)
            steam.append(int(i));
    }
    if (steam.size() == 1)
        return steam.first();

    const QString folded = SearchText::fold(title);
    if (folded.isEmpty())
        return -1;
    QList<int> same;
    for (qsizetype i = 0; i < games.size(); ++i) {
        const IgdbGame &g = games.at(i);
        bool matches = SearchText::fold(g.name) == folded;
        for (qsizetype a = 0; !matches && a < g.alternativeNames.size(); ++a)
            matches = SearchText::fold(g.alternativeNames.at(a)) == folded;
        if (matches)
            same.append(int(i));
    }
    return only(same);
}

QStringList fittingTags(const QStringList &facets, const QList<std::pair<QString, QStringList>> &tags,
                        const QStringList &has)
{
    const QSet<QString> game(facets.cbegin(), facets.cend());
    QStringList out;
    for (const auto &[name, igdb] : tags) {
        if (has.contains(name) || out.contains(name))
            continue;
        if (std::any_of(igdb.cbegin(), igdb.cend(), [&](const QString &entry) { return game.contains(entry); }))
            out.append(name);
    }
    return out;
}
}
