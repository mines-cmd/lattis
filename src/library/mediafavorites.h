#pragma once

#include <QSet>
#include <QString>

#include "parsed.h"

// See docs/code/library.md.
namespace MediaFavorites {
Parsed<QSet<QString>> read(const QString &path, const QString &home);
bool write(const QString &path, const QSet<QString> &favorites, const QString &home);
}
