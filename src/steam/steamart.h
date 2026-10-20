#pragma once

#include "steamlocate.h"

#include <QHash>
#include <QString>

// See docs/code/steam.md.
namespace SteamArt {
QHash<QString, QString> find(const SteamInstall &install, qint64 appId, const QStringList &kinds);

QString gridDir(const SteamInstall &install);
QString gridIconPath(const SteamInstall &install, qint64 appId);

struct GridFile
{
    QString name;
    qint64 size = 0;       // the file as written: once it differs, someone replaced it
    qint64 modified = 0;
    QString art;
    qint64 artSize = 0;
    qint64 artModified = 0;

    bool operator==(const GridFile &) const = default;
};

using GridFiles = QHash<qint64, QHash<QString, GridFile>>;

QStringList writeGrid(const QString &gridDir, const QHash<qint64, QHash<QString, QString>> &art, GridFiles *written);
}
