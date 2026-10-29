#pragma once

#include <QList>
#include <QString>

#include <optional>

// See docs/code/install.md.
namespace Versions {
std::optional<QList<int>> parse(const QString &text);
bool newer(const QString &candidate, const QString &current);
}
