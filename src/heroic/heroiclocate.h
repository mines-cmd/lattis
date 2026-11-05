#pragma once

#include <QString>
#include <QStringList>

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
}
