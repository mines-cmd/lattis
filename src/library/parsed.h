#pragma once

#include <QStringList>

// See docs/code/library.md.
template<typename T>
struct Parsed
{
    bool ok = false;
    T value{};
    QStringList warnings;
};
