#include "fakehttpserver.h"
#include "steamauth.h"

#include <QDir>
#include <QDirIterator>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QUrlQuery>

namespace {
const QByteArray kBegin = "/IAuthenticationService/BeginAuthSessionViaQR/v1/";
const QByteArray kPoll = "/IAuthenticationService/PollAuthSessionStatus/v1/";
const QByteArray kAccess = "/IAuthenticationService/GenerateAccessTokenForApp/v1/";
const QString kSteamId = QStringLiteral("76561198000000001");

QByteArray jwt(const QString &subject, qint64 expiresIn)
{
    const auto part = [](const QJsonObject &o) {
        return QJsonDocument(o).toJson(QJsonDocument::Compact).toBase64(QByteArray::Base64UrlEncoding
                                                                         | QByteArray::OmitTrailingEquals);
    };
    return part({{"alg", "none"}}) + "." + part({{"sub", subject}, {"exp", QDateTime::currentSecsSinceEpoch() + expiresIn}})
        + ".sig";
}

FakeHttpServer::Response answer(int eresult, const QJsonObject &response = {})
{
    return {200, QJsonDocument(QJsonObject{{"response", response}}).toJson(QJsonDocument::Compact), 0,
            "x-eresult: " + QByteArray::number(eresult) + "\r\n"};
}

QJsonObject inputOf(const FakeHttpServer::Request &request)
{
    const QUrlQuery form(QString::fromUtf8(request.body));
    return QJsonDocument::fromJson(form.queryItemValue(QStringLiteral("input_json"), QUrl::FullyDecoded).toUtf8())
        .object();
}

// Counts the keyring's writes and deletes.
class SpyStore : public MemorySecretStore
{
public:
    bool persistent() const override { return true; }
    bool remove(const Attributes &attributes, QString *error = nullptr) override
    {
        ++removals;
        return MemorySecretStore::remove(attributes, error);
    }
    std::atomic<int> removals{0};
};

QStringList listing(const QString &dir)
{
    QStringList all;
    QDirIterator it(dir, QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden, QDirIterator::Subdirectories);
    while (it.hasNext())
        all.append(it.next());
    all.sort();
    return all;
}
}

class SteamAuthTest : public QObject
{
    Q_OBJECT

    FakeHttpServer *m_server = nullptr;
    std::shared_ptr<SpyStore> m_store;
    SteamAuth *m_auth = nullptr;

    void signedInTokens(const QByteArray &refresh)
    {
        m_server->on(kBegin, answer(1, {{"challenge_url", "https://s.team/q/1/100"},
                                        {"client_id", "100"},
                                        {"request_id", "cmVx"},
                                        {"interval", 0.01}}));
        m_server->on(kPoll, answer(1, {{"refresh_token", QString::fromUtf8(refresh)},
                                       {"access_token", QString::fromUtf8(jwt(kSteamId, 3600))},
                                       {"account_name", "someone"}}));
    }

private slots:
    void init()
    {
        m_server = new FakeHttpServer;
        m_store = std::make_shared<SpyStore>();
        m_auth = new SteamAuth(m_store, m_server->url());
    }

    void cleanup()
    {
        delete m_auth;
        delete m_server;
    }

    void readsTheTokenClaims()
    {
        const auto claims = SteamAuth::parseToken(jwt(kSteamId, 100));
        QVERIFY(claims.valid);
        QCOMPARE(claims.subject, kSteamId);
        QVERIFY(claims.expires > QDateTime::currentDateTimeUtc());
        QVERIFY(!SteamAuth::parseToken("nonsense").valid);
        QVERIFY(!SteamAuth::parseToken("a.b.c").valid);
    }

    void approvedSignInKeepsTheTokenInTheKeyring()
    {
        const QByteArray refresh = jwt(kSteamId, 86400 * 200);
        signedInTokens(refresh);
        m_auth->startSignIn();
        QTRY_COMPARE(m_auth->status(), SteamAuth::Status::SignedIn);
        QCOMPARE(m_auth->steamId(), kSteamId);
        QCOMPARE(m_auth->accountName(), QStringLiteral("someone"));
        QVERIFY(m_auth->challengeUrl().isEmpty());
        QTRY_VERIFY(m_auth->keptInKeyring());
        QCOMPARE(m_store->read(SteamAuth::itemAttributes(kSteamId)), std::optional<QByteArray>(refresh));

        const QJsonObject begin = inputOf(m_server->requestsTo(kBegin).first());
        QCOMPARE(begin.value("platform_type").toInt(), 1);
        const QJsonObject poll = inputOf(m_server->requestsTo(kPoll).first());
        QCOMPARE(poll.value("client_id").toString(), QStringLiteral("100"));
        QCOMPARE(poll.value("request_id").toString(), QStringLiteral("cmVx"));
    }

    void theCodeRotatesWhileWaiting()
    {
        m_server->on(kBegin, answer(1, {{"challenge_url", "https://s.team/q/1/100"},
                                        {"client_id", "100"},
                                        {"request_id", "cmVx"},
                                        {"interval", 0.01}}));
        m_server->on(kPoll, answer(1, {{"new_challenge_url", "https://s.team/q/1/200"}, {"new_client_id", "200"}}));
        m_server->on(kPoll, answer(1));
        m_auth->startSignIn();
        QTRY_COMPARE(m_auth->status(), SteamAuth::Status::Waiting);
        QCOMPARE(m_auth->challengeUrl(), QStringLiteral("https://s.team/q/1/100"));
        QTRY_COMPARE(m_auth->challengeUrl(), QStringLiteral("https://s.team/q/1/200"));
        QTRY_VERIFY(m_server->requestsTo(kPoll).size() >= 3);
        QCOMPARE(inputOf(m_server->requestsTo(kPoll).at(2)).value("client_id").toString(), QStringLiteral("200"));
        QCOMPARE(m_auth->status(), SteamAuth::Status::Waiting);
    }

    void anExpiredCodeCanBeReplaced()
    {
        m_server->on(kBegin, answer(1, {{"challenge_url", "https://s.team/q/1/100"},
                                        {"client_id", "100"},
                                        {"request_id", "cmVx"},
                                        {"interval", 0.01}}));
        m_server->on(kPoll, answer(27));
        m_auth->startSignIn();
        QTRY_COMPARE(m_auth->status(), SteamAuth::Status::CodeExpired);
        QVERIFY(m_auth->challengeUrl().isEmpty());
        const int polls = m_server->requestsTo(kPoll).size();
        QTest::qWait(60);
        QCOMPARE(m_server->requestsTo(kPoll).size(), polls);

        m_server->clear(kBegin);
        m_server->clear(kPoll);
        signedInTokens(jwt(kSteamId, 86400));
        m_auth->startSignIn();
        QTRY_COMPARE(m_auth->status(), SteamAuth::Status::SignedIn);
    }

    void cancellingStopsPolling()
    {
        m_server->on(kBegin, answer(1, {{"challenge_url", "https://s.team/q/1/100"},
                                        {"client_id", "100"},
                                        {"request_id", "cmVx"},
                                        {"interval", 0.01}}));
        m_server->on(kPoll, answer(1));
        m_auth->startSignIn();
        QTRY_VERIFY(m_server->requestsTo(kPoll).size() >= 2);
        m_auth->cancel();
        QCOMPARE(m_auth->status(), SteamAuth::Status::SignedOut);
        QVERIFY(m_auth->challengeUrl().isEmpty());
        QTest::qWait(30);
        const int polls = m_server->requestsTo(kPoll).size();
        QTest::qWait(80);
        QCOMPARE(m_server->requestsTo(kPoll).size(), polls);
        QVERIFY(!m_store->read(SteamAuth::itemAttributes(kSteamId)));
    }

    void aRefusedBeginShowsAnError()
    {
        m_server->on(kBegin, answer(2));
        m_auth->startSignIn();
        QTRY_VERIFY(!m_auth->error().isEmpty());
        QCOMPARE(m_auth->status(), SteamAuth::Status::SignedOut);
    }

    void signOutDeletesTheItemAndTheTokens()
    {
        signedInTokens(jwt(kSteamId, 86400));
        m_auth->startSignIn();
        QTRY_VERIFY(m_auth->keptInKeyring());
        m_auth->signOut();
        QCOMPARE(m_auth->status(), SteamAuth::Status::SignedOut);
        QTRY_VERIFY(!m_store->read(SteamAuth::itemAttributes(kSteamId)));
        bool called = false;
        m_auth->accessToken(this, [&](const QString &token, const QString &error) {
            called = true;
            QVERIFY(token.isEmpty());
            QVERIFY(!error.isEmpty());
        });
        QVERIFY(called);
    }

    void restoresFromTheKeyringAndExchangesTheToken()
    {
        const QByteArray refresh = jwt(kSteamId, 86400 * 100);
        m_store->store(QStringLiteral("x"), SteamAuth::itemAttributes(kSteamId), refresh);
        const QString access = QString::fromUtf8(jwt(kSteamId, 86400));
        m_server->on(kAccess, answer(1, {{"access_token", access}}));

        QSignalSpy restored(m_auth, &SteamAuth::restored);
        m_auth->restore(kSteamId);
        QTRY_COMPARE(restored.size(), 1);
        QCOMPARE(m_auth->status(), SteamAuth::Status::SignedIn);
        QVERIFY(m_auth->keptInKeyring());

        QStringList got;
        for (int i = 0; i < 2; ++i) {
            m_auth->accessToken(this, [&](const QString &token, const QString &error) {
                QVERIFY2(error.isEmpty(), qPrintable(error));
                got.append(token);
            });
        }
        QTRY_COMPARE(got.size(), 2);
        QCOMPARE(got, (QStringList{access, access}));
        QCOMPARE(m_server->requestsTo(kAccess).size(), 1);
        const QJsonObject input = inputOf(m_server->requestsTo(kAccess).first());
        QCOMPARE(input.value("refresh_token").toString(), QString::fromUtf8(refresh));
        QCOMPARE(input.value("steamid").toString(), kSteamId);

        m_auth->accessToken(this, [&](const QString &token, const QString &) { got.append(token); });
        QCOMPARE(got.size(), 3);
        QCOMPARE(m_server->requestsTo(kAccess).size(), 1);
    }

    void aRefusedRefreshTokenDeletesTheItem()
    {
        m_store->store(QStringLiteral("x"), SteamAuth::itemAttributes(kSteamId), jwt(kSteamId, 86400));
        m_server->on(kAccess, answer(15));
        m_auth->restore(kSteamId);
        QTRY_COMPARE(m_auth->status(), SteamAuth::Status::SignedIn);

        QString error;
        m_auth->accessToken(this, [&](const QString &, const QString &e) { error = e; });
        QTRY_VERIFY(!error.isEmpty());
        QCOMPARE(m_auth->status(), SteamAuth::Status::TokenRefused);
        QTRY_COMPARE(m_store->removals.load(), 1);
        QVERIFY(!m_store->read(SteamAuth::itemAttributes(kSteamId)));
    }

    void aSteamOutageKeepsTheItem()
    {
        m_store->store(QStringLiteral("x"), SteamAuth::itemAttributes(kSteamId), jwt(kSteamId, 86400));
        m_server->on(kAccess, answer(20));
        m_auth->restore(kSteamId);
        QTRY_COMPARE(m_auth->status(), SteamAuth::Status::SignedIn);
        QString error;
        m_auth->accessToken(this, [&](const QString &, const QString &e) { error = e; });
        QTRY_VERIFY(!error.isEmpty());
        QCOMPARE(m_auth->status(), SteamAuth::Status::SignedIn);
        QCOMPARE(m_store->removals.load(), 0);
        QVERIFY(m_store->read(SteamAuth::itemAttributes(kSteamId)));
    }

    void anExpiredStoredTokenIsDeleted()
    {
        m_store->store(QStringLiteral("x"), SteamAuth::itemAttributes(kSteamId), jwt(kSteamId, -10));
        m_auth->restore(kSteamId);
        QTRY_COMPARE(m_auth->status(), SteamAuth::Status::TokenRefused);
        QTRY_VERIFY(!m_store->read(SteamAuth::itemAttributes(kSteamId)));
    }

    void anotherAccountsItemIsNotRestored()
    {
        m_store->store(QStringLiteral("x"), SteamAuth::itemAttributes(kSteamId), jwt(kSteamId, 86400));
        m_auth->restore(QStringLiteral("76561198000000002"));
        QTest::qWait(50);
        QCOMPARE(m_auth->status(), SteamAuth::Status::SignedOut);
    }

    void withoutAKeyringTheSessionLastsInMemory()
    {
        auto memory = std::make_shared<MemorySecretStore>();
        SteamAuth auth(memory, m_server->url());
        signedInTokens(jwt(kSteamId, 86400));
        auth.startSignIn();
        QTRY_COMPARE(auth.status(), SteamAuth::Status::SignedIn);
        QTest::qWait(30);
        QVERIFY(!auth.keptInKeyring());

        auth.signOut();
        QCOMPARE(auth.status(), SteamAuth::Status::SignedOut);
    }

    void nothingIsWrittenToTheDataDir()
    {
        QTemporaryDir data;
        QVERIFY(data.isValid());
        qputenv("LATTIS_DATA", data.path().toUtf8());
        const QStringList before = listing(data.path());
        signedInTokens(jwt(kSteamId, 86400));
        m_server->on(kAccess, answer(1, {{"access_token", QString::fromUtf8(jwt(kSteamId, 86400))}}));
        m_auth->startSignIn();
        QTRY_VERIFY(m_auth->keptInKeyring());
        QString token;
        m_auth->accessToken(this, [&](const QString &t, const QString &) { token = t; });
        m_auth->signOut();
        QTest::qWait(50);
        qunsetenv("LATTIS_DATA");
        QCOMPARE(listing(data.path()), before);
    }
};

QTEST_MAIN(SteamAuthTest)
#include "steamauth_test.moc"
