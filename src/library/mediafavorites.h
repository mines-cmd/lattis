#pragma once

#include <QSet>
#include <QString>

// See docs/code/library.md.
namespace MediaFavorites {
QSet<QString> read(const QString &path, const QString &home);
bool write(const QString &path, const QSet<QString> &favorites, const QString &home);
}
