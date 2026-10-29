#pragma once

#include "secretstore.h"

#include <QDateTime>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QObject>
#include <QPointer>
#include <QTimer>
#include <QUrl>

#include <functional>
#include <memory>

class QNetworkReply;

// See docs/code/steam.md.
class SteamAuth : public QObject
{
    Q_OBJECT

public:
    enum class Status { SignedOut, Waiting, CodeExpired, SignedIn, TokenRefused };
    Q_ENUM(Status)

    struct TokenClaims
    {
        QString subject;
        QDateTime expires;
        bool valid = false;
    };

    using TokenCallback = std::function<void(const QString &accessToken, const QString &error)>;

    explicit SteamAuth(std::shared_ptr<SecretStore> store,
                       QUrl apiBase = QUrl(QStringLiteral("https://api.steampowered.com")),
                       QObject *parent = nullptr);
    ~SteamAuth() override;

    Status status() const { return m_status; }
    QString challengeUrl() const { return m_challengeUrl; }
    QString accountName() const { return m_accountName; }
    QString steamId() const { return m_steamId; }
    QString error() const { return m_error; }
    // Whether the refresh token is in the keyring, not only in memory.
    bool keptInKeyring() const { return m_keptInKeyring; }
    // A Secret Service answers; without one the sign-in lasts until Lattis quits.
    bool keyringAvailable() const { return m_store->persistent(); }

    static TokenClaims parseToken(const QByteArray &jwt);
    static SecretStore::Attributes itemAttributes(const QString &steamId);

    void startSignIn();
    void cancel();
    void signOut();
    // Reads the refresh token kept for this account, if there is one; off the main thread.
    void restore(const QString &steamId);
    // Calls back with a token valid for at least a minute; a refused refresh token ends the sign-in.
    void accessToken(QObject *context, TokenCallback done);

signals:
    void statusChanged();
    void challengeUrlChanged();
    void restored();

private:
    struct Answer
    {
        int eresult = 0;
        QJsonObject response;
        QString networkError;
    };
    using AnswerCallback = std::function<void(const Answer &)>;

    void post(const QString &method, const QJsonObject &input, AnswerCallback done);
    void poll();
    void schedulePoll();
    void finishSignIn(const QJsonObject &response);
    void setStatus(Status status, const QString &error = {});
    void forgetTokens();
    void deleteItem();
    void storeItem(const QString &refreshToken);

    std::shared_ptr<SecretStore> m_store;
    QUrl m_base;
    QNetworkAccessManager m_network;
    QTimer m_pollTimer;
    QPointer<QNetworkReply> m_pending;
    int m_generation = 0;
    Status m_status = Status::SignedOut;
    QString m_challengeUrl;
    QString m_clientId;
    QString m_requestId;
    QString m_accountName;
    QString m_steamId;
    QString m_error;
    QString m_refreshToken;
    QString m_accessToken;
    QDateTime m_accessExpires;
    int m_pollIntervalMs = 5000;
    int m_pollFailures = 0;
    bool m_keptInKeyring = false;
    bool m_refreshing = false;
    QList<std::pair<QPointer<QObject>, TokenCallback>> m_waiting;
};
