#pragma once

#include <QObject>
#include <QTimer>
#include <QtQmlIntegration>

class QJSEngine;
class QQmlEngine;
class SteamLibrarySync;

// See docs/code/steam.md.
class SteamLibrary : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    // "signed_out", "waiting" (the QR code is up), "expired" (the code ran out), "signed_in" or "refused" (Steam
    // turned the sign-in down).
    Q_PROPERTY(QString authState READ authState NOTIFY changed)
    Q_PROPERTY(QString accountName READ accountName NOTIFY changed)
    Q_PROPERTY(QString qrPath READ qrPath NOTIFY changed)
    Q_PROPERTY(int qrSize READ qrSize NOTIFY changed)
    Q_PROPERTY(bool syncing READ syncing NOTIFY changed)
    Q_PROPERTY(bool hasKey READ hasKey NOTIFY changed)
    Q_PROPERTY(QString webKey READ webKey NOTIFY changed)
    // Signed in or a key set: something to fetch with.
    Q_PROPERTY(bool configured READ configured NOTIFY changed)
    Q_PROPERTY(bool enabled READ enabled NOTIFY changed)
    // Without a Secret Service the sign-in lasts until Lattis quits.
    Q_PROPERTY(bool keyringAvailable READ keyringAvailable NOTIFY changed)
    Q_PROPERTY(int gameCount READ gameCount NOTIFY changed)
    // "1 204 games, synced 2 h ago", empty before the first fetch.
    Q_PROPERTY(QString statusLine READ statusLine NOTIFY changed)
    // "", "expired", "again" (signed in before, the keyring's gone), "private", "empty" (no account), "failed".
    Q_PROPERTY(QString messageKind READ messageKind NOTIFY changed)
    Q_PROPERTY(QString message READ message NOTIFY changed)

public:
    static SteamLibrary *instance();
    static SteamLibrary *create(QQmlEngine *, QJSEngine *) { return instance(); }

    explicit SteamLibrary(QObject *parent = nullptr);

    QString authState() const;
    QString accountName() const;
    QString qrPath() const { return m_qrPath; }
    int qrSize() const { return m_qrSize; }
    bool syncing() const;
    bool hasKey() const { return !m_webKey.isEmpty(); }
    QString webKey() const { return m_webKey; }
    bool configured() const;
    bool enabled() const;
    bool keyringAvailable() const;
    int gameCount() const;
    QString statusLine() const;
    QString messageKind() const;
    QString message() const;

    Q_INVOKABLE void signIn();
    Q_INVOKABLE void cancelSignIn();
    Q_INVOKABLE void signOut();
    Q_INVOKABLE void syncNow();
    // Saves (or, empty, removes) the Web API key; the error, or empty.
    Q_INVOKABLE QString setWebKey(const QString &key);
    // Steam's page of authorized devices, where the session is revoked.
    Q_INVOKABLE QString devicesUrl() const;
    Q_INVOKABLE QString privacyUrl() const;
    Q_INVOKABLE QString keyUrl() const;

signals:
    void changed();
    // The QR sign-in went through.
    void signedIn();

private:
    void updateCode();
    QString fixtureState() const;

    SteamLibrarySync *m_sync = nullptr;
    QTimer m_minute;
    QString m_qrPath;
    int m_qrSize = 0;
    QString m_webKey;
    bool m_fixtureWaiting = false;
    bool m_wasWaiting = false;
};
