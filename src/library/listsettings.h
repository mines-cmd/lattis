#pragma once

#include "parsed.h"

#include <QByteArray>
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

    bool operator==(const ListSettings &) const = default;
};

const QStringList &listGroupings();
const QStringList &listStatuses();

Parsed<ListSettings> parseListSettings(const QByteArray &bytes, const QString &displayPath);

// An empty value means the key has its default and should be removed.
QList<std::pair<QString, QString>> listSettingsChanges(const ListSettings &from, const ListSettings &to);
