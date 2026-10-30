#pragma once

#include "game.h"
#include "parsed.h"

#include <QList>
#include <QString>
#include <QStringList>
#include <QVariant>

#include <functional>

// See docs/code/library.md.
struct ModManager
{
    QString id;
    QString name;
    QString exe;
    QString startDir;
    QString launchOptions;
    QStringList games;
    // "game" or "own"; Proton managers only. Empty = decided by the games linked.
    QString prefix;
    QString proton;
    quint32 shortcut = 0;

    // A Windows program runs through Proton as a Steam shortcut; anything else is run directly.
    bool isProton() const { return exe.endsWith(QLatin1String(".exe"), Qt::CaseInsensitive); }

    bool operator==(const ModManager &) const = default;
};

namespace ModManagers {
inline const QString kFolder = QStringLiteral("library/mod-managers/");

QStringList keys();
// Returns an invalid QVariant and sets `error` for a value the key doesn't accept.
QVariant validate(const QString &key, const QVariant &value, QString *error);
QString format(const QString &key, const QVariant &value);
void apply(ModManager &manager, const QString &key, const QVariant &value);

Parsed<ModManager> parse(const QByteArray &bytes, const QString &displayPath);

// A new manager's file: a comment on top, then the keys it has.
QString formatNew(const ModManager &manager);

// Only these platforms can be linked: the launcher knows their prefix or runs them itself.
bool linkable(const QString &platform);

struct LinkedGame
{
    QString id;
    QString title;
    // Not in the library, hidden, not installed, or on a platform that can't be linked.
    bool missing = false;
};

QList<LinkedGame> linkedGames(const ModManager &manager, const std::function<const Game *(const QString &)> &find);
}
