#pragma once

#include "game.h"
#include "parsed.h"

#include <QByteArray>
#include <QHash>
#include <QList>
#include <QSet>
#include <QString>
#include <QStringList>

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

    bool operator==(const CollectionRules &) const = default;
};

struct Collection
{
    QString id;
    QString name;
    std::optional<CollectionRules> rules;
    QStringList added;
    QStringList removed;
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
// rule's tags that aren't in it (a deleted tag) are ignored, as the tag filter ignores them.
class CollectionMatcher
{
public:
    explicit CollectionMatcher(const Collection &collection, const QSet<QString> *knownTags = nullptr);
    bool isMember(const Game &game) const;

private:
    CollectionRules m_rules;
    bool m_hasRules = false;
    QSet<QString> m_added;
    QSet<QString> m_removed;
};

bool isMember(const Game &game, const Collection &collection);
