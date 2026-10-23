#include "rominfo.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLoggingCategory>
#include <QRegularExpression>

#include <optional>
#include <utility>

Q_LOGGING_CATEGORY(lcRomInfo, "launcher.rominfo")

namespace RomInfoReader {
namespace {
// The limits are in docs/code/library.md: a few small reads at fixed offsets, never a whole file.
constexpr qint64 kBudget = 1 << 20;
constexpr qint64 kSmdhSize = 0x36C0;
constexpr qint64 kSmdhTitles = 0x2008;
constexpr qint64 kSfoMax = 1 << 16;
constexpr qint64 kIsoSector = 2048;

class Reader
{
public:
    explicit Reader(const QString &path) : m_file(path)
    {
        if (m_file.open(QIODevice::ReadOnly))
            m_size = m_file.size();
    }

    qint64 size() const { return m_size; }

    // Empty when the range is outside the file, over budget or unreadable.
    QByteArray read(qint64 pos, qint64 length)
    {
        if (pos < 0 || length <= 0 || pos > m_size || length > m_size - pos || length > kBudget - m_used)
            return {};
        if (!m_file.seek(pos))
            return {};
        const QByteArray data = m_file.read(length);
        m_used += length;
        return data.size() == length ? data : QByteArray();
    }

private:
    QFile m_file;
    qint64 m_size = 0;
    qint64 m_used = 0;
};

quint32 u8(const QByteArray &d, qsizetype at)
{
    return at >= 0 && at < d.size() ? quint8(d.at(at)) : 0;
}

quint32 u16(const QByteArray &d, qsizetype at)
{
    return u8(d, at) | (u8(d, at + 1) << 8);
}

quint32 u32(const QByteArray &d, qsizetype at)
{
    return u16(d, at) | (u16(d, at + 2) << 16);
}

qint64 u64(const QByteArray &d, qsizetype at)
{
    return qint64(u32(d, at)) | (qint64(u32(d, at + 4)) << 32);
}

qint64 alignedTo64(qint64 n)
{
    return (n + 63) & ~qint64(63);
}

QString utf16(const QByteArray &d, qsizetype at, qsizetype bytes)
{
    QString out;
    for (qsizetype i = 0; i + 1 < bytes && at + i + 1 < d.size(); i += 2) {
        const char16_t c = char16_t(u16(d, at + i));
        if (c == 0)
            break;
        out += QChar(c);
    }
    return out;
}

// Lines become one title: `Professor Layton vs. Phoenix Wright` and `Ace Attorney` read as
// "…Wright: Ace Attorney", but `The Legend of Zelda:` and `Tri Force Heroes` need no second colon.
QString joinLines(const QStringList &lines)
{
    QString out;
    for (const QString &raw : lines) {
        const QString line = raw.simplified();
        if (line.isEmpty())
            continue;
        if (!out.isEmpty())
            out += out.endsWith(u':') ? QStringLiteral(" ") : QStringLiteral(": ");
        out += line;
    }
    return out;
}

QString splitAndJoin(const QString &text)
{
    static const QRegularExpression kBreaks(QStringLiteral("[\r\n]+"));
    return joinLines(text.split(kBreaks));
}

QString clean(const QString &text)
{
    return text.simplified();
}

QString smdhTitle(const QByteArray &smdh)
{
    if (smdh.size() < kSmdhTitles || !smdh.startsWith("SMDH"))
        return {};
    // Index 1 is English; IGDB's names are English. Then the first language with a title.
    static const int kOrder[] = {1, 0, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15};
    for (const int language : kOrder) {
        const qsizetype at = 8 + language * 0x200;
        for (const auto &[offset, bytes] : {std::pair{0x80, 0x100}, std::pair{0, 0x80}}) {
            if (const QString title = splitAndJoin(utf16(smdh, at + offset, bytes)); !title.isEmpty())
                return title;
        }
    }
    return {};
}

QString hex16(qint64 id)
{
    return QStringLiteral("%1").arg(quint64(id), 16, 16, QLatin1Char('0')).toUpper();
}

// A bare NCCH (a .cxi, or partition 0 of a .3ds). The ExeFS of an encrypted dump doesn't parse,
// so it gives no title, but the program ID is in the clear.
RomInfo readNcch(Reader &r, qint64 base)
{
    RomInfo out;
    const QByteArray header = r.read(base, 0x200);
    if (header.size() < 0x200 || header.mid(0x100, 4) != "NCCH")
        return out;
    out.titleId = hex16(u64(header, 0x118));

    const int exponent = u8(header, 0x188 + 6);
    if (exponent > 6)
        return out;
    const qint64 unit = qint64(0x200) << exponent;
    const qint64 exefs = qint64(u32(header, 0x1A0)) * unit;
    const qint64 exefsSize = qint64(u32(header, 0x1A4)) * unit;
    if (exefs == 0 || exefsSize < 0x200)
        return out;
    const QByteArray files = r.read(base + exefs, 0x200);
    for (int i = 0; i < 10; ++i) {
        const qsizetype at = i * 16;
        if (files.mid(at, 8) != QByteArray("icon\0\0\0\0", 8))
            continue;
        const qint64 offset = u32(files, at + 8);
        const qint64 size = u32(files, at + 12);
        if (size < kSmdhTitles || offset + size > exefsSize - 0x200)
            break;
        out.title = smdhTitle(r.read(base + exefs + 0x200 + offset, qMin(size, kSmdhSize)));
        break;
    }
    return out;
}

RomInfo readNcsd(Reader &r)
{
    const QByteArray header = r.read(0, 0x200);
    if (header.size() < 0x200 || header.mid(0x100, 4) != "NCSD")
        return {};
    const int exponent = u8(header, 0x188 + 6);
    if (exponent > 6)
        return {};
    return readNcch(r, qint64(u32(header, 0x120)) * (qint64(0x200) << exponent));
}

// The meta section's SMDH is unencrypted even in an encrypted CIA.
RomInfo readCia(Reader &r)
{
    const QByteArray header = r.read(0, 0x20);
    if (header.size() < 0x20)
        return {};
    const qint64 metaSize = u32(header, 0x14);
    if (metaSize < 0x400 + kSmdhTitles)
        return {};
    qint64 at = alignedTo64(u32(header, 0));
    for (const qsizetype sizeAt : {8, 0xC, 0x10})
        at = alignedTo64(at + u32(header, sizeAt));
    RomInfo out;
    out.title = smdhTitle(r.read(at + 0x400, qMin(metaSize - 0x400, kSmdhSize)));
    return out;
}

RomInfo read3dsx(Reader &r)
{
    const QByteArray header = r.read(0, 0x2C);
    if (header.size() < 0x2C || !header.startsWith("3DSX") || u16(header, 4) < 0x2C)
        return {};
    const qint64 offset = u32(header, 0x20);
    const qint64 size = u32(header, 0x24);
    if (offset == 0 || size < kSmdhTitles)
        return {};
    RomInfo out;
    out.title = smdhTitle(r.read(offset, qMin(size, kSmdhSize)));
    return out;
}

RomInfo readNds(Reader &r)
{
    const qint64 banner = u32(r.read(0x68, 4), 0);
    if (banner < 0x160)
        return {};
    const QByteArray data = r.read(banner, 0x840);
    if (data.isEmpty())
        return {};
    // English, then the others: Japanese, French, German, Italian, Spanish.
    for (const qsizetype at : {0x340, 0x240, 0x440, 0x540, 0x640, 0x740}) {
        const QStringList lines = utf16(data, at, 0x100).split(u'\n');
        QStringList kept;
        for (const QString &line : lines) {
            if (!line.simplified().isEmpty())
                kept.append(line);
        }
        // Title, subtitle and publisher; or title and publisher.
        if (kept.size() >= 3)
            kept = kept.mid(0, 2);
        else if (!kept.isEmpty())
            kept = kept.mid(0, 1);
        if (const QString title = joinLines(kept); !title.isEmpty()) {
            RomInfo out;
            out.title = title;
            return out;
        }
    }
    return {};
}

RomInfo parseSfo(const QByteArray &sfo)
{
    RomInfo out;
    if (sfo.size() < 20 || !sfo.startsWith(QByteArray("\0PSF", 4)))
        return out;
    const qint64 keys = u32(sfo, 8);
    const qint64 data = u32(sfo, 12);
    const qint64 count = u32(sfo, 16);
    if (count > 512)
        return out;
    for (qint64 i = 0; i < count; ++i) {
        const qsizetype at = qsizetype(20 + i * 16);
        if (at + 16 > sfo.size())
            break;
        const qint64 keyAt = keys + u16(sfo, at);
        if (keyAt >= sfo.size())
            continue;
        const QByteArray key = QByteArray(sfo.constData() + keyAt, qstrnlen(sfo.constData() + keyAt, sfo.size() - keyAt));
        const quint32 format = u16(sfo, at + 2);
        const qint64 length = u32(sfo, at + 4);
        const qint64 valueAt = data + u32(sfo, at + 12);
        if ((format != 0x0204 && format != 0x0004) || valueAt < 0 || valueAt >= sfo.size())
            continue;
        const qint64 room = qMin(length, sfo.size() - valueAt);
        const QString value =
            clean(QString::fromUtf8(sfo.constData() + valueAt, qstrnlen(sfo.constData() + valueAt, room)));
        if (key == "TITLE")
            out.title = value;
        else if (key == "TITLE_ID")
            out.titleId = value;
    }
    return out;
}

RomInfo readPbp(Reader &r)
{
    const QByteArray header = r.read(0, 0x28);
    if (header.size() < 0x28 || !header.startsWith(QByteArray("\0PBP", 4)))
        return {};
    const qint64 from = u32(header, 8);
    const qint64 to = u32(header, 12);
    if (from == 0 || to <= from || to - from > kSfoMax)
        return {};
    return parseSfo(r.read(from, to - from));
}

struct Extent
{
    qint64 position = 0;
    qint64 size = 0;
};

// One name in an ISO 9660 directory, matched without case and without the `;1` version.
std::optional<Extent> findInDirectory(Reader &r, const Extent &directory, const QByteArray &name)
{
    const QByteArray data = r.read(directory.position, qMin(directory.size, qint64(1) << 17));
    for (qsizetype i = 0; i < data.size();) {
        const qsizetype length = u8(data, i);
        if (length == 0) {
            // Records don't cross a sector; zeros pad to the next.
            i = (i / kIsoSector + 1) * kIsoSector;
            continue;
        }
        const qsizetype nameLength = u8(data, i + 32);
        if (length < 34 || i + length > data.size() || 33 + nameLength > length)
            break;
        QByteArray entry = data.mid(i + 33, nameLength);
        if (const qsizetype version = entry.indexOf(';'); version >= 0)
            entry.truncate(version);
        if (entry.toUpper() == name)
            return Extent{qint64(u32(data, i + 2)) * kIsoSector, qint64(u32(data, i + 10))};
        i += length;
    }
    return std::nullopt;
}

RomInfo readPspIso(Reader &r)
{
    const QByteArray volume = r.read(16 * kIsoSector, kIsoSector);
    if (volume.size() < kIsoSector || u8(volume, 0) != 1 || volume.mid(1, 5) != "CD001")
        return {};
    const Extent root{qint64(u32(volume, 156 + 2)) * kIsoSector, qint64(u32(volume, 156 + 10))};
    const auto game = findInDirectory(r, root, "PSP_GAME");
    if (!game)
        return {};
    const auto param = findInDirectory(r, *game, "PARAM.SFO");
    if (!param || param->size <= 0 || param->size > kSfoMax)
        return {};
    return parseSfo(r.read(param->position, param->size));
}

// Each name is matched without case: dumps differ (`PARAM.SFO`, `param.sfo`).
QString findFile(const QString &folder, const QStringList &parts)
{
    QString at = folder;
    for (const QString &part : parts) {
        QString next;
        for (const QString &entry : QDir(at).entryList(QDir::AllEntries | QDir::NoDotAndDotDot)) {
            if (entry.compare(part, Qt::CaseInsensitive) == 0) {
                next = entry;
                break;
            }
        }
        if (next.isEmpty())
            return {};
        at += u'/' + next;
    }
    return at;
}

RomInfo readParamJson(const QString &path)
{
    Reader r(path);
    const QByteArray bytes = r.read(0, qMin(r.size(), kBudget));
    const QJsonObject root = QJsonDocument::fromJson(bytes).object();
    const QJsonObject localized = root.value(QLatin1String("localizedParameters")).toObject();
    QJsonObject entry = localized.value(localized.value(QLatin1String("defaultLanguage")).toString()).toObject();
    if (entry.isEmpty())
        entry = localized.value(QLatin1String("en-US")).toObject();
    RomInfo out;
    out.title = clean(entry.value(QLatin1String("titleName")).toString());
    if (out.title.isEmpty())
        out.title = clean(root.value(QLatin1String("titleName")).toString());
    out.titleId = root.value(QLatin1String("titleId")).toString();
    return out;
}

RomInfo readSfoFile(const QString &path)
{
    Reader r(path);
    return parseSfo(r.read(0, qMin(r.size(), kSfoMax)));
}
}

namespace {
const QHash<QString, QStringList> &formats()
{
    static const QHash<QString, QStringList> kFormats = {
        {QStringLiteral("3ds"), {QStringLiteral("3ds"), QStringLiteral("cci"), QStringLiteral("cxi"),
                                 QStringLiteral("cia"), QStringLiteral("3dsx")}},
        {QStringLiteral("nds"), {QStringLiteral("nds"), QStringLiteral("dsi"), QStringLiteral("srl")}},
        {QStringLiteral("psp"), {QStringLiteral("iso"), QStringLiteral("pbp")}},
    };
    return kFormats;
}
}

bool readsPlatform(const QString &platform)
{
    return formats().contains(platform) || platform == QLatin1String("ps3") || platform == QLatin1String("ps4");
}

bool reads(const QString &platform, const QString &extension)
{
    return formats().value(platform).contains(extension.toLower());
}

RomInfo read(const QString &path, const QString &platform)
{
    const QString extension = QFileInfo(path).suffix().toLower();
    if (!reads(platform, extension))
        return {};
    Reader r(path);
    if (r.size() == 0) {
        qCDebug(lcRomInfo).noquote() << "could not read" << path;
        return {};
    }
    RomInfo out;
    if (platform == QLatin1String("3ds")) {
        if (extension == QLatin1String("cia"))
            out = readCia(r);
        else if (extension == QLatin1String("3dsx"))
            out = read3dsx(r);
        else if (extension == QLatin1String("cxi"))
            out = readNcch(r, 0);
        else
            out = readNcsd(r);
    } else if (platform == QLatin1String("nds")) {
        out = readNds(r);
    } else if (extension == QLatin1String("pbp")) {
        out = readPbp(r);
    } else {
        out = readPspIso(r);
    }
    if (out.title.isEmpty())
        qCDebug(lcRomInfo).noquote() << "no name in" << path;
    return out;
}

RomInfo readFolder(const QString &folder, const QString &platform)
{
    QString sfo;
    QString json;
    if (platform == QLatin1String("ps3")) {
        sfo = findFile(folder, {QStringLiteral("PS3_GAME"), QStringLiteral("PARAM.SFO")});
        if (sfo.isEmpty())
            sfo = findFile(folder, {QStringLiteral("PARAM.SFO")});
    } else if (platform == QLatin1String("ps4")) {
        sfo = findFile(folder, {QStringLiteral("sce_sys"), QStringLiteral("param.sfo")});
        json = findFile(folder, {QStringLiteral("sce_sys"), QStringLiteral("param.json")});
    } else {
        return {};
    }
    RomInfo out;
    if (!sfo.isEmpty())
        out = readSfoFile(sfo);
    if (out.title.isEmpty() && !json.isEmpty())
        out = readParamJson(json);
    if (out.title.isEmpty())
        qCDebug(lcRomInfo).noquote() << "no name in" << folder;
    return out;
}
}
