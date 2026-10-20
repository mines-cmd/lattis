#pragma once

#include <QString>

// See docs/code/core.md.
namespace AppPaths {
QString dataDir();
QString cacheDir();
QString stateDir();
QString dataDirKey(const QString &dataDir);
}
