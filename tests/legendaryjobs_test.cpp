#include "appimageenv.h"
#include "legendaryjobs.h"

#include <QDateTime>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QRegularExpression>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

using namespace Qt::StringLiterals;

namespace {
QStringList captureLines(const QString &name)
{
    QFile file(QStringLiteral(LEGENDARY_DATA_DIR) + u'/' + name);
    if (!file.open(QIODevice::ReadOnly))
        return {};
    return QString::fromUtf8(file.readAll()).split(u'\n');
}

// Verification's progress has carriage returns and no newlines.
QStringList captureLinesCr(const QString &name)
{
    QFile file(QStringLiteral(LEGENDARY_DATA_DIR) + u'/' + name);
    if (!file.open(QIODevice::ReadOnly))
        return {};
    return QString::fromUtf8(file.readAll()).split(QRegularExpression(u"[\r\n]"_s));
}

QList<double> percents(const QStringList &lines)
{
    QList<double> out;
    for (const QString &line : lines) {
        if (const auto p = LegendaryOutput::progress(line))
            out.append(*p);
    }
    return out;
}

// Stands in for Legendary: `update` prints progress and stops on SIGINT as Legendary does (exit 0),
// `Bad` fails with an error line, anything else finishes quickly.
const QByteArray kFake = R"(#!/bin/sh
echo "$* [$LEGENDARY_CONFIG_PATH]" >> "$(dirname "$0")/calls.txt"
if [ "$1" = info ]; then
    echo "[Core] INFO: Trying to re-use existing login session..." >&2
    if [ "$2" = Bad ]; then
        echo "[cli] ERROR: Could not find the game" >&2
        exit 1
    fi
    echo '{"manifest": {"download_size": 1000, "disk_size": 3000}}'
    exit 0
fi
case "$2" in
Bad)
    echo "[cli] INFO: starting" >&2
    echo "[cli] ERROR: The disk is full" >&2
    exit 1 ;;
Fix)
    echo "[cli] INFO: Verifying the game" >&2
    printf 'Verification progress: 1/4 (25.0%%) [0.0 MiB/s]\t\rVerification progress: 2/4 (50.0%%) [0.0 MiB/s]\t\r'
    sleep 0.3
    printf 'Verification progress: 4/4 (100.0%%) [0.0 MiB/s]\t\r\n'
    echo "[cli] ERROR: File is missing: a.dll" >&2
    echo "[cli] ERROR: Verification failed, 1 file(s) corrupted, 2 file(s) are missing." >&2
    exit 0 ;;
Slow)
    trap 'echo "[cli] INFO: Command was aborted via KeyboardInterrupt, cleaning up..." >&2; exit 0' INT
    for p in 0.00 10.50 20.00 30.00 40.00 50.00 60.00 70.00 80.00 90.00; do
        echo "[DLManager] INFO: = Progress: $p% (1/10), Running for 00:00:01, ETA: 00:00:09" >&2
        echo "[DLManager] INFO:  - Downloaded: 1.00 MiB, Written: 1.00 MiB" >&2
        sleep 0.2 &
        wait $!
    done
    echo "[cli] INFO: Finished installation process in 2 seconds." >&2
    exit 0 ;;
Worker)
    # Stops on SIGINT, but leaves a download worker that ignores it behind, as Legendary's workers can.
    trap 'exit 0' INT
    sh -c 'trap "" INT; echo $$ > "$0"; while :; do sleep 1; done' "$(dirname "$0")/worker.pid" &
    wait ;;
Stubborn)
    # Ignores SIGINT itself.
    trap '' INT
    sh -c 'trap "" INT; echo $$ > "$0"; while :; do sleep 1; done' "$(dirname "$0")/worker.pid" &
    while :; do sleep 1; done ;;
esac
if [ "$1" = list-installed ]; then
    echo "Logging in to check for updates..." >&2
    echo "Installed games:"
    exit "${FAKE_CHECK_EXIT:-0}"
fi
echo "[cli] INFO: done" >&2
exit 0
)";
}

class LegendaryJobsTest : public QObject
{
    Q_OBJECT

private slots:
    void init();

    void progressOfARealUpdate();
    void progressOfAResumedUpdate();
    void progressStopsAtACancel();
    void otherLinesAreNotProgress();
    void failureLinePrefersTheError();
    void verificationProgressOfARealRepair();
    void repairedFilesOfRealRepairs();
    void aRepairReportsItsFixes();
    void argumentsPerKind();
    void checkDueAfterADay();
    void runsAJobWithProgress();
    void jobsKeepNoAppImageEnvironment();
    void aSecondJobWaits();
    void cancelSendsSigint();
    void cancelAQueuedJob();
    void aWorkerThatIgnoresSigintIsGoneAfterTheCancel();
    void aMainProcessThatIgnoresSigintIsKilledWithItsWorker();
    void stopAllDoesNotBlockAndEndsTheGroup();
    void destroyingTheJobsKillsTheGroup();
    void failureKeepsTheJobAndTheLine();
    void infoAnswersWithItsStdout();
    void infoFailureGivesTheLine();
    void noLegendary();
    void aGameHasOneJob();
    void uninstallAndCheckAreHidden();
    void theCheckWritesItsTime();
    void aFailedCheckWritesNothing();
    void theDailyCheckRunsOnceWhenDue();
    void theDailyCheckWaitsForABusyRunner();
    void theDailyCheckHonoursTheSwitch();

private:
    LegendaryJobs *make();
    static bool waitFor(const std::function<bool()> &done, int ms = 10000);

    QTemporaryDir m_dir;
    QString m_program;
    std::unique_ptr<LegendaryJobs> m_jobs;
    QString m_checkFile;
};

void LegendaryJobsTest::init()
{
    m_jobs.reset();
    QVERIFY(m_dir.isValid());
    QDir(m_dir.path()).removeRecursively();
    QVERIFY(QDir().mkpath(m_dir.path()));
    m_program = m_dir.filePath(u"legendary"_s);
    QFile f(m_program);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write(kFake);
    f.close();
    QVERIFY(f.setPermissions(QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner));
    m_checkFile = m_dir.filePath(u"state/check.txt"_s);
}

LegendaryJobs *LegendaryJobsTest::make()
{
    m_jobs = std::make_unique<LegendaryJobs>([this] { return LegendaryJobs::Tool{m_program, u"/cfg/legendary"_s}; });
    return m_jobs.get();
}

bool LegendaryJobsTest::waitFor(const std::function<bool()> &done, int ms)
{
    return QTest::qWaitFor(done, ms);
}

void LegendaryJobsTest::progressOfARealUpdate()
{
    const QList<double> p = percents(captureLines(u"update.stderr"_s));
    QVERIFY(p.size() >= 5);
    QCOMPARE(p.first(), 0.0);
    QVERIFY(p.last() > 90.0);
    QCOMPARE(LegendaryOutput::progress(u"[DLManager] INFO: = Progress: 47.06% (272/578), Running for 00:00:02, ETA: 00:00:02"_s),
             47.06);
    for (qsizetype i = 1; i < p.size(); ++i)
        QVERIFY(p.at(i) >= p.at(i - 1));
}

void LegendaryJobsTest::progressOfAResumedUpdate()
{
    // The resumed run counts only the chunks that are left.
    const QList<double> p = percents(captureLines(u"update-resumed.stderr"_s));
    QVERIFY(p.size() > 3);
    QCOMPARE(p.first(), 0.0);
    QVERIFY(p.last() > 90.0);
}

void LegendaryJobsTest::progressStopsAtACancel()
{
    for (const QString &name : {u"cancel-sigint.stderr"_s, u"cancel-sigterm.stderr"_s}) {
        const QList<double> p = percents(captureLines(name));
        QVERIFY2(!p.isEmpty(), qPrintable(name));
        QVERIFY2(p.last() > 25.0 && p.last() < 60.0, qPrintable(name));
    }
}

void LegendaryJobsTest::otherLinesAreNotProgress()
{
    QVERIFY(!LegendaryOutput::progress(u"[DLManager] INFO:  - Downloaded: 30.22 MiB, Written: 6.00 MiB"_s));
    QVERIFY(!LegendaryOutput::progress(u"[cli] INFO: Download size: 214.55 MiB (Compression savings: 30.6%)"_s));
    QVERIFY(!LegendaryOutput::progress({}));
}

void LegendaryJobsTest::failureLinePrefersTheError()
{
    QCOMPARE(LegendaryOutput::failureLine({u"[cli] INFO: starting"_s, u"[cli] ERROR: The disk is full"_s,
                                           u"[cli] INFO: bye"_s}),
             u"The disk is full"_s);
    QCOMPARE(LegendaryOutput::failureLine({u"[cli] INFO: starting"_s, u"Something odd"_s,
                                           u"[DLManager] INFO:  + Disk\t- 0.00 MiB/s (write)"_s,
                                           u"[DLManager] INFO: = Progress: 2.60% (15/578)"_s}),
             u"Something odd"_s);
    QCOMPARE(LegendaryOutput::failureLine({}), QString());
    // The real update's old-manifest error is the line a failing run of it would show.
    QCOMPARE(LegendaryOutput::failureLine(captureLines(u"update.stderr"_s)),
             u"Could not load old manifest, patching will not work!"_s);
}

void LegendaryJobsTest::verificationProgressOfARealRepair()
{
    for (const QString &name : {u"repair-ok.stdout"_s, u"repair-broken.stdout"_s}) {
        const QList<double> p = percents(captureLinesCr(name));
        QVERIFY2(p.size() > 100, qPrintable(name));
        QVERIFY2(p.last() > 90.0, qPrintable(name));
        for (qsizetype i = 1; i < p.size(); ++i)
            QVERIFY2(p.at(i) >= p.at(i - 1), qPrintable(name));
    }
    QCOMPARE(LegendaryOutput::progress(u"Verification progress: 2/219 (0.3%) [0.0 MiB/s]\t"_s), 0.3);
    QVERIFY(!LegendaryOutput::progress(u"[cli] INFO: Verification finished successfully."_s));
}

void LegendaryJobsTest::repairedFilesOfRealRepairs()
{
    QCOMPARE(LegendaryOutput::repairedFiles(captureLines(u"repair-ok.stderr"_s)), 0);
    // One file changed and one deleted.
    QCOMPARE(LegendaryOutput::repairedFiles(captureLines(u"repair-broken.stderr"_s)), 2);
    QCOMPARE(LegendaryOutput::repairedFiles({}), 0);
    QCOMPARE(LegendaryOutput::repairedFiles({u"[cli] ERROR: Verification failed, 3 file(s) corrupted"_s}), 3);
    // Not a verification line.
    QCOMPARE(LegendaryOutput::repairedFiles({u"[cli] INFO: 5 file(s) are missing"_s}), 0);
}

void LegendaryJobsTest::aRepairReportsItsFixes()
{
    LegendaryJobs *jobs = make();
    QSignalSpy finished(jobs, &LegendaryJobs::finished);
    QCOMPARE(jobs->enqueue(LegendaryJobs::Kind::Repair, u"Fix"_s), QString());
    QVERIFY(waitFor([&] { return jobs->job(u"Fix"_s).value(u"percent"_s).toDouble() >= 50.0; }));
    QCOMPARE(jobs->job(u"Fix"_s).value(u"kind"_s).toString(), u"repair"_s);
    QVERIFY(waitFor([&] { return finished.count() == 1; }));
    const auto result = finished.first().first().value<LegendaryJobs::Result>();
    QVERIFY(result.ok);
    QCOMPARE(result.kind, LegendaryJobs::Kind::Repair);
    QCOMPARE(result.repaired, 3);

    QFile calls(m_dir.filePath(u"calls.txt"_s));
    QVERIFY(calls.open(QIODevice::ReadOnly));
    QCOMPARE(calls.readAll().trimmed(), QByteArray("repair Fix -y [/cfg/legendary]"));

    // A fine run has nothing to fix.
    QCOMPARE(jobs->enqueue(LegendaryJobs::Kind::Repair, u"Fine"_s), QString());
    QVERIFY(waitFor([&] { return finished.count() == 2; }));
    QCOMPARE(finished.at(1).first().value<LegendaryJobs::Result>().repaired, 0);
}

void LegendaryJobsTest::argumentsPerKind()
{
    using K = LegendaryJobs::Kind;
    QCOMPARE(LegendaryJobs::arguments(K::Update, u"App"_s, {}), QStringList({u"update"_s, u"App"_s, u"-y"_s}));
    QCOMPARE(LegendaryJobs::arguments(K::Uninstall, u"App"_s, {}), QStringList({u"uninstall"_s, u"App"_s, u"-y"_s}));
    QCOMPARE(LegendaryJobs::arguments(K::Repair, u"App"_s, {}), QStringList({u"repair"_s, u"App"_s, u"-y"_s}));
    QCOMPARE(LegendaryJobs::arguments(K::Install, u"App"_s, {u"--base-path"_s, u"/g"_s}),
             QStringList({u"install"_s, u"App"_s, u"--base-path"_s, u"/g"_s, u"-y"_s}));
    QCOMPARE(LegendaryJobs::arguments(K::Check, {}, {}), QStringList({u"list-installed"_s, u"--check-updates"_s}));
}

void LegendaryJobsTest::checkDueAfterADay()
{
    const QDateTime now = QDateTime::fromString(u"2026-11-18T12:00:00Z"_s, Qt::ISODate);
    QVERIFY(LegendaryJobs::checkDue(std::nullopt, now));
    QVERIFY(!LegendaryJobs::checkDue(now.addSecs(-3600), now));
    QVERIFY(LegendaryJobs::checkDue(now.addSecs(-24 * 3600), now));
    QVERIFY(LegendaryJobs::checkDue(now.addSecs(3600), now));
    QVERIFY(!LegendaryJobs::readCheckTime(m_dir.filePath(u"nothing.txt"_s)));
}

void LegendaryJobsTest::jobsKeepNoAppImageEnvironment()
{
    const AppImageEnv appImage;
    const QString dump = m_dir.filePath(u"env.txt"_s);
    QFile f(m_program);
    QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Truncate));
    f.write(AppImageEnv::dumpScript(dump));
    f.close();

    LegendaryJobs *jobs = make();
    QSignalSpy finished(jobs, &LegendaryJobs::finished);
    QCOMPARE(jobs->enqueue(LegendaryJobs::Kind::Update, u"Game"_s), QString());
    QVERIFY(waitFor([&] { return finished.count() == 1; }));
    AppImageEnv::noBundleIn(dump);

    QVERIFY(QFile::remove(dump));
    QSignalSpy info(jobs, &LegendaryJobs::infoReady);
    jobs->queryInfo(u"Game"_s);
    QVERIFY(waitFor([&] { return info.count() == 1; }));
    AppImageEnv::noBundleIn(dump);
}

void LegendaryJobsTest::runsAJobWithProgress()
{
    LegendaryJobs *jobs = make();
    QSignalSpy finished(jobs, &LegendaryJobs::finished);
    QSignalSpy changed(jobs, &LegendaryJobs::jobsChanged);
    QCOMPARE(jobs->enqueue(LegendaryJobs::Kind::Update, u"Slow"_s), QString());
    QVERIFY(waitFor([&] { return jobs->job(u"Slow"_s).value(u"percent"_s).toDouble() >= 20.0; }));
    QCOMPARE(jobs->job(u"Slow"_s).value(u"state"_s).toString(), u"running"_s);
    QCOMPARE(jobs->job(u"Slow"_s).value(u"kind"_s).toString(), u"update"_s);
    QVERIFY(waitFor([&] { return finished.count() == 1; }));
    const auto result = finished.first().first().value<LegendaryJobs::Result>();
    QVERIFY(result.ok);
    QVERIFY(!result.cancelled);
    QCOMPARE(result.appName, u"Slow"_s);
    QVERIFY(jobs->jobs().isEmpty());
    QVERIFY(changed.count() >= 3);

    QFile calls(m_dir.filePath(u"calls.txt"_s));
    QVERIFY(calls.open(QIODevice::ReadOnly));
    QCOMPARE(calls.readAll().trimmed(), QByteArray("update Slow -y [/cfg/legendary]"));
}

void LegendaryJobsTest::aSecondJobWaits()
{
    LegendaryJobs *jobs = make();
    QSignalSpy finished(jobs, &LegendaryJobs::finished);
    QCOMPARE(jobs->enqueue(LegendaryJobs::Kind::Update, u"Slow"_s), QString());
    QCOMPARE(jobs->enqueue(LegendaryJobs::Kind::Update, u"Other"_s), QString());
    const QVariantList list = jobs->jobs();
    QCOMPARE(list.size(), 2);
    QCOMPARE(list.at(0).toMap().value(u"appName"_s).toString(), u"Slow"_s);
    QCOMPARE(list.at(1).toMap().value(u"state"_s).toString(), u"queued"_s);
    QVERIFY(waitFor([&] { return finished.count() == 2; }));
    QCOMPARE(finished.at(0).first().value<LegendaryJobs::Result>().appName, u"Slow"_s);
    QCOMPARE(finished.at(1).first().value<LegendaryJobs::Result>().appName, u"Other"_s);
}

void LegendaryJobsTest::cancelSendsSigint()
{
    LegendaryJobs *jobs = make();
    QSignalSpy finished(jobs, &LegendaryJobs::finished);
    QCOMPARE(jobs->enqueue(LegendaryJobs::Kind::Update, u"Slow"_s), QString());
    QVERIFY(waitFor([&] { return jobs->job(u"Slow"_s).value(u"percent"_s).toDouble() >= 10.0; }));
    jobs->cancel(u"Slow"_s);
    QVERIFY(waitFor([&] { return finished.count() == 1; }));
    const auto result = finished.first().first().value<LegendaryJobs::Result>();
    // The fake exits 0 on SIGINT, as Legendary does.
    QVERIFY(result.cancelled);
    QVERIFY(!result.ok);
    QVERIFY(jobs->jobs().isEmpty());
}

namespace {
// The pid the fake's worker wrote once it runs; 0 until then.
qint64 workerPid(const QString &dir)
{
    QFile f(dir + u"/worker.pid"_s);
    if (!f.open(QIODevice::ReadOnly))
        return 0;
    return f.readAll().trimmed().toLongLong();
}

// Gone, or only a zombie nobody has reaped yet.
bool processGone(qint64 pid)
{
    QFile f(u"/proc/%1/stat"_s.arg(pid));
    if (!f.open(QIODevice::ReadOnly))
        return true;
    return f.readAll().contains(") Z");
}
}

void LegendaryJobsTest::aWorkerThatIgnoresSigintIsGoneAfterTheCancel()
{
    LegendaryJobs *jobs = make();
    jobs->setCancelGraceMs(500);
    QSignalSpy finished(jobs, &LegendaryJobs::finished);
    QCOMPARE(jobs->enqueue(LegendaryJobs::Kind::Update, u"Worker"_s), QString());
    QVERIFY(waitFor([&] { return workerPid(m_dir.path()) > 0; }));
    const qint64 worker = workerPid(m_dir.path());
    QVERIFY(!processGone(worker));
    jobs->cancel(u"Worker"_s);
    QVERIFY(waitFor([&] { return finished.count() == 1; }));
    // The job is over only when its workers are, so a new install finds no second writer.
    QVERIFY(processGone(worker));
    QVERIFY(finished.first().first().value<LegendaryJobs::Result>().cancelled);
    QVERIFY(jobs->jobs().isEmpty());
}

void LegendaryJobsTest::aMainProcessThatIgnoresSigintIsKilledWithItsWorker()
{
    LegendaryJobs *jobs = make();
    jobs->setCancelGraceMs(300);
    QSignalSpy finished(jobs, &LegendaryJobs::finished);
    QCOMPARE(jobs->enqueue(LegendaryJobs::Kind::Update, u"Stubborn"_s), QString());
    QVERIFY(waitFor([&] { return workerPid(m_dir.path()) > 0; }));
    const qint64 worker = workerPid(m_dir.path());
    jobs->cancel(u"Stubborn"_s);
    QVERIFY(waitFor([&] { return finished.count() == 1; }));
    QVERIFY(processGone(worker));
    QVERIFY(finished.first().first().value<LegendaryJobs::Result>().cancelled);
    // The next job of the game can start.
    QCOMPARE(jobs->enqueue(LegendaryJobs::Kind::Update, u"Stubborn"_s), QString());
}

void LegendaryJobsTest::stopAllDoesNotBlockAndEndsTheGroup()
{
    LegendaryJobs *jobs = make();
    jobs->setCancelGraceMs(1000);
    QSignalSpy stopped(jobs, &LegendaryJobs::allStopped);
    QSignalSpy finished(jobs, &LegendaryJobs::finished);
    QCOMPARE(jobs->enqueue(LegendaryJobs::Kind::Update, u"Worker"_s), QString());
    QCOMPARE(jobs->enqueue(LegendaryJobs::Kind::Update, u"Other"_s), QString());
    QVERIFY(waitFor([&] { return workerPid(m_dir.path()) > 0; }));
    const qint64 worker = workerPid(m_dir.path());
    QElapsedTimer timer;
    timer.start();
    QVERIFY(jobs->stopAll());
    QVERIFY(timer.elapsed() < 200);
    QCOMPARE(stopped.count(), 0);
    // SIGINT stops the main process at once; the worker that ignores it goes with the kill at the grace's end.
    QVERIFY(waitFor([&] { return stopped.count() == 1; }, 15000));
    QVERIFY(processGone(worker));
    QCOMPARE(finished.count(), 1);
    QVERIFY(jobs->jobs().isEmpty());
    QVERIFY(!jobs->stopAll());
}

void LegendaryJobsTest::destroyingTheJobsKillsTheGroup()
{
    LegendaryJobs *jobs = make();
    QCOMPARE(jobs->enqueue(LegendaryJobs::Kind::Update, u"Stubborn"_s), QString());
    QVERIFY(waitFor([&] { return workerPid(m_dir.path()) > 0; }));
    const qint64 worker = workerPid(m_dir.path());
    QElapsedTimer timer;
    timer.start();
    m_jobs.reset();
    QVERIFY(timer.elapsed() < 3000);
    QVERIFY(waitFor([&] { return processGone(worker); }, 2000));
}

void LegendaryJobsTest::cancelAQueuedJob()
{
    LegendaryJobs *jobs = make();
    QSignalSpy finished(jobs, &LegendaryJobs::finished);
    QCOMPARE(jobs->enqueue(LegendaryJobs::Kind::Update, u"Slow"_s), QString());
    QCOMPARE(jobs->enqueue(LegendaryJobs::Kind::Update, u"Other"_s), QString());
    jobs->cancel(u"Other"_s);
    QCOMPARE(finished.count(), 1);
    QVERIFY(finished.first().first().value<LegendaryJobs::Result>().cancelled);
    QCOMPARE(jobs->jobs().size(), 1);
    jobs->cancel(u"Slow"_s);
    QVERIFY(waitFor([&] { return finished.count() == 2; }));
}

void LegendaryJobsTest::failureKeepsTheJobAndTheLine()
{
    LegendaryJobs *jobs = make();
    QSignalSpy finished(jobs, &LegendaryJobs::finished);
    QCOMPARE(jobs->enqueue(LegendaryJobs::Kind::Update, u"Bad"_s), QString());
    QVERIFY(waitFor([&] { return finished.count() == 1; }));
    const auto result = finished.first().first().value<LegendaryJobs::Result>();
    QVERIFY(!result.ok);
    QVERIFY(!result.cancelled);
    QCOMPARE(result.error, u"The disk is full"_s);
    const QVariantMap job = jobs->job(u"Bad"_s);
    QCOMPARE(job.value(u"state"_s).toString(), u"failed"_s);
    QCOMPARE(job.value(u"error"_s).toString(), u"The disk is full"_s);
    jobs->dismiss(u"Bad"_s);
    QVERIFY(jobs->jobs().isEmpty());

    // Trying again replaces the failed entry.
    QCOMPARE(jobs->enqueue(LegendaryJobs::Kind::Update, u"Bad"_s), QString());
    QVERIFY(waitFor([&] { return finished.count() == 2; }));
    QCOMPARE(jobs->jobs().size(), 1);
}

void LegendaryJobsTest::infoAnswersWithItsStdout()
{
    LegendaryJobs *jobs = make();
    QSignalSpy ready(jobs, &LegendaryJobs::infoReady);
    jobs->queryInfo(u"App"_s);
    jobs->queryInfo(u"App"_s);
    QVERIFY(waitFor([&] { return ready.count() == 1; }));
    QCOMPARE(ready.first().at(0).toString(), u"App"_s);
    QCOMPARE(ready.first().at(1).toByteArray().trimmed(), QByteArray(R"({"manifest": {"download_size": 1000, "disk_size": 3000}})"));
    QVERIFY(ready.first().at(2).toString().isEmpty());
    // It runs beside the queue and is not a job.
    QVERIFY(jobs->jobs().isEmpty());

    QFile calls(m_dir.filePath(u"calls.txt"_s));
    QVERIFY(calls.open(QIODevice::ReadOnly));
    QCOMPARE(calls.readAll().trimmed(), QByteArray("info App --json [/cfg/legendary]"));
}

void LegendaryJobsTest::infoFailureGivesTheLine()
{
    LegendaryJobs *jobs = make();
    QSignalSpy ready(jobs, &LegendaryJobs::infoReady);
    jobs->queryInfo(u"Bad"_s);
    QVERIFY(waitFor([&] { return ready.count() == 1; }));
    QVERIFY(ready.first().at(1).toByteArray().isEmpty());
    QCOMPARE(ready.first().at(2).toString(), u"Could not find the game"_s);

    m_jobs = std::make_unique<LegendaryJobs>([] { return LegendaryJobs::Tool{}; });
    QSignalSpy none(m_jobs.get(), &LegendaryJobs::infoReady);
    m_jobs->queryInfo(u"App"_s);
    QVERIFY(waitFor([&] { return none.count() == 1; }));
    QVERIFY(!none.first().at(2).toString().isEmpty());
}

void LegendaryJobsTest::noLegendary()
{
    m_jobs = std::make_unique<LegendaryJobs>([] { return LegendaryJobs::Tool{}; });
    QVERIFY(!m_jobs->enqueue(LegendaryJobs::Kind::Update, u"Slow"_s).isEmpty());
    QVERIFY(m_jobs->jobs().isEmpty());
    QVERIFY(!m_jobs->checkUpdates());

    m_jobs = std::make_unique<LegendaryJobs>([] { return LegendaryJobs::Tool{u"/nowhere/legendary"_s, {}}; });
    QSignalSpy finished(m_jobs.get(), &LegendaryJobs::finished);
    QCOMPARE(m_jobs->enqueue(LegendaryJobs::Kind::Uninstall, u"App"_s), QString());
    QVERIFY(waitFor([&] { return finished.count() == 1; }));
    const auto result = finished.first().first().value<LegendaryJobs::Result>();
    QVERIFY(!result.ok);
    QVERIFY(!result.started);
    QVERIFY(result.error.contains(u"Could not start"_s));
}

void LegendaryJobsTest::aGameHasOneJob()
{
    LegendaryJobs *jobs = make();
    QCOMPARE(jobs->enqueue(LegendaryJobs::Kind::Update, u"Slow"_s), QString());
    QVERIFY(!jobs->enqueue(LegendaryJobs::Kind::Uninstall, u"Slow"_s).isEmpty());
    QCOMPARE(jobs->jobs().size(), 1);
    jobs->cancel(u"Slow"_s);
    QSignalSpy finished(jobs, &LegendaryJobs::finished);
    QVERIFY(waitFor([&] { return finished.count() == 1; }));
}

void LegendaryJobsTest::uninstallAndCheckAreHidden()
{
    LegendaryJobs *jobs = make();
    QSignalSpy finished(jobs, &LegendaryJobs::finished);
    QCOMPARE(jobs->enqueue(LegendaryJobs::Kind::Uninstall, u"Gone"_s), QString());
    QVERIFY(jobs->jobs().isEmpty());
    QVERIFY(jobs->busy());
    QCOMPARE(jobs->uninstalling(), QStringList{u"Gone"_s});
    QVERIFY(!jobs->checkUpdates());
    QVERIFY(waitFor([&] { return finished.count() == 1; }));
    QVERIFY(finished.first().first().value<LegendaryJobs::Result>().ok);
    QVERIFY(!jobs->busy());
    QVERIFY(jobs->uninstalling().isEmpty());
}

void LegendaryJobsTest::theCheckWritesItsTime()
{
    LegendaryJobs *jobs = make();
    jobs->startDailyCheck(m_checkFile, [] { return false; });
    QSignalSpy finished(jobs, &LegendaryJobs::finished);
    QVERIFY(jobs->checkUpdates());
    QVERIFY(jobs->jobs().isEmpty());
    QVERIFY(waitFor([&] { return finished.count() == 1; }));
    const auto when = LegendaryJobs::readCheckTime(m_checkFile);
    QVERIFY(when);
    QVERIFY(qAbs(when->secsTo(QDateTime::currentDateTimeUtc())) < 60);
    QVERIFY(!LegendaryJobs::checkDue(when));
    QFile calls(m_dir.filePath(u"calls.txt"_s));
    QVERIFY(calls.open(QIODevice::ReadOnly));
    QVERIFY(calls.readAll().startsWith("list-installed --check-updates"));
}

void LegendaryJobsTest::aFailedCheckWritesNothing()
{
    qputenv("FAKE_CHECK_EXIT", "1");
    LegendaryJobs *jobs = make();
    jobs->startDailyCheck(m_checkFile, [] { return false; });
    QSignalSpy finished(jobs, &LegendaryJobs::finished);
    QVERIFY(jobs->checkUpdates());
    QVERIFY(waitFor([&] { return finished.count() == 1; }));
    qunsetenv("FAKE_CHECK_EXIT");
    QVERIFY(!finished.first().first().value<LegendaryJobs::Result>().ok);
    QVERIFY(!QFile::exists(m_checkFile));
    // Failures are logged only: no job is left to show.
    QVERIFY(jobs->jobs().isEmpty());
}

void LegendaryJobsTest::theDailyCheckRunsOnceWhenDue()
{
    LegendaryJobs *jobs = make();
    jobs->setCheckIntervals(20, 20, 60'000);
    QSignalSpy finished(jobs, &LegendaryJobs::finished);
    jobs->startDailyCheck(m_checkFile, [] { return true; });
    QVERIFY(waitFor([&] { return finished.count() == 1; }));
    QCOMPARE(finished.first().first().value<LegendaryJobs::Result>().kind, LegendaryJobs::Kind::Check);
    QVERIFY(QFile::exists(m_checkFile));

    // Checked just now: not due, so no second run.
    LegendaryJobs *again = make();
    again->setCheckIntervals(20, 20, 50);
    QSignalSpy second(again, &LegendaryJobs::finished);
    again->startDailyCheck(m_checkFile, [] { return true; });
    QTest::qWait(300);
    QCOMPARE(second.count(), 0);
}

void LegendaryJobsTest::theDailyCheckWaitsForABusyRunner()
{
    LegendaryJobs *jobs = make();
    jobs->setCheckIntervals(20, 50, 60'000);
    QSignalSpy finished(jobs, &LegendaryJobs::finished);
    QCOMPARE(jobs->enqueue(LegendaryJobs::Kind::Update, u"Slow"_s), QString());
    jobs->startDailyCheck(m_checkFile, [] { return true; });
    QTest::qWait(200);
    // Still the update only: the check didn't queue behind it.
    QCOMPARE(jobs->jobs().size(), 1);
    QVERIFY(!QFile::exists(m_checkFile));
    QVERIFY(waitFor([&] { return finished.count() == 2; }));
    QCOMPARE(finished.last().first().value<LegendaryJobs::Result>().kind, LegendaryJobs::Kind::Check);
    QVERIFY(QFile::exists(m_checkFile));
}

void LegendaryJobsTest::theDailyCheckHonoursTheSwitch()
{
    LegendaryJobs *jobs = make();
    jobs->setCheckIntervals(20, 20, 50);
    QSignalSpy finished(jobs, &LegendaryJobs::finished);
    bool on = false;
    jobs->startDailyCheck(m_checkFile, [&on] { return on; });
    QTest::qWait(300);
    QCOMPARE(finished.count(), 0);
    on = true;
    QVERIFY(waitFor([&] { return finished.count() == 1; }));
}

QTEST_MAIN(LegendaryJobsTest)
#include "legendaryjobs_test.moc"
