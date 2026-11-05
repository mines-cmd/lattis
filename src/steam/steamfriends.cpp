#include "steamfriends.h"
#include "credentials.h"
#include "metadatahttp.h"
#include "steampresets.h"

#include <QCoreApplication>
#include <QColor>
#include <QDir>
#include <QFileInfo>
#include <QFile>
#include <QImage>
#include <QLoggingCategory>
#include <QRegularExpression>
#include <QSet>
#include <QUrl>

#include <algorithm>

Q_LOGGING_CATEGORY(lcSteamFriends, "launcher.steam.friends")

namespace {
constexpr qint64 kDaySeconds = 86400;

// The file name of a picture, which must stay a plain name inside the pictures folder.
QString pictureName(const QString &url)
{
    static const QRegularExpression plain(QStringLiteral("^[A-Za-z0-9_][A-Za-z0-9_.-]*\\.(jpg|jpeg|png)$"),
                                          QRegularExpression::CaseInsensitiveOption);
    const QUrl parsed(url);
    if (parsed.scheme() != u"https" && parsed.scheme() != u"http")
        return {};
    const QString name = parsed.fileName();
    return plain.match(name).hasMatch() ? name : QString();
}

bool byName(const SteamFriends::Person &a, const SteamFriends::Person &b)
{
    return QString::compare(a.name, b.name, Qt::CaseInsensitive) < 0;
}
}

struct SteamFriends::Job
{
    QString key;
    QString steamId;
    SteamWebApi::FriendsCache before;
    QStringList ids;
    bool listRead = false;
    QList<SteamWebApi::FriendSummary> summaries;
    QHash<QString, QList<SteamWebApi::FriendGame>> games;
    bool failed = false;
    // Steam asked to slow down: the rest is left for the next fetch.
    bool stopped = false;
    QString error;
};

struct SteamFriends::PlayingRefresh
{
    QString key;
    QStringList ids;
    QList<SteamWebApi::FriendSummary> summaries;
};

SteamFriends::Setup &SteamFriends::pending()
{
    static Setup setup;
    return setup;
}

SteamFriends *SteamFriends::instance()
{
    if (!s_instance)
        s_instance = new SteamFriends(pending(), QCoreApplication::instance());
    return s_instance;
}

SteamFriends::SteamFriends(const Setup &setup, QObject *parent) : QObject(parent), m_setup(setup)
{
    m_daily.setSingleShot(true);
    connect(&m_daily, &QTimer::timeout, this, &SteamFriends::dailyCheck);
    SteamWebApi::loadFriendsCache(cachePath(), &m_cache);
}

QString SteamFriends::cachePath() const
{
    return SteamWebApi::friendsCachePath(m_setup.cacheDir, m_setup.cacheKey);
}

bool SteamFriends::switchOn() const
{
    QFile file(QDir(m_setup.dataDir).filePath(QStringLiteral("settings.toml")));
    const QByteArray bytes = file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
    return parseSteamSettings(bytes, QStringLiteral("settings.toml")).value.friendsActivity;
}

bool SteamFriends::enabled() const
{
    return switchOn() && CredentialsFile::load(m_setup.dataDir).value.hasSteamWebKey();
}

void SteamFriends::startDaily(int delayMs)
{
    m_daily.start(delayMs);
}

void SteamFriends::dailyCheck()
{
    qint64 wait = kDaySeconds;
    if (isHeld()) {
        wait = 60;
    } else if (enabled()) {
        const QString id = m_setup.steamId ? m_setup.steamId() : QString();
        const qint64 age = m_cache.fetched.isValid() && m_cache.steamId == id
            ? m_cache.fetched.secsTo(QDateTime::currentDateTimeUtc())
            : kDaySeconds;
        if (age >= kDaySeconds)
            begin();
        else
            wait = kDaySeconds - age;
    }
    m_daily.start(int(qMin<qint64>(wait, kDaySeconds) * 1000));
}

void SteamFriends::syncNow()
{
    begin();
}

void SteamFriends::paced(std::function<void()> go)
{
    const int interval = m_setup.fixturesDir.isEmpty() ? m_setup.requestIntervalMs : 0;
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    const qint64 at = qMax(now, m_lastSendMs + interval);
    m_lastSendMs = at;
    QTimer::singleShot(int(at - now), this, std::move(go));
}

void SteamFriends::send(const SteamWebApi::Request &request, std::function<void(const MetadataReply &)> done)
{
    paced([this, request, done = std::move(done)] {
        MetadataHttp *http = m_setup.http ? m_setup.http() : nullptr;
        if (!http) {
            MetadataReply reply;
            reply.error = tr("Steam can't be reached");
            done(reply);
            return;
        }
        if (!m_setup.fixturesDir.isEmpty())
            http->setSteamWebFixturesDir(m_setup.fixturesDir);
        http->steamWeb(request.path, request.query, this, done);
    });
}

void SteamFriends::request(const std::shared_ptr<Job> &job, const SteamWebApi::Request &request,
                           std::function<void(const QJsonDocument &json)> handle, std::function<void()> then)
{
    send(request, [job, handle = std::move(handle), then = std::move(then)](const MetadataReply &reply) {
        if (!reply.ok()) {
            if (job->error.isEmpty())
                job->error = reply.error;
            job->failed = true;
            if (reply.status == 429)
                job->stopped = true;
        } else {
            handle(reply.json);
        }
        then();
    });
}

void SteamFriends::begin()
{
    if (m_state == State::Syncing)
        return;
    m_state = State::Syncing;
    m_error.clear();
    emit stateChanged();

    if (!switchOn()) {
        finish(Outcome::Disabled);
        return;
    }
    const QString id = m_setup.steamId ? m_setup.steamId() : QString();
    if (id.isEmpty() || !m_setup.http || !m_setup.http()) {
        finish(Outcome::NoAccount);
        return;
    }
    const QString key = CredentialsFile::load(m_setup.dataDir).value.steamWebKey;
    if (key.isEmpty()) {
        finish(Outcome::NotConfigured);
        return;
    }

    auto job = std::make_shared<Job>();
    job->key = key;
    job->steamId = id;
    if (m_cache.steamId == id)
        job->before = m_cache;
    qCInfo(lcSteamFriends) << "fetching the friends' play";

    request(
        job, SteamWebApi::friendListRequest(id, key),
        [job](const QJsonDocument &json) {
            const auto parsed = SteamWebApi::parseFriendList(json);
            if (!parsed.ok) {
                job->failed = true;
                job->error = tr("Steam sent an answer the launcher couldn't read");
                return;
            }
            job->ids = parsed.items;
            job->listRead = true;
        },
        [this, job] {
            if (!job->listRead) {
                finish(Outcome::Failed, job->error);
                return;
            }
            // A list with no friends can't be told from one Steam hides: the cache stays as it was.
            if (job->ids.isEmpty()) {
                finish(Outcome::Synced);
                return;
            }
            fetchSummaries(job, 0);
        });
}

void SteamFriends::fetchSummaries(const std::shared_ptr<Job> &job, qsizetype from)
{
    if (from >= job->ids.size() || job->stopped) {
        fetchGames(job, 0);
        return;
    }
    const QStringList batch = job->ids.mid(from, SteamWebApi::friendSummariesBatch());
    request(
        job, SteamWebApi::playerSummariesRequest(batch, job->key),
        [job](const QJsonDocument &json) {
            const auto parsed = SteamWebApi::parseFriendSummaries(json);
            if (!parsed.ok) {
                job->failed = true;
                job->error = tr("Steam sent an answer the launcher couldn't read");
                return;
            }
            job->summaries.append(parsed.items);
        },
        [this, job, from] { fetchSummaries(job, from + SteamWebApi::friendSummariesBatch()); });
}

void SteamFriends::fetchGames(const std::shared_ptr<Job> &job, qsizetype index)
{
    if (index >= job->ids.size() || job->stopped) {
        finishFetch(job);
        return;
    }
    const QString id = job->ids.at(index);
    request(
        job, SteamWebApi::friendGamesRequest(id, job->key),
        [job, id](const QJsonDocument &json) {
            const auto parsed = SteamWebApi::parseFriendGames(json);
            if (!parsed.ok) {
                job->failed = true;
                job->error = tr("Steam sent an answer the launcher couldn't read");
            } else if (!parsed.items.isEmpty()) {
                // No games can't be told from a private profile: the last answer stays.
                job->games.insert(id, parsed.items);
            }
        },
        [this, job, index] { fetchGames(job, index + 1); });
}

void SteamFriends::finishFetch(const std::shared_ptr<Job> &job)
{
    QHash<QString, SteamWebApi::Friend> old;
    for (const SteamWebApi::Friend &f : std::as_const(job->before.friends))
        old.insert(f.steamId, f);
    QHash<QString, SteamWebApi::FriendSummary> summaries;
    for (const SteamWebApi::FriendSummary &s : std::as_const(job->summaries))
        summaries.insert(s.steamId, s);

    SteamWebApi::FriendsCache next;
    next.steamId = job->steamId;
    // A fetch that lost a part is tried again at the next start rather than in a day.
    next.fetched = job->failed ? job->before.fetched : QDateTime::currentDateTimeUtc();
    for (const QString &id : std::as_const(job->ids)) {
        SteamWebApi::Friend f = old.value(id);
        f.steamId = id;
        if (const auto s = summaries.constFind(id); s != summaries.cend()) {
            if (!s->name.isEmpty())
                f.name = s->name;
            if (!s->avatarUrl.isEmpty())
                f.avatarUrl = s->avatarUrl;
        }
        if (const auto g = job->games.constFind(id); g != job->games.cend())
            f.games = *g;
        next.friends.append(f);
    }

    m_cache = next;
    if (!SteamWebApi::saveFriendsCache(cachePath(), m_cache))
        qCWarning(lcSteamFriends) << "could not save the friends cache";
    // A friend no longer in the list is no longer playing.
    for (auto it = m_playing.begin(); it != m_playing.end();) {
        it = job->ids.contains(it.key()) ? std::next(it) : m_playing.erase(it);
    }
    applyPlaying(job->summaries);
    emit changed();

    QStringList urls;
    for (const SteamWebApi::Friend &f : std::as_const(m_cache.friends)) {
        if (const QString name = pictureName(f.avatarUrl); !name.isEmpty() && !QFile::exists(avatarFile(f.avatarUrl))
            && !urls.contains(f.avatarUrl))
            urls.append(f.avatarUrl);
    }
    fetchAvatars(urls, [this, job] {
        pruneAvatars();
        emit changed();
        finish(Outcome::Synced, job->failed ? job->error : QString());
    });
}

void SteamFriends::fetchAvatars(QStringList urls, std::function<void()> done)
{
    if (urls.isEmpty()) {
        done();
        return;
    }
    const QString url = urls.takeFirst();
    const QString path = avatarFile(url);
    QDir().mkpath(QFileInfo(path).absolutePath());

    if (!m_setup.fixturesDir.isEmpty()) {
        QImage image(64, 64, QImage::Format_RGB32);
        image.fill(QColor::fromHsv(int(qHash(url) % 360), 120, 200));
        // Named like the real files; Qt reads the content, not the extension.
        image.save(path, "PNG");
        fetchAvatars(urls, std::move(done));
        return;
    }
    paced([this, url, path, urls, done = std::move(done)]() mutable {
        MetadataHttp *http = m_setup.http ? m_setup.http() : nullptr;
        if (!http) {
            fetchAvatars(urls, std::move(done));
            return;
        }
        http->download(QUrl(url), pictureName(url), this,
                       [this, path, urls, done = std::move(done)](const QString &file, const QString &error) mutable {
                           if (error.isEmpty() && !file.isEmpty()) {
                               QFile::remove(path);
                               if (!QFile::rename(file, path) && !QFile::copy(file, path))
                                   qCWarning(lcSteamFriends) << "could not save a friend's picture";
                               QFile::remove(file);
                               emit changed();
                           }
                           fetchAvatars(urls, std::move(done));
                       });
    });
}

void SteamFriends::pruneAvatars()
{
    QSet<QString> used;
    for (const SteamWebApi::Friend &f : std::as_const(m_cache.friends)) {
        if (const QString name = pictureName(f.avatarUrl); !name.isEmpty())
            used.insert(name);
    }
    const QDir dir(QDir(m_setup.cacheDir).filePath(QStringLiteral("steam-avatars")));
    for (const QString &name : dir.entryList(QDir::Files)) {
        if (!used.contains(name))
            QFile::remove(dir.filePath(name));
    }
}

QString SteamFriends::avatarFile(const QString &url) const
{
    const QString name = pictureName(url);
    return name.isEmpty() ? QString() : QDir(m_setup.cacheDir).filePath(QStringLiteral("steam-avatars/") + name);
}

void SteamFriends::finish(Outcome outcome, const QString &error)
{
    m_state = State::Idle;
    m_outcome = outcome;
    m_error = error;
    qCInfo(lcSteamFriends).noquote() << "friends sync finished:" << int(outcome) << error;
    emit stateChanged();
}

bool SteamFriends::applyPlaying(const QList<SteamWebApi::FriendSummary> &summaries)
{
    const QHash<QString, Playing> before = m_playing;
    const QSet<QString> onlineBefore = m_online;
    for (const SteamWebApi::FriendSummary &s : summaries) {
        if (s.gameId > 0)
            m_playing.insert(s.steamId, {s.gameId, s.gameName});
        else
            m_playing.remove(s.steamId);
        if (s.online)
            m_online.insert(s.steamId);
        else
            m_online.remove(s.steamId);
    }
    return m_playing != before || m_online != onlineBefore;
}

bool SteamFriends::refreshPlaying()
{
    if (m_state == State::Syncing || m_playingBusy || m_cache.friends.isEmpty() || !enabled())
        return false;
    const QDateTime now = QDateTime::currentDateTimeUtc();
    if (m_lastPlayingRefresh.isValid() && m_lastPlayingRefresh.msecsTo(now) < m_setup.playingIntervalMs)
        return false;
    if (const QString id = m_setup.steamId ? m_setup.steamId() : QString(); id != m_cache.steamId)
        return false;
    auto refresh = std::make_shared<PlayingRefresh>();
    refresh->key = CredentialsFile::load(m_setup.dataDir).value.steamWebKey;
    for (const SteamWebApi::Friend &f : std::as_const(m_cache.friends))
        refresh->ids.append(f.steamId);
    m_lastPlayingRefresh = now;
    m_playingBusy = true;
    refreshPlayingFrom(refresh, 0);
    return true;
}

void SteamFriends::refreshPlayingFrom(const std::shared_ptr<PlayingRefresh> &refresh, qsizetype from)
{
    if (from >= refresh->ids.size()) {
        m_playingBusy = false;
        if (applyPlaying(refresh->summaries))
            emit changed();
        return;
    }
    const QStringList batch = refresh->ids.mid(from, SteamWebApi::friendSummariesBatch());
    send(SteamWebApi::playerSummariesRequest(batch, refresh->key), [this, refresh, from](const MetadataReply &reply) {
        if (reply.ok()) {
            const auto parsed = SteamWebApi::parseFriendSummaries(reply.json);
            if (parsed.ok)
                refresh->summaries.append(parsed.items);
        }
        refreshPlayingFrom(refresh, from + SteamWebApi::friendSummariesBatch());
    });
}

SteamFriends::Person SteamFriends::personOf(const SteamWebApi::Friend &f) const
{
    Person p;
    p.steamId = f.steamId;
    p.name = f.name.isEmpty() ? f.steamId : f.name;
    if (const QString file = avatarFile(f.avatarUrl); !file.isEmpty() && QFile::exists(file))
        p.avatarPath = file;
    if (const auto playing = m_playing.constFind(f.steamId); playing != m_playing.cend()) {
        p.gameId = playing->gameId;
        p.gameName = playing->gameName;
    }
    p.online = m_online.contains(f.steamId);
    return p;
}

SteamFriends::GameFriends SteamFriends::friendsOf(qint64 appId) const
{
    GameFriends out;
    if (m_setup.steamId) {
        const QString id = m_setup.steamId();
        if (!id.isEmpty() && !m_cache.steamId.isEmpty() && id != m_cache.steamId)
            return out;
    }
    for (const SteamWebApi::Friend &f : m_cache.friends) {
        Person p = personOf(f);
        if (p.gameId == appId)
            out.playing.append(p);
        for (const SteamWebApi::FriendGame &g : f.games) {
            if (g.appId != appId)
                continue;
            p.recentMinutes = g.recentMinutes;
            p.totalMinutes = g.totalMinutes;
            if (SteamWebApi::groupOf(g) == SteamWebApi::FriendGroup::Recent)
                out.recent.append(p);
            else if (SteamWebApi::groupOf(g) == SteamWebApi::FriendGroup::Previous)
                out.previous.append(p);
            break;
        }
    }
    std::ranges::sort(out.playing, byName);
    std::ranges::stable_sort(out.recent, byName);
    std::ranges::stable_sort(out.recent, [](const Person &a, const Person &b) { return a.recentMinutes > b.recentMinutes; });
    std::ranges::stable_sort(out.previous, byName);
    std::ranges::stable_sort(out.previous, [](const Person &a, const Person &b) { return a.totalMinutes > b.totalMinutes; });
    return out;
}

QList<SteamFriends::Person> SteamFriends::playingNow() const
{
    QList<Person> out;
    for (const SteamWebApi::Friend &f : m_cache.friends) {
        if (Person p = personOf(f); p.gameId > 0)
            out.append(p);
    }
    std::ranges::sort(out, byName);
    return out;
}
