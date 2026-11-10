#include "artimage.h"

#include <algorithm>

#include <QBuffer>
#include <QCoreApplication>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QImageReader>
#include <QImageWriter>

namespace ArtImage {
namespace {
QString tr(const char *text)
{
    return QCoreApplication::translate("Library", text);
}

// Lossy WebP alpha leaves faint specks in the "empty" margins; under this they can't be seen.
constexpr int kVisibleAlpha = 4;

QRect visibleBounds(const QImage &image)
{
    if (!image.hasAlphaChannel())
        return image.rect();
    const QImage argb = image.convertToFormat(QImage::Format_ARGB32);
    int left = argb.width(), right = -1, top = argb.height(), bottom = -1;
    for (int y = 0; y < argb.height(); ++y) {
        const auto *line = reinterpret_cast<const QRgb *>(argb.constScanLine(y));
        for (int x = 0; x < argb.width(); ++x) {
            if (qAlpha(line[x]) < kVisibleAlpha)
                continue;
            left = std::min(left, x);
            right = std::max(right, x);
            top = std::min(top, y);
            bottom = y;
        }
    }
    if (right < 0)
        return image.rect();
    return QRect(QPoint(left, top), QPoint(right, bottom));
}

constexpr char kPngSignature[] = "\x89PNG\r\n\x1a\n";
constexpr qsizetype kPngSignatureSize = 8;

quint32 bigEndian(const char *bytes)
{
    const auto *b = reinterpret_cast<const uchar *>(bytes);
    return quint32(b[0]) << 24 | quint32(b[1]) << 16 | quint32(b[2]) << 8 | quint32(b[3]);
}

bool isPngText(QByteArrayView type)
{
    return type == "tEXt" || type == "zTXt" || type == "iTXt";
}

// Chunk by chunk, so a damaged file is copied as far as it goes and the decoder reports the rest.
QByteArray stripPngText(const QByteArray &png)
{
    if (!png.startsWith(QByteArrayView(kPngSignature, kPngSignatureSize)))
        return png;
    QByteArray out = png.first(kPngSignatureSize);
    qsizetype at = kPngSignatureSize;
    while (at + 12 <= png.size()) {
        const qsizetype chunk = 12 + qsizetype(bigEndian(png.constData() + at));
        if (chunk < 12 || at + chunk > png.size())
            break;
        if (!isPngText(QByteArrayView(png.constData() + at + 4, 4)))
            out.append(png.constData() + at, chunk);
        at += chunk;
    }
    out.append(png.sliced(at));
    return out;
}
}

Header readHeader(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return {};
    if (file.read(kPngSignatureSize) != QByteArrayView(kPngSignature, kPngSignatureSize)) {
        return {QImageReader(path).size()};
    }
    Header header;
    while (true) {
        const QByteArray head = file.read(8);
        if (head.size() < 8)
            break;
        const qint64 length = bigEndian(head.constData());
        const QByteArrayView type(head.constData() + 4, 4);
        if (type == "IHDR" && length >= 8) {
            const QByteArray ihdr = file.read(8);
            if (ihdr.size() == 8)
                header.size = QSize(int(bigEndian(ihdr.constData())), int(bigEndian(ihdr.constData() + 4)));
            if (!file.seek(file.pos() + length - 8 + 4))
                break;
            continue;
        }
        if (type == "IDAT" || type == "IEND")
            break;
        if (isPngText(type))
            header.pngText += length;
        if (!file.seek(file.pos() + length + 4))
            break;
    }
    return header;
}

void allowLargeImages()
{
    if (QImageReader::allocationLimit() < kAllocationLimitMb)
        QImageReader::setAllocationLimit(kAllocationLimitMb);
}

int maxSide(const QString &kind)
{
    if (kind == u"hero")
        return 3840;
    if (kind == u"logo")
        return 1280;
    if (kind == u"cover")
        return 1800;
    return 512;
}

QString copy(const QString &source, const QString &target, const QString &kind, bool *changed)
{
    if (changed)
        *changed = true;
    const QString name = QFileInfo(source).fileName();
    QFile in(source);
    if (!in.open(QIODevice::ReadOnly))
        return tr("Could not copy %1").arg(name);
    const QByteArray original = in.readAll();
    in.close();
    QByteArray bytes = stripPngText(original);
    const bool stripText = original.size() - bytes.size() > kMaxPngText;
    QBuffer buffer(&bytes);
    QImageReader reader(&buffer);
    const QSize size = reader.size();
    const int limit = maxSide(kind);
    const bool trim = kind == u"logo";
    const bool oversized = size.isValid() && (size.width() > limit || size.height() > limit);
    const auto copyAsIs = [&] {
        if (changed && !stripText) {
            *changed = false;
            return QString();
        }
        QFile::remove(target);
        QFile out(target);
        if (out.open(QIODevice::WriteOnly) && out.write(bytes) == bytes.size() && out.flush())
            return QString();
        out.remove();
        return tr("Could not copy %1").arg(name);
    };
    if (!size.isValid() || (!oversized && !trim))
        return copyAsIs();

    allowLargeImages();
    const QByteArray format = reader.format();
    reader.setAutoTransform(true);
    QImage image = reader.read();
    if (image.isNull())
        return tr("%1 can't be shown: %2").arg(name, reader.errorString());
    if (trim) {
        const QRect bounds = visibleBounds(image);
        if (bounds != image.rect())
            image = image.copy(bounds);
        else if (!oversized)
            return copyAsIs();
    }
    if (image.width() > limit || image.height() > limit)
        image = image.scaled(QSize(limit, limit), Qt::KeepAspectRatio, Qt::SmoothTransformation);

    QFile::remove(target);
    QImageWriter writer(target, format);
    if (format != "png")
        writer.setQuality(92);
    if (!writer.write(image)) {
        QFile::remove(target);
        return tr("Could not save a smaller copy of %1: %2").arg(name, writer.errorString());
    }
    return {};
}
}
