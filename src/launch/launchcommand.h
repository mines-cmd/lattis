#pragma once

#include "emulators.h"
#include "launchoptions.h"

#include <QList>
#include <QString>
#include <QStringList>

#include <utility>

// See docs/code/launch.md.

struct LaunchCommand
{
    QString program;
    QStringList arguments;
    QList<std::pair<QString, QString>> env;

    QString toString() const;

    bool operator==(const LaunchCommand &) const = default;
};

LaunchCommand buildLaunchCommand(const EmulatorDef &emulator, const EmulatorInstall &install, const QString &romPath,
                                 const QStringList &gameArgs = {});

LaunchCommand buildProgramCommand(const QString &exe, const LaunchOptions::Parts &options);

QStringList programEnvironment(const QStringList &environment, const QList<std::pair<QString, QString>> &env = {});
