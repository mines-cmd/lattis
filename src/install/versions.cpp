#include "versions.h"

#include <QStringList>

namespace Versions {
std::optional<QList<int>> parse(const QString &text)
{
    QString t = text.trimmed();
    if (t.startsWith(u'v') || t.startsWith(u'V'))
        t.remove(0, 1);
    if (t.isEmpty())
        return std::nullopt;
    QList<int> numbers;
    for (const QString &part : t.split(u'.')) {
        bool ok = false;
        const int n = part.toInt(&ok);
        if (!ok || n < 0)
            return std::nullopt;
        numbers.append(n);
    }
    return numbers;
}

bool newer(const QString &candidate, const QString &current)
{
    const auto a = parse(candidate);
    const auto b = parse(current);
    if (!a || !b)
        return false;
    const qsizetype size = qMax(a->size(), b->size());
    for (qsizetype i = 0; i < size; ++i) {
        const int x = a->value(i, 0);
        const int y = b->value(i, 0);
        if (x != y)
            return x > y;
    }
    return false;
}
}
