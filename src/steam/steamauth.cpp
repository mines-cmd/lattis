#include "steamauth.h"

#include <QFutureWatcher>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QtConcurrent>

namespace {
constexpr int kPlatformMobileApp = 3;
constexpr int kOk = 1;
// The QR session ran out: Steam answers Expired, then DuplicateRequest once it's forgotten.
constexpr int kExpired = 27;
constexpr int kDuplicateRequest = 9;
// Answers that say Steam or the network had a problem, not that the sign-in is over.
const QList<int> kTransient = {2, 3, 16, 20, 84};
constexpr int kMinPollMs = 10;
constexpr int kMaxPollFailures = 5;
constexpr int kTokenMarginSeconds = 60;

QString interfaceMethod(const QString &name)
{
    return QStringLiteral("/IAuthenticationService/%1/v1/").arg(name);
}

// Runs `work` on a worker thread (the keyring can block on a prompt) and `done` on the main one.
template<typename Result, typename Work, typename Done>
void inBackground(QObject *context, Work work, Done done)
{
    auto *watcher = new QFutureWatcher<Result>(context);
    QObject::connect(watcher, &QFutureWatcherBase::finished, context, [watcher, done] {
        done(watcher->result());
        watcher->deleteLater();
    });
    watcher->setFuture(QtConcurrent::run(std::move(work)));
}
}

SteamAuth::SteamAuth(std::shared_ptr<SecretStore> store, QUrl apiBase, QObject *parent)
    : QObject(parent)
    , m_store(std::move(store))
    , m_base(std::move(apiBase))
{
    m_pollTimer.setSingleShot(true);
    connect(&m_pollTimer, &QTimer::timeout, this, &SteamAuth::poll);
}

SteamAuth::~SteamAuth()
{
    ++m_generation;
    if (m_pending)
        m_pending->abort();
}

SteamAuth::TokenClaims SteamAuth::parseToken(const QByteArray &jwt)
{
    TokenClaims claims;
    const QList<QByteArray> parts = jwt.split('.');
    if (parts.size() != 3)
        return claims;
    const QByteArray payload = QByteArray::fromBase64(parts.at(1), QByteArray::Base64UrlEncoding
                                                                       | QByteArray::OmitTrailingEquals);
    const QJsonObject object = QJsonDocument::fromJson(payload).object();
    if (!object.contains(QLatin1String("sub")) || !object.contains(QLatin1String("exp")))
        return claims;
    claims.subject = object.value(QLatin1String("sub")).toString();
    claims.expires = QDateTime::fromSecsSinceEpoch(qint64(object.value(QLatin1String("exp")).toDouble()));
    claims.valid = !claims.subject.isEmpty();
    return claims;
}

SecretStore::Attributes SteamAuth::itemAttributes(const QString &steamId)
{
    return {{QStringLiteral("application"), QStringLiteral("lattis")}, {QStringLiteral("steamid"), steamId}};
}

void SteamAuth::post(const QString &method, const QJsonObject &input, AnswerCallback done)
{
    QUrl url = m_base;
    url.setPath(url.path() + interfaceMethod(method));
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/x-www-form-urlencoded"));
    const QByteArray body = "input_json="
        + QUrl::toPercentEncoding(QString::fromUtf8(QJsonDocument(input).toJson(QJsonDocument::Compact)));
    QNetworkReply *reply = m_network.post(request, body);
    m_pending = reply;
    const int generation = m_generation;
    connect(reply, &QNetworkReply::finished, this, [this, reply, generation, done = std::move(done)] {
        reply->deleteLater();
        if (generation != m_generation)
            return;
        Answer answer;
        const QByteArray eresult = reply->rawHeader("x-eresult");
        if (reply->error() != QNetworkReply::NoError && reply->error() != QNetworkReply::ProtocolInvalidOperationError
            && reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() != 200) {
            answer.networkError = reply->errorString();
        } else if (eresult.isEmpty()) {
            answer.networkError = QStringLiteral("Steam sent no result");
        } else {
            answer.eresult = eresult.toInt();
            answer.response = QJsonDocument::fromJson(reply->readAll()).object().value(QLatin1String("response")).toObject();
        }
        done(answer);
    });
}

void SteamAuth::setStatus(Status status, const QString &error)
{
    m_error = error;
    if (m_status == status)
        return;
    m_status = status;
    emit statusChanged();
}

void SteamAuth::startSignIn()
{
    cancel();
    const int generation = m_generation;
    QJsonObject input;
    input.insert(QLatin1String("device_friendly_name"), QStringLiteral("Lattis"));
    input.insert(QLatin1String("platform_type"), kPlatformMobileApp);
    post(QStringLiteral("BeginAuthSessionViaQR"), input, [this, generation](const Answer &answer) {
        if (generation != m_generation)
            return;
        const QString url = answer.response.value(QLatin1String("challenge_url")).toString();
        if (!answer.networkError.isEmpty() || answer.eresult != kOk || url.isEmpty()) {
            setStatus(Status::SignedOut, answer.networkError.isEmpty()
                          ? QStringLiteral("Steam refused the sign-in (%1)").arg(answer.eresult)
                          : answer.networkError);
            return;
        }
        m_challengeUrl = url;
        m_clientId = answer.response.value(QLatin1String("client_id")).toString();
        m_requestId = answer.response.value(QLatin1String("request_id")).toString();
        m_pollIntervalMs = std::max(kMinPollMs, int(answer.response.value(QLatin1String("interval")).toDouble(5) * 1000));
        m_pollFailures = 0;
        emit challengeUrlChanged();
        setStatus(Status::Waiting);
        schedulePoll();
    });
}

void SteamAuth::schedulePoll()
{
    m_pollTimer.start(m_pollIntervalMs);
}

void SteamAuth::poll()
{
    const int generation = m_generation;
    QJsonObject input;
    input.insert(QLatin1String("client_id"), m_clientId);
    input.insert(QLatin1String("request_id"), m_requestId);
    post(QStringLiteral("PollAuthSessionStatus"), input, [this, generation](const Answer &answer) {
        if (generation != m_generation)
            return;
        if (!answer.networkError.isEmpty() || kTransient.contains(answer.eresult)) {
            if (++m_pollFailures >= kMaxPollFailures) {
                m_challengeUrl.clear();
                emit challengeUrlChanged();
                setStatus(Status::SignedOut, answer.networkError.isEmpty()
                              ? QStringLiteral("Steam did not answer") : answer.networkError);
                return;
            }
            schedulePoll();
            return;
        }
        m_pollFailures = 0;
        if (answer.eresult == kExpired || answer.eresult == kDuplicateRequest) {
            m_challengeUrl.clear();
            emit challengeUrlChanged();
            setStatus(Status::CodeExpired);
            return;
        }
        if (answer.eresult != kOk) {
            m_challengeUrl.clear();
            emit challengeUrlChanged();
            setStatus(Status::SignedOut, QStringLiteral("Steam refused the sign-in (%1)").arg(answer.eresult));
            return;
        }
        if (!answer.response.value(QLatin1String("refresh_token")).toString().isEmpty()) {
            finishSignIn(answer.response);
            return;
        }
        // The code rotates about every 20 s; the new one replaces it and is polled under its own client id.
        const QString rotated = answer.response.value(QLatin1String("new_challenge_url")).toString();
        if (!rotated.isEmpty()) {
            m_challengeUrl = rotated;
            m_clientId = answer.response.value(QLatin1String("new_client_id")).toString();
            emit challengeUrlChanged();
        }
        schedulePoll();
    });
}

void SteamAuth::finishSignIn(const QJsonObject &response)
{
    const QString refresh = response.value(QLatin1String("refresh_token")).toString();
    const TokenClaims claims = parseToken(refresh.toUtf8());
    if (!claims.valid) {
        setStatus(Status::SignedOut, QStringLiteral("Steam sent a sign-in Lattis can't read"));
        return;
    }
    m_pollTimer.stop();
    m_challengeUrl.clear();
    emit challengeUrlChanged();
    m_refreshToken = refresh;
    m_steamId = claims.subject;
    m_accountName = response.value(QLatin1String("account_name")).toString();
    const QString access = response.value(QLatin1String("access_token")).toString();
    const TokenClaims accessClaims = parseToken(access.toUtf8());
    m_accessToken = accessClaims.valid ? access : QString();
    m_accessExpires = accessClaims.expires;
    storeItem(refresh);
    setStatus(Status::SignedIn);
}

void SteamAuth::storeItem(const QString &refreshToken)
{
    const auto store = m_store;
    const QString id = m_steamId;
    inBackground<bool>(
        this,
        [store, id, refreshToken] {
            return store->store(QStringLiteral("Lattis Steam sign-in"), itemAttributes(id), refreshToken.toUtf8());
        },
        [this, id](bool kept) {
            if (id == m_steamId && m_status == Status::SignedIn)
                m_keptInKeyring = kept && m_store->persistent();
        });
}

void SteamAuth::deleteItem()
{
    if (m_steamId.isEmpty())
        return;
    const auto store = m_store;
    const QString id = m_steamId;
    inBackground<bool>(
        this, [store, id] { return store->remove(itemAttributes(id)); }, [](bool) {});
}

void SteamAuth::forgetTokens()
{
    m_refreshToken.clear();
    m_accessToken.clear();
    m_accessExpires = {};
    m_keptInKeyring = false;
}

void SteamAuth::cancel()
{
    ++m_generation;
    m_pollTimer.stop();
    if (m_pending)
        m_pending->abort();
    m_waiting.clear();
    m_refreshing = false;
    if (m_status == Status::Waiting || m_status == Status::CodeExpired) {
        m_challengeUrl.clear();
        emit challengeUrlChanged();
        setStatus(Status::SignedOut);
    }
}

void SteamAuth::signOut()
{
    ++m_generation;
    m_pollTimer.stop();
    if (m_pending)
        m_pending->abort();
    m_waiting.clear();
    m_refreshing = false;
    deleteItem();
    forgetTokens();
    m_accountName.clear();
    if (!m_challengeUrl.isEmpty()) {
        m_challengeUrl.clear();
        emit challengeUrlChanged();
    }
    setStatus(Status::SignedOut);
}

void SteamAuth::restore(const QString &steamId)
{
    if (m_status == Status::SignedIn || steamId.isEmpty())
        return;
    const auto store = m_store;
    inBackground<std::optional<QByteArray>>(
        this, [store, steamId] { return store->read(itemAttributes(steamId)); },
        [this, steamId](const std::optional<QByteArray> &secret) {
            if (m_status != Status::SignedOut || !secret) {
                emit restored();
                return;
            }
            const QString token = QString::fromUtf8(*secret);
            const TokenClaims claims = parseToken(*secret);
            m_steamId = steamId;
            if (!claims.valid || claims.subject != steamId || claims.expires <= QDateTime::currentDateTimeUtc()) {
                deleteItem();
                setStatus(Status::TokenRefused);
                emit restored();
                return;
            }
            m_refreshToken = token;
            m_keptInKeyring = m_store->persistent();
            setStatus(Status::SignedIn);
            emit restored();
        });
}

void SteamAuth::accessToken(QObject *context, TokenCallback done)
{
    if (m_status != Status::SignedIn) {
        done({}, QStringLiteral("Not signed in to Steam"));
        return;
    }
    if (!m_accessToken.isEmpty() && m_accessExpires > QDateTime::currentDateTimeUtc().addSecs(kTokenMarginSeconds)) {
        done(m_accessToken, {});
        return;
    }
    m_waiting.append({context, std::move(done)});
    if (m_refreshing)
        return;
    m_refreshing = true;
    const int generation = m_generation;
    QJsonObject input;
    input.insert(QLatin1String("refresh_token"), m_refreshToken);
    input.insert(QLatin1String("steamid"), m_steamId);
    post(QStringLiteral("GenerateAccessTokenForApp"), input, [this, generation](const Answer &answer) {
        if (generation != m_generation)
            return;
        m_refreshing = false;
        QString token;
        QString error;
        const QString fresh = answer.response.value(QLatin1String("access_token")).toString();
        if (!answer.networkError.isEmpty()) {
            error = answer.networkError;
        } else if (answer.eresult == kOk && !fresh.isEmpty()) {
            const TokenClaims claims = parseToken(fresh.toUtf8());
            m_accessToken = fresh;
            m_accessExpires = claims.valid ? claims.expires : QDateTime::currentDateTimeUtc().addSecs(3600);
            token = fresh;
        } else if (kTransient.contains(answer.eresult)) {
            error = QStringLiteral("Steam did not answer (%1)").arg(answer.eresult);
        } else {
            error = QStringLiteral("Your Steam sign-in expired");
            deleteItem();
            forgetTokens();
            setStatus(Status::TokenRefused, error);
        }
        const auto waiting = std::exchange(m_waiting, {});
        for (const auto &[context, callback] : waiting) {
            if (context)
                callback(token, error);
        }
    });
}
