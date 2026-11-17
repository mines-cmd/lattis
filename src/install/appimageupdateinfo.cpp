#include "appimageupdateinfo.h"

#include "releasesource.h"

#include <QFile>
#include <QtEndian>

namespace {
constexpr qint64 kMaxSections = 4096;
constexpr qint64 kMaxSectionSize = 64 * 1024;

template<typename T>
T le(const QByteArray &bytes, qsizetype at)
{
    if (at < 0 || at + qsizetype(sizeof(T)) > bytes.size())
        return 0;
    return qFromLittleEndian<T>(bytes.constData() + at);
}

QByteArray readAt(QFile &file, quint64 offset, quint64 size)
{
    if (size > quint64(kMaxSectionSize) || offset > quint64(file.size()) || !file.seek(qint64(offset)))
        return {};
    return file.read(qint64(size));
}
}

std::optional<QString> AppImageUpdateInfo::readSection(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return std::nullopt;
    const QByteArray head = file.read(64);
    // Only 64-bit little-endian ELF: what AppImage runtimes are on the machines Lattis runs on.
    if (head.size() < 64 || !head.startsWith("\x7f" "ELF") || head.at(4) != 2 || head.at(5) != 1)
        return std::nullopt;
    const quint64 shoff = le<quint64>(head, 40);
    const quint16 shentsize = le<quint16>(head, 58);
    const quint16 shnum = le<quint16>(head, 60);
    const quint16 shstrndx = le<quint16>(head, 62);
    if (shoff == 0 || shentsize < 64 || shnum == 0 || shnum > kMaxSections || shstrndx >= shnum)
        return std::nullopt;

    if (shoff + quint64(shnum) * shentsize > quint64(file.size()))
        return std::nullopt;
    const QByteArray table = readAt(file, shoff, quint64(shnum) * shentsize);
    if (table.size() != qsizetype(shnum) * shentsize)
        return std::nullopt;
    const qsizetype strtab = qsizetype(shstrndx) * shentsize;
    const QByteArray names = readAt(file, le<quint64>(table, strtab + 24), le<quint64>(table, strtab + 32));
    if (names.isEmpty())
        return std::nullopt;

    for (quint16 i = 0; i < shnum; ++i) {
        const qsizetype header = qsizetype(i) * shentsize;
        const quint32 nameAt = le<quint32>(table, header);
        if (nameAt >= quint32(names.size()))
            continue;
        if (QByteArrayView(names.constData() + nameAt) != QByteArrayView(".upd_info"))
            continue;
        QByteArray data = readAt(file, le<quint64>(table, header + 24), le<quint64>(table, header + 32));
        if (const qsizetype end = data.indexOf('\0'); end >= 0)
            data.truncate(end);
        const QString text = QString::fromUtf8(data).trimmed();
        return text.isEmpty() ? std::nullopt : std::optional<QString>(text);
    }
    return std::nullopt;
}

std::optional<AppImageUpdateInfo> AppImageUpdateInfo::parse(const QString &info)
{
    const QStringList parts = info.split(u'|');
    if (parts.size() != 5 || (parts.at(0) != QLatin1String("gh-releases-zsync") && parts.at(0) != QLatin1String("gh-releases-direct")))
        return std::nullopt;
    AppImageUpdateInfo out;
    out.source = QStringLiteral("github:%1/%2").arg(parts.at(1), parts.at(2));
    if (!ReleaseSources::parse(out.source))
        return std::nullopt;
    out.asset = ReleaseSources::withoutZsync(parts.at(4));
    out.prereleases = parts.at(3) != QLatin1String("latest");
    return out;
}

std::optional<AppImageUpdateInfo> AppImageUpdateInfo::read(const QString &path)
{
    const std::optional<QString> section = readSection(path);
    return section ? parse(*section) : std::nullopt;
}
