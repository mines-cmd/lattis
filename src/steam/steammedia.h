#pragma once

#include "steamlocate.h"

#include <QDateTime>
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

QList<Clip> listClips(const SteamInstall &install, quint64 appId);
QList<Screenshot> listScreenshots(const SteamInstall &install, quint64 appId);

qint64 parseDurationMs(const QString &isoDuration);
QString lengthText(qint64 durationMs);

QString remuxClip(const Clip &clip, const QString &ffmpeg, const QString &outPath);
}
