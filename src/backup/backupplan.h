#pragma once

#include "backuparchive.h"
#include "besiderom.h"
#include "emulators.h"
#include "game.h"
#include "libraryfiles.h"
#include "parsed.h"
#include "protonsaves.h"

#include <QByteArray>
#include <QDateTime>
#include <QHash>
#include <QList>
#include <QString>
#include <QStringList>

// See docs/code/backup.md.
namespace BackupPlan {
// Bumped when an archive's layout changes in a way an older app can't restore.
constexpr int kFormat = 1;

struct Settings
{
    bool credentials = true;
    bool keys = true;
    bool proton = true;

    bool operator==(const Settings &) const = default;
};

Parsed<Settings> parseSettings(const QByteArray &bytes, const QString &displayPath);

bool saveSetting(const QString &settingsFile, const QString &key, bool value, QString *error = nullptr);

// `bytes` is -1 until measure().
struct Place
{
    QString path;
    bool exists = false;
    qint64 bytes = -1;

    bool operator==(const Place &) const = default;
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
};

struct SavesPlan
{
    QString home;
    LibraryInfo library;
    bool keys = true;
    bool proton = true;
    QList<EmulatorPart> emulators;
    QList<ProtonPart> protonGames;

    QList<Platform> platforms;
    QList<Game> games;
    QList<EmulatorDef> definitions;
    bool measured = false;

    qint64 bytes() const; // what the backup takes, as chosen; -1 until measured
};

SavesPlan planSaves(const Sources &sources, const Settings &settings);
void measure(SavesPlan &plan);

QByteArray manifest(const LibraryPlan &plan, const QDateTime &created, const QString &appVersion);
QByteArray manifest(const SavesPlan &plan, const QDateTime &created, const QString &appVersion);

QString fileName(const QString &kind, const QDate &date);

QString tilde(const QString &path, const QString &home);
}
