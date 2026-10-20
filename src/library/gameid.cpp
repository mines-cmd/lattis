#include "gameid.h"

namespace GameId {
QString slug(const QString &text)
{
    // NFKD splits `é` into `e` + a combining accent (and `ﬁ` into `fi`); the accent is then
    // dropped with every other character outside a-z and 0-9.
    const QString decomposed = text.normalized(QString::NormalizationForm_KD).toLower();
    QString out;
    out.reserve(decomposed.size());
    for (const QChar c : decomposed) {
        if (c.category() == QChar::Mark_NonSpacing)
            continue;
        if ((c >= u'a' && c <= u'z') || (c >= u'0' && c <= u'9'))
            out += c;
        else if (!out.isEmpty() && !out.endsWith(u'-'))
            out += u'-';
    }
    while (out.endsWith(u'-'))
        out.chop(1);
    return out;
}

bool isValid(const QString &id)
{
    if (id.isEmpty() || id.startsWith(u'-') || id.endsWith(u'-'))
        return false;
    for (const QChar c : id) {
        if (!((c >= u'a' && c <= u'z') || (c >= u'0' && c <= u'9') || c == u'-'))
            return false;
    }
    return true;
}

QString forTitle(const QString &platform, const QString &title, const QSet<QString> &taken)
{
    QString titleSlug = slug(title);
    if (titleSlug.isEmpty())
        titleSlug = QStringLiteral("game");
    const QString base = platform + u'-' + titleSlug;
    QString id = base;
    for (int n = 2; taken.contains(id); ++n)
        id = base + u'-' + QString::number(n);
    return id;
}

QString forSteamApp(qint64 appId)
{
    return QStringLiteral("steam-") + QString::number(appId);
}

qint64 steamAppId(const QString &id)
{
    if (!id.startsWith(QStringLiteral("steam-")))
        return 0;
    bool ok = false;
    const qint64 appId = id.mid(6).toLongLong(&ok);
    return ok && appId > 0 ? appId : 0;
}
}
