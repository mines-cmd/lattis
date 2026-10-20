#include "launchoptions.h"

#include <QRegularExpression>

#include <algorithm>

namespace LaunchOptions {
namespace {
const QString kCommand = QStringLiteral("%command%");

bool isSpace(QChar c)
{
    return c == u' ' || c == u'\t' || c == u'\n' || c == u'\r';
}
}

QList<Word> split(const QString &s, QChar *unclosed)
{
    QList<Word> words;
    QChar open;
    const qsizetype n = s.size();
    qsizetype i = 0;
    while (true) {
        while (i < n && isSpace(s.at(i)))
            ++i;
        if (i >= n)
            break;
        Word word;
        word.begin = i;
        while (i < n && !isSpace(s.at(i))) {
            const QChar c = s.at(i);
            if (c == u'\'') {
                for (++i; i < n && s.at(i) != u'\''; ++i)
                    word.text += s.at(i);
                if (i >= n)
                    open = c;
                ++i;
            } else if (c == u'"') {
                for (++i; i < n && s.at(i) != u'"'; ++i) {
                    if (s.at(i) == u'\\' && i + 1 < n && QStringView(u"\"\\$`").contains(s.at(i + 1)))
                        ++i;
                    word.text += s.at(i);
                }
                if (i >= n)
                    open = c;
                ++i;
            } else if (c == u'\\' && i + 1 < n) {
                word.text += s.at(i + 1);
                i += 2;
            } else {
                word.text += c;
                ++i;
            }
        }
        word.end = std::min(i, n);
        words.append(word);
    }
    if (unclosed)
        *unclosed = open;
    return words;
}

QStringList words(const QString &options)
{
    QStringList out;
    for (const Word &w : split(options))
        out.append(w.text);
    return out;
}

bool isAssignment(const QString &word)
{
    static const QRegularExpression re(QStringLiteral("^[A-Za-z_][A-Za-z0-9_]*="));
    return re.match(word).hasMatch();
}

Parts parse(const QString &options)
{
    Parts out;
    QChar unclosed;
    const QList<Word> all = split(options, &unclosed);
    if (!unclosed.isNull())
        out.problem = QStringLiteral("the launch options have a %1 quote that isn't closed").arg(unclosed);

    const auto command = std::find_if(all.cbegin(), all.cend(), [](const Word &w) { return w.text == kCommand; });
    if (command == all.cend()) {
        for (const Word &w : all)
            out.args.append(w.text);
        return out;
    }
    auto it = all.cbegin();
    for (; it != command && isAssignment(it->text); ++it) {
        const qsizetype eq = it->text.indexOf(u'=');
        out.env.append({it->text.left(eq), it->text.mid(eq + 1)});
    }
    for (; it != command; ++it)
        out.wrapper.append(it->text);
    for (++it; it != all.cend(); ++it)
        out.args.append(it->text);
    return out;
}
}
