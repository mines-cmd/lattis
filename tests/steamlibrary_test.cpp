#include "fakehttpserver.h"
#include "testfiles.h"
#include "credentials.h"
#include "metadatahttp.h"
#include "pluralfallback.h"
#include "qrcode.h"
#include "steamfriends.h"
#include "steamlibrary.h"
#include "steamlibrarysync.h"

#include <QCoreApplication>
#include <QDir>
#include <QJsonArray>
#include <QJsonObject>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

namespace {
const QString kMe = QStringLiteral("76561198000000001");
const QByteArray kBegin = "/IAuthenticationService/BeginAuthSessionViaQR/v1/";
const QByteArray kPoll = "/IAuthenticationService/PollAuthSessionStatus/v1/";
const QByteArray kAccess = "/IAuthenticationService/GenerateAccessTokenForApp/v1/";
const QByteArray kOwned = "/IPlayerService/GetOwnedGames/v1/";
const QByteArray kItems = "/IStoreBrowseService/GetItems/v1";
const QByteArray kGroupFor = "/IFamilyGroupsService/GetFamilyGroupForUser/v1/";
const QByteArray kFriends = "/ISteamUser/GetFriendList/v1/";
const QByteArray kClient = "/IClientCommService/GetClientAppList/v1/";

QByteArray jwt(const QString &subject, qint64 expiresIn)
{
    const auto part = [](const QJsonObject &o) {
        return QJsonDocument(o).toJson(QJsonDocument::Compact).toBase64(QByteArray::Base64UrlEncoding
                                                                         | QByteArray::OmitTrailingEquals);
    };
    return part({{"alg", "none"}}) + "." + part({{"sub", subject}, {"exp", QDateTime::currentSecsSinceEpoch() + expiresIn}})
        + ".sig";
}

FakeHttpServer::Response answer(int eresult, const QJsonObject &response = {}, int delayMs = 0)
{
    return {200, QJsonDocument(QJsonObject{{"response", response}}).toJson(QJsonDocument::Compact), delayMs,
            "x-eresult: " + QByteArray::number(eresult) + "\r\n"};
}

QJsonArray ownedGames(int count)
{
    QJsonArray games;
    for (int i = 0; i < count; ++i)
        games.append(QJsonObject{{"appid", 1000 + i}, {"name", QStringLiteral("Game %1").arg(1000 + i)}});
    return games;
}

// A keyring that answers (so the sign-in is kept) and may refuse the write.
class Keyring : public MemorySecretStore
{
public:
    bool persistent() const override { return persistentStore; }
    bool store(const QString &label, const Attributes &attributes, const QByteArray &secret,
               QString *error = nullptr) override
    {
        return !refuse && MemorySecretStore::store(label, attributes, secret, error);
    }
    std::atomic<bool> refuse{false};
    bool persistentStore = true;
};
}

// The object the Steam settings page binds to, over a real sync and sign-in against a local server.
class SteamLibraryTest : public QObject
{
    Q_OBJECT

    QTemporaryDir m_root;
    FakeHttpServer *m_server = nullptr;
    MetadataHttp *m_http = nullptr;
    std::shared_ptr<Keyring> m_keyring;
    SteamLibrarySync::Setup m_setup;
    SteamLibrarySync *m_sync = nullptr;
    SteamLibrary *m_library = nullptr;
    QString m_steamId;
    bool m_held = false;

    QString data() const { return m_root.filePath(QStringLiteral("data")); }
    QString cache() const { return m_root.filePath(QStringLiteral("cache")); }
    QString fixtures() const { return m_root.filePath(QStringLiteral("fixtures")); }
    QString stateFile() const { return m_root.filePath(QStringLiteral("state/steam-library.toml")); }
    QString cacheFile() const { return SteamWebApi::cachePath(cache(), QStringLiteral("test")); }
    QByteArray credentials() const { return readFile(CredentialsFile::path(data())); }

    void make(const QString &fixturesDir = {})
    {
        m_setup = {};
        m_setup.dataDir = data();
        m_setup.cacheDir = cache();
        m_setup.cacheKey = QStringLiteral("test");
        m_setup.stateFile = stateFile();
        m_setup.steamId = [this] { return m_steamId; };
        m_setup.held = [this] { return m_held; };
        m_setup.http = [this] { return m_http; };
        m_setup.secrets = m_keyring;
        m_setup.authBase = m_server->url();
        m_setup.fixturesDir = fixturesDir;
        m_sync = new SteamLibrarySync(m_setup);
        m_library = new SteamLibrary(m_sync);
    }

    void discard()
    {
        delete m_library;
        delete m_sync;
        m_library = nullptr;
        m_sync = nullptr;
    }

    void withKey()
    {
        QVERIFY(CredentialsFile::setValue(data(), QStringLiteral("steam"), QStringLiteral("web_key"),
                                          QStringLiteral("SECRETKEY")));
    }
    void tokenInKeyring()
    {
        m_keyring->store(QStringLiteral("x"), SteamAuth::itemAttributes(kMe), jwt(kMe, 86400 * 100));
    }
    void signedInHereBefore() { writeFile(stateFile(), "signed_in = \"" + kMe.toUtf8() + "\"\n"); }
    bool keptInKeyring() { return m_keyring->read(SteamAuth::itemAttributes(kMe)).has_value(); }

    // A QR code that stays up: the next poll is a minute away.
    void codeWaits()
    {
        m_server->on(kBegin, answer(1, {{"challenge_url", "https://s.team/q/1/100"}, {"client_id", "100"},
                                        {"request_id", "cmVx"}, {"interval", 60}}));
    }

    // A QR code that the phone approves at the first poll.
    void codeApproved()
    {
        m_server->on(kBegin, answer(1, {{"challenge_url", "https://s.team/q/1/100"}, {"client_id", "100"},
                                        {"request_id", "cmVx"}, {"interval", 0.01}}));
        m_server->on(kPoll, answer(1, {{"refresh_token", QString::fromUtf8(jwt(kMe, 86400 * 200))},
                                       {"access_token", QString::fromUtf8(jwt(kMe, 3600))},
                                       {"account_name", "someone"}}));
    }

    void ownedAnswers(int games)
    {
        m_server->clear(kOwned);
        const QJsonObject list = games ? QJsonObject{{"game_count", games}, {"games", ownedGames(games)}} : QJsonObject();
        m_server->on(kOwned, answer(1, list));
        m_server->on(kItems, answer(1, {{"store_items", QJsonArray()}}));
        m_server->on(kGroupFor, answer(1, {{"is_not_member_of_any_group", true}}));
        m_server->on(kClient, answer(1, {{"apps", QJsonArray()}}));
    }

    void syncAndWait()
    {
        m_library->syncNow();
        QTRY_VERIFY(!m_library->syncing());
    }

private slots:
    void initTestCase()
    {
        QVERIFY(m_root.isValid());
        QCoreApplication::installTranslator(new PluralFallback(QCoreApplication::instance()));
        // setWebKey() and syncNow() also ask the friends list; it shares the test's data folder and server.
        SteamFriends::Setup friends;
        friends.dataDir = data();
        friends.cacheDir = cache();
        friends.cacheKey = QStringLiteral("friends");
        friends.steamId = [this] { return m_steamId; };
        friends.held = [] { return false; };
        friends.http = [this] { return m_http; };
        friends.requestIntervalMs = 0;
        SteamFriends::setup(friends);
    }

    void init()
    {
        QDir().mkpath(data());
        m_server = new FakeHttpServer;
        MetadataEndpoints endpoints;
        endpoints.steamWeb = m_server->url();
        m_http = new MetadataHttp(cache(), endpoints);
        m_keyring = std::make_shared<Keyring>();
        m_steamId = kMe;
        m_held = false;
    }

    void cleanup()
    {
        QTRY_COMPARE(SteamFriends::instance()->state(), SteamFriends::State::Idle);
        discard();
        delete m_http;
        delete m_server;
        for (const QString &entry : QDir(m_root.path()).entryList(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden)) {
            QFileInfo info(m_root.filePath(entry));
            if (info.isDir())
                QDir(info.filePath()).removeRecursively();
            else
                QFile::remove(info.filePath());
        }
    }

    void aSignInSteamRefusesShowsNoCode()
    {
        m_server->on(kBegin, answer(2));
        make();
        QSignalSpy signedIn(m_library, &SteamLibrary::signedIn);
        m_library->signIn();
        QTRY_COMPARE(m_server->requestsTo(kBegin).size(), 1);
        QTRY_COMPARE(m_sync->auth()->status(), SteamAuth::Status::SignedOut);
        QTest::qWait(50);
        QCOMPARE(m_library->authState(), QStringLiteral("signed_out"));
        QVERIFY(m_library->qrPath().isEmpty());
        QCOMPARE(m_library->qrSize(), 0);
        QCOMPARE(m_library->messageKind(), QString());
        QVERIFY(signedIn.isEmpty());
        QVERIFY(!m_library->configured());
        QVERIFY(m_server->requestsTo(kOwned).isEmpty());
        QVERIFY(!keptInKeyring());
        QVERIFY(!QFileInfo::exists(stateFile()));
    }

    void aCodeThatRunsOutIsExpired()
    {
        m_server->on(kBegin, answer(1, {{"challenge_url", "https://s.team/q/1/100"}, {"client_id", "100"},
                                        {"request_id", "cmVx"}, {"interval", 0.01}}));
        m_server->on(kPoll, answer(27));
        make();
        m_library->signIn();
        QTRY_COMPARE(m_library->authState(), QStringLiteral("expired"));
        QVERIFY(m_library->qrPath().isEmpty());
        QCOMPARE(m_library->qrSize(), 0);
        QCOMPARE(m_library->messageKind(), QString());
        QVERIFY(!keptInKeyring());

        // Asking again puts a new code up.
        m_server->clear(kBegin);
        m_server->clear(kPoll);
        codeWaits();
        m_library->signIn();
        QTRY_COMPARE(m_library->authState(), QStringLiteral("waiting"));
        QVERIFY(!m_library->qrPath().isEmpty());
    }

    void theCodeIsShownRotatedAndTakenDown()
    {
        // A failed sync first: its message gives way to the code, and comes back when the code goes.
        withKey();
        m_server->on(kOwned, {503, "busy"});
        make();
        syncAndWait();
        QCOMPARE(m_library->messageKind(), QStringLiteral("failed"));

        m_server->on(kBegin, answer(1, {{"challenge_url", "https://s.team/q/1/100"}, {"client_id", "100"},
                                        {"request_id", "cmVx"}, {"interval", 0.05}}));
        m_server->on(kPoll, answer(1, {{"new_challenge_url", "https://s.team/q/1/200"}, {"new_client_id", "200"}}));
        m_server->on(kPoll, answer(1));
        QSignalSpy changed(m_library, &SteamLibrary::changed);
        m_library->signIn();
        QVERIFY(!changed.isEmpty());
        QTRY_COMPARE(m_library->authState(), QStringLiteral("waiting"));
        QCOMPARE(m_library->messageKind(), QString());
        QCOMPARE(m_library->message(), QString());
        const QString first = m_library->qrPath();
        QVERIFY(!first.isEmpty());
        QCOMPARE(m_library->qrSize(), QrCode::encode("https://s.team/q/1/100").size);

        QTRY_VERIFY(m_library->qrPath() != first);
        QVERIFY(!m_library->qrPath().isEmpty());
        QCOMPARE(m_library->authState(), QStringLiteral("waiting"));

        m_library->cancelSignIn();
        QCOMPARE(m_library->authState(), QStringLiteral("signed_out"));
        QVERIFY(m_library->qrPath().isEmpty());
        QCOMPARE(m_library->qrSize(), 0);
        QCOMPARE(m_library->messageKind(), QStringLiteral("failed"));
        QVERIFY(!keptInKeyring());
    }

    void anApprovedSignInSyncs()
    {
        codeApproved();
        ownedAnswers(3);
        make();
        QSignalSpy signedIn(m_library, &SteamLibrary::signedIn);
        m_library->signIn();
        QTRY_COMPARE(signedIn.size(), 1);
        QCOMPARE(m_library->authState(), QStringLiteral("signed_in"));
        QCOMPARE(m_library->accountName(), QStringLiteral("someone"));
        QVERIFY(m_library->configured());
        QVERIFY(m_library->qrPath().isEmpty());
        QCOMPARE(m_library->qrSize(), 0);
        QTRY_VERIFY(keptInKeyring());
        QVERIFY(m_library->keyringAvailable());
        QVERIFY(!m_library->keyringRefused());
        QVERIFY(readFile(stateFile()).contains("signed_in = \"" + kMe.toUtf8() + "\""));

        // The sign-in fetches at once, with the account's token.
        QTRY_COMPARE(m_server->requestsTo(kOwned).size(), 1);
        QVERIFY(m_server->requestsTo(kOwned).first().target.contains("access_token="));
        QTRY_VERIFY(!m_library->syncing());
        QCOMPARE(m_sync->outcome(), SteamLibrarySync::Outcome::Synced);
        QCOMPARE(m_library->messageKind(), QString());
        QCOMPARE(m_library->statusLine(), QStringLiteral("3 games · synced just now"));
    }

    void aSignInWhileTheSetupGuideIsOpenFetchesNothing()
    {
        m_held = true;
        codeApproved();
        ownedAnswers(3);
        make();
        QSignalSpy signedIn(m_library, &SteamLibrary::signedIn);
        m_library->signIn();
        QTRY_COMPARE(signedIn.size(), 1);
        QCOMPARE(m_library->authState(), QStringLiteral("signed_in"));
        QTest::qWait(100);
        QVERIFY(m_server->requestsTo(kOwned).isEmpty());
        QVERIFY(!m_library->syncing());
    }

    void aKeyringThatRefusesIsShown()
    {
        m_keyring->refuse = true;
        codeApproved();
        ownedAnswers(1);
        make();
        QSignalSpy changed(m_library, &SteamLibrary::changed);
        m_library->signIn();
        QTRY_VERIFY(m_library->keyringRefused());
        QCOMPARE(m_library->authState(), QStringLiteral("signed_in"));
        QVERIFY(m_library->keyringAvailable());
        QVERIFY(!keptInKeyring());
        QTRY_VERIFY(!m_library->syncing());

        // Without a Secret Service, the page says the sign-in lasts until Lattis quits.
        discard();
        m_keyring = std::make_shared<Keyring>();
        m_keyring->persistentStore = false;
        make();
        QVERIFY(!m_library->keyringAvailable());
        QVERIFY(!m_library->keyringRefused());
    }

    void signOutForgetsTheAccount()
    {
        codeApproved();
        ownedAnswers(2);
        make();
        m_library->signIn();
        QTRY_COMPARE(m_library->authState(), QStringLiteral("signed_in"));
        QTRY_VERIFY(keptInKeyring());
        QTRY_VERIFY(!m_library->syncing());
        const QByteArray cached = readFile(cacheFile());

        QSignalSpy changed(m_library, &SteamLibrary::changed);
        m_library->signOut();
        QVERIFY(!changed.isEmpty());
        QCOMPARE(m_library->authState(), QStringLiteral("signed_out"));
        QCOMPARE(m_library->accountName(), QString());
        QVERIFY(!m_library->configured());
        QTRY_VERIFY(!keptInKeyring());
        QVERIFY(!readFile(stateFile()).contains("signed_in"));
        // Signing out isn't "sign in again", and the games fetched stay.
        QCOMPARE(m_library->messageKind(), QString());
        QCOMPARE(readFile(cacheFile()), cached);
        QCOMPARE(m_library->statusLine(), QStringLiteral("2 games · synced just now"));

        // Signing out while the code is up takes the code down.
        m_server->clear(kBegin);
        codeWaits();
        m_library->signIn();
        QTRY_VERIFY(!m_library->qrPath().isEmpty());
        m_library->signOut();
        QVERIFY(m_library->qrPath().isEmpty());
        QCOMPARE(m_library->authState(), QStringLiteral("signed_out"));
    }

    void aRefusedSignInSaysItExpired()
    {
        tokenInKeyring();
        signedInHereBefore();
        m_server->on(kAccess, answer(5));
        make();
        syncAndWait();
        QCOMPARE(m_sync->outcome(), SteamLibrarySync::Outcome::SignInExpired);
        QCOMPARE(m_library->authState(), QStringLiteral("refused"));
        QCOMPARE(m_library->messageKind(), QStringLiteral("expired"));
        QCOMPARE(m_library->message(), QStringLiteral("Your Steam sign-in expired."));
        QTRY_VERIFY(!keptInKeyring());
        QVERIFY(!readFile(stateFile()).contains("signed_in"));
        QVERIFY(m_server->requestsTo(kOwned).isEmpty());

        // The code going up and away again leaves the sync's outcome to say it.
        codeWaits();
        m_library->signIn();
        QTRY_COMPARE(m_library->authState(), QStringLiteral("waiting"));
        QCOMPARE(m_library->messageKind(), QString());
        m_library->cancelSignIn();
        QCOMPARE(m_library->authState(), QStringLiteral("signed_out"));
        QCOMPARE(m_library->messageKind(), QStringLiteral("expired"));
        QCOMPARE(m_library->message(), QStringLiteral("Your Steam sign-in expired."));
    }

    void aRefusedSignInStillSaysSoWhenTheKeyFetched()
    {
        tokenInKeyring();
        signedInHereBefore();
        withKey();
        m_server->on(kAccess, answer(5));
        ownedAnswers(2);
        make();
        syncAndWait();
        QCOMPARE(m_sync->outcome(), SteamLibrarySync::Outcome::Synced);
        QVERIFY(m_server->requestsTo(kOwned).first().target.contains("key=SECRETKEY"));
        QCOMPARE(m_library->authState(), QStringLiteral("refused"));
        QCOMPARE(m_library->messageKind(), QStringLiteral("expired"));
    }

    void anAccountWhoseKeyringIsGoneSignsInAgain()
    {
        signedInHereBefore();
        make();
        QCOMPARE(m_library->messageKind(), QString()); // the keyring wasn't asked yet
        m_sync->startDaily(3600000);
        QTRY_COMPARE(m_library->messageKind(), QStringLiteral("again"));
        QCOMPARE(m_library->authState(), QStringLiteral("signed_out"));
        QCOMPARE(m_library->message(),
                 QStringLiteral("Sign in again to update your whole library. The games already fetched stay in the library."));
    }

    void anAccountRestoredFromTheKeyringSaysNothing()
    {
        signedInHereBefore();
        tokenInKeyring();
        make();
        QSignalSpy changed(m_library, &SteamLibrary::changed);
        m_sync->startDaily(3600000);
        QTRY_COMPARE(m_library->authState(), QStringLiteral("signed_in"));
        QTRY_VERIFY(!changed.isEmpty());
        QCOMPARE(m_library->messageKind(), QString());
        QVERIFY(m_library->configured());
    }

    void aKeyThatGetsNoGamesAsksAboutPrivacy()
    {
        withKey();
        ownedAnswers(0);
        make();
        syncAndWait();
        QCOMPARE(m_sync->outcome(), SteamLibrarySync::Outcome::NoGames);
        QCOMPARE(m_library->messageKind(), QStringLiteral("private"));
        QCOMPARE(m_library->message(), QStringLiteral("Steam returned no games: is Game details public?"));
        QVERIFY(m_library->statusLine().isEmpty());

        // Signed in, the account's own token sees private details: no question to ask.
        m_held = true;
        codeApproved();
        m_library->signIn();
        QTRY_COMPARE(m_library->authState(), QStringLiteral("signed_in"));
        QCOMPARE(m_sync->outcome(), SteamLibrarySync::Outcome::NoGames);
        QCOMPARE(m_library->messageKind(), QString());
    }

    void anOutageSaysWhatFailed()
    {
        withKey();
        m_server->on(kOwned, {503, "busy"});
        make();
        syncAndWait();
        QCOMPARE(m_library->messageKind(), QStringLiteral("failed"));
        QVERIFY(!m_sync->error().isEmpty());
        QCOMPARE(m_library->message(), QStringLiteral("Couldn't reach Steam: %1").arg(m_sync->error()));
        QVERIFY(!m_library->message().contains(QLatin1String("SECRETKEY")));
        QVERIFY(m_library->statusLine().isEmpty());

        // While the next sync runs, nothing is said; after it, nothing is wrong.
        m_server->clear(kOwned);
        m_server->on(kOwned, answer(1, {{"games", ownedGames(1)}}, 300));
        m_server->on(kItems, answer(1, {{"store_items", QJsonArray()}}));
        m_library->syncNow();
        QVERIFY(m_library->syncing());
        QCOMPARE(m_library->messageKind(), QString());
        QCOMPARE(m_library->message(), QString());
        QTRY_VERIFY(!m_library->syncing());
        QCOMPARE(m_library->messageKind(), QString());
        QCOMPARE(m_library->statusLine(), QStringLiteral("1 game · synced just now"));
    }

    void noSteamAccountOnThisComputer()
    {
        m_steamId.clear();
        make();
        syncAndWait();
        QCOMPARE(m_sync->outcome(), SteamLibrarySync::Outcome::NoAccount);
        // Nothing set up: nothing to complain about.
        QCOMPARE(m_library->messageKind(), QString());

        QCOMPARE(m_library->setWebKey(QStringLiteral("SECRETKEY")), QString());
        QTRY_VERIFY(!m_library->syncing());
        QCOMPARE(m_sync->outcome(), SteamLibrarySync::Outcome::NoAccount);
        QCOMPARE(m_library->messageKind(), QStringLiteral("empty"));
        QCOMPARE(m_library->message(),
                 QStringLiteral("Steam hasn't logged in on this computer yet, so Lattis doesn't know which account to fetch."));
        QVERIFY(m_server->requests.isEmpty());
    }

    void fixtureStatesStandForSteam()
    {
        writeFile(fixtures() + QStringLiteral("/signin.json"), R"({"state":"expired"})");
        make(fixtures());
        QCOMPARE(m_library->authState(), QStringLiteral("refused"));
        QCOMPARE(m_library->messageKind(), QStringLiteral("expired"));
        QCOMPARE(m_library->message(), QStringLiteral("Your Steam sign-in expired."));

        writeFile(fixtures() + QStringLiteral("/signin.json"), R"({"state":"again"})");
        QCOMPARE(m_library->authState(), QStringLiteral("signed_out"));
        QCOMPARE(m_library->messageKind(), QStringLiteral("again"));
        writeFile(fixtures() + QStringLiteral("/signin.json"), R"({"state":"private"})");
        QCOMPARE(m_library->messageKind(), QStringLiteral("private"));
        QCOMPARE(m_library->message(), QStringLiteral("Steam returned no games: is Game details public?"));

        // A fixture sign-in shows a fixed code, and no request leaves.
        writeFile(fixtures() + QStringLiteral("/signin.json"), R"({"state":"again","keyring":false,"keyring_refused":true})");
        QVERIFY(!m_library->keyringAvailable());
        QVERIFY(m_library->keyringRefused());
        m_library->signIn();
        QCOMPARE(m_library->authState(), QStringLiteral("waiting"));
        QCOMPARE(m_library->qrSize(), QrCode::encode("https://s.team/q/1/1234567890123456789").size);
        QVERIFY(!m_library->qrPath().isEmpty());
        m_library->cancelSignIn();
        QCOMPARE(m_library->authState(), QStringLiteral("signed_out"));
        QVERIFY(m_library->qrPath().isEmpty());
        QVERIFY(m_server->requests.isEmpty());
        QVERIFY(!keptInKeyring());
    }

    void theStatusLineSaysHowLongAgo()
    {
        make();
        QVERIFY(m_library->statusLine().isEmpty()); // never fetched
        discard();

        SteamWebApi::Cache c;
        c.steamId = kMe;
        c.owned = {{1, QStringLiteral("One"), 0, 0}, {2, QStringLiteral("Two"), 0, 0}};
        c.shared = {{2, QStringLiteral("Two"), {QStringLiteral("9")}, 1, 0, 0},
                    {3, QStringLiteral("Three"), {QStringLiteral("9")}, 1, 0, 0}};
        QVERIFY(SteamWebApi::saveCache(cacheFile(), c));
        const auto fetchedAgo = [&](qint64 seconds) {
            writeFile(stateFile(), "last_fetch = \""
                          + QDateTime::currentDateTimeUtc().addSecs(-seconds).toString(Qt::ISODate).toUtf8() + "\"\n");
            discard();
            make();
            return m_library->statusLine();
        };
        QCOMPARE(fetchedAgo(10), QStringLiteral("3 games · synced just now"));
        QCOMPARE(fetchedAgo(-3600), QStringLiteral("3 games · synced just now")); // the clock went back
        QCOMPARE(fetchedAgo(60), QStringLiteral("3 games · synced 1 min ago"));
        QCOMPARE(fetchedAgo(59 * 60 + 30), QStringLiteral("3 games · synced 59 min ago"));
        QCOMPARE(fetchedAgo(3600), QStringLiteral("3 games · synced 1 h ago"));
        QCOMPARE(fetchedAgo(23 * 3600 + 1800), QStringLiteral("3 games · synced 23 h ago"));
        QCOMPARE(fetchedAgo(86400), QStringLiteral("3 games · synced 1 day ago"));
        QCOMPARE(fetchedAgo(10 * 86400), QStringLiteral("3 games · synced 10 days ago"));

        // A fetch that found nothing has no line.
        QVERIFY(SteamWebApi::saveCache(cacheFile(), SteamWebApi::Cache{}));
        QVERIFY(fetchedAgo(10).isEmpty());
    }

    void aKeyThatCantBeSavedChangesNothing()
    {
        QDir().mkpath(CredentialsFile::path(data())); // a folder where the file goes
        make();
        QSignalSpy changed(m_library, &SteamLibrary::changed);
        const QString error = m_library->setWebKey(QStringLiteral("SECRETKEY"));
        QVERIFY(error.startsWith(QLatin1String("Couldn't save credentials.toml: ")));
        QVERIFY(error.size() > QStringLiteral("Couldn't save credentials.toml: ").size());
        QVERIFY(!error.contains(QLatin1String("SECRETKEY")));
        QVERIFY(changed.isEmpty());
        QCOMPARE(m_library->webKey(), QString());
        QVERIFY(!m_library->hasKey());
        QVERIFY(!m_library->configured());
        QVERIFY(!m_library->syncing());
        QTest::qWait(50);
        QVERIFY(m_server->requests.isEmpty());
        QVERIFY(QFileInfo(CredentialsFile::path(data())).isDir());
    }

    void aKeyIsSavedAndFetchesAtOnce()
    {
        ownedAnswers(2);
        make();
        QSignalSpy changed(m_library, &SteamLibrary::changed);
        QCOMPARE(m_library->setWebKey(QStringLiteral("  SECRETKEY \n")), QString());
        QVERIFY(!changed.isEmpty());
        QCOMPARE(m_library->webKey(), QStringLiteral("SECRETKEY"));
        QVERIFY(m_library->hasKey());
        QVERIFY(m_library->configured());
        QVERIFY(credentials().contains("web_key = \"SECRETKEY\""));
        QCOMPARE(QFileInfo(CredentialsFile::path(data())).permissions() & (QFile::ReadGroup | QFile::ReadOther),
                 QFileDevice::Permissions());

        QTRY_VERIFY(!m_library->syncing());
        QCOMPARE(m_server->requestsTo(kOwned).size(), 1);
        QVERIFY(m_server->requestsTo(kOwned).first().target.contains("key=SECRETKEY"));
        QTRY_COMPARE(m_server->requestsTo(kFriends).size(), 1);
        QCOMPARE(m_library->statusLine(), QStringLiteral("2 games · synced just now"));

        // A new instance (the next start) reads it back.
        delete m_library;
        m_library = new SteamLibrary(m_sync);
        QCOMPARE(m_library->webKey(), QStringLiteral("SECRETKEY"));
    }

    void aKeyWithTheSwitchOffOnlyAsksTheFriends()
    {
        writeFile(data() + QStringLiteral("/settings.toml"), "[steam]\nwhole_library = false\n");
        make();
        QVERIFY(!m_library->enabled());
        QCOMPARE(m_library->setWebKey(QStringLiteral("SECRETKEY")), QString());
        QTRY_COMPARE(m_server->requestsTo(kFriends).size(), 1);
        QVERIFY(m_server->requestsTo(kOwned).isEmpty());
        QVERIFY(!m_library->syncing());
    }

    void anEmptyKeyRemovesIt()
    {
        withKey();
        make();
        QCOMPARE(m_library->webKey(), QStringLiteral("SECRETKEY"));
        QSignalSpy changed(m_library, &SteamLibrary::changed);
        QCOMPARE(m_library->setWebKey(QStringLiteral("   ")), QString());
        QVERIFY(!changed.isEmpty());
        QCOMPARE(m_library->webKey(), QString());
        QVERIFY(!m_library->hasKey());
        QVERIFY(!m_library->configured());
        QVERIFY(!credentials().contains("web_key"));
        QTest::qWait(50);
        QVERIFY(m_server->requests.isEmpty());
        QVERIFY(!m_library->syncing());
    }
};

QTEST_GUILESS_MAIN(SteamLibraryTest)
#include "steamlibrary_test.moc"
