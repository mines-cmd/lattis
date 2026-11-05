#include "protondb.h"
#include "apppaths.h"
#include "metadataclient.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>

namespace {
const QString kCacheFile = QStringLiteral("protondb.json");
constexpr int kMaxFailures = 2; // the first try and one more, per run
}

ProtonDb *ProtonDb::instance()
{
    static ProtonDb *protonDb = new ProtonDb(MetadataClient::instance()->http(), AppPaths::cacheDir(),
                                             QCoreApplication::instance());
    return protonDb;
}

ProtonDb::ProtonDb(MetadataHttp *http, const QString &cacheDir, QObject *parent)
    : QObject(parent), m_http(http), m_cacheDir(cacheDir)
{
    m_wait.setSingleShot(true);
    connect(&m_wait, &QTimer::timeout, this, [this] {
        m_ready = std::exchange(m_waiting, 0);
        pump();
    });
}

qint64 ProtonDb::now() const
{
    return m_now ? m_now : QDateTime::currentSecsSinceEpoch();
}

bool ProtonDb::fresh(const Entry &entry) const
{
    return now() - entry.fetched < (entry.noReports ? kNoReportsSeconds : kFreshSeconds);
}

void ProtonDb::load()
{
    m_loaded = true;
    QFile file(QDir(m_cacheDir).filePath(kCacheFile));
    if (!file.open(QIODevice::ReadOnly))
        return;
    const QJsonObject apps = QJsonDocument::fromJson(file.readAll()).object().value(QLatin1String("apps")).toObject();
    for (auto it = apps.begin(); it != apps.end(); ++it) {
        const qint64 id = it.key().toLongLong();
        const QJsonObject o = it.value().toObject();
        if (id <= 0 || !o.value(QLatin1String("fetched")).isDouble())
            continue;
        Entry entry;
        entry.fetched = o.value(QLatin1String("fetched")).toInteger();
        entry.noReports = o.value(QLatin1String("none")).toBool();
        entry.tier = o.value(QLatin1String("tier")).toString();
        entry.trending = o.value(QLatin1String("trending")).toString();
        entry.total = o.value(QLatin1String("total")).toInt();
        if (entry.noReports || !entry.tier.isEmpty())
            m_entries.insert(id, entry);
    }
}

void ProtonDb::save() const
{
    QJsonObject apps;
    for (auto it = m_entries.cbegin(); it != m_entries.cend(); ++it) {
        QJsonObject o{{QStringLiteral("fetched"), it->fetched}};
        if (it->noReports) {
            o.insert(QStringLiteral("none"), true);
        } else {
            o.insert(QStringLiteral("tier"), it->tier);
            o.insert(QStringLiteral("trending"), it->trending);
            o.insert(QStringLiteral("total"), it->total);
        }
        apps.insert(QString::number(it.key()), o);
    }
    QDir().mkpath(m_cacheDir);
    QSaveFile file(QDir(m_cacheDir).filePath(kCacheFile));
    if (file.open(QIODevice::WriteOnly)) {
        file.write(QJsonDocument(QJsonObject{{QStringLiteral("apps"), apps}}).toJson(QJsonDocument::Compact));
        file.commit();
    }
}

QVariantMap ProtonDb::summary(qint64 appId)
{
    if (appId <= 0)
        return {{QStringLiteral("state"), QStringLiteral("none")}};
    if (!m_loaded)
        load();
    const auto entry = m_entries.constFind(appId);
    const bool stale = entry == m_entries.cend() || !fresh(*entry);
    const bool busy = appId == m_waiting || appId == m_ready || appId == m_inFlight;
    const int failures = m_failures.value(appId);
    // A failed game is asked again when it's selected again, not when its own failure re-evaluates the chip.
    const bool reselected = appId != m_lastAsked;
    m_lastAsked = appId;
    if (stale && !busy && failures < kMaxFailures && (failures == 0 || reselected))
        schedule(appId);
    return describe(appId);
}

QVariantMap ProtonDb::describe(qint64 appId) const
{
    const auto entry = m_entries.constFind(appId);
    if (entry == m_entries.cend()) {
        const bool busy = appId == m_waiting || appId == m_ready || appId == m_inFlight;
        const bool failed = !busy && m_failures.value(appId) > 0;
        return {{QStringLiteral("state"), failed ? QStringLiteral("failed") : QStringLiteral("checking")}};
    }
    if (entry->noReports)
        return {{QStringLiteral("state"), QStringLiteral("none")}};
    return {{QStringLiteral("state"), QStringLiteral("ready")},
            {QStringLiteral("tier"), entry->tier},
            {QStringLiteral("trending"), entry->trending},
            {QStringLiteral("total"), entry->total}};
}

void ProtonDb::schedule(qint64 appId)
{
    m_waiting = appId; // a game passed over while arrowing down never gets its request
    m_wait.start(m_waitMs);
}

void ProtonDb::pump()
{
    if (m_inFlight || !m_ready)
        return;
    m_inFlight = std::exchange(m_ready, 0);
    const qint64 appId = m_inFlight;
    m_http->protonDb(QStringLiteral("/api/v1/reports/summaries/%1.json").arg(appId), this,
                     [this, appId](const MetadataReply &reply) { answered(appId, reply); });
}

void ProtonDb::answered(qint64 appId, const MetadataReply &reply)
{
    m_inFlight = 0;
    Entry entry;
    entry.fetched = now();
    bool good = false;
    if (reply.status == 404) {
        entry.noReports = true;
        good = true;
    } else if (reply.ok() && reply.json.isObject()) {
        const QJsonObject o = reply.json.object();
        entry.tier = o.value(QLatin1String("tier")).toString().toLower();
        entry.trending = o.value(QLatin1String("trendingTier")).toString().toLower();
        entry.total = o.value(QLatin1String("total")).toInt();
        good = !entry.tier.isEmpty();
    }
    if (good) {
        m_failures.remove(appId);
        m_entries.insert(appId, entry);
        save();
    } else {
        ++m_failures[appId];
    }
    emit changed(appId);
    pump();
}
