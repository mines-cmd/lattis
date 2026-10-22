#pragma once

#include "appimageinstall.h"
#include "emulators.h"
#include "flatpakinstall.h"
#include "installrecord.h"

#include <QHash>
#include <QSet>
#include <QObject>
#include <QPointer>
#include <QTimer>
#include <QVariantMap>
#include <QtQmlIntegration>

#include <functional>
#include <optional>

class QJSEngine;
class QNetworkAccessManager;
class QNetworkReply;
class QQmlEngine;

// See docs/code/install.md.
class Installer : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(QVariantMap emulators READ emulators NOTIFY emulatorsChanged)

public:
    struct Setup
    {
        QString stateDir;
        QString cacheDir;
        std::function<QList<EmulatorDef>()> emulators;
        std::function<InstallLocations()> locations;
        // Asks the library to look for installed emulators again.
        std::function<void()> redetect;
        // The environment flatpak runs with (QProcess::environment() form); empty: the app's own.
        std::function<QStringList()> environment;
        std::function<bool(const QString &path, QString *error)> trash;
        // The title of a game running in the emulator with this name, or empty.
        std::function<QString(const QString &emulatorName)> runningGame;
    };
    // Called once at startup; nothing is made until QML uses it.
    static void setup(const Setup &setup) { s_setup = setup; }
    static Installer *instance();
    static Installer *create(QQmlEngine *, QJSEngine *) { return instance(); }

    explicit Installer(const Setup &setup, QObject *parent = nullptr);
    ~Installer() override;

    QVariantMap emulators() const { return m_emulators; }

    Q_INVOKABLE void install(const QString &id, const QString &route = QStringLiteral("appimage"));
    Q_INVOKABLE void update(const QString &id);
    Q_INVOKABLE void uninstall(const QString &id);
    // One emulator's, or (no id) every AppImage Lattis installed, quietly.
    Q_INVOKABLE void checkUpdates(const QString &id = {});
    // Runs a failed job again.
    Q_INVOKABLE void retry(const QString &id);
    Q_INVOKABLE void cancel(const QString &id);
    Q_INVOKABLE void refresh();

    void setFixturesDir(const QString &dir) { m_fixturesDir = dir; }
    void setFixtureDelay(int ms) { m_fixtureDelayMs = ms; }
    // 0: the canned download arrives at once.
    void setFixtureTick(int ms) { m_fixtureTickMs = ms; }

signals:
    void emulatorsChanged();

private:
    enum class State { Idle, Checking, Downloading, Extracting, Installing, Removing, Failed };
    enum class Kind { Install, Update, Check, Uninstall };

    struct Job
    {
        quint64 serial = 0;
        Kind kind = Kind::Install;
        QString route;
        State state = State::Idle;
        double progress = -1;
        QString error;
        // A failed update check that nobody asked for is only logged.
        bool quiet = false;
        QPointer<QNetworkReply> reply;
        QPointer<AppImageDownload> download;
        QPointer<FlatpakJob> flatpak;
        QString emulatorName;
        Release release;
        ReleaseAsset asset;
        QString folder;
        QString oldFile;
        QString appId;
    };

    struct Update
    {
        Release release;
        ReleaseAsset asset;
    };

    QNetworkAccessManager *network();
    bool busy(const QString &id) const;
    Job &startJob(const EmulatorDef &emulator, Kind kind);
    std::optional<InstallRecord> installedByApp(const QString &id) const;
    QString gameRunning(const EmulatorDef &emulator) const;
    void begin(const EmulatorDef &emulator);
    void gotRelease(const EmulatorDef &emulator, quint64 serial, const QByteArray &json, const QString &networkError,
                    bool fromCache);
    void checked(const EmulatorDef &emulator);
    void download(const EmulatorDef &emulator);
    void downloadFinished(const QString &id, const AppImageDownload::Result &result);
    void beginFlatpak(const EmulatorDef &emulator, Kind kind, const QString &appId);
    void flatpakFinished(const QString &id, const FlatpakJob::Result &result);
    void removeAppImage(const EmulatorDef &emulator, const InstallRecord &record);
    void saveRecord(const QString &id, const InstallRecord *record);
    QString trashFile(const QString &path);
    void fail(const QString &id, const QString &error);
    void changed();
    void useFixtureAsset(ReleaseAsset *asset) const;
    QString cachedReleasePath(const QString &release) const;
    void rebuild();
    static QString stateName(State state);

    static Setup s_setup;
    Setup m_setup;
    QString m_fixturesDir;
    int m_fixtureDelayMs = 400;
    int m_fixtureTickMs = 60;
    QNetworkAccessManager *m_network = nullptr;
    QString m_flatpakProgram;
    QHash<QString, Job> m_jobs;
    QHash<QString, Update> m_updates;
    // Checked this session and found no newer release.
    QSet<QString> m_upToDate;
    InstallRecords::Records m_records;
    QVariantMap m_emulators;
    QTimer m_changed;
    quint64 m_serial = 0;
};
