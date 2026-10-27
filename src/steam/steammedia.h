#pragma once

#include "steamlocate.h"

#include <QDateTime>
#include <QHash>
#include <QList>
#include <QString>

// See docs/code/steam.md.
namespace SteamMedia {
struct Clip
{
    QString id;        // the clip folder's name
    QString folder;
    QString thumbnail;
    QString videoDir;
    qint64 durationMs = 0;
    QDateTime time;
};

struct Screenshot
{
    QString id;   // the file's name
    QString path;
    QDateTime time;
};

struct AppMedia
{
    QList<Clip> clips;
    QList<Screenshot> screenshots;
};

QList<Clip> listClips(const SteamInstall &install, quint64 appId);
QList<Screenshot> listScreenshots(const SteamInstall &install, quint64 appId);

QHash<quint64, AppMedia> listAll(const SteamInstall &install);

qint64 parseDurationMs(const QString &isoDuration);
QString lengthText(qint64 durationMs);

QString remuxClip(const Clip &clip, const QString &ffmpeg, const QString &outPath);
}
