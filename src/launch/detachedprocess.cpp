#include "detachedprocess.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QSocketNotifier>

#include <algorithm>
#include <cerrno>
#include <csignal>
#include <cstring>
#include <fcntl.h>
#include <spawn.h>
#include <sys/syscall.h>
#include <sys/wait.h>
#include <unistd.h>

#ifndef P_PIDFD
#define P_PIDFD 3
#endif

DetachedProcess::DetachedProcess(QObject *parent)
    : QObject(parent)
{
    m_sessionPoll.setInterval(2000);
    connect(&m_sessionPoll, &QTimer::timeout, this, &DetachedProcess::checkSession);
}

DetachedProcess::~DetachedProcess()
{
    if (m_pidfd >= 0)
        ::close(m_pidfd);
}

QString DetachedProcess::start(const LaunchCommand &command, const QString &workingDir, const QString &logPath)
{
    if (command.program.isEmpty())
        return QStringLiteral("no program to run");
    QDir().mkpath(QFileInfo(logPath).absolutePath());

    const QByteArray program = QFile::encodeName(command.program);
    const QByteArray dir = QFile::encodeName(workingDir);
    const QByteArray log = QFile::encodeName(logPath);
    QList<QByteArray> args = {program};
    for (const QString &arg : command.arguments)
        args.append(arg.toLocal8Bit());
    QList<char *> argv;
    for (QByteArray &arg : args)
        argv.append(arg.data());
    argv.append(nullptr);
    QList<QByteArray> vars;
    for (const QString &var : programEnvironment(QProcess::systemEnvironment(), command.env))
        vars.append(var.toLocal8Bit());
    QList<char *> envp;
    for (QByteArray &var : vars)
        envp.append(var.data());
    envp.append(nullptr);

    posix_spawn_file_actions_t actions;
    posix_spawn_file_actions_init(&actions);
    posix_spawn_file_actions_addopen(&actions, STDIN_FILENO, "/dev/null", O_RDONLY, 0);
    posix_spawn_file_actions_addopen(&actions, STDOUT_FILENO, log.constData(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
    posix_spawn_file_actions_adddup2(&actions, STDOUT_FILENO, STDERR_FILENO);
    if (!dir.isEmpty())
        posix_spawn_file_actions_addchdir_np(&actions, dir.constData());
    // Nothing else of ours, like the single-instance lock, goes to the game.
    posix_spawn_file_actions_addclosefrom_np(&actions, STDERR_FILENO + 1);

    posix_spawnattr_t attr;
    posix_spawnattr_init(&attr);
    sigset_t none;
    sigemptyset(&none);
    sigset_t all;
    sigfillset(&all);
    posix_spawnattr_setsigmask(&attr, &none);
    posix_spawnattr_setsigdefault(&attr, &all);
    posix_spawnattr_setflags(&attr, POSIX_SPAWN_SETSID | POSIX_SPAWN_SETSIGMASK | POSIX_SPAWN_SETSIGDEF);

    pid_t pid = 0;
    const int error = posix_spawnp(&pid, program.constData(), &actions, &attr, argv.data(), envp.data());
    posix_spawn_file_actions_destroy(&actions);
    posix_spawnattr_destroy(&attr);
    if (error != 0)
        return QString::fromLocal8Bit(std::strerror(error));
    m_pid = pid;

    m_pidfd = int(::syscall(SYS_pidfd_open, pid, 0));
    if (m_pidfd < 0) {
        // Can't watch it; it still runs. Linux older than 5.3.
        return {};
    }
    m_notifier = new QSocketNotifier(m_pidfd, QSocketNotifier::Read, this);
    connect(m_notifier, &QSocketNotifier::activated, this, &DetachedProcess::onExited);
    return {};
}

void DetachedProcess::onExited()
{
    m_notifier->setEnabled(false);
    siginfo_t info{};
    // Reaps it too, so no zombie is left behind.
    if (::waitid(idtype_t(P_PIDFD), id_t(m_pidfd), &info, WEXITED) != 0) {
        emit finished(-1, true);
        return;
    }
    ::close(m_pidfd);
    m_pidfd = -1;
    m_crashed = info.si_code == CLD_KILLED || info.si_code == CLD_DUMPED;
    m_exitCode = info.si_status;
    // Its session is named after it (setsid), and lives on in what it started.
    if (m_followSession && sessionHasProcesses(m_pid)) {
        m_sessionPoll.start();
        return;
    }
    emit finished(m_exitCode, m_crashed);
}

void DetachedProcess::checkSession()
{
    if (sessionHasProcesses(m_pid))
        return;
    m_sessionPoll.stop();
    emit finished(m_exitCode, m_crashed);
}

namespace {
struct ProcEntry
{
    qint64 pid;
    qint64 parent;
    qint64 session;
};

QList<ProcEntry> liveProcesses()
{
    QList<ProcEntry> result;
    QDirIterator it(QStringLiteral("/proc"), QDir::Dirs | QDir::NoDotAndDotDot);
    while (it.hasNext()) {
        const QString name = it.next().section(u'/', -1);
        if (name.isEmpty() || !name.at(0).isDigit())
            continue;
        QFile stat(it.filePath() + QStringLiteral("/stat"));
        if (!stat.open(QIODevice::ReadOnly))
            continue;
        // pid (comm) state ppid pgrp session …; comm can hold spaces and parentheses.
        const QByteArray line = stat.readAll();
        const QList<QByteArray> fields = line.mid(line.lastIndexOf(')') + 2).split(' ');
        if (fields.size() > 3 && fields.at(0) != "Z")
            result.append({name.toLongLong(), fields.at(1).toLongLong(), fields.at(3).toLongLong()});
    }
    return result;
}
}

bool DetachedProcess::sessionHasProcesses(qint64 sessionId)
{
    const QList<ProcEntry> processes = liveProcesses();
    return std::any_of(processes.begin(), processes.end(),
                       [sessionId](const ProcEntry &p) { return p.session == sessionId; });
}

void DetachedProcess::killSession(qint64 sessionId)
{
    for (const ProcEntry &p : liveProcesses()) {
        if (p.session == sessionId)
            ::kill(pid_t(p.pid), SIGKILL);
    }
}

void DetachedProcess::killTree(qint64 pid)
{
    const QList<ProcEntry> processes = liveProcesses();
    QList<qint64> doomed = {pid};
    for (qsizetype i = 0; i < doomed.size(); ++i) {
        for (const ProcEntry &p : processes) {
            if (p.parent == doomed.at(i) && !doomed.contains(p.pid))
                doomed.append(p.pid);
        }
    }
    for (const qint64 victim : doomed)
        ::kill(pid_t(victim), SIGKILL);
}
