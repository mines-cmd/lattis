#include "steamartfetch.h"
#include "metadataclient.h"

#include <QJSEngine>
#include <QLoggingCategory>
#include <QSet>
#include <QUrl>

namespace {
constexpr int kParallel = 4;
constexpr int kBatch = 25;
}

SteamArtFetch::SteamArtFetch(Source source, BulkLookup::ArtWriter art, MetadataClient *client, QObject *parent)
    : QObject(parent), m_source(std::move(source)), m_writeArt(std::move(art)), m_client(client)
{
    s_instance = this;
}

SteamArtFetch::~SteamArtFetch()
{
    qDeleteAll(m_jobs);
    if (s_instance == this)
        s_instance = nullptr;
}

SteamArtFetch *SteamArtFetch::create(QQmlEngine *, QJSEngine *)
{
    Q_ASSERT(s_instance);
    QJSEngine::setObjectOwnership(s_instance, QJSEngine::CppOwnership);
    return s_instance;
}

MetadataClient *SteamArtFetch::client() const
{
    return m_client ? m_client : MetadataClient::instance();
}

void SteamArtFetch::add(const QStringList &ids)
{
    QList<Wanted> wanted = m_source(ids);
    wanted.removeIf([&](const Wanted &w) {
        if (w.kinds.isEmpty() || m_jobs.contains(w.id))
            return true;
        return std::any_of(m_waiting.cbegin(), m_waiting.cend(), [&](const Wanted &o) { return o.id == w.id; });
    });
    if (wanted.isEmpty())
        return;
    if (!m_connected) {
        m_connected = true;
        connect(client(), &MetadataClient::finished, this, &SteamArtFetch::onFinished);
    }
    const bool wasRunning = running();
    if (!wasRunning) {
        m_done = 0;
        m_total = 0;
        m_written = 0;
    }
    m_total += int(wanted.size());
    m_waiting += wanted;
    if (!wasRunning)
        emit runningChanged();
    emit progressChanged();
    pump();
}

void SteamArtFetch::pump()
{
    while (m_active < kParallel && !m_waiting.isEmpty()) {
        auto *job = new Job;
        job->wanted = m_waiting.takeFirst();
        m_jobs.insert(job->wanted.id, job);
        ++m_active;
        m_lookups.insert(client()->steamArt(job->wanted.appId), job->wanted.id);
    }
}

void SteamArtFetch::onFinished(int request, const QVariantMap &result)
{
    if (const auto lookup = m_lookups.find(request); lookup != m_lookups.end()) {
        const QString id = lookup.value();
        m_lookups.erase(lookup);
        Job *job = m_jobs.value(id);
        if (!job)
            return;
        if (!result.value(QStringLiteral("error")).toString().isEmpty()) {
            jobDone(job);
            return;
        }
        QSet<QString> asked;
        const QVariantList images = result.value(QStringLiteral("images")).toList();
        for (const QVariant &v : images) {
            const QVariantMap image = v.toMap();
            const QString kind = image.value(QStringLiteral("kind")).toString();
            if (!job->wanted.kinds.contains(kind) || asked.contains(kind))
                continue;
            asked.insert(kind);
            ++job->pending;
            m_downloads.insert(client()->download(image.value(QStringLiteral("url")).toString(), kind), {id, kind});
        }
        if (job->pending == 0)
            jobDone(job);
        return;
    }
    const auto download = m_downloads.find(request);
    if (download == m_downloads.end())
        return;
    const auto [id, kind] = download.value();
    m_downloads.erase(download);
    Job *job = m_jobs.value(id);
    if (!job)
        return;
    if (result.value(QStringLiteral("error")).toString().isEmpty())
        job->files.append({id, kind, result.value(QStringLiteral("path")).toString()});
    if (--job->pending == 0)
        jobDone(job);
}

void SteamArtFetch::jobDone(Job *job)
{
    m_ready += job->files;
    m_jobs.remove(job->wanted.id);
    delete job;
    --m_active;
    ++m_done;
    flush(false);
    pump();
    if (m_active == 0 && m_waiting.isEmpty()) {
        flush(true);
        const int written = m_written;
        qCInfo(lcMetadata).noquote() << QStringLiteral("fetched Steam art for %1 game(s): %2 file(s)").arg(m_done).arg(written);
        emit runningChanged();
        emit progressChanged();
        emit finished(written);
        return;
    }
    emit progressChanged();
}

void SteamArtFetch::flush(bool all)
{
    if (m_ready.isEmpty() || (!all && m_ready.size() < kBatch))
        return;
    // The lookup may have filled a kind in the meantime.
    QStringList ids;
    for (const auto &r : std::as_const(m_ready)) {
        if (!ids.contains(r[0]))
            ids.append(r[0]);
    }
    QSet<QString> missing;
    for (const Wanted &w : m_source(ids)) {
        for (const QString &kind : w.kinds)
            missing.insert(w.id + u'/' + kind);
    }
    QList<std::array<QString, 3>> write;
    for (const auto &r : std::as_const(m_ready)) {
        if (missing.contains(r[0] + u'/' + r[1]))
            write.append(r);
    }
    m_ready.clear();
    if (write.isEmpty())
        return;
    const QHash<QString, QString> errors = m_writeArt(write);
    for (auto it = errors.cbegin(); it != errors.cend(); ++it)
        qCWarning(lcMetadata).noquote() << "could not save Steam art for" << it.key() << it.value();
    m_written += int(write.size()) - int(errors.size());
}
