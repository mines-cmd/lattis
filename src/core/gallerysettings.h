#pragma once

#include "parsed.h"

#include <QByteArray>
#include <QString>
#include <QStringList>

// See docs/code/core.md.
struct GallerySettings
{
    QString group = QStringLiteral("month");
    QString sort = QStringLiteral("newest");
    int tileWidth = 0;   // 0 = the theme's default
    QStringList foldedSections;
    bool captions = true;

    bool operator==(const GallerySettings &) const = default;
};

const QStringList &galleryGroups();
const QStringList &gallerySorts();
const QStringList &gallerySections();

Parsed<GallerySettings> parseGallerySettings(const QByteArray &bytes, const QString &displayPath);
