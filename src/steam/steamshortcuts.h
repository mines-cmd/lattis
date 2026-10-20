#pragma once

#include "binaryvdf.h"

#include <QList>
#include <QSet>
#include <QString>

// See docs/code/steam.md.
struct SteamShortcut
{
    quint32 appId = 0;       // Steam stores it as a signed int32
    QString name;
    QString exe;
    QString exeArgs;
    QString startDir;
    QString icon;
    QString launchOptions;
    qint64 lastPlayTime = 0;
    bool hidden = false;

    bool operator==(const SteamShortcut &) const = default;
};

namespace SteamShortcuts {
QList<SteamShortcut> read(const BinaryVdf::Node &root);

quint64 gameId(quint32 appId);

inline qint32 signedAppId(quint32 appId)
{
    return qint32(appId);
}

QString exeField(const QString &program, const QString &args = {});
void splitExe(const QString &field, QString *program, QString *args);
QString unquoted(const QString &path);

quint32 newAppId(const QString &exeField, const QString &name, const QSet<quint32> &taken);

bool add(BinaryVdf::Node *root, const SteamShortcut &shortcut);
bool update(BinaryVdf::Node *root, const SteamShortcut &shortcut);
bool remove(BinaryVdf::Node *root, quint32 appId);
}
