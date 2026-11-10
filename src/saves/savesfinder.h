#pragma once

#include "savesindex.h"

#include <QHash>
#include <QList>
#include <QString>
#include <QStringList>

// See docs/code/saves.md.
namespace SavesFinder {

struct Env
{
    QString home;
    QString xdgData;
    QString xdgConfig;
    QString user;
    QString steamRoot; // empty = <root> paths are skipped

    static Env system();
};

struct Query
{
    QString id;
    QString title;
    int steamAppId = 0; // 0 = none
    QString savesGame;  // the manifest name the user chose
    QString baseDir;    // the game's install folder
    QString prefix;     // its Wine prefix (a Proton one ends in /pfx); empty = it runs natively
    QString wineUser;   // the prefix's user folder; empty = steamuser
    QString saveFolder; // the user's own paths, `~` allowed
    QStringList savePaths;
    QStringList storeFolders; // the store's own save folders, absolute globs already expanded; used instead of the manifest
    // An Epic game run by Heroic: find() reads its prefix, user and Epic's folder from this root, where the
    // worker thread is, and fills the three fields above.
    QString heroicRoot;
    QString heroicApp;
};

enum class Match { None, SteamId, Title, Chosen, Epic };

struct Found
{
    QString gameId; // the first copy's
    QString matched; // the manifest game's name
    Match match = Match::None;
    bool cloudSync = false; // an Epic copy has Heroic's autoSyncSaves on
    QStringList paths; // existing, absolute, none inside another
};

class Matcher
{
public:
    explicit Matcher(const SavesIndex &index);

    const SavesGame *find(const Query &query, Match *how = nullptr) const;
    const SavesIndex &index() const { return m_index; }

private:
    const SavesIndex &m_index;
    QHash<int, qsizetype> m_steam;
    QHash<QString, qsizetype> m_names;
    QHash<QString, qsizetype> m_exact;
};

// The first copy is the group's main one; all copies' paths are found together.
Found find(const Matcher &matcher, const QList<Query> &copies, const Env &env);
}
