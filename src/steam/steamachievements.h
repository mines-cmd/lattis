#pragma once

#include "steamlocate.h"

#include <QDateTime>
#include <QList>
#include <QString>

// See docs/code/steam.md.
namespace SteamAchievements {
struct Achievement
{
    QString apiName;
    QString name;
    QString description;
    bool hidden = false;
    bool unlocked = false;
    QDateTime unlockTime;
    QString iconUrl; // the colored icon when unlocked, the gray one when locked
    QString iconFallbackUrl; // the same file on the older host, which some games' icons are only on
};

// Unlocked first (newest first), then locked in the schema's order. Empty when there is no
// Steam user, no schema file or no achievements in it.
QList<Achievement> readAchievements(const SteamInstall &install, qint64 appId, const QString &language);

// The language Steam itself runs in, "english" when it can't be read.
QString steamLanguage(const SteamInstall &install, const QString &home);
}
