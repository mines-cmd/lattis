#pragma once

#include <QString>
#include <QStringList>
#include <QtTypes>

struct HeroicProgram
{
    QString program;
    QStringList args;   // what goes before the heroic:// URL
    bool noGui = false; // false for xdg-open, which can't pass --no-gui

    bool found() const { return !program.isEmpty(); }

    bool operator==(const HeroicProgram &) const = default;
};

struct HeroicInstall
{
    enum class Kind { None, Native, Flatpak };

    Kind kind = Kind::None;
    QString root;
    bool pathMissing = false;
    HeroicProgram program;

    bool found() const { return kind != Kind::None; }

    bool operator==(const HeroicInstall &) const = default;
};

struct HeroicLocations
{
    QString home;
    QString configHome; // Heroic follows XDG_CONFIG_HOME
    QStringList pathDirs;
    QStringList flatpakDirs;

    static HeroicLocations system();
    QString nativeRoot() const;
    QString flatpakRoot() const;
};

namespace HeroicLocate {
HeroicInstall find(const HeroicLocations &where, const QString &configuredPath = {});

HeroicProgram findProgram(HeroicInstall::Kind kind, const HeroicLocations &where);

// Heroic's own copy of Legendary, which it runs to uninstall a game; empty for a Flatpak (its files aren't
// reachable from outside the sandbox) and when none is found.
QString findLegendary(const HeroicLocations &where);

QStringList launchArgs(const HeroicProgram &program, const QString &appName);
QStringList openArgs(const HeroicProgram &program);

// The pid of the process that started the game's tree (`... -epicapp=<appName>` or `launch <appName>`); 0 when
// there is none.
qint64 gamePid(const QString &appName, const QString &procDir = QStringLiteral("/proc"));

// Whether Heroic itself is running, native or Flatpak: a process whose program is named `heroic`, other than
// its helper processes (`--type=`). Heroic keeps a game's config in memory and writes it back, so a file
// edited while it runs is overwritten.
bool running(const QString &procDir = QStringLiteral("/proc"));
// Those processes' pids (a Flatpak's too: its pids are visible from outside); `limit` 0 = all of them.
QList<qint64> mainPids(const QString &procDir = QStringLiteral("/proc"), int limit = 0);
}
