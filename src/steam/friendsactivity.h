#pragma once

#include <QObject>
#include <QString>
#include <QVariantMap>
#include <QtQmlIntegration>

#include <functional>

class QJSEngine;
class QQmlEngine;

// See docs/code/steam.md.
class FriendsActivity : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

public:
    // Opens a steam:// URL in the running client; returns an error text, or empty.
    using OpenUrl = std::function<QString(const QString &url)>;

    static void setup(OpenUrl open) { pendingOpen() = std::move(open); }
    static FriendsActivity *instance();
    static FriendsActivity *create(QQmlEngine *, QJSEngine *) { return instance(); }

    // {playing, recent, previous}, each a list of {steamId, name, avatar (a file URL, or empty), recentMinutes,
    // totalMinutes, gameName}. Reads the cache: nothing is sent.
    Q_INVOKABLE QVariantMap forGame(qint64 appId) const;
    // The friends who played the game, recent and previous; 0 when friends activity is off or no key is set.
    Q_INVOKABLE int playedCount(qint64 appId) const;
    // Asks who is playing now; the call is dropped within a minute of the last one.
    Q_INVOKABLE void refreshPlaying();
    Q_INVOKABLE QString openProfile(const QString &steamId);
    Q_INVOKABLE QString openFriendsWhoPlay(qint64 appId);

signals:
    void changed();

private:
    explicit FriendsActivity(QObject *parent);

    static OpenUrl &pendingOpen();
};
