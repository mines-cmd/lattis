#include "steamgriddb.h"
#include "searchtext.h"

#include <QDateTime>
#include <QHash>
#include <QJsonArray>
#include <QJsonObject>
#include <QTimeZone>

namespace {
// Covers are the tall Steam capsules; the wide ones would be cropped.
const QStringList kCoverSizes = {QStringLiteral("600x900"), QStringLiteral("342x482"), QStringLiteral("660x930")};

QString unreadable()
{
    return QStringLiteral("SteamGridDB sent an answer the launcher couldn't read");
}

QJsonValue data(const QJsonDocument &json, QString *error)
{
    const QJsonObject o = json.object();
    if (!o.value(QLatin1String("success")).toBool() || !o.contains(QLatin1String("data"))) {
        if (error)
            *error = unreadable();
        return {};
    }
    return o.value(QLatin1String("data"));
}

const QStringList kRoman = {QString(),        QStringLiteral("I"),     QStringLiteral("II"),   QStringLiteral("III"),
                             QStringLiteral("IV"),   QStringLiteral("V"),     QStringLiteral("VI"),   QStringLiteral("VII"),
                             QStringLiteral("VIII"), QStringLiteral("IX"),    QStringLiteral("X"),    QStringLiteral("XI"),
                             QStringLiteral("XII"),  QStringLiteral("XIII"),  QStringLiteral("XIV"),  QStringLiteral("XV"),
                             QStringLiteral("XVI"),  QStringLiteral("XVII"),  QStringLiteral("XVIII"), QStringLiteral("XIX"),
                             QStringLiteral("XX")};

int sequelNumber(const QString &word)
{
    bool ok = false;
    const int n = word.toInt(&ok);
    if (ok)
        return n >= 2 && n <= 20 && word == QString::number(n) ? n : 0;
    const int roman = int(kRoman.indexOf(word.toUpper()));
    return roman >= 2 ? roman : 0;
}

// Folded, with sequel numbers as digits: "Mortal Shell II" and "Mortal Shell 2" are the same.
QString foldedTitle(const QString &title)
{
    QStringList words = SearchText::fold(title).split(u' ', Qt::SkipEmptyParts);
    for (QString &word : words) {
        if (const int n = sequelNumber(word))
            word = QString::number(n);
    }
    return words.join(u' ');
}
}

namespace SteamGridDb {
QString otherNumerals(const QString &title)
{
    QStringList words = title.simplified().split(u' ', Qt::SkipEmptyParts);
    bool changed = false;
    for (QString &word : words) {
        // Punctuation after it stays: "Hades II:" → "Hades 2:".
        qsizetype end = word.size();
        while (end > 0 && !word.at(end - 1).isLetterOrNumber())
            --end;
        const QString core = word.first(end);
        const int n = sequelNumber(core);
        if (n == 0)
            continue;
        word = (core.front().isDigit() ? kRoman.at(n) : QString::number(n)) + word.sliced(end);
        changed = true;
    }
    return changed ? words.join(u' ') : QString();
}

QStringList styles(const QString &kind)
{
    if (kind == QLatin1String("cover"))
        return {QStringLiteral("alternate"), QStringLiteral("blurred"), QStringLiteral("white_logo"),
                QStringLiteral("material"), QStringLiteral("no_logo")};
    if (kind == QLatin1String("hero"))
        return {QStringLiteral("alternate"), QStringLiteral("blurred"), QStringLiteral("material")};
    if (kind == QLatin1String("logo"))
        return {QStringLiteral("official"), QStringLiteral("white"), QStringLiteral("black"), QStringLiteral("custom")};
    if (kind == QLatin1String("icon"))
        return {QStringLiteral("official"), QStringLiteral("custom")};
    return {};
}

QString searchPath(const QString &title)
{
    return QStringLiteral("/search/autocomplete/") + QString::fromLatin1(QUrl::toPercentEncoding(title.simplified()));
}

QString steamPath(qint64 appId)
{
    return QStringLiteral("/games/steam/%1").arg(appId);
}

QList<SgdbGame> parseGames(const QJsonDocument &json, QString *error)
{
    const QJsonValue d = data(json, error);
    const QJsonArray list = d.isArray() ? d.toArray() : QJsonArray{d};
    QList<SgdbGame> out;
    for (const QJsonValue &v : list) {
        const QJsonObject o = v.toObject();
        SgdbGame g;
        g.id = o.value(QLatin1String("id")).toInt();
        g.name = o.value(QLatin1String("name")).toString();
        if (g.id <= 0 || g.name.isEmpty())
            continue;
        if (const qint64 released = o.value(QLatin1String("release_date")).toInteger(); released > 0)
            g.year = QDateTime::fromSecsSinceEpoch(released, QTimeZone::UTC).date().year();
        out.append(g);
    }
    return out;
}

int clearMatch(const QList<SgdbGame> &games, const QString &title, int year)
{
    const QString folded = foldedTitle(title);
    if (folded.isEmpty())
        return -1;
    QList<int> same;
    for (qsizetype i = 0; i < games.size(); ++i) {
        if (foldedTitle(games.at(i).name) == folded)
            same.append(int(i));
    }
    if (same.size() > 1 && year > 0)
        same.removeIf([&](int i) { return games.at(i).year != year; });
    return same.size() == 1 ? same.first() : -1;
}

std::pair<QString, QUrlQuery> artRequest(int gameId, const QString &kind, const QString &style, int page)
{
    static const QHash<QString, QString> paths = {{QStringLiteral("cover"), QStringLiteral("grids")},
                                                  {QStringLiteral("hero"), QStringLiteral("heroes")},
                                                  {QStringLiteral("logo"), QStringLiteral("logos")},
                                                  {QStringLiteral("icon"), QStringLiteral("icons")}};
    QUrlQuery query;
    if (kind == QLatin1String("cover"))
        query.addQueryItem(QStringLiteral("dimensions"), kCoverSizes.join(u','));
    if (!style.isEmpty())
        query.addQueryItem(QStringLiteral("styles"), style);
    query.addQueryItem(QStringLiteral("types"), QStringLiteral("static"));
    query.addQueryItem(QStringLiteral("nsfw"), QStringLiteral("false"));
    query.addQueryItem(QStringLiteral("humor"), QStringLiteral("false"));
    if (page > 0)
        query.addQueryItem(QStringLiteral("page"), QString::number(page));
    return {QStringLiteral("/%1/game/%2").arg(paths.value(kind)).arg(gameId), query};
}

QList<SgdbImage> parseImages(const QJsonDocument &json, const QString &kind, QString *error)
{
    const QJsonValue d = data(json, error);
    if (!d.isArray()) {
        if (error)
            *error = unreadable();
        return {};
    }
    QList<SgdbImage> out;
    for (const QJsonValue &v : d.toArray()) {
        const QJsonObject o = v.toObject();
        SgdbImage image;
        image.id = o.value(QLatin1String("id")).toInt();
        image.url = QUrl(o.value(QLatin1String("url")).toString());
        image.thumbnail = QUrl(o.value(QLatin1String("thumb")).toString());
        image.width = o.value(QLatin1String("width")).toInt();
        image.height = o.value(QLatin1String("height")).toInt();
        image.style = o.value(QLatin1String("style")).toString();
        image.mime = o.value(QLatin1String("mime")).toString();
        image.author = o.value(QLatin1String("author")).toObject().value(QLatin1String("name")).toString();
        const bool animated = image.mime == QLatin1String("image/gif") || image.mime == QLatin1String("image/webp");
        const bool unsafe = o.value(QLatin1String("nsfw")).toBool() || o.value(QLatin1String("humor")).toBool();
        const bool wrongSize = kind == QLatin1String("cover")
            && !kCoverSizes.contains(QStringLiteral("%1x%2").arg(image.width).arg(image.height));
        if (image.id <= 0 || !image.url.isValid() || animated || unsafe || wrongSize)
            continue;
        out.append(image);
    }
    return out;
}
}
