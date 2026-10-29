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

double similarity(const QString &a, const QString &b);

// names[i] is game i's name and alternative names. tieBreak gets the games tied on the same key
// and returns the one that fits, or -1.
int clearMatch(const QList<QStringList> &names, const QString &title,
               const std::function<int(const QList<int> &)> &tieBreak = {});
}
