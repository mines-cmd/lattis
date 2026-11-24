#pragma once

#include "rominfo.h"

#include <QHash>
#include <QSet>
#include <QString>

#include <functional>

// See docs/code/library.md.
struct RomDbOptions
{
    QString dir;
    // Fetches a platform's database into `path`; empty means never (the scan then uses only what is in `dir`).
    std::function<bool(const QString &platform, const QString &path)> download;
    // Polled by the scan's thread between slow steps; true means stop and return what there is.
    std::function<bool()> cancelled;
};

class RomDb
{
public:
    explicit RomDb(RomDbOptions options = {});

    // Gives a file that carries no name the database's: `info->title` and `fromDatabase`, or the disc's
    // own name (`fallbackTitle`) when the database has none. `fileTitleId` is the title ID in the file name.
    void name(const QString &platform, const QString &fileTitleId, RomInfo *info);

    // Whether the platform's database says this file is a BIOS or a device, not a game (arcade zips).
    bool isNotAGame(const QString &platform, const QString &path);

    // The database file of a platform (GameCube and Wii share one); empty for platforms without.
    static QString fileOf(const QString &platform);
    static bool covers(const QString &platform);

    // Lines of `<key><TAB><name>`; `#` starts a comment line.
    static QHash<QString, QString> parse(const QByteArray &bytes);

private:
    const QHash<QString, QString> &database(const QString &platform);

    RomDbOptions m_options;
    QHash<QString, QHash<QString, QString>> m_loaded;
    QSet<QString> m_tried;
};
