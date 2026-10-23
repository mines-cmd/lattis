#pragma once

#include <QString>

// See docs/code/library.md.
struct RomInfo
{
    QString title;
    QString titleId;

    bool operator==(const RomInfo &) const = default;
};

namespace RomInfoReader {
// Whether any game of this platform carries its name (read(), readFolder()).
bool readsPlatform(const QString &platform);

// Whether read() has a parser for files with this extension on this platform.
bool reads(const QString &platform, const QString &extension);

// What a file says about itself. Empty fields where it says nothing or can't be read.
RomInfo read(const QString &path, const QString &platform);

// A folder game's dump (PS3, PS4): its PARAM.SFO or param.json.
RomInfo readFolder(const QString &folder, const QString &platform);
}
