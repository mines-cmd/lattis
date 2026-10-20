#pragma once

#include <QString>
#include <QStringList>

// See docs/code/core.md.
namespace OldName {
struct Folders {
    QString dataFrom, dataTo;
    QString stateFrom, stateTo;
};

QStringList move(const Folders &folders);
Folders folders(const QString &runtimeDir);
}
