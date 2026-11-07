#pragma once

#include <QDateTime>
#include <QList>
#include <QString>
#include <QVariantMap>

// See docs/code/library.md.
namespace LibraryOverview {
struct Game
{
    QString id;
    QString title;
    QString platform;
    bool hidden = false;
    bool installed = true;
    QString status; // as in the game file; empty = none
    QString groupId; // empty: not linked to other copies
    int playMinutes = 0;
    QDateTime lastPlayed;
    int groupPlayMinutes = 0;
    QDateTime groupLastPlayed;
    QString coverUrl;
};

struct Counted
{
    QString id;
    QString title;
    QString coverUrl;
    QDateTime lastPlayed;
};

struct Totals
{
    int games = 0;
    int installed = 0;
    int platforms = 0;
    qint64 minutes = 0;
    int backlog = 0;
};

struct Overview
{
    Totals totals;
    QList<Counted> continuePlaying;
};

constexpr int kContinueLimit = 7;

Overview overview(const QList<Game> &games);
QVariantMap toVariantMap(const Overview &overview);
} // namespace LibraryOverview
