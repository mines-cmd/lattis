#pragma once

#include "emulators.h"
#include "launchoptions.h"

#include <QList>
#include <QProcessEnvironment>
#include <QString>
#include <QStringList>
#include <QUrl>

#include <optional>
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

// programEnvironment() of the launcher's own environment, for QProcess::setProcessEnvironment.
QProcessEnvironment childEnvironment();

// `xdg-open <url>`, detached, with the cleaned environment. The one way the app opens a folder, a file or a link.
bool openUrl(const QUrl &url);

// main() calls it when it set QT_QPA_PLATFORMTHEME itself: children get `original` back (nothing when it's empty); nullopt clears it.
void restorePlatformTheme(const std::optional<QString> &original);
