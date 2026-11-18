#include "legendaryjobs.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QLoggingCategory>
#include <QProcessEnvironment>
#include <QRegularExpression>

#include <csignal>

Q_LOGGING_CATEGORY(lcLegendary, "lattis.legendary")

namespace {
bool isDetail(const QString &line)
{
    // The lines under a progress line: " - Downloaded: …", " - Cache usage: …", " + Download …", " + Disk …".
    static const QRegularExpression detail(QStringLiteral(R"(^(\[\w+\] \w+: )? [-+] )"));
    return detail.match(line).hasMatch();
}

QString kindName(LegendaryJobs::Kind kind)
{
    switch (kind) {
    case LegendaryJobs::Kind::Update:
        return QStringLiteral("update");
    case LegendaryJobs::Kind::Install:
        return QStringLiteral("install");
    case LegendaryJobs::Kind::Repair:
        return QStringLiteral("repair");
    case LegendaryJobs::Kind::Uninstall:
        return QStringLiteral("uninstall");
    case LegendaryJobs::Kind::Check:
        return QStringLiteral("check");
    }
    return {};
}

constexpr qint64 kDaySeconds = 24 * 60 * 60;
constexpr int kTailLines = 300;
}

std::optional<double> LegendaryOutput::progress(const QString &line)
{
    static const QRegularExpression re(
        QStringLiteral(R"((?:= Progress: |Verification progress: \d+/\d+ \()(\d+(?:\.\d+)?)%)"));
    const QRegularExpressionMatch m = re.match(line);
    if (!m.hasMatch())
        return std::nullopt;
    return m.captured(1).toDouble();
}

int LegendaryOutput::repairedFiles(const QStringList &lines)
{
    static const QRegularExpression corrupted(QStringLiteral(R"((\d+) file\(s\) corrupted)"));
    static const QRegularExpression missing(QStringLiteral(R"((\d+) file\(s\) are missing)"));
    for (auto it = lines.crbegin(); it != lines.crend(); ++it) {
        if (!it->contains(QLatin1String("Verification failed")))
            continue;
        int count = 0;
        if (const auto m = corrupted.match(*it); m.hasMatch())
            count += m.captured(1).toInt();
        if (const auto m = missing.match(*it); m.hasMatch())
            count += m.captured(1).toInt();
        return count;
    }
    return 0;
}

QString LegendaryOutput::failureLine(const QStringList &lines)
{
    static const QRegularExpression error(QStringLiteral(R"((?:ERROR|CRITICAL): (.*)$)"));
    for (auto it = lines.crbegin(); it != lines.crend(); ++it) {
        if (const QRegularExpressionMatch m = error.match(*it); m.hasMatch() && !m.captured(1).trimmed().isEmpty())
            return m.captured(1).trimmed();
    }
    for (auto it = lines.crbegin(); it != lines.crend(); ++it) {
        const QString line = it->trimmed();
        if (!line.isEmpty() && !isDetail(*it) && !progress(line))
            return line;
    }
    return {};
}

LegendaryJobs::LegendaryJobs(std::function<Tool()> tool, QObject *parent)
    : QObject(parent)
    , m_tool(std::move(tool))
{
    qRegisterMetaType<LegendaryJobs::Result>();
    m_cancelTimer.setSingleShot(true);
    connect(&m_cancelTimer, &QTimer::timeout, this, [this] {
        if (m_process)
            m_process->kill();
    });
    m_checkTimer.setSingleShot(true);
    connect(&m_checkTimer, &QTimer::timeout, this, &LegendaryJobs::runDailyCheck);
}

LegendaryJobs::~LegendaryJobs()
{
    if (!m_process)
        return;
    m_process->disconnect(this);
    ::kill(pid_t(m_process->processId()), SIGINT);
    if (!m_process->waitForFinished(5000))
        m_process->kill();
}

QStringList LegendaryJobs::arguments(Kind kind, const QString &appName, const QStringList &extraArgs)
{
    if (kind == Kind::Check)
        return {QStringLiteral("list-installed"), QStringLiteral("--check-updates")};
    return QStringList{kindName(kind), appName} + extraArgs + QStringList{QStringLiteral("-y")};
}

QVariantList LegendaryJobs::jobs() const
{
    QVariantList out;
    const auto add = [&out](const Job &job, const QString &state) {
        out.append(QVariantMap{{QStringLiteral("appName"), job.appName},
                               {QStringLiteral("kind"), kindName(job.kind)},
                               {QStringLiteral("percent"), job.percent},
                               {QStringLiteral("state"), state},
                               {QStringLiteral("error"), job.error}});
    };
    for (const Job &job : m_queue) {
        if (!hidden(job))
            add(job, job.state == State::Running ? QStringLiteral("running") : QStringLiteral("queued"));
    }
    for (const Job &job : m_failed)
        add(job, QStringLiteral("failed"));
    return out;
}

QVariantMap LegendaryJobs::job(const QString &appName) const
{
    for (const QVariant &v : jobs()) {
        if (v.toMap().value(QStringLiteral("appName")) == appName)
            return v.toMap();
    }
    return {};
}

QString LegendaryJobs::enqueue(Kind kind, const QString &appName, const QStringList &extraArgs)
{
    if (m_tool().program.isEmpty())
        return tr("Heroic's Legendary isn't found");
    for (const Job &job : std::as_const(m_queue)) {
        if (!appName.isEmpty() && job.appName == appName)
            return tr("Legendary is already working on it");
    }
    const bool hadFailed = m_failed.removeIf([&](const Job &j) { return j.appName == appName; }) > 0;

    Job job;
    job.kind = kind;
    job.appName = appName;
    job.args = arguments(kind, appName, extraArgs);
    m_queue.append(job);
    if (!hidden(job) || hadFailed)
        emit jobsChanged();
    startNext();
    return {};
}

void LegendaryJobs::cancel(const QString &appName)
{
    for (qsizetype i = 0; i < m_queue.size(); ++i) {
        if (m_queue.at(i).appName != appName)
            continue;
        if (m_queue.at(i).state == State::Running) {
            if (m_cancelling || !m_process)
                return;
            m_cancelling = true;
            ::kill(pid_t(m_process->processId()), SIGINT);
            m_cancelTimer.start(m_cancelGraceMs);
            return;
        }
        const Job job = m_queue.takeAt(i);
        emit jobsChanged();
        Result result;
        result.kind = job.kind;
        result.appName = job.appName;
        result.cancelled = true;
        emit finished(result);
        return;
    }
}

void LegendaryJobs::dismiss(const QString &appName)
{
    if (m_failed.removeIf([&](const Job &j) { return j.appName == appName; }) > 0)
        emit jobsChanged();
}

void LegendaryJobs::queryInfo(const QString &appName)
{
    if (m_infoRunning.contains(appName))
        return;
    const Tool tool = m_tool();
    if (tool.program.isEmpty()) {
        QTimer::singleShot(0, this, [this, appName] {
            emit infoReady(appName, {}, tr("Heroic's Legendary isn't found"));
        });
        return;
    }
    m_infoRunning.insert(appName);
    auto *process = new QProcess(this);
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    if (!tool.configPath.isEmpty())
        env.insert(QStringLiteral("LEGENDARY_CONFIG_PATH"), tool.configPath);
    process->setProcessEnvironment(env);
    connect(process, &QProcess::errorOccurred, this, [this, process, appName](QProcess::ProcessError error) {
        if (error != QProcess::FailedToStart)
            return;
        m_infoRunning.remove(appName);
        emit infoReady(appName, {}, tr("Could not start Legendary: %1").arg(process->errorString()));
        process->deleteLater();
    });
    connect(process, &QProcess::finished, this, [this, process, appName](int exitCode, QProcess::ExitStatus status) {
        m_infoRunning.remove(appName);
        const QByteArray out = process->readAllStandardOutput();
        QString error;
        if (status != QProcess::NormalExit || exitCode != 0) {
            const QStringList lines = QString::fromLocal8Bit(process->readAllStandardError()).split(u'\n');
            error = status == QProcess::NormalExit ? LegendaryOutput::failureLine(lines)
                                                   : tr("Legendary stopped unexpectedly");
            if (error.isEmpty())
                error = tr("it failed");
        }
        emit infoReady(appName, error.isEmpty() ? out : QByteArray(), error);
        process->deleteLater();
    });
    // Not a download: it should answer in seconds, so a stuck one is stopped rather than left asking forever.
    QTimer::singleShot(60'000, process, [process] {
        if (process->state() != QProcess::NotRunning)
            process->kill();
    });
    process->start(tool.program, {QStringLiteral("info"), appName, QStringLiteral("--json")});
}

bool LegendaryJobs::checkUpdates()
{
    if (busy() || m_tool().program.isEmpty())
        return false;
    return enqueue(Kind::Check, {}).isEmpty();
}

void LegendaryJobs::setCheckIntervals(int firstMs, int busyMs, int recheckMs)
{
    m_firstCheckMs = firstMs;
    m_busyMs = busyMs;
    m_recheckMs = recheckMs;
}

void LegendaryJobs::startDailyCheck(const QString &checkFile, std::function<bool()> enabled)
{
    m_checkFile = checkFile;
    m_checkEnabled = std::move(enabled);
    m_checkTimer.start(m_firstCheckMs);
}

std::optional<QDateTime> LegendaryJobs::readCheckTime(const QString &file)
{
    QFile f(file);
    if (!f.open(QIODevice::ReadOnly))
        return std::nullopt;
    const QDateTime when = QDateTime::fromString(QString::fromUtf8(f.readAll()).trimmed(), Qt::ISODate);
    if (!when.isValid())
        return std::nullopt;
    return when;
}

bool LegendaryJobs::checkDue(const std::optional<QDateTime> &last, const QDateTime &now)
{
    // A time in the future (the clock was set back) counts as due too.
    return !last || last->secsTo(now) >= kDaySeconds || last->secsTo(now) < 0;
}

void LegendaryJobs::runDailyCheck()
{
    if (!m_checkEnabled || !m_checkEnabled() || m_tool().program.isEmpty() || !checkDue(readCheckTime(m_checkFile))) {
        m_checkTimer.start(m_recheckMs);
        return;
    }
    if (busy()) {
        m_checkTimer.start(m_busyMs);
        return;
    }
    qCInfo(lcLegendary) << "checking for Epic updates";
    checkUpdates();
    m_checkTimer.start(m_recheckMs);
}

void LegendaryJobs::startNext()
{
    while (!m_process && !m_queue.isEmpty()) {
        Job &job = m_queue.first();
        const Tool tool = m_tool();
        Result result;
        result.kind = job.kind;
        result.appName = job.appName;
        if (tool.program.isEmpty()) {
            result.started = false;
            result.error = tr("Heroic's Legendary isn't found");
            finishJob(result);
            continue;
        }

        m_process = new QProcess(this);
        QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
        if (!tool.configPath.isEmpty())
            env.insert(QStringLiteral("LEGENDARY_CONFIG_PATH"), tool.configPath);
        m_process->setProcessEnvironment(env);
        m_process->setProcessChannelMode(QProcess::MergedChannels);
        m_pending.clear();
        m_tail.clear();
        m_cancelling = false;
        connect(m_process, &QProcess::readyRead, this, &LegendaryJobs::onProcessOutput);
        connect(m_process, &QProcess::finished, this, &LegendaryJobs::onFinished);
        connect(m_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
            if (error != QProcess::FailedToStart || !m_process)
                return;
            Result r;
            r.kind = m_queue.first().kind;
            r.appName = m_queue.first().appName;
            r.started = false;
            r.error = tr("Could not start Legendary: %1").arg(m_process->errorString());
            m_process->deleteLater();
            m_process = nullptr;
            finishJob(r);
        });
        qCInfo(lcLegendary).noquote() << QStringLiteral("legendary %1").arg(job.args.join(u' '));
        job.state = State::Running;
        m_process->start(tool.program, job.args);
        if (!hidden(job))
            emit jobsChanged();
        // A start that failed already ran finishJob from errorOccurred, which may have emptied the queue.
        return;
    }
}

void LegendaryJobs::onProcessOutput()
{
    if (!m_process || m_queue.isEmpty())
        return;
    m_pending += m_process->readAll();
    qsizetype end;
    bool changed = false;
    // Verification's progress is on stdout, one update per carriage return and no newline until it ends.
    const auto lineEnd = [this] {
        for (qsizetype i = 0; i < m_pending.size(); ++i) {
            if (m_pending.at(i) == '\n' || m_pending.at(i) == '\r')
                return i;
        }
        return qsizetype(-1);
    };
    while ((end = lineEnd()) >= 0) {
        const QString line = QString::fromLocal8Bit(m_pending.left(end));
        m_pending.remove(0, end + 1);
        if (const auto percent = LegendaryOutput::progress(line)) {
            Job &job = m_queue.first();
            if (job.percent != *percent) {
                job.percent = *percent;
                changed = !hidden(job);
            }
        } else if (!line.trimmed().isEmpty() && !isDetail(line)) {
            m_tail.append(line);
            if (m_tail.size() > kTailLines)
                m_tail.removeFirst();
        }
    }
    if (changed)
        emit jobsChanged();
}

void LegendaryJobs::onFinished(int exitCode, QProcess::ExitStatus status)
{
    if (!m_process || m_queue.isEmpty())
        return;
    onProcessOutput();
    if (!m_pending.isEmpty()) {
        m_tail.append(QString::fromLocal8Bit(m_pending).trimmed());
        m_pending.clear();
    }
    m_cancelTimer.stop();
    m_process->deleteLater();
    m_process = nullptr;

    Result result;
    result.kind = m_queue.first().kind;
    result.appName = m_queue.first().appName;
    // Legendary exits with 0 after SIGINT too.
    result.cancelled = m_cancelling;
    result.ok = !m_cancelling && status == QProcess::NormalExit && exitCode == 0;
    if (result.ok && result.kind == Kind::Repair)
        result.repaired = LegendaryOutput::repairedFiles(m_tail);
    m_cancelling = false;
    if (!result.ok && !result.cancelled) {
        result.error = status == QProcess::NormalExit ? LegendaryOutput::failureLine(m_tail)
                                                      : tr("Legendary stopped unexpectedly");
        qCWarning(lcLegendary).noquote() << QStringLiteral("legendary %1 failed (%2):\n%3")
                                                .arg(kindName(result.kind)).arg(exitCode).arg(m_tail.join(u'\n'));
    }
    finishJob(result);
}

void LegendaryJobs::finishJob(const Result &result)
{
    Job job = m_queue.takeFirst();
    if (result.kind == Kind::Check) {
        if (result.ok) {
            QDir().mkpath(QFileInfo(m_checkFile).absolutePath());
            QFile f(m_checkFile);
            if (!m_checkFile.isEmpty() && f.open(QIODevice::WriteOnly | QIODevice::Truncate))
                f.write(QDateTime::currentDateTimeUtc().toString(Qt::ISODate).toUtf8());
        } else {
            qCWarning(lcLegendary).noquote() << QStringLiteral("the update check failed: %1").arg(result.error);
        }
    }
    if (!result.ok && !result.cancelled && !hidden(job)) {
        job.state = State::Failed;
        job.error = result.error;
        m_failed.append(job);
    }
    if (!hidden(job))
        emit jobsChanged();
    emit finished(result);
    startNext();
}
