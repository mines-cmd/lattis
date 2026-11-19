#pragma once

#include <QByteArray>
#include <QString>
#include <QStringList>

#include <optional>

// See docs/code/heroic.md.
struct EpicInstallInfo
{
    qint64 downloadSize = -1; // bytes; -1 when the answer has none
    qint64 diskSize = -1;
    QString platform;         // the platform the sizes are for; empty when unknown
    bool hasLinux = false;    // Epic has a native Linux build of the game

    bool operator==(const EpicInstallInfo &) const = default;
};

namespace EpicInstall {
// `legendary info <appName> --json`'s object (or one entry of Heroic's cache, which has the same shape).
std::optional<EpicInstallInfo> parseInfo(const QByteArray &json);

// Heroic's store_cache/legendary_install_info.json entry for the game; nothing when it has no sizes.
std::optional<EpicInstallInfo> cachedInfo(const QString &heroicRoot, const QString &appName);

// config.json's defaultSettings.defaultInstallPath, else ~/Games/Heroic.
QString defaultFolder(const QString &heroicRoot);

// The folder Legendary makes inside the install folder: the game's FolderName from Legendary's metadata;
// empty when the game has none cached.
QString gameFolder(const QString &heroicRoot, const QString &appName);

// What `legendary install` is given after the game; Windows unless the info says there is a Linux build.
QString platformFor(const EpicInstallInfo &info);
QStringList installArgs(const QString &folder, const QString &platform);

// Bytes free on the drive that holds the folder, or its nearest existing parent; -1 when that can't be read.
qint64 freeSpace(const QString &folder);

// "~" and "~/..." expanded; an empty folder stays empty.
QString expandHome(const QString &folder);
}
