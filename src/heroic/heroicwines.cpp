#include "heroicwines.h"

#include <QDir>
#include <QFileInfo>

namespace HeroicWines {
namespace {
QStringList folders(const QString &dir)
{
    QStringList names = QDir(dir).entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    names.sort(Qt::CaseInsensitive);
    return names;
}

void addProton(QList<HeroicWineChoice> &out, const QString &dir, const QString &source)
{
    for (const QString &name : folders(dir)) {
        const QString bin = dir + u'/' + name + QStringLiteral("/proton");
        if (QFileInfo::exists(bin))
            out.append({{bin, name, QStringLiteral("proton"), {}}, source});
    }
}
}

QList<HeroicWineChoice> find(const QString &heroicRoot, const QStringList &steamToolDirs, const QString &systemWine)
{
    QList<HeroicWineChoice> out;
    const QString tools = heroicRoot + QStringLiteral("/tools");
    addProton(out, tools + QStringLiteral("/proton"), QStringLiteral("heroic"));
    const QString wines = tools + QStringLiteral("/wine");
    for (const QString &name : folders(wines)) {
        const QString bin = wines + u'/' + name + QStringLiteral("/bin/wine");
        if (QFileInfo::exists(bin))
            out.append({{bin, name, QStringLiteral("wine"), wines + u'/' + name + QStringLiteral("/bin/wineserver")},
                        QStringLiteral("heroic")});
    }
    for (const QString &dir : steamToolDirs)
        addProton(out, dir, QStringLiteral("steam"));
    if (!systemWine.isEmpty()) {
        const QString server = QFileInfo(systemWine).absolutePath() + QStringLiteral("/wineserver");
        out.append({{systemWine, QStringLiteral("Wine (system)"), QStringLiteral("wine"),
                     QFileInfo::exists(server) ? server : QString()},
                    QStringLiteral("system")});
    }
    return out;
}
}
