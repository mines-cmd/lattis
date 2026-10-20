#include "scanlookup.h"
#include "metadataclient.h"
#include "metadatasettings.h"
#include "tomlpatch.h"

#include <QFile>
#include <QJSEngine>
#include <QTimer>

ScanLookup::ScanLookup(BulkLookup::Source source, BulkLookup::ValueWriter values, BulkLookup::ArtWriter art,
                       const QString &settingsFile, MetadataClient *client, QObject *parent, BulkLookup::TagMappings tags)
    : QObject(parent), m_source(std::move(source)), m_writeValues(std::move(values)), m_writeArt(std::move(art)),
      m_settingsFile(settingsFile), m_client(client), m_tagMappings(std::move(tags))
{
    s_instance = this;
}

ScanLookup::~ScanLookup()
{
    if (s_instance == this)
        s_instance = nullptr;
}

ScanLookup *ScanLookup::create(QQmlEngine *, QJSEngine *)
{
    Q_ASSERT(s_instance);
    QJSEngine::setObjectOwnership(s_instance, QJSEngine::CppOwnership);
    return s_instance;
}

bool ScanLookup::enabled() const
{
    QFile file(m_settingsFile);
    if (!file.open(QIODevice::ReadOnly))
        return MetadataSettings().lookUpNewGames;
    const Parsed<MetadataSettings> parsed = parseMetadataSettings(file.readAll(), QStringLiteral("settings.toml"));
    for (const QString &warning : parsed.warnings)
        qCWarning(lcMetadata).noquote() << warning;
    return parsed.value.lookUpNewGames;
}

QString ScanLookup::setEnabled(bool enabled)
{
    QString error;
    const QString section = QStringLiteral("metadata");
    const QString key = QStringLiteral("look_up_new_games");
    if (!QFile::exists(m_settingsFile)) {
        if (enabled)
            return {};
        if (QFile file(m_settingsFile); !file.open(QIODevice::WriteOnly))
            return tr("Couldn't create %1: %2").arg(m_settingsFile, file.errorString());
    }
    const bool ok = enabled ? TomlPatch::removeValueInFile(m_settingsFile, section, key, &error)
                            : TomlPatch::setValueInFile(m_settingsFile, section, key, QStringLiteral("false"), &error);
    if (!ok)
        return tr("Couldn't save %1 in settings.toml: %2").arg(key, error);
    if (!enabled && !m_waiting.isEmpty()) {
        m_waiting.clear();
        if (!m_bulk)
            stopped();
    }
    emit enabledChanged();
    return {};
}

void ScanLookup::add(const QStringList &ids)
{
    if (!enabled()) {
        qCInfo(lcMetadata).noquote() << QStringLiteral("not looking up %1 new game(s): turned off").arg(ids.size());
        return;
    }
    for (const QString &id : ids) {
        if (!m_waiting.contains(id) && !m_batch.contains(id))
            m_waiting.append(id);
    }
    if (m_bulk)
        emit progressChanged();
    else
        next();
}

int ScanLookup::progressDone() const
{
    // BulkLookup counts the games it looks up, then what it writes; once written, the whole batch is done.
    return m_batchesDone + (!m_bulk ? 0 : m_applying ? int(m_batch.size()) : m_bulk->done());
}

int ScanLookup::progressTotal() const
{
    return m_batchesDone + int(m_batch.size() + m_waiting.size());
}

void ScanLookup::stopped()
{
    m_batchesDone = 0;
    emit runningChanged();
    emit progressChanged();
}

void ScanLookup::next()
{
    if (m_bulk)
        return;
    if (m_waiting.isEmpty()) {
        stopped();
        return;
    }
    MetadataClient *client = m_client ? m_client : MetadataClient::instance();
    client->reloadKeys();
    if (!client->hasIgdbKeys()) {
        qCInfo(lcMetadata).noquote() << QStringLiteral("not looking up %1 new game(s): no IGDB keys").arg(m_waiting.size());
        m_waiting.clear();
        stopped();
        return;
    }
    qCInfo(lcMetadata).noquote() << QStringLiteral("looking up %1 new game(s)").arg(m_waiting.size());
    m_batch = QSet<QString>(m_waiting.cbegin(), m_waiting.cend());
    m_waiting.clear();
    m_applying = false;
    m_unresolved = 0;
    m_bulk = new BulkLookup(
        client,
        [this] {
            QList<LookupGame> games = m_source();
            games.removeIf([&](const LookupGame &g) { return !m_batch.contains(g.id); });
            return games;
        },
        m_writeValues, m_writeArt, m_tagMappings, this);
    connect(m_bulk, &BulkLookup::progressChanged, this, &ScanLookup::progressed);
    connect(m_bulk, &BulkLookup::progressChanged, this, &ScanLookup::progressChanged);
    connect(m_bulk, &BulkLookup::applied, this, &ScanLookup::done);
    emit runningChanged();
    m_bulk->start(); // with nothing to look up, it's applied (and done) right away
}

void ScanLookup::progressed()
{
    if (!m_bulk || m_applying || m_bulk->running() || m_bulk->applying())
        return;
    m_applying = true;
    for (const QVariant &r : m_bulk->results()) {
        const QString state = r.toMap().value(QStringLiteral("state")).toString();
        if (state != QLatin1String("matched"))
            ++m_unresolved;
    }
    // Without art to download, it's done within apply(); with nothing to write, it never is.
    BulkLookup *bulk = m_bulk;
    bulk->apply();
    if (m_bulk == bulk && !bulk->applying())
        done(0);
}

void ScanLookup::done(int written)
{
    if (!m_bulk)
        return;
    const int unresolved = m_unresolved;
    qCInfo(lcMetadata).noquote() << QStringLiteral("looked up the new games: %1 filled in, %2 without a clear match")
                                        .arg(written)
                                        .arg(unresolved);
    // Only while more wait: stopped, its progress starts over.
    m_batchesDone = m_waiting.isEmpty() ? 0 : m_batchesDone + int(m_batch.size());
    // Called from the BulkLookup's own signals.
    m_bulk->deleteLater();
    m_bulk = nullptr;
    m_batch.clear();
    m_applying = false;
    emit runningChanged();
    emit progressChanged();
    emit finished(written, unresolved);

    QString text;
    if (written > 0)
        text = written == 1 ? tr("Looked up 1 new game") : tr("Looked up %1 new games").arg(written);
    if (unresolved > 0 && written > 0)
        text += unresolved == 1 ? tr(", 1 had no clear match") : tr(", %1 had no clear match").arg(unresolved);
    else if (unresolved > 0)
        text = unresolved == 1 ? tr("1 new game had no clear match") : tr("%1 new games had no clear match").arg(unresolved);
    if (!text.isEmpty())
        emit message(text, unresolved > 0);
    // Not from within the BulkLookup's signals.
    QTimer::singleShot(0, this, &ScanLookup::next);
}
