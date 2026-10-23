#include "namematch.h"
#include "searchtext.h"

#include <QHash>

#include <algorithm>

namespace {
int romanValue(const QString &word)
{
    static const QStringList kRoman = {QString(),        QStringLiteral("i"),     QStringLiteral("ii"),
                                       QStringLiteral("iii"),   QStringLiteral("iv"),    QStringLiteral("v"),
                                       QStringLiteral("vi"),    QStringLiteral("vii"),   QStringLiteral("viii"),
                                       QStringLiteral("ix"),    QStringLiteral("x"),     QStringLiteral("xi"),
                                       QStringLiteral("xii"),   QStringLiteral("xiii"),  QStringLiteral("xiv"),
                                       QStringLiteral("xv"),    QStringLiteral("xvi"),   QStringLiteral("xvii"),
                                       QStringLiteral("xviii"), QStringLiteral("xix"),   QStringLiteral("xx")};
    // "I" and "X" alone are as often a name or a letter as a number (Mega Man X, Pokémon X).
    const qsizetype n = kRoman.indexOf(word);
    return n >= 2 && word != QLatin1String("x") ? int(n) : 0;
}

QStringList normalizedWords(const QString &name)
{
    QStringList words = SearchText::fold(name).split(u' ', Qt::SkipEmptyParts);
    if (words.size() > 1 && (words.first() == QLatin1String("the") || words.first() == QLatin1String("a")
                             || words.first() == QLatin1String("an")))
        words.removeFirst();
    QStringList out;
    for (QString &word : words) {
        if (word == QLatin1String("and"))
            continue;
        if (word == QLatin1String("versus"))
            word = QStringLiteral("vs");
        else if (const int n = romanValue(word))
            word = QString::number(n);
        out.append(word);
    }
    return out;
}

// A sequel number is never "nearly" another one.
QStringList numbers(const QString &name)
{
    QStringList out;
    for (const QString &word : normalizedWords(name)) {
        if (std::all_of(word.cbegin(), word.cend(), [](QChar c) { return c.isDigit(); }))
            out.append(word);
    }
    return out;
}
}

namespace NameMatch {
QString compactKey(const QString &name)
{
    return normalizedWords(name).join(QString());
}

QString splitCamelCase(const QString &title)
{
    QString out;
    out.reserve(title.size() + 4);
    for (qsizetype i = 0; i < title.size(); ++i) {
        if (i > 0 && title.at(i).isUpper() && title.at(i - 1).isLower())
            out += u' ';
        out += title.at(i);
    }
    return out;
}

double similarity(const QString &a, const QString &b)
{
    if (a.size() < 2 || b.size() < 2)
        return a == b ? 1.0 : 0.0;
    QHash<QString, int> pairs;
    for (qsizetype i = 0; i + 1 < a.size(); ++i)
        ++pairs[a.mid(i, 2)];
    int shared = 0;
    for (qsizetype i = 0; i + 1 < b.size(); ++i) {
        if (const auto it = pairs.find(b.mid(i, 2)); it != pairs.end() && it.value() > 0) {
            --it.value();
            ++shared;
        }
    }
    return 2.0 * shared / double((a.size() - 1) + (b.size() - 1));
}

int clearMatch(const QList<QStringList> &names, const QString &title, const std::function<int(const QList<int> &)> &tieBreak)
{
    const QString key = compactKey(title);
    if (key.isEmpty())
        return -1;

    QList<int> same;
    for (int i = 0; i < names.size(); ++i) {
        if (std::any_of(names.at(i).cbegin(), names.at(i).cend(), [&](const QString &n) { return compactKey(n) == key; }))
            same.append(i);
    }
    if (same.size() == 1)
        return same.first();
    if (same.size() > 1)
        return tieBreak ? tieBreak(same) : -1;

    const QStringList titleNumbers = numbers(title);
    double best = 0;
    double next = 0;
    int bestIndex = -1;
    for (int i = 0; i < names.size(); ++i) {
        double score = 0;
        for (const QString &n : names.at(i)) {
            if (numbers(n) == titleNumbers)
                score = std::max(score, similarity(key, compactKey(n)));
        }
        if (score > best) {
            next = best;
            best = score;
            bestIndex = i;
        } else if (score > next) {
            next = score;
        }
    }
    return best >= kMinScore && best - next >= kMinGap ? bestIndex : -1;
}
}
