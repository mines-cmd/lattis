#include "restart.h"
#include "launchcommand.h"

#include <QCoreApplication>
#include <QProcess>

namespace Restart {
bool waits(const QStringList &arguments)
{
    return arguments.mid(1).contains(kWaitFlag);
}

Command command(const QStringList &arguments, const QString &program, const QString &appImage)
{
    Command c;
    c.program = appImage.isEmpty() ? program : appImage;
    c.arguments = arguments.mid(1);
    c.arguments.removeAll(kWaitFlag);
    c.arguments.append(kWaitFlag);
    return c;
}

bool start(const QStringList &arguments, QString *error)
{
    const QString appImage = qEnvironmentVariable("APPIMAGE");
    const Command c = command(arguments, QCoreApplication::applicationFilePath(), appImage);
    QProcess process;
    process.setProgram(c.program);
    process.setArguments(c.arguments);
    // An AppImage sets its variables again as it starts; the old ones point into this copy's
    // mount.
    if (!appImage.isEmpty())
        process.setEnvironment(programEnvironment(QProcess::systemEnvironment()));
    if (!process.startDetached()) {
        *error = process.errorString();
        return false;
    }
    return true;
}
}
