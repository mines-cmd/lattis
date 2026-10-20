#pragma once

#include <QString>

// See docs/code/library.md.
namespace RomFolderMatch {
QString normalize(const QString &name);

QString byAlias(const QString &folderName);
}
