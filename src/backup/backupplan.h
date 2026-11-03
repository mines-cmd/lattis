#pragma once

#include "backuparchive.h"
#include "besiderom.h"
#include "emulators.h"
#include "game.h"
#include "libraryfiles.h"
#include "parsed.h"
#include "protonsaves.h"
#include "savesfinder.h"

#include <QByteArray>
#include <QDateTime>
#include <QHash>
#include <QList>
#include <QString>
#include <QStringList>

// See docs/code/backup.md.
namespace BackupPlan {
// Bumped when an archive's layout changes in a way an older app can't restore.
// 2: a saves archive can hold games/<id>/ parts. A library archive has no such parts, so older apps
// still open it: it stays at 1.
constexpr int kFormat = 2;
constexpr int kLibraryFormat = 1;

struct Settings
{
    bool credentials = true;
    bool keys = true;
    bool proton = true;
    // Rows the user unchecked: "saves:<emulator>", "keys:<emulator>", "proton:<game>".
    QStringList ignored;
    // Automatic save backups; an empty folder is the default one.
    QString autoFolder;
    int autoKeep = 5;
    int autoMaxMb = 1024;

    bool operator==(const Settings &) const = default;
};

Parsed<Settings> parseSettings(const QByteArray &bytes, const QString &displayPath);

bool saveSetting(const QString &settingsFile, const QString &key, bool value, QString *error = nullptr);
// [backup] auto_folder and auto_keep: the default (an empty folder, 5) is the key's absence.
bool saveAutoFolder(const QString &settingsFile, const QString &folder, QString *error = nullptr);
bool saveAutoKeep(const QString &settingsFile, int keep, QString *error = nullptr);
bool saveIgnored(const QString &settingsFile, const QStringList &ignored, QString *error = nullptr);

// `bytes` is -1 until measure().
struct Place
{
    QString path;
    bool exists = false;
    qint64 bytes = -1;

    bool operator==(const Place &) const = default;
};

// A game's copies for the saves finder; the first is the group's main one.
struct GameQueries
{
    QString id;
    QString title;
    QList<SavesFinder::Query> copies;
};

struct Sources
{
    QString dataDir;
    QString home;
    LibraryInfo library;
    QList<Platform> platforms;
    QList<Game> games;
    QList<EmulatorDef> emulators;
    QHash<QString, EmulatorInstall> installs;
    QString compatdata;
    QList<GameQueries> saveGames;
    SavesFinder::Env savesEnv;
    QString manifestDir;
};

struct LibraryPlan
{
    QString dataDir;
    int games = 0;
    LibraryInfo library;
    bool credentials = true;
    Place credentialsFile;
    QList<Place> roms;
    qint64 dataBytes = -1;

    // What the backup takes, as chosen; -1 until measured.
    qint64 bytes() const;

    QStringList excluded() const;
    Archive::Skip skip() const;
};

LibraryPlan planLibrary(const Sources &sources, const Settings &settings);
void measure(LibraryPlan &plan);

struct EmulatorPart
{
    enum class Skipped {
        No,
        NotInstalled,
        NoSaves,
    };

    QString id;
    QString name;
    QString install;
    QString describe;
    Skipped skipped = Skipped::No;
    QList<Place> saves;
    QList<Place> keys;
    QList<BesideRom::GameSaves> besideRoms;
    bool besideRomSaves = false;
    bool measured = false;
    bool takeSaves = true;
    bool takeKeys = true;

    // What the backup takes of it with keys chosen or not; -1 until measured.
    qint64 bytes(bool withKeys) const;
    bool takesAnything(bool withKeys) const;
};

struct ProtonPart
{
    QString gameId;
    QString title;
    ProtonSaves::GameSaves saves;
    QString proton;
    bool measured = false;
    bool take = true;
    // Taken as the game's matched saves instead (a GamePart), so the archive never holds it twice.
    bool superseded = false;
};

// A game whose saves the manifest or its own paths found.
struct GamePart
{
    QString gameId;
    QString title;
    QString matched;
    SavesFinder::Match match = SavesFinder::Match::None;
    QList<Place> places;
    bool measured = false;
    bool take = true;
    bool tooBig = false;

    // What the backup takes of it; -1 until measured.
    qint64 bytes() const;
    bool taken() const { return take && !tooBig; }
};

struct SavesPlan
{
    QString home;
    LibraryInfo library;
    bool keys = true;
    bool proton = true;
    QList<EmulatorPart> emulators;
    QList<ProtonPart> protonGames;
    QList<GamePart> savedGames; // found by measure()
    // Whether the saves manifest was there when measure() looked for them.
    bool hasManifest = false;
    QStringList ignored;
    qint64 maxBytes = qint64(1024) << 20;

    QList<GameQueries> findGames;
    SavesFinder::Env savesEnv;
    QString manifestDir;

    QList<Platform> platforms;
    QList<Game> games;
    QList<EmulatorDef> definitions;
    bool measured = false;

    qint64 bytes() const; // what the backup takes, as chosen; -1 until measured
};

SavesPlan planSaves(const Sources &sources, const Settings &settings);
// Sets keys, proton and each row's take flags from the choices.
void applyChoices(SavesPlan &plan, const Settings &settings);
void measure(SavesPlan &plan);

QByteArray manifest(const LibraryPlan &plan, const QDateTime &created, const QString &appVersion);
QByteArray manifest(const SavesPlan &plan, const QDateTime &created, const QString &appVersion);

QString fileName(const QString &kind, const QDate &date);

QString tilde(const QString &path, const QString &home);
}
