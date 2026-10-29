#include "namematch.h"
#include "searchtext.h"

#include <QHash>
#include <QRegularExpression>

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
        if (word == QLatin1String("im")) { // "I'm" folded: the same as "I am"
            out.append(QStringLiteral("i"));
            out.append(QStringLiteral("am"));
            continue;
        }
        if (word == QLatin1String("versus"))
            word = QStringLiteral("vs");
        else if (word == QLatin1String("ep"))
            word = QStringLiteral("episode");
        else if (const int n = romanValue(word))
            word = QString::number(n);
        out.append(word);
    }
    // "Part II" and "2" name the same sequel.
    for (qsizetype i = out.size() - 2; i >= 0; --i) {
        if (out.at(i) == QLatin1String("part")
            && std::all_of(out.at(i + 1).cbegin(), out.at(i + 1).cend(), [](QChar c) { return c.isDigit(); }))
            out.removeAt(i);
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

QString withoutTrailingParenthetical(const QString &title)
{
    const QString trimmed = title.trimmed();
    if (!trimmed.endsWith(u')'))
        return title;
    const qsizetype open = trimmed.lastIndexOf(u'(');
    if (open <= 0)
        return title;
    const QString head = trimmed.left(open).trimmed();
    return head.isEmpty() ? title : head;
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

QString withoutPartWord(const QString &title)
{
    static const QRegularExpression part(QStringLiteral("\\bpart\\s+(?=(?:\\d+|[ivx]+)\\b)"),
                                         QRegularExpression::CaseInsensitiveOption);
    QString out = title;
    out.remove(part);
    return out;
}

QString withoutEdition(const QString &title)
{
    static const QRegularExpression edition(QStringLiteral("[\\s:\\-–]+(?:[\\w']+\\s+){0,3}edition$"),
                                            QRegularExpression::CaseInsensitiveOption);
    QString head = title.trimmed();
    head.remove(edition);
    return head.trimmed().isEmpty() ? title : head.trimmed();
}

// These are the game itself, shared, tested or trimmed down: IGDB lists only the game.
QString withoutDemoSuffix(const QString &title)
{
    static const QRegularExpression suffix(
        QStringLiteral("(?:\\s*[-:–]\\s*|\\s+)(?:friend'?s?\\s+pass|public\\s+(?:beta|test(?:ing)?)(?:\\s+client)?|beta(?:\\s+client)?|unstable"
                       "|demo|playtest|single\\s*player|(?:dedicated|test)\\s+server)$"),
        QRegularExpression::CaseInsensitiveOption);
    QString head = title.trimmed();
    head.remove(suffix);
    return head.trimmed().isEmpty() ? title : head.trimmed();
}

QString baseTitle(const QString &title)
{
    static const QRegularExpression goty(QStringLiteral("\\bgoty\\b"), QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression colon(QStringLiteral(":(?=[^\\s])"));
    static const QRegularExpression iAm(QStringLiteral("\\bI['’]m\\b"), QRegularExpression::CaseInsensitiveOption);
    // Steam's regional names ("趣拼拼：肖像画 Pleasure Puzzle:Portrait") carry the native name in front; IGDB has the English one.
    QString latin;
    for (const QChar c : title) {
        const QChar::Script script = c.script();
        const bool foreign = script == QChar::Script_Han || script == QChar::Script_Hiragana
            || script == QChar::Script_Katakana || script == QChar::Script_Hangul
            || (c.unicode() >= 0x3000 && c.unicode() <= 0x303F) || (c.unicode() >= 0xFF00 && c.unicode() <= 0xFFEF);
        if (!foreign)
            latin += c;
    }
    latin = latin.simplified();
    QString out = withoutPartWord(withoutDemoSuffix(withoutTrailingParenthetical(latin.isEmpty() ? title : latin)));
    out.replace(goty, QStringLiteral("Game of the Year"));
    out.replace(iAm, QStringLiteral("I am"));
    out.replace(colon, QStringLiteral(": "));
    return out;
}

int clearMatch(const QList<QStringList> &names, const QString &fullTitle, const std::function<int(const QList<int> &)> &tieBreak)
{
    const QString title = withoutDemoSuffix(fullTitle);
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
    if (same.size() > 1) {
        // "S.P.L.I.C.E.D." and "Spliced" share a key; the one spelled as the title is meant.
        QList<int> spelled;
        for (int i : same) {
            if (std::any_of(names.at(i).cbegin(), names.at(i).cend(),
                            [&](const QString &n) { return n.compare(title, Qt::CaseInsensitive) == 0; }))
                spelled.append(i);
        }
        if (spelled.size() == 1)
            return spelled.first();
        return tieBreak ? tieBreak(same) : -1;
    }

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
    if (best >= kMinScore && best - next >= kMinGap)
        return bestIndex;

    // Steam's "Back to the Future: Ep 2 - Get Tannen!" is IGDB's "… The Game - Episode 2: Get Tannen!":
    // the one name that holds all the title's words in order, plus a couple of its own.
    const QStringList titleWords = normalizedWords(title);
    if (titleWords.size() < 3 || titleNumbers.isEmpty())
        return -1;
    int only = -1;
    for (int i = 0; i < names.size(); ++i) {
        const bool fits = std::any_of(names.at(i).cbegin(), names.at(i).cend(), [&](const QString &n) {
            const QStringList words = normalizedWords(n);
            if (numbers(n) != titleNumbers || words.size() <= titleWords.size() || words.size() > titleWords.size() + 2)
                return false;
            qsizetype at = 0;
            for (const QString &w : words) {
                if (at < titleWords.size() && w == titleWords.at(at))
                    ++at;
            }
            return at == titleWords.size();
        });
        if (fits) {
            if (only >= 0)
                return -1;
            only = i;
        }
    }
    return only;
}
}
