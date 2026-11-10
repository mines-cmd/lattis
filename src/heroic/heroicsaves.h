#pragma once

#include <QString>
#include <QStringList>

struct HeroicSavesInfo
{
    // Empty when neither the game's config nor Heroic's defaults name one.
    QString prefix;
    // The folder under drive_c/users: Proton's own user, or the system user for Wine.
    QString wineUser;
    // Epic's CloudSaveFolder as Legendary has it, variables unexpanded; empty when its metadata has none.
    QString cloudSaveFolder;
    // Heroic's autoSyncSaves for the game (its own setting over the default): it may download Epic's copy on launch.
    bool autoSync = false;
    // installed.json says the game is a native Linux build: no prefix.
    bool native = false;
    QStringList warnings;

    bool operator==(const HeroicSavesInfo &) const = default;
};

namespace HeroicSaves {
// Reads only; `systemUser` is the user a non-Proton Wine prefix is made for.
HeroicSavesInfo read(const QString &root, const QString &appName, const QString &systemUser);

// Epic's folder with its variables filled in, as one absolute glob. Empty when a variable can't be
// resolved: one under the prefix with no prefix, or one Legendary doesn't have.
QString expandCloudSaveFolder(const QString &folder, const QString &prefix, const QString &wineUser,
                              const QString &installDir);
}
