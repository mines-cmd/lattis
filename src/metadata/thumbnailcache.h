#pragma once

#include "metadatahttp.h"

#include <QNetworkAccessManager>
#include <QNetworkDiskCache>
#include <QQmlNetworkAccessManagerFactory>

// See docs/code/metadata.md.
class ThumbnailCache : public QQmlNetworkAccessManagerFactory
{
public:
    explicit ThumbnailCache(const QString &cacheDir) : m_dir(cacheDir + QStringLiteral("/thumbnails")) {}

    QNetworkAccessManager *create(QObject *parent) override
    {
        qCDebug(lcMetadata) << "thumbnail network manager created";
        auto *network = new QNetworkAccessManager(parent);
        auto *cache = new QNetworkDiskCache(network);
        cache->setCacheDirectory(m_dir);
        cache->setMaximumCacheSize(200 * 1024 * 1024);
        network->setCache(cache);
        return network;
    }

private:
    const QString m_dir;
};
