#pragma once

#include "steamlocate.h"
#include "steampresets.h"
#include "steamshortcuts.h"
#include "steamupdate.h"

#include <QHash>
#include <QList>
#include <QString>
#include <QStringList>

// See docs/code/steam.md.
struct SteamStamp
{
    qint64 mtimeMs = 0;
    qint64 size = -1;

    bool operator==(const SteamStamp &) const = default;
};

struct SteamApp
{
    qint64 appId = 0;
    QString name;
    QString installDir;
    bool installed = false;
    qint64 sizeBytes = 0;
    SteamUpdate update = SteamUpdate::None;
    qint64 updateBytes = 0;
    int updatePercent = -1;     // as Steam shows it; -1 while update is None, or when Steam wrote no sizes

    bool operator==(const SteamApp &) const = default;
};

struct SteamAppConfig
{
    int playMinutes = -1;  // Playtime; -1 when Steam has none (never played on this account)
    qint64 lastPlayed = 0;
    QString launchOptions;

    bool operator==(const SteamAppConfig &) const = default;
};

struct CompatTool
{
    QString name;
    QString displayName;

    bool operator==(const CompatTool &) const = default;
};

struct SteamManifest
{
    SteamStamp stamp;
    SteamApp app;
    bool ok = false;

    bool operator==(const SteamManifest &) const = default;
};

struct SteamState
{
    bool enabled = false;
    SteamInstall install;

    SteamStamp libraryFoldersStamp;
    QStringList libraries;
    QHash<QString, SteamManifest> manifests;

    QString localConfigPath;
    SteamStamp localConfigStamp;
    QHash<qint64, SteamAppConfig> apps;
    bool localConfigOk = false;

    QString shortcutsPath;
    SteamStamp shortcutsStamp;
    QList<SteamShortcut> shortcuts;
    // shortcuts.vdf was read, or doesn't exist (no shortcuts yet): `shortcuts` is what Steam has
    bool shortcutsOk = false;

    SteamStamp configStamp;
    bool configOk = false;
    // CompatToolMapping: app id → the tool's internal name. App id 0 is the default for
    // every game ("Enable Steam Play for all other titles").
    QHash<qint64, QString> compatTools;

    QStringList toolDirs;
    QHash<QString, SteamStamp> toolFiles;
    QList<CompatTool> customTools;

    QStringList warnings;

    QList<SteamApp> games() const;
    QList<CompatTool> tools() const;
    SteamAppConfig shortcutConfig(quint32 appId) const;
    QStringList watchPaths() const;

    bool operator==(const SteamState &) const = default;
};

namespace SteamScan {
SteamState scan(const SteamSettings &settings, const SteamLocations &where, const SteamState &previous);

bool isBlocked(qint64 appId, const QString &name);

QString valveToolName(qint64 appId, const QString &name);
}
