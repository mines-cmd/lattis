#pragma once

#include <QString>

// See docs/code/install.md.

namespace DesktopEntry {
struct Made
{
    QString desktop;
    QString icon;
};

// The AppImage's own .desktop file with Exec pointing at it, plus its icon, in the user's applications menu.
// Empty `desktop` and a reason in `error` when the AppImage has none or can't be read.
Made make(const QString &id, const QString &appImage, const QString &dataDir, QString *error);

// The file with every Exec pointing at the AppImage, TryExec dropped and Icon set.
QByteArray rewrite(const QByteArray &source, const QString &appImage, const QString &icon);

void remove(const Made &made);
}
