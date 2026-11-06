#include "friendsactivity.h"
#include "steamfriends.h"

#include <QCoreApplication>
#include <QUrl>

namespace {
QVariantList peopleOf(const QList<SteamFriends::Person> &people)
{
    QVariantList out;
    for (const SteamFriends::Person &p : people) {
        out.append(QVariantMap{
            {QStringLiteral("steamId"), p.steamId},
            {QStringLiteral("name"), p.name},
            {QStringLiteral("avatar"), p.avatarPath.isEmpty() ? QString() : QUrl::fromLocalFile(p.avatarPath).toString()},
            {QStringLiteral("recentMinutes"), p.recentMinutes},
            {QStringLiteral("totalMinutes"), p.totalMinutes},
            {QStringLiteral("gameName"), p.gameName},
            {QStringLiteral("online"), p.online},
        });
    }
    return out;
}
}

FriendsActivity::OpenUrl &FriendsActivity::pendingOpen()
{
    static OpenUrl open;
    return open;
}

FriendsActivity *FriendsActivity::instance()
{
    static FriendsActivity *activity = new FriendsActivity(QCoreApplication::instance());
    return activity;
}

FriendsActivity::FriendsActivity(QObject *parent) : QObject(parent)
{
    connect(SteamFriends::instance(), &SteamFriends::changed, this, &FriendsActivity::changed);
}

QVariantMap FriendsActivity::forGame(qint64 appId) const
{
    const SteamFriends::GameFriends friends = SteamFriends::instance()->friendsOf(appId);
    return {
        {QStringLiteral("playing"), peopleOf(friends.playing)},
        {QStringLiteral("recent"), peopleOf(friends.recent)},
        {QStringLiteral("previous"), peopleOf(friends.previous)},
    };
}

int FriendsActivity::playedCount(qint64 appId) const
{
    const SteamFriends *friends = SteamFriends::instance();
    if (!friends->enabled())
        return 0;
    const SteamFriends::GameFriends played = friends->friendsOf(appId);
    return int(played.recent.size() + played.previous.size());
}

void FriendsActivity::refreshPlaying()
{
    SteamFriends::instance()->refreshPlaying();
}

// Neither URL has been tried in the real client.
QString FriendsActivity::openChat(const QString &steamId)
{
    const bool digits = !steamId.isEmpty() && std::all_of(steamId.cbegin(), steamId.cend(), [](QChar c) { return c.isDigit(); });
    if (!digits || !pendingOpen())
        return {};
    return pendingOpen()(QStringLiteral("steam://friends/message/") + steamId);
}

QString FriendsActivity::openFriendsWhoPlay(qint64 appId)
{
    if (appId <= 0 || !pendingOpen())
        return {};
    return pendingOpen()(QStringLiteral("steam://openurl/https://steamcommunity.com/my/friendsthatplay/%1").arg(appId));
}
