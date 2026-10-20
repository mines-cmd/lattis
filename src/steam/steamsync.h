#pragma once

#include "steamart.h"
#include "steamscan.h"

#include <QHash>
#include <QList>
#include <QString>
#include <QStringList>
#include <QVariant>

#include <functional>
#include <optional>
#include <utility>

// See docs/code/steam.md.
namespace SteamSync {
struct ShortcutRecord
{
    QString title;
    QString exe;
    QString startDir;
    QString launchOptions;
    QString proton;

    bool operator==(const ShortcutRecord &) const = default;
};

struct Record
{
    QString library;
    QHash<qint64, QString> launchOptions;
    QHash<qint64, QString> proton;
    QHash<qint64, ShortcutRecord> shortcuts;
    SteamArt::GridFiles grid;
    QHash<qint64, QString> removed;

    bool operator==(const Record &) const = default;
};

ShortcutRecord recordOf(const SteamShortcut &shortcut, const QString &proton);

QString recordPath(const QString &stateDir, const QString &dataDirKey);

Record readRecord(const QString &path, const QString &library, QStringList *warnings);
bool writeRecord(const QString &path, const Record &record, const QString &dataDir, QString *error);

enum class Side { InSync, Queued, FromSteam };

Side compare(const QString &gameFile, const QString &steam, const std::optional<QString> &record);

struct Game
{
    QString id;
    qint64 appId = 0;
    QString launchOptions;
    QString proton;
};

struct ShortcutGame
{
    QString id;
    quint32 appId = 0;
    QString title;
    QString exe;
    QString startDir;
    QString launchOptions;
    QString proton;
    bool installed = true;
    QString icon;

    QString startFolder() const;
};

inline const QString kAddShortcut = QStringLiteral("add_shortcut");
inline const QString kRemoveShortcut = QStringLiteral("remove_shortcut");
// Its icon field: pointed at the launcher's icon in grid/ while Steam has none, and cleared
// once that file is gone. Not a reason to restart Steam to launch.
inline const QString kShortcutIcon = QStringLiteral("icon");

QString removedKey(quint32 appId);
quint32 removedAppId(const QString &key);

struct Result
{
    QHash<QString, QStringList> queued;
    QHash<QString, QList<std::pair<QString, QVariant>>> copies;
    Record record;
    QStringList log;
};

Result sync(const QList<Game> &games, const SteamState &steam, const Record &record,
            const QList<ShortcutGame> &shortcutGames = {});

QString withDefaults(const QString &launchOptions, const SteamSettings &settings, bool forLinux = false);

struct Edit
{
    qint64 appId = 0;
    QString key;
    QString value;
    QString steam;

    bool operator==(const Edit &) const = default;
};

struct ShortcutEdit
{
    enum class Kind { Add, Update, Remove };
    Kind kind = Kind::Update;
    SteamShortcut shortcut;
    std::optional<SteamShortcut> steam;

    bool operator==(const ShortcutEdit &) const = default;
};

bool sameShortcut(const SteamShortcut &a, const SteamShortcut &b);

struct ApplyResult
{
    QList<Edit> applied;
    QList<Edit> stale;
    QList<ShortcutEdit> shortcutsApplied;
    QList<ShortcutEdit> shortcutsStale;
    QString error;
    QStringList log;
};

ApplyResult apply(const SteamState &steam, const QList<Edit> &edits, const QString &backupDir,
                  const std::function<bool()> &steamRunning, const QList<ShortcutEdit> &shortcuts = {});
}
