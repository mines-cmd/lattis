#pragma once

#include "game.h"
#include "parsed.h"

#include <QByteArray>
#include <QHash>
#include <QList>
#include <QSet>
#include <QString>
#include <QStringList>

#include <functional>
#include <optional>
#include <utility>

// See docs/code/library.md.
struct CollectionRules
{
    QString platform;
    bool installedOnly = false;
    bool favoritesOnly = false;
    QStringList statuses;
    QStringList tags;
    bool matchAllTags = false;
    QStringList steamOwnership;
    int yearFrom = 0; // 0 = no bound
    int yearTo = 0;
    double minRating = -1; // 0-5 in half steps, -1 = off
    QStringList playTimes;
    QString lastPlayed; // "" = any
    QStringList achievements;

    bool operator==(const CollectionRules &) const = default;
};

struct Collection
{
    QString id;
    QString name;
    std::optional<CollectionRules> rules;
    QStringList added;
    QStringList removed;
    // Empty = the list's current sort / grouping stays while the collection is open.
    QString sort;
    bool sortDescending = false;
    QString groupBy;
    // The id Steam gave it, for an imported collection.
    QString steamId;

    bool operator==(const Collection &) const = default;
};

Parsed<QList<Collection>> parseCollections(const QByteArray &bytes, const QString &displayPath);

// The [[collection]] block's keys as TOML text, for TomlPatch. `remove` lists keys the collection
// doesn't use, which a block written earlier may still have.
struct CollectionValues
{
    QList<std::pair<QString, QString>> set;
    QStringList remove;
};
CollectionValues collectionValues(const Collection &collection);

// Membership of many games in one collection: the id lists are looked up in sets. With `knownTags`, a
// rule's tags that aren't in it (a deleted tag) are ignored; a rule whose tags are all gone matches no game
// (the games added by hand still belong), so a collection never turns into every game.
//
// `achievementState` answers a game's achievements state (not_started, in_progress, completed, none).
// Without it every game is "none", which is also what a Steam game's count not read yet counts as.
class CollectionMatcher
{
public:
    using AchievementState = std::function<QString(const Game &)>;
    explicit CollectionMatcher(const Collection &collection, const QSet<QString> *knownTags = nullptr,
                               AchievementState achievementState = {});
    bool isMember(const Game &game) const;

private:
    CollectionRules m_rules;
    bool m_hasRules = false;
    bool m_tagsGone = false;
    QSet<QString> m_added;
    QSet<QString> m_removed;
    AchievementState m_achievementState;
};

bool isMember(const Game &game, const Collection &collection);
