#pragma once

#include "libraryfiles.h"
#include "steamwebapi.h"

#include <QHash>
#include <QList>
#include <QSet>

// See docs/code/library.md.
namespace SteamWebMerge {
struct Input
{
    QList<IndexEntry> index;
    // The Web API's cache; null or empty: nothing was ever fetched.
    const SteamWebApi::Cache *cache = nullptr;
    QString steamId;
    // Apps with a manifest: they keep their entry when the API stops listing them.
    QSet<qint64> installed;
    // [steam] whole_library
    bool wholeLibrary = true;
};

struct Result
{
    QList<IndexEntry> index;
    bool changed = false;
    QHash<qint64, SteamWebApp> web;
    QHash<QString, QString> names;
    bool active = false;
    // App IDs the merge brought into the index.
    QList<qint64> added;
};

Result merge(const Input &input);
}
