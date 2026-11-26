#pragma once

#include <QDateTime>
#include <QHash>
#include <QList>
#include <QObject>
#include <QProcess>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QTimer>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>

#include <functional>
#include <optional>

#include <sys/types.h>

// See docs/code/heroic.md.
namespace LegendaryOutput {
// The percentage of a `= Progress: 47.06% (272/578), …` line or a `Verification progress: 12/219 (5.5%)` one;
// none for any other line.
std::optional<double> progress(const QString &line);

// The line to show when a run failed: the last error line, else the last line that isn't a progress detail.
QString failureLine(const QStringList &lines);

// How many files a `repair` run found broken or missing (and so downloaded again), from its
// `Verification failed, N file(s) corrupted, M file(s) are missing.` line; 0 when every file was fine.
int repairedFiles(const QStringList &lines);
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
        int repaired = 0; // a repair's broken and missing files
        QString error; // the last Legendary line, or why it didn't start
    };

    explicit LegendaryJobs(std::function<Tool()> tool, QObject *parent = nullptr);
    ~LegendaryJobs() override;

    QVariantList jobs() const;
    Q_INVOKABLE QVariantMap job(const QString &appName) const;
    bool busy() const { return !m_queue.isEmpty(); }
    // The games whose uninstall is running or queued: a hidden job, but one that quitting would leave half done.
    QStringList uninstalling() const;

    // Queues `legendary <verb> <appName> <extraArgs> -y`. Empty on success, else why not (no Legendary, the
    // game already has a job). A failed job of the game is replaced.
    QString enqueue(Kind kind, const QString &appName, const QStringList &extraArgs = {});

    // Cancels a queued or a running job of the game: its process group gets SIGINT, as Ctrl-C does, which keeps the
    // resume file, and SIGKILL after the grace. The job ends (`finished`) only when no process of the group is left.
    Q_INVOKABLE void cancel(const QString &appName);
    Q_INVOKABLE void dismiss(const QString &appName);

    // For quitting: drops the queue and cancels the running job, without waiting. True when a job is running, and
    // `allStopped` comes once its group is gone (after the grace at most); false when there is nothing to wait for.
    Q_INVOKABLE bool stopAll();

    // `legendary info <appName> --json` beside the queue: it only asks Epic, so it needn't wait for a download.
    // The answer comes as `infoReady` (stdout, or why not); a second ask for a game still being asked is ignored.
    void queryInfo(const QString &appName);

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
    void setInfoTimeoutMs(int ms) { m_infoTimeoutMs = ms; }
    void setCheckIntervals(int firstMs, int busyMs, int recheckMs);

signals:
    void jobsChanged();
    void allStopped();
    void finished(const LegendaryJobs::Result &result);
    void infoReady(const QString &appName, const QByteArray &json, const QString &error);

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
    bool groupAlive() const;
    void signalGroup(int signal);
    void pollGroup();
    void completeJob();
    void runDailyCheck();
    void scheduleCheck(int ms);

    std::function<Tool()> m_tool;
    QList<Job> m_queue; // the running job first
    QList<Job> m_failed;
    QSet<QString> m_infoRunning;
    QProcess *m_process = nullptr;
    QByteArray m_pending;
    QStringList m_tail;
    bool m_cancelling = false;
    bool m_stopping = false;
    pid_t m_group = 0; // the running job's process group (its process id: it leads the group)
    std::optional<Result> m_waiting; // the job's result, held until its group is gone
    int m_groupWaitedMs = 0;
    QTimer m_groupTimer;
    QTimer m_cancelTimer;
    int m_cancelGraceMs = 10'000;
    int m_infoTimeoutMs = 60'000;

    QString m_checkFile;
    std::function<bool()> m_checkEnabled;
    QTimer m_checkTimer;
    int m_firstCheckMs = 10'000;
    int m_busyMs = 60'000;
    int m_recheckMs = 60 * 60 * 1000;
};

Q_DECLARE_METATYPE(LegendaryJobs::Result)
