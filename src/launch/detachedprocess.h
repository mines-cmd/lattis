#pragma once

#include "launchcommand.h"

#include <QObject>
#include <QTimer>

// See docs/code/launch.md.

class QSocketNotifier;

class DetachedProcess : public QObject
{
    Q_OBJECT

public:
    explicit DetachedProcess(QObject *parent = nullptr);
    ~DetachedProcess() override;

    QString start(const LaunchCommand &command, const QString &workingDir, const QString &logPath);

    qint64 pid() const { return m_pid; }

    // Set it before start().
    void setFollowSession(bool follow) { m_followSession = follow; }
    void setSessionPollInterval(int ms) { m_sessionPoll.setInterval(ms); }
    int sessionPollInterval() const { return m_sessionPoll.interval(); }

    static bool sessionHasProcesses(qint64 sessionId);
    static void killSession(qint64 sessionId);
    static void killTree(qint64 pid);

    void kill() { killSession(m_pid); }

signals:
    // `crashed`: ended by a signal; `exitCode` is then the signal number. Both are the
    // program's, when following its session too.
    void finished(int exitCode, bool crashed);

private:
    void onExited();
    void checkSession();

    qint64 m_pid = 0;
    int m_pidfd = -1;
    QSocketNotifier *m_notifier = nullptr;
    bool m_followSession = false;
    QTimer m_sessionPoll;
    int m_exitCode = 0;
    bool m_crashed = false;
};
