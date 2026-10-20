#pragma once

#include <QList>
#include <QString>
#include <QStringList>

#include <functional>
#include <utility>

// See docs/code/library.md.
namespace Uninstall {
struct Places
{
    QString home;
    QString dataDir;
    QString steamRoot;
    QStringList steamLibraries;
    QStringList userDirs;
    QList<std::pair<QString, QString>> otherGames;
};

QString refusal(const QString &folder, const Places &places);

using Trash = std::function<bool(const QString &path, QString *error)>;
bool systemTrash(const QString &path, QString *error);

struct Removal
{
    QString error;
    bool needsPermanent = false; // there's no trash for it; only deleting it for good would do
};

Removal remove(const QString &folder, bool permanent, const Trash &trash);
}
