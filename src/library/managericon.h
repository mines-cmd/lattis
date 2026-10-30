#pragma once

#include <QByteArray>
#include <QImage>
#include <QString>
#include <QStringList>

// See docs/code/library.md.
namespace ManagerIcon {
// The largest icon of the first icon group in a Windows program's resources; null when there is none
// or the file isn't a PE.
QImage fromExe(const QString &exe);

struct Entry
{
    QStringList exec;
    QString icon;
};

Entry parseDesktop(const QByteArray &text);
// Whether the entry starts `program`: a path, a bare command, or a Flatpak app id.
bool launches(const Entry &entry, const QString &program);

// The icon of the .desktop entry that launches `program`, looked up in `appDirs` (applications folders)
// and `iconDirs` (icon theme roots); the system's and the user's when empty.
QImage fromDesktop(const QString &program, const QStringList &appDirs = {}, const QStringList &iconDirs = {});

// The PNG of a manager's icon under `cacheDir`, made on the first call; empty when the program has none.
QString cached(const QString &cacheDir, const QString &id, const QString &program, bool proton);
void forget(const QString &cacheDir, const QString &id);
}
