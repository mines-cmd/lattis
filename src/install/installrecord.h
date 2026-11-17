#pragma once

#include "parsed.h"

#include <QMap>
#include <QSet>
#include <QString>

// See docs/code/install.md.

struct InstallRecord
{
    QString route;
    QString file;
    QString tag;
    bool prerelease = false;
    QString sha256;
    QString assetId;
    QString assetDate;
    QString installed;
    QString desktop;
    QString icon;

    bool operator==(const InstallRecord &) const = default;
};

// What Lattis knows about the installed build of an AppImage game (game-updates.toml, one table per game id).
struct GameUpdateRecord
{
    QString file;
    qint64 size = 0;
    QString sha256;
    QString assetId;
    QString assetDate;
    QString tag;
    QString checked;

    bool operator==(const GameUpdateRecord &) const = default;
};

namespace GameUpdateRecords {
using Records = QMap<QString, GameUpdateRecord>;

QString path(const QString &stateDir);
Parsed<Records> parse(const QByteArray &bytes);
Parsed<Records> read(const QString &path);
bool write(const QString &path, const Records &records, QString *error);
}

namespace InstallRecords {
using Records = QMap<QString, InstallRecord>;

QString path(const QString &stateDir);
Parsed<Records> parse(const QByteArray &bytes);
Parsed<Records> read(const QString &path);
bool write(const QString &path, const Records &records, QString *error);

// A file Gear Lever moved, or a Flatpak removed since, stops counting as installed by the app.
bool stillThere(const InstallRecord &record, const QSet<QString> &userFlatpaks);
}
