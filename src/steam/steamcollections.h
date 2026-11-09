#pragma once

#include <QByteArray>
#include <QList>
#include <QString>
#include <QStringList>

// See docs/code/steam.md.
struct SteamCollection
{
    QString steamId;
    QString name;
    QList<qint64> appIds;

    bool operator==(const SteamCollection &) const = default;
};

struct SteamCollections
{
    bool ok = false;
    QList<SteamCollection> collections;
    int dynamic = 0;
    QStringList problems;
};

QString steamCollectionsPath(const QString &steamRoot, const QString &accountId);

SteamCollections parseSteamCollections(const QByteArray &bytes);
