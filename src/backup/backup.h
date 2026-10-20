#pragma once

#include "backupplan.h"

#include <QDateTime>
#include <QFutureWatcher>
#include <QHash>
#include <QObject>
#include <QString>

#include <atomic>
#include <functional>

// See docs/code/backup.md.
namespace Backup {
inline const QString kLibrary = QStringLiteral("library");
inline const QString kSaves = QStringLiteral("saves");

struct Control
{
    std::function<void(qint64 bytes, const QString &part)> progress;
    const std::atomic_bool *cancel = nullptr;
};

QString write(BackupPlan::LibraryPlan plan, const QString &path, const QDateTime &created,
              const QString &appVersion, const Control &control = {});
QString write(BackupPlan::SavesPlan plan, const QString &path, const QDateTime &created,
              const QString &appVersion, const Control &control = {});

QString gamesRunning(const QStringList &titles);

QString placeName(const QString &emulatorId, const QString &table, int n);

struct Last
{
    QString path;
    QDateTime time;

    bool operator==(const Last &) const = default;
};

QString lastFile(const QString &stateDir, const QString &dataDirKey);
QHash<QString, Last> readLast(const QString &file);
bool writeLast(const QString &file, const QString &kind, const Last &last, QString *error = nullptr);
}

class BackupJob : public QObject
{
    Q_OBJECT

    Q_PROPERTY(bool running READ running NOTIFY runningChanged)
    Q_PROPERTY(QString kind READ kind NOTIFY runningChanged)
    Q_PROPERTY(qint64 bytes READ bytes NOTIFY progressChanged)
    Q_PROPERTY(qint64 total READ total NOTIFY runningChanged)
    Q_PROPERTY(QString part READ part NOTIFY progressChanged)

public:
    BackupJob(const QString &lastFile, std::function<QStringList()> runningGames, QObject *parent = nullptr);
    ~BackupJob() override;

    bool running() const { return m_watcher.isRunning(); }
    QString kind() const { return m_kind; }
    qint64 bytes() const { return m_bytes; }
    qint64 total() const { return m_total; }
    QString part() const { return m_part; }

    QString start(const BackupPlan::LibraryPlan &plan, const QString &path);
    QString start(const BackupPlan::SavesPlan &plan, const QString &path);
    void cancel();

    QString gamesRunning() const;

    Backup::Last last(const QString &kind) const;

    void setAppVersion(const QString &version) { m_appVersion = version; }

signals:
    void runningChanged();
    void progressChanged();
    void finished(const QString &kind, const QString &path, const QString &error);

private:
    QString begin(const QString &kind, const QString &path, qint64 total,
                  const std::function<QString(const Backup::Control &)> &run);
    void onFinished();

    QString m_lastFile;
    std::function<QStringList()> m_runningGames;
    QString m_appVersion;
    QFutureWatcher<QString> m_watcher;
    std::atomic_bool m_cancel = false;
    QString m_kind;
    QString m_path;
    QDateTime m_created;
    qint64 m_bytes = 0;
    qint64 m_total = 0;
    QString m_part;
};
