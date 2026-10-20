#pragma once

#include <QDateTime>
#include <QList>
#include <QString>
#include <QStringList>

#include <functional>

// See docs/code/backup.md.
namespace MovedAside {
struct Import
{
    QString kind;
    QString archive;
    QDateTime time;
    QStringList paths;
    bool notice = true;
};

QString file(const QString &stateDir, const QString &dataDirKey);

QList<Import> read(const QString &file);
bool write(const QString &file, const QList<Import> &imports, QString *error = nullptr);
bool record(const QString &file, const Import &import, QString *error = nullptr);

QList<Import> present(QList<Import> imports);

qint64 bytes(const QStringList &paths);

using Trash = std::function<bool(const QString &path, QString *error)>;
bool moveToTrash(const QString &path, QString *error);

QString remove(const QStringList &paths, const Trash &trash = moveToTrash);
}
