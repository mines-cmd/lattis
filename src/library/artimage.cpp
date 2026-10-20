#include "artimage.h"

#include <algorithm>

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
    QImageReader reader(source);
    const QSize size = reader.size();
    const int limit = maxSide(kind);
    const bool trim = kind == u"logo";
    const bool oversized = size.isValid() && (size.width() > limit || size.height() > limit);
    const auto copyAsIs = [&] {
        if (changed) {
            *changed = false;
            return QString();
        }
        QFile::remove(target);
        return QFile::copy(source, target) ? QString() : tr("Could not copy %1").arg(name);
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
