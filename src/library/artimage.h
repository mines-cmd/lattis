#pragma once

#include <QString>

// See docs/code/library.md.
namespace ArtImage {
// Qt refuses to decode an image over 256 MB; big art needs more while it's read (the
// 13471×6421 logo takes 346 MB), even when only a scaled copy is kept.
constexpr int kAllocationLimitMb = 1024;
void allowLargeImages();

int maxSide(const QString &kind);

// With `changed`, an image that needs neither shrinking nor trimming isn't written, and *changed is false.
QString copy(const QString &source, const QString &target, const QString &kind, bool *changed = nullptr);
}
