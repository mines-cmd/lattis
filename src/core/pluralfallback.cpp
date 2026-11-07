#include "pluralfallback.h"

QString PluralFallback::plural(const QString &text, int n)
{
    QString out = text;
    out.replace(QLatin1String("(s)"), n == 1 ? QLatin1String("") : QLatin1String("s"));
    return out;
}

QString PluralFallback::translate(const char *, const char *sourceText, const char *, int n) const
{
    if (n < 0 || !sourceText)
        return {};
    const QString text = QString::fromUtf8(sourceText);
    return text.contains(QLatin1String("(s)")) ? plural(text, n) : QString();
}
