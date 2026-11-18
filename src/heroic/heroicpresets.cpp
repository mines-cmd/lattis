#include "heroicpresets.h"

#include <QJsonObject>

namespace HeroicPresets {
namespace {
const QString kCommand = QStringLiteral("%command%");

// Heroic has a switch for these two instead of a wrapper entry.
enum class Switch { None, MangoHud, GameMode };

Switch switchOf(const SteamPreset &preset)
{
    if (!preset.builtIn)
        return Switch::None;
    return preset.id == u"mangohud" ? Switch::MangoHud : preset.id == u"gamemode" ? Switch::GameMode : Switch::None;
}

QString wrapperArgs(const SteamPreset &preset)
{
    return preset.wrapper.mid(1).join(u' ');
}

bool isEnv(const QJsonValue &entry, const QString &key, const QString &value)
{
    const QJsonObject o = entry.toObject();
    return o.value(QStringLiteral("key")).toString() == key && o.value(QStringLiteral("value")).toString() == value;
}

bool isWrapper(const QJsonValue &entry, const SteamPreset &preset)
{
    const QJsonObject o = entry.toObject();
    return o.value(QStringLiteral("exe")).toString() == preset.wrapper.value(0)
        && o.value(QStringLiteral("args")).toString().trimmed() == wrapperArgs(preset);
}

// A preset's arguments go after Heroic's launcher arguments, which LaunchOptions handles as the words after
// %command%.
QString asOptions(const QString &launcherArgs)
{
    return launcherArgs.trimmed().isEmpty() ? kCommand : kCommand + u' ' + launcherArgs.trimmed();
}

QString fromOptions(QString options)
{
    options = options.trimmed();
    if (options.startsWith(kCommand))
        options.remove(0, kCommand.size());
    return options.trimmed();
}

SteamPreset argsOnly(const SteamPreset &preset)
{
    SteamPreset out;
    out.args = preset.args;
    return out;
}
}

bool isOn(const HeroicGameSettings &settings, const SteamPreset &preset)
{
    switch (switchOf(preset)) {
    case Switch::MangoHud:
        return settings.showMangohud;
    case Switch::GameMode:
        return settings.useGameMode;
    case Switch::None:
        break;
    }
    if (preset.isEmpty())
        return false;
    for (const auto &[key, value] : preset.env) {
        bool found = false;
        for (const QJsonValue &entry : settings.environment)
            found = found || isEnv(entry, key, value);
        if (!found)
            return false;
    }
    if (!preset.wrapper.isEmpty()) {
        bool found = false;
        for (const QJsonValue &entry : settings.wrappers)
            found = found || isWrapper(entry, preset);
        if (!found)
            return false;
    }
    return preset.args.isEmpty() || LaunchOptions::isOn(asOptions(settings.launcherArgs), argsOnly(preset));
}

void turnOn(HeroicGameSettings &settings, const SteamPreset &preset)
{
    switch (switchOf(preset)) {
    case Switch::MangoHud:
        settings.showMangohud = true;
        return;
    case Switch::GameMode:
        settings.useGameMode = true;
        return;
    case Switch::None:
        break;
    }
    for (const auto &[key, value] : preset.env) {
        bool found = false;
        for (const QJsonValue &entry : settings.environment)
            found = found || isEnv(entry, key, value);
        if (!found)
            settings.environment.append(QJsonObject{{QStringLiteral("key"), key}, {QStringLiteral("value"), value}});
    }
    if (!preset.wrapper.isEmpty()) {
        bool found = false;
        for (const QJsonValue &entry : settings.wrappers)
            found = found || isWrapper(entry, preset);
        if (!found)
            settings.wrappers.append(QJsonObject{{QStringLiteral("exe"), preset.wrapper.first()},
                                                 {QStringLiteral("args"), wrapperArgs(preset)}});
    }
    if (!preset.args.isEmpty())
        settings.launcherArgs = fromOptions(LaunchOptions::turnOn(asOptions(settings.launcherArgs), argsOnly(preset)));
}

void turnOff(HeroicGameSettings &settings, const SteamPreset &preset)
{
    switch (switchOf(preset)) {
    case Switch::MangoHud:
        settings.showMangohud = false;
        return;
    case Switch::GameMode:
        settings.useGameMode = false;
        return;
    case Switch::None:
        break;
    }
    for (const auto &[key, value] : preset.env) {
        for (qsizetype i = settings.environment.size() - 1; i >= 0; --i) {
            if (isEnv(settings.environment.at(i), key, value))
                settings.environment.removeAt(i);
        }
    }
    if (!preset.wrapper.isEmpty()) {
        for (qsizetype i = settings.wrappers.size() - 1; i >= 0; --i) {
            if (isWrapper(settings.wrappers.at(i), preset))
                settings.wrappers.removeAt(i);
        }
    }
    if (!preset.args.isEmpty())
        settings.launcherArgs = fromOptions(LaunchOptions::turnOff(asOptions(settings.launcherArgs), argsOnly(preset)));
}
}
