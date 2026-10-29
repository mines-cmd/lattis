#pragma once

#include "steamauth.h"
#include "steamwebapi.h"

#include <QDateTime>
#include <QObject>
#include <QPointer>
#include <QTimer>
#include <QUrl>

#include <functional>
#include <memory>

class MetadataHttp;

// See docs/code/steam.md.
class SteamLibrarySync : public QObject
{
    Q_OBJECT

public:
    struct Setup
    {
        QString dataDir;
        QString cacheDir;
        // Where the last fetch time is kept.
        QString stateFile;
        std::function<QString()> steamId;
        // The setup guide is still open: nothing is fetched.
        std::function<bool()> held;
        std::function<MetadataHttp *()> http;
        // The cache file changed: the library should scan again.
        std::function<void()> changed;
        std::shared_ptr<SecretStore> secrets;
        QUrl authBase = QUrl(QStringLiteral("https://api.steampowered.com"));
        // LATTIS_STEAM_WEB_FIXTURES: a folder of canned answers; signin.json in it stands for a signed-in account.
        QString fixturesDir;
    };

    enum class State { Idle, Syncing };
    Q_ENUM(State)

    // What the last sync found, for the settings page.
    enum class Outcome { None, Synced, NotConfigured, NoAccount, SignInExpired, NoGames, Failed };
    Q_ENUM(Outcome)

    static void setup(const Setup &setup) { pending() = setup; }
    static SteamLibrarySync *instance();

    explicit SteamLibrarySync(const Setup &setup, QObject *parent = nullptr);

    State state() const { return m_state; }
    Outcome outcome() const { return m_outcome; }
    QString error() const { return m_error; }
    QDateTime lastFetch() const { return m_lastFetch; }
    int gameCount() const { return m_gameCount; }
    SteamAuth *auth();
    // The account's name when signed in.
    QString accountName();
    bool signedIn();

    QString dataDir() const { return m_setup.dataDir; }
    QString fixturesDir() const { return m_setup.fixturesDir; }
    // The setup guide is still open: nothing is fetched.
    bool isHeld() const { return m_setup.held && m_setup.held(); }
    // The sign-in object exists (it is made when first needed).
    bool authStarted() const { return m_auth != nullptr; }
    // The keyring was asked for the refresh token this run.
    bool restoreTried() const { return m_restoreTried; }
    // An account signed in here before; its refresh token may be in the keyring.
    bool signedInBefore() const { return !m_signedInAs.isEmpty(); }

    // The switch in settings.toml ([steam] whole_library), on unless it says false.
    bool enabled() const;

    // The first check runs delayMs from now, then each time a day has passed since the last fetch.
    void startDaily(int delayMs = 10000);
    // Fetches now, whatever the last fetch time says.
    void syncNow();

signals:
    void stateChanged();

private:
    struct Job;

    void dailyCheck();
    void begin(bool manual);
    void fetch(const std::shared_ptr<Job> &job);
    void request(const std::shared_ptr<Job> &job, const SteamWebApi::Request &request,
                 std::function<void(const QJsonDocument &json)> done);
    void fetchTypes(const std::shared_ptr<Job> &job);
    void fetchFamily(const std::shared_ptr<Job> &job);
    void requestDone(const std::shared_ptr<Job> &job);
    void finish(Outcome outcome, const QString &error = {});
    void readLastFetch();
    void writeState();

    Setup m_setup;
    std::unique_ptr<SteamAuth> m_auth;
    QTimer m_daily;
    State m_state = State::Idle;
    Outcome m_outcome = Outcome::None;
    QString m_error;
    QDateTime m_lastFetch;
    int m_gameCount = 0;
    bool m_restoreTried = false;
    QString m_fixtureAccount;
    QString m_signedInAs;

    static Setup &pending();
    static inline SteamLibrarySync *s_instance = nullptr;
};
