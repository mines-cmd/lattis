#include "steamlibrarysync.h"
#include "credentials.h"
#include "metadatahttp.h"
#include "steampresets.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonObject>
#include <QLoggingCategory>
#include <QRegularExpression>
#include <QSaveFile>

Q_LOGGING_CATEGORY(lcSteamWeb, "launcher.steam.web")

namespace {
constexpr qint64 kDaySeconds = 86400;

QString quotedValue(const QByteArray &text, const char *key)
{
    const QRegularExpression pattern(QStringLiteral("^%1\\s*=\\s*\"([^\"]*)\"").arg(QLatin1String(key)),
                                     QRegularExpression::MultilineOption);
    return pattern.match(QString::fromUtf8(text)).captured(1);
}
}

struct SteamLibrarySync::Job
{
    SteamWebApi::Auth auth;
    bool token = false;
    QString steamId;
    MetadataHttp *http = nullptr;
    SteamWebApi::Cache before;
    SteamWebApi::Cache cache;
    bool ownedFresh = false;
    bool ownedEmpty = false;
    bool sharedFresh = false;
    int pending = 0;
    bool failed = false;
    QString error;
    bool done = false;
};

SteamLibrarySync::Setup &SteamLibrarySync::pending()
{
    static Setup setup;
    return setup;
}

SteamLibrarySync *SteamLibrarySync::instance()
{
    if (!s_instance) {
        s_instance = new SteamLibrarySync(pending(), QCoreApplication::instance());
    }
    return s_instance;
}

SteamLibrarySync::SteamLibrarySync(const Setup &setup, QObject *parent) : QObject(parent), m_setup(setup)
{
    m_daily.setSingleShot(true);
    connect(&m_daily, &QTimer::timeout, this, &SteamLibrarySync::dailyCheck);
    if (QFileInfo(QDir(m_setup.fixturesDir).filePath(QStringLiteral("signin.json"))).isFile()) {
        QFile file(QDir(m_setup.fixturesDir).filePath(QStringLiteral("signin.json")));
        QJsonObject fixture;
        if (file.open(QIODevice::ReadOnly))
            fixture = QJsonDocument::fromJson(file.readAll()).object();
        // A "state" (expired, again) stands for a sign-in that isn't there: no account.
        if (!fixture.contains(QLatin1String("state"))) {
            m_fixtureAccount = fixture.value(QLatin1String("account_name")).toString();
            if (m_fixtureAccount.isEmpty())
                m_fixtureAccount = QStringLiteral("fixture");
        }
    }
    readLastFetch();
    if (SteamWebApi::Cache cache; SteamWebApi::loadCache(SteamWebApi::cachePath(m_setup.cacheDir), &cache)) {
        QSet<qint64> ids;
        for (const auto &g : std::as_const(cache.owned))
            ids.insert(g.appId);
        for (const auto &a : std::as_const(cache.shared))
            ids.insert(a.appId);
        m_gameCount = int(ids.size());
        if (!m_lastFetch.isValid())
            m_lastFetch = cache.fetched;
    }
}

SteamAuth *SteamLibrarySync::auth()
{
    if (!m_auth) {
        m_auth = std::make_unique<SteamAuth>(m_setup.secrets ? m_setup.secrets : SecretStore::create(), m_setup.authBase);
        connect(m_auth.get(), &SteamAuth::statusChanged, this, [this] {
            const bool signedIn = m_auth->status() == SteamAuth::Status::SignedIn;
            if (signedIn == (m_signedInAs == m_auth->steamId()) && signedIn)
                return;
            m_signedInAs = signedIn ? m_auth->steamId() : QString();
            writeState();
            emit stateChanged();
        });
    }
    return m_auth.get();
}

QString SteamLibrarySync::accountName()
{
    if (!m_fixtureAccount.isEmpty())
        return m_fixtureAccount;
    return m_auth && m_auth->status() == SteamAuth::Status::SignedIn ? m_auth->accountName() : QString();
}

bool SteamLibrarySync::signedIn()
{
    return !m_fixtureAccount.isEmpty() || (m_auth && m_auth->status() == SteamAuth::Status::SignedIn);
}

bool SteamLibrarySync::enabled() const
{
    QFile file(QDir(m_setup.dataDir).filePath(QStringLiteral("settings.toml")));
    const QByteArray bytes = file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
    return parseSteamSettings(bytes, QStringLiteral("settings.toml")).value.wholeLibrary;
}

void SteamLibrarySync::startDaily(int delayMs)
{
    // A fetch may be a day away, so the account that signed in before is read back now, not on the next sync.
    if (m_fixtureAccount.isEmpty() && !m_restoreTried) {
        const QString id = m_setup.steamId ? m_setup.steamId() : QString();
        if (!id.isEmpty() && id == m_signedInAs)
            restore(id);
    }
    m_daily.start(delayMs);
}

void SteamLibrarySync::dailyCheck()
{
    qint64 wait = kDaySeconds;
    // The setup guide is open: look again soon, so a sign-in made in it fetches once the guide closes.
    if (isHeld())
        wait = 60;
    else if (enabled()) {
        readLastFetch();
        const qint64 age = m_lastFetch.isValid() ? m_lastFetch.secsTo(QDateTime::currentDateTimeUtc()) : kDaySeconds;
        if (age >= kDaySeconds)
            begin(false);
        else
            wait = kDaySeconds - age;
    }
    m_daily.start(int(qMin<qint64>(wait, kDaySeconds) * 1000));
}

void SteamLibrarySync::syncNow()
{
    begin(true);
}

void SteamLibrarySync::begin(bool manual)
{
    if (m_state == State::Syncing)
        return;
    m_state = State::Syncing;
    m_error.clear();
    emit stateChanged();

    const QString id = m_setup.steamId ? m_setup.steamId() : QString();
    MetadataHttp *http = m_setup.http ? m_setup.http() : nullptr;
    if (id.isEmpty() || !http) {
        finish(Outcome::NoAccount);
        return;
    }
    if (!m_setup.fixturesDir.isEmpty())
        http->setSteamWebFixturesDir(m_setup.fixturesDir);
    auto job = std::make_shared<Job>();
    job->steamId = id;
    job->http = http;
    if (SteamWebApi::Cache cache; SteamWebApi::loadCache(SteamWebApi::cachePath(m_setup.cacheDir), &cache)
                                  && cache.steamId == id)
        job->before = cache;
    job->before.steamId = id;
    job->cache = job->before;

    if (!m_fixtureAccount.isEmpty()) {
        job->token = true;
        job->auth.accessToken = QStringLiteral("fixture-token");
        fetch(job);
        return;
    }

    const QString key = CredentialsFile::load(m_setup.dataDir).value.steamWebKey;
    // The keyring is asked only for an account that signed in here before (or when the user presses Sync now), so
    // a user who never signs in is never shown an unlock prompt.
    const bool mayRestore = manual || m_signedInAs == id;
    const auto proceed = [this, job, key] {
        SteamAuth *a = auth();
        if (a->status() == SteamAuth::Status::SignedIn) {
            a->accessToken(this, [this, job, key](const QString &token, const QString &error) {
                if (!error.isEmpty()) {
                    if (m_auth->status() == SteamAuth::Status::TokenRefused && key.isEmpty())
                        finish(Outcome::SignInExpired, error);
                    else if (!key.isEmpty() && m_auth->status() == SteamAuth::Status::TokenRefused) {
                        job->auth.key = key;
                        fetch(job);
                    } else
                        finish(Outcome::Failed, error);
                    return;
                }
                job->token = true;
                job->auth.accessToken = token;
                fetch(job);
            });
        } else if (!key.isEmpty()) {
            job->auth.key = key;
            fetch(job);
        } else if (a->status() == SteamAuth::Status::TokenRefused) {
            finish(Outcome::SignInExpired, a->error());
        } else {
            finish(Outcome::NotConfigured);
        }
    };
    if (!mayRestore) {
        // Without the keyring only the key can work.
        if (key.isEmpty()) {
            finish(Outcome::NotConfigured);
        } else {
            job->auth.key = key;
            fetch(job);
        }
        return;
    }
    if (m_restoring) {
        connect(auth(), &SteamAuth::restored, this, proceed, Qt::SingleShotConnection);
        return;
    }
    if (auth()->status() == SteamAuth::Status::SignedIn || m_restoreTried) {
        proceed();
        return;
    }
    connect(auth(), &SteamAuth::restored, this, proceed, Qt::SingleShotConnection);
    restore(id);
}

void SteamLibrarySync::restore(const QString &steamId)
{
    m_restoreTried = true;
    m_restoring = true;
    // The status handler stays quiet when the restored account is the one already recorded, so the UI is told here.
    connect(auth(), &SteamAuth::restored, this, [this] {
        m_restoring = false;
        emit stateChanged();
    }, Qt::SingleShotConnection);
    auth()->restore(steamId);
}

void SteamLibrarySync::request(const std::shared_ptr<Job> &job, const SteamWebApi::Request &request,
                               std::function<void(const QJsonDocument &json)> done)
{
    ++job->pending;
    job->http->steamWeb(request.path, request.query, this, [this, job, done = std::move(done)](const MetadataReply &reply) {
        if (!reply.ok()) {
            if (!job->failed)
                job->error = reply.error;
            job->failed = true;
        } else {
            done(reply.json);
        }
        requestDone(job);
    });
}

void SteamLibrarySync::fetch(const std::shared_ptr<Job> &job)
{
    qCInfo(lcSteamWeb) << "fetching the library" << (job->token ? "with the sign-in" : "with the key");
    request(job, SteamWebApi::ownedGamesRequest(job->steamId, job->auth), [this, job](const QJsonDocument &json) {
        const auto parsed = SteamWebApi::parseOwnedGames(json);
        if (!parsed.ok) {
            job->failed = true;
            job->error = tr("Steam sent an answer the launcher couldn't read");
        } else if (parsed.items.isEmpty()) {
            job->ownedEmpty = true;
        } else {
            job->cache.owned = parsed.items;
            job->ownedFresh = true;
            fetchTypes(job);
        }
    });
    if (!job->token) {
        // The family list is only fetched with the sign-in; the key still names the lenders already cached.
        QSet<QString> unknown;
        for (const SteamWebApi::SharedApp &a : std::as_const(job->cache.shared)) {
            for (const QString &owner : a.owners) {
                if (owner != job->steamId && !job->cache.names.contains(owner))
                    unknown.insert(owner);
            }
        }
        if (!unknown.isEmpty() && !job->auth.key.isEmpty()) {
            request(job, SteamWebApi::playerSummariesRequest(unknown.values().mid(0, 100), job->auth.key),
                    [job](const QJsonDocument &names) {
                        const auto parsed = SteamWebApi::parseUserNames(names);
                        for (auto it = parsed.cbegin(); it != parsed.cend(); ++it)
                            job->cache.names.insert(it.key(), it.value());
                    });
        }
        return;
    }
    fetchFamily(job);
    request(job, SteamWebApi::clientAppsRequest(job->auth.accessToken), [job](const QJsonDocument &json) {
        const auto sizes = SteamWebApi::parseClientApps(json);
        if (!sizes.isEmpty())
            job->cache.sizes = sizes;
    });
}

void SteamLibrarySync::fetchTypes(const std::shared_ptr<Job> &job)
{
    QList<qint64> missing;
    for (const SteamWebApi::OwnedGame &g : std::as_const(job->cache.owned)) {
        if (!job->cache.types.contains(g.appId))
            missing.append(g.appId);
        if (missing.size() == SteamWebApi::itemsBatch())
            break;
    }
    if (missing.isEmpty())
        return;
    request(job, SteamWebApi::itemsRequest(missing), [this, job, missing](const QJsonDocument &json) {
        const QHash<qint64, int> types = SteamWebApi::parseItemTypes(json, missing);
        for (auto it = types.cbegin(); it != types.cend(); ++it)
            job->cache.types.insert(it.key(), it.value());
        fetchTypes(job);
    });
}

void SteamLibrarySync::fetchFamily(const std::shared_ptr<Job> &job)
{
    request(job, SteamWebApi::familyGroupForUserRequest(job->steamId, job->auth.accessToken),
            [this, job](const QJsonDocument &json) {
                const SteamWebApi::FamilyGroup group = SteamWebApi::parseFamilyGroup(json);
                if (!group.ok || !group.member)
                    return;
                request(job, SteamWebApi::familyGroupRequest(group.groupId, job->auth.accessToken),
                        [this, job](const QJsonDocument &members) {
                            const QStringList ids = SteamWebApi::parseFamilyMembers(members);                            if (ids.isEmpty())
                                return;
                            request(job, SteamWebApi::userSummariesRequest(ids, job->auth.accessToken),
                                    [job](const QJsonDocument &names) {
                                        const auto parsed = SteamWebApi::parseUserNames(names);                                        for (auto it = parsed.cbegin(); it != parsed.cend(); ++it)
                                            job->cache.names.insert(it.key(), it.value());
                                    });
                        });
                request(job, SteamWebApi::sharedAppsRequest(job->steamId, group.groupId, job->auth.accessToken),
                        [job](const QJsonDocument &apps) {
                            const auto parsed = SteamWebApi::parseSharedApps(apps);
                            if (parsed.ok && !parsed.items.isEmpty()) {
                                job->cache.shared = parsed.items;
                                job->sharedFresh = true;
                            }
                        });
            });
}

void SteamLibrarySync::requestDone(const std::shared_ptr<Job> &job)
{
    if (--job->pending > 0 || job->done)
        return;
    job->done = true;
    const bool fresh = job->ownedFresh || job->sharedFresh;
    if (!fresh) {
        if (job->failed)
            finish(Outcome::Failed, job->error);
        else if (job->ownedEmpty && !job->token)
            finish(Outcome::NoGames);
        else
            finish(Outcome::Synced);
        return;
    }
    job->cache.fetched = QDateTime::currentDateTimeUtc();
    SteamWebApi::Cache saved = job->cache;
    // Only a new time is not worth a scan.
    saved.fetched = job->before.fetched;
    if (saved != job->before) {
        saved.fetched = job->cache.fetched;
        if (!SteamWebApi::saveCache(SteamWebApi::cachePath(m_setup.cacheDir), saved))
            qCWarning(lcSteamWeb) << "could not save the library cache";
        else if (m_setup.changed)
            m_setup.changed();
    }
    // A fetch that lost a part is tried again at the next start rather than in a day.
    if (!job->failed) {
        m_lastFetch = job->cache.fetched;
        writeState();
    }
    QSet<qint64> ids;
    for (const auto &g : std::as_const(job->cache.owned))
        ids.insert(g.appId);
    for (const auto &a : std::as_const(job->cache.shared))
        ids.insert(a.appId);
    m_gameCount = int(ids.size());
    finish(Outcome::Synced, job->failed ? job->error : QString());
}

void SteamLibrarySync::finish(Outcome outcome, const QString &error)
{
    m_state = State::Idle;
    m_outcome = outcome;
    m_error = error;
    qCInfo(lcSteamWeb).noquote() << "library sync finished:" << int(outcome) << error;
    emit stateChanged();
}

void SteamLibrarySync::readLastFetch()
{
    QFile file(m_setup.stateFile);
    if (m_setup.stateFile.isEmpty() || !file.open(QIODevice::ReadOnly))
        return;
    const QByteArray text = file.readAll();
    m_lastFetch = QDateTime::fromString(quotedValue(text, "last_fetch"), Qt::ISODate);
    m_signedInAs = quotedValue(text, "signed_in");
}

void SteamLibrarySync::writeState()
{
    if (m_setup.stateFile.isEmpty())
        return;
    QDir().mkpath(QFileInfo(m_setup.stateFile).absolutePath());
    QSaveFile file(m_setup.stateFile);
    if (!file.open(QIODevice::WriteOnly))
        return;
    QString text;
    if (m_lastFetch.isValid())
        text += QStringLiteral("last_fetch = \"%1\"\n").arg(m_lastFetch.toUTC().toString(Qt::ISODate));
    if (!m_signedInAs.isEmpty())
        text += QStringLiteral("signed_in = \"%1\"\n").arg(m_signedInAs);
    file.write(text.toUtf8());
    file.commit();
}
