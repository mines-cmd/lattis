#include "herocache.h"

#include "artimage.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QImageReader>
#include <QImageWriter>
#include <QPainter>

namespace HeroCache {
bool isBaselineJpeg(const QString &path)
{
    if (QImageReader(path).format() != "jpeg")
        return false;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return false;
    if (file.read(2) != QByteArrayLiteral("\xFF\xD8"))
        return false;
    // Walk the segments up to the first frame header; the scan (FF DA) can't come before it in a valid file.
    for (;;) {
        char byte = 0;
        do {
            if (!file.getChar(&byte))
                return false;
        } while (byte != '\xFF');
        do {
            if (!file.getChar(&byte))
                return false;
        } while (byte == '\xFF');
        const auto marker = static_cast<uchar>(byte);
        if (marker == 0xC0 || marker == 0xC1)
            return true;
        if (marker == 0xC2 || marker == 0xDA || marker == 0xD9)
            return false;
        if (marker == 0x00 || marker == 0x01 || (marker >= 0xD0 && marker <= 0xD8))
            continue;
        const QByteArray length = file.read(2);
        if (length.size() != 2)
            return false;
        const int skip = (static_cast<uchar>(length[0]) << 8 | static_cast<uchar>(length[1])) - 2;
        if (skip < 0 || !file.seek(file.pos() + skip))
            return false;
    }
}

QString stem(const QString &id, qint64 modified)
{
    return id + u'-' + QString::number(modified);
}

QHash<QString, QString> list(const QString &cacheDir)
{
    QHash<QString, QString> out;
    QDirIterator it(QDir(cacheDir).filePath(QStringLiteral("heroes")), QDir::Files);
    while (it.hasNext()) {
        const QString path = it.next();
        const QString name = it.fileName();
        if (name.endsWith(u".jpg"))
            out.insert(name.left(name.size() - 4), path);
    }
    return out;
}

void prune(const QString &cacheDir, const QSet<QString> &keep)
{
    QDirIterator it(QDir(cacheDir).filePath(QStringLiteral("heroes")), QDir::Files);
    while (it.hasNext()) {
        const QString path = it.next();
        const QString name = it.fileName();
        if (!name.endsWith(u".jpg") || !keep.contains(name.left(name.size() - 4)))
            QFile::remove(path);
    }
}

Result make(const QString &source, const QString &id, qint64 modified, const QString &cacheDir)
{
    QImageReader reader(source);
    const QSize size = reader.size();
    if (!size.isValid())
        return {{}, reader.errorString()};
    if (size.width() <= kWidth && isBaselineJpeg(source))
        return {};

    const QString target = QDir(cacheDir).filePath(QStringLiteral("heroes/") + stem(id, modified) + QStringLiteral(".jpg"));
    if (QFileInfo::exists(target))
        return {target, {}};

    ArtImage::allowLargeImages();
    QImage image = reader.read();
    if (image.isNull())
        return {{}, reader.errorString()};
    if (image.width() > kWidth)
        image = image.scaledToWidth(kWidth, Qt::SmoothTransformation);

    // Heroes never use transparency; painting onto black keeps leftover colors under it from showing.
    if (image.hasAlphaChannel()) {
        QImage flat(image.size(), QImage::Format_RGB32);
        flat.fill(Qt::black);
        QPainter painter(&flat);
        painter.drawImage(0, 0, image);
        painter.end();
        image = flat;
    } else {
        image.convertTo(QImage::Format_RGB32);
    }
    const QString partial = target + QStringLiteral(".partial");

    if (!QDir().mkpath(QFileInfo(target).path()))
        return {{}, QStringLiteral("Could not create %1").arg(QFileInfo(target).path())};
    QImageWriter writer(partial, "jpeg");
    writer.setQuality(kQuality);
    if (!writer.write(image)) {
        QFile::remove(partial);
        return {{}, writer.errorString()};
    }
    if (!QFile::rename(partial, target)) {
        QFile::remove(partial);
        return {{}, QStringLiteral("Could not save %1").arg(target)};
    }
    return {target, {}};
}
}
