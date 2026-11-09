#include "steamcollections.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>

namespace {
const QString kPrefix = QStringLiteral("user-collections.");
}

QString steamCollectionsPath(const QString &steamRoot, const QString &accountId)
{
    return steamRoot + QStringLiteral("/userdata/") + accountId
           + QStringLiteral("/config/cloudstorage/cloud-storage-namespace-1.json");
}

SteamCollections parseSteamCollections(const QByteArray &bytes)
{
    SteamCollections out;
    QJsonParseError error;
    const QJsonDocument doc = QJsonDocument::fromJson(bytes, &error);
    if (error.error != QJsonParseError::NoError || !doc.isArray()) {
        out.problems.append(QStringLiteral("Steam's collections file isn't the list of entries it was expected to be"));
        return out;
    }
    out.ok = true;

    QSet<QString> seen;
    for (const QJsonValue &item : doc.array()) {
        const QJsonArray pair = item.toArray();
        const QString key = pair.size() == 2 ? pair.at(0).toString() : QString();
        if (!key.startsWith(kPrefix))
            continue;
        const QString id = key.mid(kPrefix.size());
        const QJsonObject meta = pair.at(1).toObject();
        if (meta.value(QStringLiteral("is_deleted")).toBool())
            continue;
        // Steam keeps its own favorites and hidden games here; Lattis has both of its own.
        if (id == QLatin1String("favorite") || id == QLatin1String("hidden"))
            continue;
        if (seen.contains(id)) {
            out.problems.append(QStringLiteral("%1 is listed twice; using the first").arg(id));
            continue;
        }
        seen.insert(id);

        QJsonParseError valueError;
        const QJsonDocument valueDoc = QJsonDocument::fromJson(meta.value(QStringLiteral("value")).toString().toUtf8(), &valueError);
        if (valueError.error != QJsonParseError::NoError || !valueDoc.isObject()) {
            out.problems.append(QStringLiteral("%1: its value can't be read").arg(id));
            continue;
        }
        const QJsonObject value = valueDoc.object();
        if (value.contains(QStringLiteral("filterSpec"))) {
            ++out.dynamic;
            continue;
        }
        const QString name = value.value(QStringLiteral("name")).toString().trimmed();
        if (name.isEmpty()) {
            out.problems.append(QStringLiteral("%1: it has no name").arg(id));
            continue;
        }
        const QJsonValue added = value.value(QStringLiteral("added"));
        if (!added.isUndefined() && !added.isArray()) {
            out.problems.append(QStringLiteral("%1: its list of games can't be read").arg(id));
            continue;
        }

        SteamCollection collection;
        collection.steamId = id;
        collection.name = name;
        QSet<qint64> listed;
        int skipped = 0;
        for (const QJsonValue &app : added.toArray()) {
            const double number = app.toDouble(-1);
            const qint64 appId = qint64(number);
            if (!app.isDouble() || number != double(appId) || appId == 0)
                ++skipped;
            else if (!listed.contains(appId)) {
                listed.insert(appId);
                collection.appIds.append(appId);
            }
        }
        if (skipped > 0)
            out.problems.append(QStringLiteral("%1: %2 entries in its list of games aren't app ids; skipping them").arg(id).arg(skipped));
        out.collections.append(collection);
    }
    return out;
}
