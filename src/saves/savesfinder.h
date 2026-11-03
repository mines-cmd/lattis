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
    QString prefix;     // its Proton prefix (…/pfx); empty = it runs natively
    QString saveFolder; // the user's own paths, `~` allowed
    QStringList savePaths;
};

enum class Match { None, SteamId, Title, Chosen };

struct Found
{
    QString gameId; // the first copy's
    QString matched; // the manifest game's name
    Match match = Match::None;
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
