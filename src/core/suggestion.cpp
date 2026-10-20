#include "suggestion.h"

#include <QList>

#include <algorithm>

namespace {
int editDistance(const QString &a, const QString &b)
{
    QList<int> prev(b.size() + 1), cur(b.size() + 1);
    for (int j = 0; j <= b.size(); ++j)
        prev[j] = j;
    for (int i = 1; i <= a.size(); ++i) {
        cur[0] = i;
        for (int j = 1; j <= b.size(); ++j)
            cur[j] = std::min({prev[j] + 1, cur[j - 1] + 1, prev[j - 1] + (a[i - 1] == b[j - 1] ? 0 : 1)});
        std::swap(prev, cur);
    }
    return prev[b.size()];
}
}

QString suggestion(const QString &name, const QStringList &candidates)
{
    QString best;
    int bestDistance = 4;
    for (const QString &c : candidates) {
        const int d = editDistance(name, c);
        if (d < bestDistance) {
            bestDistance = d;
            best = c;
        }
    }
    return best.isEmpty() ? QString() : QStringLiteral(" — did you mean \"%1\"?").arg(best);
}
