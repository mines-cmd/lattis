#pragma once

#include <QList>
#include <QObject>
#include <QPointer>
#include <QProcess>
#include <QStringList>
#include <QTimer>

// See docs/code/install.md.

namespace FlatpakInstall {
// $LATTIS_FLATPAK when set (a fake for tests and screenshots), else flatpak on PATH; empty when there is none.
QString program();
bool validId(const QString &appId);
QList<QStringList> installSteps(const QString &appId);
QStringList uninstallStep(const QString &appId);
QString failureText(const QString &standardError, const QString &standardOutput, int exitCode);
}

class FlatpakJob : public QObject
{
    Q_OBJECT

public:
    struct Result
    {
        bool ok = false;
        bool cancelled = false;
        QString error;
    };

    // `environment` is in QProcess::environment() form; empty means the app's own.
    FlatpakJob(const QString &program, const QList<QStringList> &steps, const QStringList &environment,
               QObject *parent = nullptr);
    ~FlatpakJob() override;

    void start();
    void cancel();

signals:
    void finished(const FlatpakJob::Result &result);

private:
    void next();
    void stepFinished(int exitCode, QProcess::ExitStatus status);
    void end(const Result &result);

    QString m_program;
    QList<QStringList> m_steps;
    QStringList m_environment;
    QProcess m_process;
    QTimer m_killTimer;
    qsizetype m_step = -1;
    bool m_cancelled = false;
    bool m_ended = false;
    bool m_started = false;
};

Q_DECLARE_METATYPE(FlatpakJob::Result)
