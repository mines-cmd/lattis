#pragma once

#include "parsed.h"

#include <QByteArray>
#include <QList>
#include <QString>
#include <QStringList>

#include <optional>

// See docs/code/saves.md.

struct SavesWhen
{
    QString os;    // "windows" or "linux"; empty = any
    QString store; // empty = any
};

struct SavesPath
{
    QString path; // with the manifest's placeholders
    QList<SavesWhen> when; // empty = always
};

struct SavesGame
{
    QString name;
    QStringList aliases;
    int steamId = 0; // 0 = none
    int gogId = 0;
    QStringList installDirs;
    QList<SavesPath> paths;
};

struct SavesIndex
{
    QList<SavesGame> games;
};

namespace SavesIndexFile {
// Bump whenever what the parser keeps changes: the manifest is then read again from the file on disk.
constexpr quint32 kFormatVersion = 1;

Parsed<SavesIndex> parse(const QByteArray &yaml);

bool save(const QString &path, const SavesIndex &index);
std::optional<SavesIndex> load(const QString &path);
}
