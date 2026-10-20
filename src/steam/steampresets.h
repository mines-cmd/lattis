#pragma once

#include "launchoptions.h"
#include "parsed.h"

#include <QByteArray>
#include <QList>
#include <QString>
#include <QStringList>

#include <utility>

// See docs/code/steam.md.
struct SteamPreset
{
    QString id;
    QString name;
    QString note;
    QList<std::pair<QString, QString>> env;
    QStringList wrapper;
    QStringList args;
    bool builtIn = false;

    bool isEmpty() const { return env.isEmpty() && wrapper.isEmpty() && args.isEmpty(); }
    QString describe() const;
    bool protonOnly() const;

    bool operator==(const SteamPreset &) const = default;
};

struct SteamSettings
{
    bool enabled = true;
    QString path;
    QString user;
    QStringList defaultPresets;
    // The compatibility tool new Proton games start with; empty = the one Steam uses for other
    // titles (CompatToolMapping "0"), else proton_experimental
    QString defaultProton;
    QList<SteamPreset> presets;

    const SteamPreset *preset(const QString &id) const;
    QStringList presetIds() const;

    bool operator==(const SteamSettings &) const = default;
};

QList<SteamPreset> builtInSteamPresets();

Parsed<SteamSettings> parseSteamSettings(const QByteArray &bytes, const QString &displayPath);

namespace LaunchOptions {
bool isOn(const QString &options, const SteamPreset &preset);
QString turnOn(const QString &options, const SteamPreset &preset);
QString turnOff(const QString &options, const SteamPreset &preset);
}
