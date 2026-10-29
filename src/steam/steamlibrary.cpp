#include "steamlibrary.h"
#include "credentials.h"
#include "qrcode.h"
#include "steamlibrarysync.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocale>

namespace {
QJsonObject fixtureSignIn(const QString &folder)
{
    QFile file(QDir(folder).filePath(QStringLiteral("signin.json")));
    return !folder.isEmpty() && file.open(QIODevice::ReadOnly) ? QJsonDocument::fromJson(file.readAll()).object()
                                                               : QJsonObject();
}
}

SteamLibrary *SteamLibrary::instance()
{
    static SteamLibrary *library = new SteamLibrary(QCoreApplication::instance());
    return library;
}

SteamLibrary::SteamLibrary(QObject *parent) : QObject(parent), m_sync(SteamLibrarySync::instance())
{
    m_webKey = CredentialsFile::load(m_sync->dataDir()).value.steamWebKey;
    connect(m_sync, &SteamLibrarySync::stateChanged, this, &SteamLibrary::changed);
    m_minute.setInterval(60000);
    connect(&m_minute, &QTimer::timeout, this, &SteamLibrary::changed);
    m_minute.start();
}

QString SteamLibrary::fixtureState() const
{
    return fixtureSignIn(m_sync->fixturesDir()).value(QLatin1String("state")).toString();
}

QString SteamLibrary::authState() const
{
    const QString fixture = fixtureState();
    if (fixture == u"expired")
        return QStringLiteral("refused");
    if (m_fixtureWaiting)
        return QStringLiteral("waiting");
    if (m_sync->signedIn())
        return QStringLiteral("signed_in");
    if (!m_sync->authStarted())
        return QStringLiteral("signed_out");
    switch (m_sync->auth()->status()) {
    case SteamAuth::Status::Waiting: return QStringLiteral("waiting");
    case SteamAuth::Status::CodeExpired: return QStringLiteral("expired");
    case SteamAuth::Status::TokenRefused: return QStringLiteral("refused");
    case SteamAuth::Status::SignedIn: return QStringLiteral("signed_in");
    case SteamAuth::Status::SignedOut: break;
    }
    return QStringLiteral("signed_out");
}

QString SteamLibrary::accountName() const
{
    return m_sync->accountName();
}

bool SteamLibrary::syncing() const
{
    return m_sync->state() == SteamLibrarySync::State::Syncing;
}

bool SteamLibrary::configured() const
{
    return m_sync->signedIn() || hasKey();
}

bool SteamLibrary::enabled() const
{
    return m_sync->enabled();
}

bool SteamLibrary::keyringAvailable() const
{
    if (!m_sync->fixturesDir().isEmpty())
        return fixtureSignIn(m_sync->fixturesDir()).value(QLatin1String("keyring")).toBool(true);
    return m_sync->auth()->keyringAvailable();
}

int SteamLibrary::gameCount() const
{
    return m_sync->gameCount();
}

QString SteamLibrary::statusLine() const
{
    const QDateTime fetched = m_sync->lastFetch();
    if (!fetched.isValid() || m_sync->gameCount() == 0)
        return {};
    const qint64 seconds = qMax<qint64>(0, fetched.secsTo(QDateTime::currentDateTimeUtc()));
    QString ago;
    if (seconds < 60)
        ago = tr("just now");
    else if (seconds < 3600)
        ago = tr("%n min ago", nullptr, int(seconds / 60));
    else if (seconds < 86400)
        ago = tr("%n h ago", nullptr, int(seconds / 3600));
    else
        ago = tr("%n day(s) ago", nullptr, int(seconds / 86400));
    return tr("%n game(s) · synced %1", nullptr, m_sync->gameCount()).arg(ago);
}

QString SteamLibrary::messageKind() const
{
    const QString fixture = fixtureState();
    if (fixture == u"expired" || fixture == u"again" || fixture == u"private")
        return fixture;
    const QString state = authState();
    if (state == u"refused")
        return QStringLiteral("expired");
    if (syncing() || state == u"waiting")
        return {};
    switch (m_sync->outcome()) {
    case SteamLibrarySync::Outcome::SignInExpired: return QStringLiteral("expired");
    case SteamLibrarySync::Outcome::NoGames: return hasKey() && state != u"signed_in" ? QStringLiteral("private") : QString();
    case SteamLibrarySync::Outcome::Failed: return QStringLiteral("failed");
    case SteamLibrarySync::Outcome::NoAccount: return configured() ? QStringLiteral("empty") : QString();
    default: break;
    }
    if (state != u"signed_in" && m_sync->signedInBefore() && m_sync->restoreTried())
        return QStringLiteral("again");
    return {};
}

QString SteamLibrary::message() const
{
    const QString kind = messageKind();
    if (kind == u"expired")
        return tr("Your Steam sign-in expired.");
    if (kind == u"again")
        return tr("Sign in again to update your whole library. The games already fetched stay in the library.");
    if (kind == u"private")
        return tr("Steam returned no games: is Game details public?");
    if (kind == u"empty")
        return tr("Steam hasn't logged in on this computer yet, so Lattis doesn't know which account to fetch.");
    if (kind == u"failed")
        return tr("Couldn't reach Steam: %1").arg(m_sync->error());
    return {};
}

void SteamLibrary::updateCode()
{
    QString url;
    if (m_fixtureWaiting)
        url = QStringLiteral("https://s.team/q/1/1234567890123456789");
    else if (m_sync->authStarted())
        url = m_sync->auth()->challengeUrl();
    const QrCode::Symbol symbol = url.isEmpty() ? QrCode::Symbol() : QrCode::encode(url.toUtf8());
    m_qrPath = QrCode::toSvgPath(symbol);
    m_qrSize = symbol.size;
}

void SteamLibrary::signIn()
{
    if (!m_sync->fixturesDir().isEmpty()) {
        m_fixtureWaiting = true;
        updateCode();
        emit changed();
        return;
    }
    SteamAuth *auth = m_sync->auth();
    static bool connected = false;
    if (!connected) {
        connected = true;
        connect(auth, &SteamAuth::challengeUrlChanged, this, [this] {
            updateCode();
            emit changed();
        });
        connect(auth, &SteamAuth::statusChanged, this, [this, auth] {
            const bool waiting = auth->status() == SteamAuth::Status::Waiting;
            if (m_wasWaiting && auth->status() == SteamAuth::Status::SignedIn) {
                emit signedIn();
                if (!m_sync->isHeld())
                    m_sync->syncNow();
            }
            m_wasWaiting = waiting;
        });
    }
    auth->startSignIn();
    updateCode();
    emit changed();
}

void SteamLibrary::cancelSignIn()
{
    if (m_fixtureWaiting) {
        m_fixtureWaiting = false;
        updateCode();
        emit changed();
        return;
    }
    if (m_sync->authStarted())
        m_sync->auth()->cancel();
}

void SteamLibrary::signOut()
{
    m_sync->auth()->signOut();
}

void SteamLibrary::syncNow()
{
    m_sync->syncNow();
}

QString SteamLibrary::setWebKey(const QString &key)
{
    QString error;
    if (!CredentialsFile::setValue(m_sync->dataDir(), QStringLiteral("steam"), QStringLiteral("web_key"), key.trimmed(),
                                   &error))
        return tr("Couldn't save credentials.toml: %1").arg(error);
    m_webKey = key.trimmed();
    emit changed();
    if (hasKey() && enabled())
        m_sync->syncNow();
    return {};
}

QString SteamLibrary::devicesUrl() const
{
    return QStringLiteral("https://store.steampowered.com/account/authorizeddevices");
}

QString SteamLibrary::privacyUrl() const
{
    return QStringLiteral("https://steamcommunity.com/my/edit/settings");
}

QString SteamLibrary::keyUrl() const
{
    return QStringLiteral("https://steamcommunity.com/dev/apikey");
}
