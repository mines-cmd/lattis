#pragma once

#include <QSet>
#include <QString>

// Lowercase ASCII `a-z`, `0-9` and `-`: ids name files (library/games/<id>.toml, art/<id>/),
// which must match on case-insensitive FAT/exFAT drives and look the same to every program.
// See docs/code/library.md.
namespace GameId {
QString slug(const QString &text);

bool isValid(const QString &id);

QString forTitle(const QString &platform, const QString &title, const QSet<QString> &taken);

QString forSteamApp(qint64 appId);

qint64 steamAppId(const QString &id);
}
