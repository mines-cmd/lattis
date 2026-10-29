#pragma once

#include <QDateTime>
#include <QList>
#include <QString>
#include <QStringList>

// See docs/code/library.md.
namespace MediaFolders {
constexpr int kLimit = 2000;

struct File
{
    QString name;
    QString path; // canonical
    QString folder;
    bool video = false;
    QDateTime time;

    bool operator==(const File &) const = default;
};

struct Listing
{
    QList<File> files; // newest first
    bool limited = false;
};

Listing list(const QStringList &folders, const QString &home);
}
