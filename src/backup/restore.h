#pragma once

#include "backupplan.h"

#include <QDate>
#include <QDateTime>
#include <QFutureWatcher>
#include <QList>
#include <QObject>
#include <QString>
#include <QStringList>

#include <atomic>
#include <functional>
#include <optional>

// See docs/code/backup.md.
namespace Restore {
struct Manifest
{
    struct Emulator
    {
        QString id;
        QString name;
        QString install;
        QStringList saves;
        QStringList keys;
        QStringList besideRoms;
        qint64 bytes = -1;
    };
    struct ProtonGame
    {
        QString id;
        QString title;
        quint32 shortcut = 0;
        QString proton;
        qint64 bytes = -1;
    };

    QString kind;
    int format = 0;
    QString appVersion;
    QString libraryId;
    QDateTime created;
    int libraryVersion = 0;
    int games = -1;
    qint64 bytes = -1;
    bool credentials = false;
    bool keys = false;
    bool proton = false;
    QList<Emulator> emulators;
    QList<ProtonGame> protonGames;
};

struct Opened
{
    std::optional<Manifest> manifest;
    QString error;
    QString warning;
};

Opened open(const QString &archive);
Opened parseManifest(const QByteArray &text, const QString &display);

struct Target
{
    QString source;
    QString label;
    QString gameId;
    QString path;
    bool exists = false;
    QString problem;
};

struct Part
{
    enum class Kind { Library, Saves, Keys, BesideRoms, Proton };

    Kind kind = Kind::Library;
    QString id;
    QString name;
    QString install;
    QString backedUp;
    qint64 bytes = -1;
    QString problem;
    QList<Target> targets;
    bool checked = false;

    bool restorable() const;
};

struct Plan
{
    QString archive;
    Manifest manifest;
    QString warning;
    bool sameLibrary = false;
    QString dataDir;
    int games = 0;
    // Paths relative to the data dir that aren't in the backup but are the user's now, and go
    // into the restored data dir unless it has them: the ROMs kept there (moved), and
    // credentials.toml when the backup has none (copied).
    QStringList carriedOver;
    QList<Part> parts;
    QString compatdata;

    qint64 bytes() const;
};

Plan plan(const QString &archive, const Manifest &manifest, const BackupPlan::Sources &now);

struct Result
{
    QString error;
    QStringList movedAside;
    int restored = 0;
};

Result apply(const Plan &plan, const QDate &date, const Archive::Control &control = {});
}

class RestoreJob : public QObject
{
    Q_OBJECT

    Q_PROPERTY(bool running READ running NOTIFY runningChanged)
    Q_PROPERTY(qint64 bytes READ bytes NOTIFY progressChanged)
    Q_PROPERTY(qint64 total READ total NOTIFY runningChanged)

public:
    explicit RestoreJob(std::function<QStringList()> runningGames, QObject *parent = nullptr);
    ~RestoreJob() override;

    bool running() const { return m_watcher.isRunning(); }
    qint64 bytes() const { return m_bytes; }
    qint64 total() const { return m_total; }

    QString start(const Restore::Plan &plan);
    void cancel();

    Restore::Result result() const { return m_result; }

signals:
    void runningChanged();
    void progressChanged();
    void finished(const QString &error);

private:
    void onFinished();

    std::function<QStringList()> m_runningGames;
    QFutureWatcher<Restore::Result> m_watcher;
    std::atomic_bool m_cancel = false;
    qint64 m_bytes = 0;
    qint64 m_total = 0;
    Restore::Result m_result;
};
