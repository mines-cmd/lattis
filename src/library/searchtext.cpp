#include "searchtext.h"

#include <algorithm>

namespace SearchText {
QString fold(const QString &text)
{
    // NFKD splits `é` into `e` + a combining accent, which is then dropped.
    // NFKD would otherwise turn ™ into the letters "tm".
    QString plain = text;
    plain.remove(u'™').remove(u'®').remove(u'©');
    const QString decomposed = plain.normalized(QString::NormalizationForm_KD).toCaseFolded();
    QString out;
    out.reserve(decomposed.size());
    for (const QChar c : decomposed) {
        if (c.category() == QChar::Mark_NonSpacing || c == u'\'' || c == u'’')
            continue;
        if (c.isLetterOrNumber())
            out += c;
        else if (!out.isEmpty() && !out.endsWith(u' '))
            out += u' ';
    }
    if (out.endsWith(u' '))
        out.chop(1);
    return out;
}

QStringList words(const QString &search)
{
    return fold(search).split(u' ', Qt::SkipEmptyParts);
}

bool matches(const QString &foldedTitle, const QStringList &words)
{
    return std::all_of(words.cbegin(), words.cend(), [&](const QString &w) { return foldedTitle.contains(w); });
}
}
