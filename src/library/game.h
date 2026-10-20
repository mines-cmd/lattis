#pragma once

#include "steamupdate.h"

#include <QDate>
#include <QDateTime>
#include <QList>
#include <QString>
#include <QUrl>

struct GameTag
{
    QString name;
    int color = 0;
};

// See docs/code/library.md.
struct Game
{
    QString id;
    QString title;
    QString platform;
    QString platformName;
    QString description;
    int year = 0;
    int playMinutes = 0;
    QDateTime lastPlayed;
    QDate added;
    double rating = -1; // 0-5, -1 = unknown
    bool ratingFromIgdb = false;
    bool favorite = false;
    QString status;
    double logoScale = 1;
    QString logoPosition;
    double logoOffsetX = 0;
    double logoOffsetY = 0;
    QString logoScrim;
    QString logoScrimSide;
    double logoScrimStrength = 1;
    bool hidden = false;
    QList<GameTag> tags;

    QString emulator;
    QStringList args;

    QString launchOptions;
    QString proton;

    bool rom = false;
    bool romKey = false;
    QString romPath;

    // Not in the file: sources fill these in.
    qint64 sizeBytes = 0;
    bool installed = true;
    bool pendingSteamEdits = false;

    SteamUpdate steamUpdate = SteamUpdate::None;
    qint64 updateBytes = 0;
    int updatePercent = -1; // -1: unknown

    QString installDir;
    int steamPlayMinutes = -1; // -1: Steam has none (never played on this account)
    QDateTime steamLastPlayed;

    QString exe;
    quint32 shortcut = 0;
    bool uninstalled = false;

    QUrl heroUrl;
    QUrl logoUrl;
    QUrl iconUrl;
    QUrl coverUrl;
};
