#pragma once

#include "savesfinder.h"

#include <QDateTime>
#include <QElapsedTimer>
#include <QFuture>
#include <QHash>
#include <QList>
#include <QObject>
#include <QSet>
#include <QString>
#include <QStringList>

#include <functional>
#include <optional>

// See docs/code/saves.md.
namespace SavesBackup {
constexpr int kFormat = 1;
inline const QString kKind = QStringLiteral("game-saves");

using Trash = std::function<bool(const QString &path, QString *error)>;

// A game's own auto_backup (-1 = follow its platform, 0 = off, 1 = on) over its platform's.
inline bool autoBackupResolved(int own, bool platformOn)
{
    return own >= 0 ? own == 1 : platformOn;
}

struct Options
{
    QString folder; // absolute
    int keep = 5;
    qint64 maxBytes = qint64(1024) << 20;
};

struct Input
{
    QString gameId; // the group's main copy
    QString title;
    QList<SavesFinder::Query> copies;
    SavesFinder::Env env;
    QString manifestDir;
    // A launcher Proton game's compatdata folder: backed up whole (as the saves backup does) when
    // nothing else is found.
    QString protonPrefix;
    Options options;
    QString appVersion;
    QDateTime now;
    Trash trash; // empty = the system trash
};

enum class Status { Nothing, Unchanged, TooBig, Written, Failed };

struct Result
{
    Status status = Status::Nothing;
    QString gameId;
    QString title;
    QString archive;
    QString error;
    qint64 bytes = 0;
    int files = 0;
};

Result run(const Input &input);

// Newest first.
QStringList backups(const QString &folder, const QString &gameId);

QString archiveName(const QDateTime &when);

// A name the user gave an archive, kept beside it as `<archive>.name`; empty = none.
QString backupName(const QString &archive);
// Empty `name` clears it; returns an error or nothing.
QString setBackupName(const QString &archive, const QString &name);

class Service : public QObject
{
    Q_OBJECT

public:
    // Called on the main thread when a session ends: nothing = automatic backup is off for it.
    using Gather = std::function<std::optional<Input>(const QString &gameId)>;
    // Whether the game or one of its mod managers still runs.
    using Busy = std::function<bool(const QString &gameId)>;

    Service(Gather gather, Busy busy, std::function<int()> minSessionSeconds, QObject *parent = nullptr);
    ~Service() override;

    void started(const QString &gameId);
    void ended(const QString &gameId);
    bool running() const { return m_running > 0; }

signals:
    void message(const QString &text);
    void finished(const SavesBackup::Result &result);

private:
    void flush();
    void start(const QString &gameId);

    Gather m_gather;
    Busy m_busy;
    std::function<int()> m_minSeconds;
    QHash<QString, QElapsedTimer> m_started;
    QSet<QString> m_pending;
    int m_running = 0;
    QList<QFuture<Result>> m_futures;
};
}

Q_DECLARE_METATYPE(SavesBackup::Result)
