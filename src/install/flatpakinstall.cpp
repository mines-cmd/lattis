#include "flatpakinstall.h"

#include <QCoreApplication>
#include <QProcessEnvironment>
#include <QRegularExpression>
#include <QStandardPaths>

namespace FlatpakInstall {
namespace {
QString tr(const char *text)
{
    return QCoreApplication::translate("FlatpakInstall", text);
}

QString lastLine(const QString &text)
{
    const QStringList lines = text.split(u'\n', Qt::SkipEmptyParts);
    for (auto it = lines.crbegin(); it != lines.crend(); ++it) {
        if (!it->trimmed().isEmpty())
            return it->trimmed();
    }
    return {};
}
}

QString program()
{
    const QString given = qEnvironmentVariable("LATTIS_FLATPAK");
    if (!given.isEmpty())
        return given;
    return QStandardPaths::findExecutable(QStringLiteral("flatpak"));
}

bool validId(const QString &appId)
{
    static const QRegularExpression pattern(QStringLiteral("^[A-Za-z0-9_][A-Za-z0-9_.-]*$"));
    return pattern.match(appId).hasMatch();
}

QList<QStringList> installSteps(const QString &appId)
{
    return {{QStringLiteral("remote-add"), QStringLiteral("--user"), QStringLiteral("--if-not-exists"),
             QStringLiteral("flathub"), QStringLiteral("https://dl.flathub.org/repo/flathub.flatpakrepo")},
            {QStringLiteral("install"), QStringLiteral("--user"), QStringLiteral("--noninteractive"),
             QStringLiteral("-y"), QStringLiteral("flathub"), appId}};
}

QStringList uninstallStep(const QString &appId)
{
    return {QStringLiteral("uninstall"), QStringLiteral("--user"), QStringLiteral("--noninteractive"),
            QStringLiteral("-y"), appId};
}

QString failureText(const QString &standardError, const QString &standardOutput, int exitCode)
{
    if (const QString line = lastLine(standardError); !line.isEmpty())
        return line;
    if (const QString line = lastLine(standardOutput); !line.isEmpty())
        return line;
    return tr("flatpak stopped with code %1").arg(exitCode);
}
}

FlatpakJob::FlatpakJob(const QString &program, const QList<QStringList> &steps, const QStringList &environment,
                       QObject *parent)
    : QObject(parent)
    , m_program(program)
    , m_steps(steps)
    , m_environment(environment)
{
    m_killTimer.setSingleShot(true);
    m_killTimer.setInterval(3000);
    connect(&m_killTimer, &QTimer::timeout, &m_process, &QProcess::kill);
    connect(&m_process, &QProcess::finished, this, &FlatpakJob::stepFinished);
    connect(&m_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error != QProcess::FailedToStart)
            return;
        end({false, m_cancelled, m_cancelled ? QString() : tr("Couldn't run flatpak: %1").arg(m_process.errorString())});
    });
}

FlatpakJob::~FlatpakJob()
{
    m_ended = true;
    if (m_process.state() != QProcess::NotRunning) {
        m_process.disconnect(this);
        m_process.kill();
        m_process.waitForFinished(1000);
    }
}

void FlatpakJob::start()
{
    if (m_started)
        return;
    m_started = true;
    if (!m_environment.isEmpty()) {
        QProcessEnvironment env;
        for (const QString &var : std::as_const(m_environment)) {
            const qsizetype eq = var.indexOf(u'=');
            if (eq > 0)
                env.insert(var.left(eq), var.mid(eq + 1));
        }
        m_process.setProcessEnvironment(env);
    }
    next();
}

void FlatpakJob::next()
{
    if (m_cancelled) {
        end({false, true, {}});
        return;
    }
    if (++m_step >= m_steps.size()) {
        end({true, false, {}});
        return;
    }
    m_process.start(m_program, m_steps.at(m_step));
}

void FlatpakJob::stepFinished(int exitCode, QProcess::ExitStatus status)
{
    if (m_ended)
        return;
    if (m_cancelled) {
        end({false, true, {}});
        return;
    }
    if (status != QProcess::NormalExit || exitCode != 0) {
        end({false, false,
             FlatpakInstall::failureText(QString::fromLocal8Bit(m_process.readAllStandardError()),
                                         QString::fromLocal8Bit(m_process.readAllStandardOutput()), exitCode)});
        return;
    }
    next();
}

void FlatpakJob::cancel()
{
    if (m_ended || m_cancelled)
        return;
    m_cancelled = true;
    if (m_process.state() == QProcess::NotRunning) {
        end({false, true, {}});
        return;
    }
    m_process.terminate();
    m_killTimer.start();
}

void FlatpakJob::end(const Result &result)
{
    if (m_ended)
        return;
    m_ended = true;
    m_killTimer.stop();
    emit finished(result);
}
