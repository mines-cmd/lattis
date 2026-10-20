#pragma once

#include "parsed.h"

#include <QByteArray>
#include <QString>

// See docs/code/metadata.md.
struct MetadataSettings
{
    bool lookUpNewGames = true;

    bool operator==(const MetadataSettings &) const = default;
};

Parsed<MetadataSettings> parseMetadataSettings(const QByteArray &bytes, const QString &displayPath);
