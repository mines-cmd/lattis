#include "romfoldermatch.h"

#include <QHash>
#include <QStringList>

namespace RomFolderMatch {
QString normalize(const QString &name)
{
    QString out;
    out.reserve(name.size());
    for (const QChar &c : name) {
        if (c.isLetterOrNumber())
            out.append(c.toLower());
    }
    return out;
}

namespace {
const QHash<QString, QStringList> &aliasTable()
{
    static const QHash<QString, QStringList> table = [] {
        QHash<QString, QStringList> t;
        t.insert(QStringLiteral("switch"), {QStringLiteral("switch"), QStringLiteral("nintendoswitch"), QStringLiteral("ns"), QStringLiteral("nsw")});
        t.insert(QStringLiteral("switch-2"), {QStringLiteral("switch2"), QStringLiteral("nintendoswitch2"), QStringLiteral("ns2"), QStringLiteral("nsw2")});
        t.insert(QStringLiteral("3ds"), {QStringLiteral("3ds"), QStringLiteral("nintendo3ds"), QStringLiteral("n3ds")});
        t.insert(QStringLiteral("wiiu"), {QStringLiteral("wiiu"), QStringLiteral("nintendowiiu")});
        t.insert(QStringLiteral("wii"), {QStringLiteral("wii"), QStringLiteral("nintendowii")});
        t.insert(QStringLiteral("gamecube"), {QStringLiteral("gamecube"), QStringLiteral("gc"), QStringLiteral("ngc"), QStringLiteral("nintendogamecube")});
        t.insert(QStringLiteral("n64"), {QStringLiteral("n64"), QStringLiteral("nintendo64")});
        t.insert(QStringLiteral("nds"), {QStringLiteral("nds"), QStringLiteral("ds"), QStringLiteral("nintendods")});
        t.insert(QStringLiteral("gba"), {QStringLiteral("gba"), QStringLiteral("gameboyadvance")});
        t.insert(QStringLiteral("nes"), {QStringLiteral("nes"), QStringLiteral("famicom"), QStringLiteral("fc"), QStringLiteral("nintendo"), QStringLiteral("nintendoentertainmentsystem"), QStringLiteral("fds"), QStringLiteral("famicomdisksystem")});
        t.insert(QStringLiteral("snes"), {QStringLiteral("snes"), QStringLiteral("superfamicom"), QStringLiteral("sfc"), QStringLiteral("supernintendo"), QStringLiteral("supernes")});
        t.insert(QStringLiteral("genesis"), {QStringLiteral("genesis"), QStringLiteral("megadrive"), QStringLiteral("md"), QStringLiteral("segagenesis"), QStringLiteral("segamegadrive"), QStringLiteral("gen")});
        t.insert(QStringLiteral("gb"), {QStringLiteral("gb"), QStringLiteral("gameboy"), QStringLiteral("nintendogameboy")});
        t.insert(QStringLiteral("gbc"), {QStringLiteral("gbc"), QStringLiteral("gameboycolor")});
        t.insert(QStringLiteral("sms"), {QStringLiteral("sms"), QStringLiteral("mastersystem"), QStringLiteral("segamastersystem")});
        t.insert(QStringLiteral("gamegear"), {QStringLiteral("gamegear"), QStringLiteral("gg"), QStringLiteral("segagamegear")});
        t.insert(QStringLiteral("dreamcast"), {QStringLiteral("dreamcast"), QStringLiteral("dc"), QStringLiteral("segadreamcast")});
        t.insert(QStringLiteral("arcade"), {QStringLiteral("arcade"), QStringLiteral("mame")});
        t.insert(QStringLiteral("ps4"), {QStringLiteral("ps4"), QStringLiteral("playstation4")});
        t.insert(QStringLiteral("ps3"), {QStringLiteral("ps3"), QStringLiteral("playstation3")});
        t.insert(QStringLiteral("ps2"), {QStringLiteral("ps2"), QStringLiteral("playstation2")});
        t.insert(QStringLiteral("ps1"), {QStringLiteral("ps1"), QStringLiteral("psx"), QStringLiteral("playstation"), QStringLiteral("playstation1")});
        t.insert(QStringLiteral("psp"), {QStringLiteral("psp"), QStringLiteral("playstationportable")});
        return t;
    }();
    return table;
}
}

QString byAlias(const QString &folderName)
{
    const QString n = normalize(folderName);
    if (n.isEmpty())
        return {};
    const QHash<QString, QStringList> &table = aliasTable();
    for (auto it = table.cbegin(); it != table.cend(); ++it) {
        if (it.value().contains(n))
            return it.key();
    }
    return {};
}
}
