#pragma once

#include <QMap>
#include <QString>
#include <QVariantMap>

// See docs/code/core.md.
namespace GamePageMigration {
// The `details` keys the game page's content used to be set by, each with the [game_page] key it became.
const QMap<QString, QString> &legacyKeys();

// Copies what the style set into [game_page] (not over a value already there) the first time, then
// leaves a marker file. Returns whether it ran; on a write error it stops without the marker, so the
// next start tries again.
bool run(const QString &settingsFile, const QString &markerFile, const QVariantMap &legacy, QString *error);
}
