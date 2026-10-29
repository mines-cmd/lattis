#pragma once

#include <QList>
#include <QString>
#include <QStringList>

#include <functional>

// See docs/code/metadata.md.
namespace NameMatch {
constexpr double kMinScore = 0.92;
constexpr double kMinGap = 0.08;

QString compactKey(const QString &name);

// "TriForce Heroes" -> "Tri Force Heroes": a space before each capital that follows a lowercase letter.
QString splitCamelCase(const QString &title);

// "Yakuza Kiwami 2 (Legacy)" -> "Yakuza Kiwami 2". Unchanged when nothing else would be left.
QString withoutTrailingParenthetical(const QString &title);

// "Premium Edition", "Complete Edition", "Game of the Year Edition": a trailing edition name of up to four words.
// Unchanged when nothing else would be left.
QString withoutEdition(const QString &title);

// The title as IGDB would spell the game: no trailing "(…)", Demo or Beta tag, no "Part" before a sequel number,
// and GOTY spelled out.
QString baseTitle(const QString &title);

// "Welcome Back To 2007 Part II" -> "Welcome Back To 2007 II": IGDB's search needs every word, and it spells the sequel "2".
QString withoutPartWord(const QString &title);

// "Conan Exiles - Public Beta Client" -> "Conan Exiles": a trailing Demo, Beta, Public Test, Playtest,
// Single Player, Friend's Pass or test/dedicated server. Unchanged when nothing else would be left.
QString withoutDemoSuffix(const QString &title);

double similarity(const QString &a, const QString &b);

// names[i] is game i's name and alternative names. tieBreak gets the games tied on the same key
// and returns the one that fits, or -1.
int clearMatch(const QList<QStringList> &names, const QString &title,
               const std::function<int(const QList<int> &)> &tieBreak = {});
}
