#pragma once

#include "steamwebapi.h"

#include <QDateTime>
#include <QHash>
#include <QObject>
#include <QSet>
#include <QString>
#include <QTimer>

#include <functional>
#include <memory>

class MetadataHttp;
struct MetadataReply;

// See docs/code/steam.md.
class SteamFriends : public QObject
{
    Q_OBJECT

public:
    struct Setup
    {
        QString dataDir;
        QString cacheDir;
        QString cacheKey;
        std::function<QString()> steamId;
        // The setup guide is still open: nothing is fetched.
        std::function<bool()> held;
        std::function<MetadataHttp *()> http;
        // LATTIS_STEAM_WEB_FIXTURES: a folder of canned answers; pictures are made on the spot.
        QString fixturesDir;
        // Between two requests of a fetch.
        int requestIntervalMs = 1000;
        // Between two refreshes of who is playing now.
        int playingIntervalMs = 60000;
    };

    struct Person
    {
        QString steamId;
        QString name;
        // The cached picture's file; empty until it is downloaded.
        QString avatarPath;
        int recentMinutes = 0;
        int totalMinutes = 0;
        // What a friend playing now plays.
        qint64 gameId = 0;
        QString gameName;
        bool online = false;
    };

    // Recent: most played in the last two weeks first; previous: most played first; playing: by name.
    // A friend playing now is also listed in the group their play time puts them in.
    struct GameFriends
    {
        QList<Person> playing;
        QList<Person> recent;
        QList<Person> previous;

        bool isEmpty() const { return playing.isEmpty() && recent.isEmpty() && previous.isEmpty(); }
    };

    enum class State { Idle, Syncing };
    Q_ENUM(State)
    enum class Outcome { None, Synced, Disabled, NotConfigured, NoAccount, Failed };
    Q_ENUM(Outcome)

    static void setup(const Setup &setup) { pending() = setup; }
    static SteamFriends *instance();

    explicit SteamFriends(const Setup &setup, QObject *parent = nullptr);

    State state() const { return m_state; }
    Outcome outcome() const { return m_outcome; }
    QString error() const { return m_error; }
    QDateTime lastFetch() const { return m_cache.fetched; }
    int friendCount() const { return int(m_cache.friends.size()); }

    // [steam] friends_activity (on unless it says false) and a Web API key.
    bool enabled() const;

    // The first check runs delayMs from now, then each time a day has passed since the last fetch.
    void startDaily(int delayMs = 10000);
    // Fetches now, whatever the last fetch time says. Nothing is sent when the switch is off or no key is set.
    void syncNow();

    // Reads the cache only: sends nothing.
    GameFriends friendsOf(qint64 appId) const;
    // Everyone playing something now, by name. Only as fresh as the last refreshPlaying() or fetch.
    QList<Person> playingNow() const;
    // Asks Steam who is playing now (one request per 100 friends). Does nothing, and returns false, within
    // playingIntervalMs of the last refresh, during a fetch, or when disabled. Call it only while something shows it.
    bool refreshPlaying();

signals:
    void stateChanged();
    // The cache or who is playing now changed.
    void changed();

private:
    struct Job;
    struct PlayingRefresh;
    struct Playing
    {
        qint64 gameId = 0;
        QString gameName;

        bool operator==(const Playing &) const = default;
    };

    void dailyCheck();
    void begin();
    // Runs go once the interval since the last request has passed.
    void paced(std::function<void()> go);
    void send(const SteamWebApi::Request &request, std::function<void(const MetadataReply &)> done);
    void request(const std::shared_ptr<Job> &job, const SteamWebApi::Request &request,
                 std::function<void(const QJsonDocument &json)> handle, std::function<void()> then);
    void fetchSummaries(const std::shared_ptr<Job> &job, qsizetype from);
    void fetchGames(const std::shared_ptr<Job> &job, qsizetype index);
    void finishFetch(const std::shared_ptr<Job> &job);
    void fetchAvatars(QStringList urls, std::function<void()> done);
    void pruneAvatars();
    void finish(Outcome outcome, const QString &error = {});
    bool applyPlaying(const QList<SteamWebApi::FriendSummary> &summaries);
    void refreshPlayingFrom(const std::shared_ptr<PlayingRefresh> &refresh, qsizetype from);
    QString avatarFile(const QString &url) const;
    Person personOf(const SteamWebApi::Friend &f) const;
    bool isHeld() const { return m_setup.held && m_setup.held(); }
    bool switchOn() const;
    QString cachePath() const;

    Setup m_setup;
    QTimer m_daily;
    State m_state = State::Idle;
    Outcome m_outcome = Outcome::None;
    QString m_error;
    SteamWebApi::FriendsCache m_cache;
    QHash<QString, Playing> m_playing;
    QSet<QString> m_online;
    qint64 m_lastSendMs = 0;
    QDateTime m_lastPlayingRefresh;
    bool m_playingBusy = false;

    static Setup &pending();
    static inline SteamFriends *s_instance = nullptr;
};
