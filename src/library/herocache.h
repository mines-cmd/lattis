#pragma once

#include <QHash>
#include <QSet>
#include <QString>

// See docs/code/library.md.
namespace HeroCache {
constexpr int kWidth = 2560;
constexpr int kQuality = 90;

QString stem(const QString &id, qint64 modified);

bool isBaselineJpeg(const QString &path);

struct Result
{
    QString path;
    QString error;
};

QHash<QString, QString> list(const QString &cacheDir);

void prune(const QString &cacheDir, const QSet<QString> &keep);

// Always a baseline JPEG. An empty path with no error means the art needs no copy: it is 2560 px wide or
// narrower and already a baseline JPEG.
Result make(const QString &source, const QString &id, qint64 modified, const QString &cacheDir);
}
