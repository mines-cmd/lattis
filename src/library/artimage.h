#pragma once

#include <QSize>
#include <QString>

// See docs/code/library.md.
namespace ArtImage {
// Qt refuses to decode an image over 256 MB; big art needs more while it's read (the
// 13471×6421 logo takes 346 MB), even when only a scaled copy is kept.
constexpr int kAllocationLimitMb = 1024;
void allowLargeImages();

int maxSide(const QString &kind);

// Qt reads a PNG's text chunks into memory whenever it opens one, even for its size; a few Steam
// logos carry megabytes of compressed metadata that take hundreds of MB that way.
constexpr qint64 kMaxPngText = 64 * 1024;

struct Header
{
    QSize size;
    qint64 pngText = 0; // bytes of tEXt, zTXt and iTXt chunks
};
Header readHeader(const QString &path);

// A PNG loses its text chunks on the way.
// With `changed`, an image that needs no shrinking, trimming or text stripping isn't written, and *changed is false.
QString copy(const QString &source, const QString &target, const QString &kind, bool *changed = nullptr);
}
