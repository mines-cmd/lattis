#pragma once

#include "parsed.h"

#include <QByteArray>
#include <QDateTime>
#include <QList>
#include <QString>
#include <QStringList>

#include <utility>

// See docs/code/library.md.
struct ListSettings
{
    QString sort = QStringLiteral("title");
    bool descending = false;
    bool installedOnly = true;
    bool favoritesOnly = false;
    QStringList tags;
    bool matchAllTags = false;
    bool showHidden = false;
    bool showSortValue = true;
    QString groupBy = QStringLiteral("platform");
    QStringList statuses;
    int yearFrom = 0; // 0 = no bound
    int yearTo = 0;
    double minRating = -1; // 0-5 in half steps, -1 = off
    QStringList playTimes;
    QString lastPlayed; // "" = any
    QStringList achievements;
    bool moreOpen = false;

    bool operator==(const ListSettings &) const = default;
};

const QStringList &listGroupings();
const QStringList &listStatuses();
const QStringList &listPlayTimes();
const QStringList &listLastPlayed();
const QStringList &listAchievementStates();

QString playTimeBucket(int minutes);
bool playedWithin(const QString &period, const QDateTime &played, const QDateTime &now);

Parsed<ListSettings> parseListSettings(const QByteArray &bytes, const QString &displayPath);

// An empty value means the key has its default and should be removed.
QList<std::pair<QString, QString>> listSettingsChanges(const ListSettings &from, const ListSettings &to);
