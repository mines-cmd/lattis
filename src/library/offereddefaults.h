#pragma once

#include "tomlpatch.h"

#include <QString>
#include <QStringList>

// See docs/code/library.md.
namespace OfferedDefaults {
QStringList bootstrap();

TomlPatch::Result apply(const QString &text, const QString &defaults, const QStringList &bootstrap,
                        QStringList *added = nullptr);
}
