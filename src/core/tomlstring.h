#pragma once

#include <QString>

// The one way to write a string into a TOML file. `\`, `"` and every character below 0x20 or 0x7F are
// written as an escape, so nothing it returns can make a file unreadable or add a line to it.
inline QString tomlQuote(const QString &text)
{
    QString out;
    out.reserve(text.size() + 2);
    out += u'"';
    for (const QChar c : text) {
        switch (c.unicode()) {
        case '"': out += QLatin1String("\\\""); break;
        case '\\': out += QLatin1String("\\\\"); break;
        case '\n': out += QLatin1String("\\n"); break;
        case '\t': out += QLatin1String("\\t"); break;
        case '\r': out += QLatin1String("\\r"); break;
        case '\b': out += QLatin1String("\\b"); break;
        case '\f': out += QLatin1String("\\f"); break;
        default:
            if (c.unicode() < 0x20 || c.unicode() == 0x7f)
                out += QStringLiteral("\\u%1").arg(uint(c.unicode()), 4, 16, u'0');
            else
                out += c;
        }
    }
    out += u'"';
    return out;
}
