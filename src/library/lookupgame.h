#pragma once

#include <QList>
#include <QString>
#include <QStringList>
#include <QUrl>
#include <QVariantMap>

// See docs/code/library.md.
struct LookupGame
{
    QString id;
    QString title;
    QString platform;
    QString platformName;
    QUrl icon;
    bool hasDescription = false;
    bool hasYear = false;
    bool hasRating = false;
    QString description;
    int year = 0;
    double rating = -1;
    bool ratingFromIgdb = false;
    QStringList art;
    QVariantMap artUrls;
    QStringList tags;
    int igdb = -1;
    int steamgriddb = -1;
    qint64 steamAppId = 0;
    QList<int> igdbPlatforms;

    QStringList missing() const
    {
        QStringList out;
        if (!hasDescription)
            out.append(QStringLiteral("description"));
        if (!hasYear)
            out.append(QStringLiteral("year"));
        if (!hasRating)
            out.append(QStringLiteral("rating"));
        for (const QString &kind : {QStringLiteral("cover"), QStringLiteral("hero"), QStringLiteral("logo"),
                                    QStringLiteral("icon")}) {
            if (!art.contains(kind))
                out.append(kind);
        }
        return out;
    }
};
