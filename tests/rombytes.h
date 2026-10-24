#pragma once

#include <QByteArray>
#include <QList>
#include <QString>

// Header bytes of the formats rominfo.cpp reads, for tests.
namespace RomBytes {
inline void put(QByteArray &d, qsizetype at, quint64 value, int bytes)
{
    if (d.size() < at + bytes)
        d.resize(at + bytes);
    for (int i = 0; i < bytes; ++i)
        d[at + i] = char((value >> (8 * i)) & 0xFF);
}

inline void putText16(QByteArray &d, qsizetype at, const QString &text)
{
    for (qsizetype i = 0; i < text.size(); ++i)
        put(d, at + i * 2, text.at(i).unicode(), 2);
}

inline void putText(QByteArray &d, qsizetype at, const QByteArray &text)
{
    if (d.size() < at + text.size())
        d.resize(at + text.size());
    d.replace(at, text.size(), text);
}

inline QByteArray smdh(const QString &englishLong, const QString &englishShort = {}, const QString &japaneseLong = {})
{
    QByteArray d(0x36C0, '\0');
    putText(d, 0, "SMDH");
    putText16(d, 8 + 0x80, japaneseLong);
    putText16(d, 8 + 0x200, englishShort);
    putText16(d, 8 + 0x200 + 0x80, englishLong);
    return d;
}

// An NCCH whose ExeFS holds `icon`: header, a gap, the ExeFS header, then the file.
inline QByteArray ncch(const QByteArray &icon, quint64 programId)
{
    QByteArray d(0x400 + 0x200 + icon.size(), '\0');
    putText(d, 0x100, "NCCH");
    put(d, 0x118, programId, 8);
    put(d, 0x1A0, 2, 4);
    put(d, 0x1A4, (0x200 + icon.size() + 0x1FF) / 0x200, 4);
    putText(d, 0x400, "icon");
    put(d, 0x400 + 8, 0, 4);
    put(d, 0x400 + 12, icon.size(), 4);
    putText(d, 0x600, icon);
    return d;
}

inline QByteArray ncsd(const QByteArray &partition)
{
    QByteArray d(0x200, '\0');
    putText(d, 0x100, "NCSD");
    put(d, 0x120, 1, 4);
    return d + partition;
}

inline QByteArray cia(const QByteArray &icon)
{
    const qsizetype cert = 0x40;
    const qsizetype ticket = 0x80;
    const qsizetype tmd = 0x100;
    const qsizetype meta = 0x400 + icon.size();
    QByteArray d(0x40, '\0');
    put(d, 0, 0x2020, 4);
    put(d, 8, cert, 4);
    put(d, 0xC, ticket, 4);
    put(d, 0x10, tmd, 4);
    put(d, 0x14, meta, 4);
    // The header's 0x2020 bytes are rounded up to 64: this test's sections follow it directly.
    d.resize(0x2040);
    d.append(QByteArray(cert + ticket + tmd, '\0'));
    d.append(QByteArray(0x400, '\0'));
    d.append(icon);
    return d;
}

inline QByteArray threeDsx(const QByteArray &icon)
{
    QByteArray d(0x2C, '\0');
    putText(d, 0, "3DSX");
    put(d, 4, 0x2C, 2);
    put(d, 0x20, 0x2C, 4);
    put(d, 0x24, icon.size(), 4);
    return d + icon;
}

inline QByteArray nds(const QString &banner, qsizetype slot = 0x340)
{
    QByteArray d(0x200 + 0x840, '\0');
    put(d, 0x68, 0x200, 4);
    putText16(d, 0x200 + slot, banner);
    return d;
}

inline QByteArray sfo(const QByteArray &title, const QByteArray &id)
{
    QByteArray keys = QByteArray("TITLE\0TITLE_ID\0", 15);
    QByteArray data = title + QByteArray(1, '\0');
    const qsizetype idAt = data.size();
    data += id + QByteArray(1, '\0');
    QByteArray d(20, '\0');
    putText(d, 0, QByteArray("\0PSF", 4));
    put(d, 4, 0x101, 4);
    const qsizetype keyTable = 20 + 2 * 16;
    put(d, 8, keyTable, 4);
    put(d, 12, keyTable + keys.size(), 4);
    put(d, 16, 2, 4);
    const struct
    {
        qsizetype key;
        qsizetype length;
        qsizetype offset;
    } entries[] = {{0, title.size() + 1, 0}, {6, id.size() + 1, idAt}};
    for (const auto &e : entries) {
        QByteArray entry(16, '\0');
        put(entry, 0, e.key, 2);
        put(entry, 2, 0x0204, 2);
        put(entry, 4, e.length, 4);
        put(entry, 8, e.length, 4);
        put(entry, 12, e.offset, 4);
        d += entry;
    }
    return d + keys + data;
}

inline QByteArray isoRecord(const QByteArray &name, qsizetype sector, qsizetype size, bool directory)
{
    QByteArray r(33 + name.size(), '\0');
    r[0] = char(r.size());
    put(r, 2, sector, 4);
    put(r, 10, size, 4);
    r[25] = directory ? 2 : 0;
    r[32] = char(name.size());
    putText(r, 33, name);
    return r;
}

// Root at sector 17, PSP_GAME at 18, PARAM.SFO at 19.
inline QByteArray pspIso(const QByteArray &param)
{
    QByteArray d(19 * 2048 + param.size(), '\0');
    put(d, 16 * 2048, 1, 1);
    putText(d, 16 * 2048 + 1, "CD001");
    const QByteArray root = isoRecord(QByteArray(1, '\0'), 17, 2048, true);
    putText(d, 16 * 2048 + 156, root);
    putText(d, 17 * 2048, isoRecord("PSP_GAME", 18, 2048, true));
    putText(d, 18 * 2048, isoRecord("PARAM.SFO;1", 19, param.size(), false));
    putText(d, 19 * 2048, param);
    return d;
}

inline QByteArray pbp(const QByteArray &param)
{
    QByteArray d(0x28, '\0');
    putText(d, 0, QByteArray("\0PBP", 4));
    put(d, 8, 0x28, 4);
    put(d, 12, 0x28 + param.size(), 4);
    return d + param;
}

// An ISO 9660 image with one file in its root: volume descriptor at sector 16, root at 17, the file at 18.
// `sector` and `header` give a raw .bin's layout (2352 and 16 or 24).
inline QByteArray isoWithFile(const QByteArray &name, const QByteArray &content, qsizetype sector = 2048,
                              qsizetype header = 0)
{
    QByteArray d;
    const auto at = [&](qsizetype n) { return n * sector + header; };
    put(d, at(16), 1, 1);
    putText(d, at(16) + 1, "CD001");
    putText(d, at(16) + 156, isoRecord(QByteArray(1, '\0'), 17, 2048, true));
    putText(d, at(17), isoRecord(name, 18, content.size(), false));
    putText(d, at(18), content);
    return d;
}

inline QByteArray discHeader(const QByteArray &id, const QByteArray &name, bool wii)
{
    QByteArray d(0x100, '\0');
    putText(d, 0, id);
    putText(d, 0x20, name);
    putText(d, wii ? 0x18 : 0x1C, wii ? QByteArray("\x5D\x1C\x9E\xA3", 4) : QByteArray("\xC2\x33\x9F\x3D", 4));
    return d;
}

// A PFS0 header with these file names (no file contents).
inline QByteArray nsp(const QList<QByteArray> &names)
{
    QByteArray strings;
    QByteArray entries;
    for (const QByteArray &name : names) {
        QByteArray entry(24, '\0');
        put(entry, 16, strings.size(), 4);
        entries += entry;
        strings += name + QByteArray(1, '\0');
    }
    QByteArray d(16, '\0');
    putText(d, 0, "PFS0");
    put(d, 4, names.size(), 4);
    put(d, 8, strings.size(), 4);
    return d + entries + strings;
}
}
