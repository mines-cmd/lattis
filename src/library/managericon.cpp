#include "managericon.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QImageReader>
#include <QStandardPaths>
#include <QtEndian>

#include <algorithm>

namespace ManagerIcon {
namespace {

constexpr qint64 kMaxResources = 64 * 1024 * 1024;
constexpr int kIconSize = 256;
constexpr qint64 kNoneLifetimeSecs = 3600;
constexpr quint32 kGroupIcon = 14;
constexpr quint32 kIcon = 3;
constexpr quint32 kHighBit = 0x80000000u;

bool has(const QByteArray &b, qint64 offset, qint64 length)
{
    return offset >= 0 && length >= 0 && offset + length <= b.size();
}

quint16 u16(const QByteArray &b, qint64 o)
{
    return has(b, o, 2) ? qFromLittleEndian<quint16>(b.constData() + o) : 0;
}

quint32 u32(const QByteArray &b, qint64 o)
{
    return has(b, o, 4) ? qFromLittleEndian<quint32>(b.constData() + o) : 0;
}

void put16(QByteArray &b, quint16 v)
{
    b.append(char(v & 0xff));
    b.append(char(v >> 8));
}

void put32(QByteArray &b, quint32 v)
{
    put16(b, quint16(v & 0xffff));
    put16(b, quint16(v >> 16));
}

struct Item
{
    quint32 id = 0;
    quint32 offset = 0;
    bool dir = false;
    bool named = false;
};

QList<Item> items(const QByteArray &r, quint32 dirOffset)
{
    QList<Item> out;
    const int count = int(u16(r, dirOffset + 12)) + int(u16(r, dirOffset + 14));
    for (int i = 0; i < count; ++i) {
        const qint64 at = qint64(dirOffset) + 16 + 8 * qint64(i);
        if (!has(r, at, 8))
            break;
        const quint32 name = u32(r, at);
        const quint32 target = u32(r, at + 4);
        out.append({name & ~kHighBit, target & ~kHighBit, (target & kHighBit) != 0, (name & kHighBit) != 0});
    }
    return out;
}

// The first leaf under `item`, whatever the language level.
QByteArray leaf(const QByteArray &r, quint32 rootRva, Item item)
{
    for (int depth = 0; depth < 4 && item.dir; ++depth) {
        const QList<Item> next = items(r, item.offset);
        if (next.isEmpty())
            return {};
        item = next.first();
    }
    if (item.dir || !has(r, item.offset, 16))
        return {};
    const quint32 rva = u32(r, item.offset);
    const quint32 size = u32(r, item.offset + 4);
    if (rva < rootRva || !has(r, rva - rootRva, size))
        return {};
    return r.mid(rva - rootRva, size);
}

Item typeItem(const QByteArray &r, quint32 type)
{
    for (const Item &i : items(r, 0)) {
        if (!i.named && i.id == type)
            return i;
    }
    return {0, 0, false, true};
}

QByteArray readResources(const QString &exe, quint32 *rootRva)
{
    QFile file(exe);
    if (!file.open(QIODevice::ReadOnly))
        return {};
    const QByteArray head = file.read(1 << 16);
    const qint64 pe = u32(head, 0x3c);
    if (head.left(2) != "MZ" || head.mid(pe, 4) != QByteArray("PE\0\0", 4))
        return {};
    const qint64 coff = pe + 4;
    const int sections = u16(head, coff + 2);
    const qint64 opt = coff + 20;
    const quint16 magic = u16(head, opt);
    if (magic != 0x10b && magic != 0x20b)
        return {};
    const qint64 dir = opt + (magic == 0x20b ? 112 : 96) + 16;
    const quint32 rva = u32(head, dir);
    const quint32 size = u32(head, dir + 4);
    if (rva == 0 || size == 0 || size > kMaxResources)
        return {};
    const qint64 table = opt + u16(head, coff + 16);
    for (int i = 0; i < sections; ++i) {
        const qint64 s = table + 40 * qint64(i);
        const quint32 virtualSize = u32(head, s + 8);
        const quint32 va = u32(head, s + 12);
        const quint32 raw = u32(head, s + 16);
        const quint32 pointer = u32(head, s + 20);
        if (rva < va || rva >= va + std::max(virtualSize, raw))
            continue;
        if (!file.seek(qint64(pointer) + (rva - va)))
            return {};
        const QByteArray data = file.read(size);
        if (data.size() != qint64(size))
            return {};
        *rootRva = rva;
        return data;
    }
    return {};
}

QImage loadFile(const QString &path)
{
    QImageReader reader(path);
    if (reader.format() == "svg" || reader.format() == "svgz")
        reader.setScaledSize(QSize(kIconSize, kIconSize));
    return reader.read();
}

QStringList defaultAppDirs()
{
    QStringList dirs = QStandardPaths::standardLocations(QStandardPaths::ApplicationsLocation);
    dirs << QDir::homePath() + QStringLiteral("/.local/share/flatpak/exports/share/applications")
         << QStringLiteral("/var/lib/flatpak/exports/share/applications");
    dirs.removeDuplicates();
    return dirs;
}

QStringList defaultIconDirs()
{
    QStringList dirs;
    for (const QString &data : QStandardPaths::standardLocations(QStandardPaths::GenericDataLocation))
        dirs << data + QStringLiteral("/icons") << data + QStringLiteral("/pixmaps");
    dirs << QDir::homePath() + QStringLiteral("/.icons")
         << QDir::homePath() + QStringLiteral("/.local/share/flatpak/exports/share/icons")
         << QStringLiteral("/var/lib/flatpak/exports/share/icons");
    dirs.removeDuplicates();
    return dirs;
}

QString resolveIcon(const QString &name, const QStringList &iconDirs)
{
    if (name.isEmpty())
        return {};
    if (QDir::isAbsolutePath(name))
        return QFileInfo::exists(name) ? name : QString();

    const QStringList suffixes = {QStringLiteral(".png"), QStringLiteral(".svg"), QStringLiteral(".xpm")};
    QString bestPng;
    int bestSize = 0;
    QString scalable;
    QString flat;
    for (const QString &root : iconDirs) {
        for (const QString &suffix : suffixes) {
            if (flat.isEmpty() && QFileInfo::exists(root + u'/' + name + suffix))
                flat = root + u'/' + name + suffix;
        }
        const QDir hicolor(root + QStringLiteral("/hicolor"));
        for (const QString &sizeDir : hicolor.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
            if (sizeDir == QLatin1String("scalable")) {
                const QString svg = hicolor.filePath(sizeDir + QStringLiteral("/apps/") + name + QStringLiteral(".svg"));
                if (scalable.isEmpty() && QFileInfo::exists(svg))
                    scalable = svg;
                continue;
            }
            const int size = sizeDir.section(u'x', 0, 0).toInt();
            const QString png = hicolor.filePath(sizeDir + QStringLiteral("/apps/") + name + QStringLiteral(".png"));
            if (size > bestSize && QFileInfo::exists(png)) {
                bestSize = size;
                bestPng = png;
            }
        }
    }
    if (bestSize >= 128)
        return bestPng;
    if (!scalable.isEmpty())
        return scalable;
    return !bestPng.isEmpty() ? bestPng : flat;
}

}

QImage fromExe(const QString &exe)
{
    quint32 rootRva = 0;
    const QByteArray r = readResources(exe, &rootRva);
    if (r.isEmpty())
        return {};
    const Item groups = typeItem(r, kGroupIcon);
    const Item icons = typeItem(r, kIcon);
    if (groups.named || icons.named || !groups.dir || !icons.dir)
        return {};
    const QList<Item> groupList = items(r, groups.offset);
    if (groupList.isEmpty())
        return {};
    const QByteArray group = leaf(r, rootRva, groupList.first());
    const int count = u16(group, 4);

    int bestWidth = -1;
    int bestBits = 0;
    int bestId = 0;
    quint8 bestW = 0;
    quint8 bestH = 0;
    quint8 bestColors = 0;
    quint16 bestPlanes = 0;
    for (int i = 0; i < count; ++i) {
        const qint64 at = 6 + 14 * qint64(i);
        if (!has(group, at, 14))
            break;
        const quint8 w = quint8(group[at]);
        const int width = w == 0 ? 256 : w;
        const int bits = u16(group, at + 6);
        if (width > bestWidth || (width == bestWidth && bits > bestBits)) {
            bestWidth = width;
            bestBits = bits;
            bestId = u16(group, at + 12);
            bestW = w;
            bestH = quint8(group[at + 1]);
            bestColors = quint8(group[at + 2]);
            bestPlanes = u16(group, at + 4);
        }
    }
    if (bestWidth < 0)
        return {};

    QByteArray image;
    for (const Item &i : items(r, icons.offset)) {
        if (!i.named && int(i.id) == bestId) {
            image = leaf(r, rootRva, i);
            break;
        }
    }
    if (image.isEmpty())
        return {};

    QByteArray ico;
    put16(ico, 0);
    put16(ico, 1);
    put16(ico, 1);
    ico.append(char(bestW));
    ico.append(char(bestH));
    ico.append(char(bestColors));
    ico.append(char(0));
    put16(ico, bestPlanes);
    put16(ico, quint16(bestBits));
    put32(ico, quint32(image.size()));
    put32(ico, 22);
    ico.append(image);
    return QImage::fromData(ico, "ICO");
}

Entry parseDesktop(const QByteArray &text)
{
    Entry entry;
    bool inEntry = false;
    QString tryExec;
    for (const QByteArray &raw : text.split('\n')) {
        const QString line = QString::fromUtf8(raw).trimmed();
        if (line.startsWith(u'[')) {
            inEntry = line == QLatin1String("[Desktop Entry]");
            continue;
        }
        if (!inEntry)
            continue;
        if (line.startsWith(QLatin1String("Icon=")))
            entry.icon = line.mid(5).trimmed();
        else if (line.startsWith(QLatin1String("TryExec=")))
            tryExec = line.mid(8).trimmed();
        else if (line.startsWith(QLatin1String("Exec="))) {
            QString token;
            bool quoted = false;
            bool started = false;
            const QString value = line.mid(5);
            for (int i = 0; i < value.size(); ++i) {
                const QChar c = value[i];
                if (c == u'\\' && i + 1 < value.size()) {
                    token += value[++i];
                    started = true;
                } else if (c == u'"') {
                    quoted = !quoted;
                    started = true;
                } else if (c.isSpace() && !quoted) {
                    if (started && !token.startsWith(u'%'))
                        entry.exec.append(token);
                    token.clear();
                    started = false;
                } else {
                    token += c;
                    started = true;
                }
            }
            if (started && !token.startsWith(u'%'))
                entry.exec.append(token);
        }
    }
    if (!tryExec.isEmpty())
        entry.exec.append(tryExec);
    return entry;
}

bool launches(const Entry &entry, const QString &program)
{
    if (program.isEmpty())
        return false;
    const bool path = program.contains(u'/');
    const QString canonical = path ? QFileInfo(program).canonicalFilePath() : QString();
    for (const QString &token : entry.exec) {
        if (token == program)
            return true;
        if (!path) {
            if (QFileInfo(token).fileName() == program)
                return true;
        } else if (!canonical.isEmpty() && token.contains(u'/') && QFileInfo(token).canonicalFilePath() == canonical) {
            return true;
        }
    }
    return false;
}

QImage fromDesktop(const QString &program, const QStringList &appDirs, const QStringList &iconDirs)
{
    if (program.isEmpty())
        return {};
    const QStringList icons = iconDirs.isEmpty() ? defaultIconDirs() : iconDirs;
    for (const QString &dir : appDirs.isEmpty() ? defaultAppDirs() : appDirs) {
        QDirIterator it(dir, {QStringLiteral("*.desktop")}, QDir::Files);
        while (it.hasNext()) {
            QFile file(it.next());
            if (!file.open(QIODevice::ReadOnly))
                continue;
            const Entry entry = parseDesktop(file.read(1 << 16));
            if (entry.icon.isEmpty() || !launches(entry, program))
                continue;
            const QImage image = loadFile(resolveIcon(entry.icon, icons));
            if (!image.isNull())
                return image;
        }
    }
    return {};
}

static QString prefixFor(const QString &cacheDir, const QString &id)
{
    return cacheDir + QStringLiteral("/mod-manager-icons/") + id + u'-';
}

QString cached(const QString &cacheDir, const QString &id, const QString &program, bool proton)
{
    if (program.isEmpty())
        return {};
    const QString key = QString::fromLatin1(QCryptographicHash::hash(program.toUtf8(), QCryptographicHash::Sha1).toHex().left(8));
    const QString base = prefixFor(cacheDir, id) + key;
    const QFileInfo png(base + QStringLiteral(".png"));
    const QFileInfo none(base + QStringLiteral(".none"));
    const QFileInfo source(program);
    const QDateTime changed = source.exists() ? source.lastModified() : QDateTime();
    if (png.exists() && (!changed.isValid() || png.lastModified() >= changed))
        return png.filePath();
    if (none.exists() && none.lastModified().secsTo(QDateTime::currentDateTime()) < kNoneLifetimeSecs
        && (!changed.isValid() || none.lastModified() >= changed))
        return {};

    forget(cacheDir, id);
    QImage image = proton ? fromExe(program) : fromDesktop(program);
    QDir().mkpath(QFileInfo(base).absolutePath());
    if (image.isNull()) {
        (void)QFile(none.filePath()).open(QIODevice::WriteOnly);
        return {};
    }
    if (image.width() > kIconSize || image.height() > kIconSize)
        image = image.scaled(kIconSize, kIconSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    return image.save(png.filePath(), "PNG") ? png.filePath() : QString();
}

void forget(const QString &cacheDir, const QString &id)
{
    const QString prefix = prefixFor(cacheDir, id);
    const QDir dir(QFileInfo(prefix).absolutePath());
    for (const QString &name : dir.entryList({QFileInfo(prefix).fileName() + QStringLiteral("????????.*")}, QDir::Files))
        QFile::remove(dir.filePath(name));
}
}
