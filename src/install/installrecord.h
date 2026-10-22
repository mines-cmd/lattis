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
    QString sha256;
    QString assetId;
    QString assetDate;
    QString installed;

    bool operator==(const InstallRecord &) const = default;
};

namespace InstallRecords {
using Records = QMap<QString, InstallRecord>;

QString path(const QString &stateDir);
Parsed<Records> parse(const QByteArray &bytes);
Parsed<Records> read(const QString &path);
bool write(const QString &path, const Records &records, QString *error);

// A file Gear Lever moved, or a Flatpak removed since, stops counting as installed by the app.
bool stillThere(const InstallRecord &record, const QSet<QString> &userFlatpaks);
}
