#pragma once

#include <QString>

// See docs/code/core.md.
namespace Autostart {
struct Entry
{
    bool exists = false;
    bool ours = false;
    bool enabled = true;
    QString exec;
};

QString entryExec(const QString &program, const QString &dataDir = {});
Entry readEntry(const QString &folder);
QString writeEntry(const QString &folder, const QString &program, const QString &dataDir = {});
QString removeEntry(const QString &folder);
QString autostartFolder();
QString currentProgram();
}
