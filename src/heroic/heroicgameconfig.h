#pragma once

#include <QJsonArray>
#include <QString>

#include <optional>

struct HeroicWine
{
    QString bin;
    QString name;
    // "proton" or "wine".
    QString type;
    // Heroic's own `wineserver` of a Wine build; empty for Proton, and then not written.
    QString wineserver;

    bool operator==(const HeroicWine &) const = default;
};

// The per-game keys Lattis shows. Each is the game's own value, else Heroic's defaultSettings, else empty.
struct HeroicGameSettings
{
    QString launcherArgs;
    bool showMangohud = false;
    bool useGameMode = false;
    // Heroic's `enviromentOptions` (sic) and `wrapperOptions`, as it stores them.
    QJsonArray environment;
    QJsonArray wrappers;
    HeroicWine wine;
};

// The keys to change; the ones left unset are not touched.
struct HeroicGameEdits
{
    std::optional<QString> launcherArgs;
    std::optional<bool> showMangohud;
    std::optional<bool> useGameMode;
    std::optional<QJsonArray> environment;
    std::optional<QJsonArray> wrappers;
    std::optional<HeroicWine> wine;
};

namespace HeroicGameConfig {
QString path(const QString &root, const QString &appName);

// `root` is Heroic's config folder. A game with no file or an empty entry reads as Heroic's defaults. False, with
// `error` set, when the file isn't a JSON object.
bool read(const QString &root, const QString &appName, HeroicGameSettings &out, QString *error = nullptr);

// Writes only the edited keys into the game's file; every other key, its order and the indentation stay. A missing
// file or an empty `{}` entry becomes a full entry (Heroic's defaultSettings plus the edits). Refuses, writing
// nothing, when the existing file isn't a JSON object.
bool write(const QString &root, const QString &appName, const HeroicGameEdits &edits, QString *error = nullptr);
}
