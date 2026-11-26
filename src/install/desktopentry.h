#pragma once

#include <QString>
#include <QStringList>

// See docs/code/install.md.

namespace DesktopEntry {
struct Made
{
    QString desktop;
    QString icon;
};

// The AppImage's own .desktop file with Exec pointing at it, plus its icon, in the user's applications menu.
// Empty `desktop` and a reason in `error` when the AppImage has none or can't be read.
Made make(const QString &id, const QString &appImage, const QString &dataDir, QString *error,
          const QString &icon = {});

// The file with every Exec pointing at the AppImage, TryExec dropped and Icon set.
QByteArray rewrite(const QByteArray &source, const QString &appImage, const QString &icon);
QByteArray withIcon(const QByteArray &source, const QString &icon);

void remove(const Made &made);

// The running Lattis AppImage's own menu entry (`lattis-lattis.desktop`). Written when missing, when its Exec no
// longer runs `appImage` or when its Icon isn't `icon`; nothing happens when it is already right. `made.desktop`
// is empty when nothing was written.
Made ensureOwn(const QString &appImage, const QString &dataDir, const QString &icon, QString *error);

void removeOwn(const QString &dataDir);

QString userCopyPath(const QString &dataDir);
bool isUserCopy(const QString &path);
QString systemEntry(const QStringList &dataDirs);
bool ensureUserCopy(const QString &systemEntry, const QString &dataDir, const QString &icon, QString *error);
void removeUserCopy(const QString &dataDir);
}
