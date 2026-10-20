#pragma once

#include "parsed.h"

#include <QByteArray>
#include <QString>

// See docs/code/launch.md.

struct LaunchSettings
{
    enum class WhileRunning { Stay, Minimize };

    // Hyprland has no minimize, so it stays.
    WhileRunning whileRunning = WhileRunning::Stay;
    int minSessionSeconds = 30;

    bool operator==(const LaunchSettings &) const = default;
};

Parsed<LaunchSettings> parseLaunchSettings(const QByteArray &bytes, const QString &displayPath);
