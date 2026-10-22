#pragma once

#include "parsed.h"

#include <QHash>
#include <QMap>
#include <QSet>
#include <QStringList>
#include <QVariantMap>

#include <span>

// See docs/code/library.md.

namespace EmulatorFields {
enum class Type { String, StringList, PathTable };

struct Field
{
    const char *key;
    Type type;
    const char *description;
};

std::span<const Field> all();
const Field *find(const QString &key);
QStringList keys();
QString typeName(Type type);

QStringList installTypes();
}

struct EmulatorEntry
{
    QString id;
    QVariantMap values;
    QString at;

    bool operator==(const EmulatorEntry &) const = default;
};

struct EmulatorDef
{
    QString id;
    QString name;
    QStringList platforms;
    QString flatpak;
    QStringList executables;
    QString appimage;
    QString release;
    QString releaseAsset;
    QString releaseUnofficial;
    QString releaseInside;
    QString path;
    QStringList command;
    QStringList args;
    QStringList extensions;
    QStringList files;
    QString portable;
    QMap<QString, QStringList> saves;
    QMap<QString, QStringList> keys;
    QStringList savesBesideRom;

    bool preset = false;
    QStringList userKeys;

    bool operator==(const EmulatorDef &) const = default;
};

namespace Emulators {
Parsed<QList<EmulatorEntry>> parseFile(const QByteArray &bytes, const QString &displayPath);

Parsed<EmulatorEntry> parsePreset(const QByteArray &bytes, const QString &displayPath);

const QList<EmulatorEntry> &presets();

QList<EmulatorDef> merge(const QList<EmulatorEntry> &presets, const QList<EmulatorEntry> &user,
                         QStringList *warnings);

const EmulatorDef *find(const QList<EmulatorDef> &emulators, const QString &id);

QString referenceText(const QList<EmulatorDef> &emulators);

// Callers skip this while the library is read-only.
bool writeReference(const QString &dataDir, const QList<EmulatorDef> &emulators);

QString expandHome(const QString &path, const QString &home);
}

struct EmulatorInstall
{
    enum class Kind { None, Path, Flatpak, Executable, AppImage };

    Kind kind = Kind::None;
    QString program;
    bool pathMissing = false;
    bool portable = false;
    QStringList filesystems;

    bool found() const { return kind != Kind::None; }
    QString describe(const QString &home) const;
    QString installType() const;

    bool operator==(const EmulatorInstall &) const = default;
};

struct InstallLocations
{
    QString home;
    QStringList pathDirs;
    QStringList flatpakDirs;
    QHash<QString, QString> xdgDirs;
    QStringList applicationsDirs;

    static InstallLocations system();
};

namespace Emulators {
QSet<QString> installedFlatpaks(const QStringList &flatpakDirs);

EmulatorInstall detect(const EmulatorDef &emulator, const InstallLocations &where,
                       const QSet<QString> &flatpaks);

QStringList flatpakFilesystems(const QString &appId, const QStringList &flatpakDirs);

bool flatpakCanRead(const QStringList &filesystems, const QString &folder, const InstallLocations &where);

QHash<QString, EmulatorInstall> detectAll(const QList<EmulatorDef> &emulators, const InstallLocations &where);

QStringList installPaths(const QMap<QString, QStringList> &table, const EmulatorInstall &install,
                         const QString &home);

QString choose(const QString &pinned, const QStringList &preferred, const QHash<QString, EmulatorInstall> &installs);
}
