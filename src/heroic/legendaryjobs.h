#pragma once

#include <QDateTime>
#include <QHash>
#include <QList>
#include <QObject>
#include <QProcess>
#include <QString>
#include <QStringList>
#include <QTimer>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>

#include <functional>
#include <optional>

// See docs/code/heroic.md.
namespace LegendaryOutput {
// The percentage of a `= Progress: 47.06% (272/578), …` line; none for any other line.
std::optional<double> progress(const QString &line);

// The line to show when a run failed: the last error line, else the last line that isn't a progress detail.
QString failureLine(const QStringList &lines);
}

class LegendaryJobs : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Reached through GameRunner.legendary")

    // The jobs the user sees (not the check or an uninstall), in queue order: appName, kind, percent, state, error.
    Q_PROPERTY(QVariantList jobs READ jobs NOTIFY jobsChanged)

public:
    enum class Kind { Update, Install, Repair, Uninstall, Check };
    Q_ENUM(Kind)
    enum class State { Queued, Running, Failed };
    Q_ENUM(State)

    struct Tool
    {
        QString program;
        QString configPath; // LEGENDARY_CONFIG_PATH
    };

    struct Result
    {
        Kind kind = Kind::Update;
        QString appName;
        bool ok = false;
        bool cancelled = false;
        bool started = true;
        QString error; // the last Legendary line, or why it didn't start
    };

    explicit LegendaryJobs(std::function<Tool()> tool, QObject *parent = nullptr);
    ~LegendaryJobs() override;

    QVariantList jobs() const;
    Q_INVOKABLE QVariantMap job(const QString &appName) const;
    bool busy() const { return !m_queue.isEmpty(); }

    // Queues `legendary <verb> <appName> <extraArgs> -y`. Empty on success, else why not (no Legendary, the
    // game already has a job). A failed job of the game is replaced.
    QString enqueue(Kind kind, const QString &appName, const QStringList &extraArgs = {});

    // Cancels a queued or a running job of the game: the process gets SIGINT, as Ctrl-C does, which keeps the
    // resume file. SIGTERM would leave its workers downloading.
    Q_INVOKABLE void cancel(const QString &appName);
    Q_INVOKABLE void dismiss(const QString &appName);

    // The daily `list-installed --check-updates`: hidden, and only when nothing else is queued or running.
    // False when it didn't start.
    bool checkUpdates();

    // From the first frame on: checks once a day while `enabled()` says so, the time of the last good check
    // kept in `checkFile`.
    void startDailyCheck(const QString &checkFile, std::function<bool()> enabled);

    static std::optional<QDateTime> readCheckTime(const QString &file);
    static bool checkDue(const std::optional<QDateTime> &last, const QDateTime &now = QDateTime::currentDateTimeUtc());

    static QStringList arguments(Kind kind, const QString &appName, const QStringList &extraArgs);

    void setCancelGraceMs(int ms) { m_cancelGraceMs = ms; }
    void setCheckIntervals(int firstMs, int busyMs, int recheckMs);

signals:
    void jobsChanged();
    void finished(const LegendaryJobs::Result &result);

private:
    struct Job
    {
        Kind kind = Kind::Update;
        QString appName;
        QStringList args;
        State state = State::Queued;
        double percent = 0;
        QString error;
    };

    bool hidden(const Job &job) const { return job.kind == Kind::Uninstall || job.kind == Kind::Check; }
    void startNext();
    void onProcessOutput();
    void onFinished(int exitCode, QProcess::ExitStatus status);
    void finishJob(const Result &result);
    void runDailyCheck();
    void scheduleCheck(int ms);

    std::function<Tool()> m_tool;
    QList<Job> m_queue; // the running job first
    QList<Job> m_failed;
    QProcess *m_process = nullptr;
    QByteArray m_pending;
    QStringList m_tail;
    bool m_cancelling = false;
    QTimer m_cancelTimer;
    int m_cancelGraceMs = 10'000;

    QString m_checkFile;
    std::function<bool()> m_checkEnabled;
    QTimer m_checkTimer;
    int m_firstCheckMs = 10'000;
    int m_busyMs = 60'000;
    int m_recheckMs = 60 * 60 * 1000;
};

Q_DECLARE_METATYPE(LegendaryJobs::Result)
