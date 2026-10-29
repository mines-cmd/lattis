#pragma once

#include <QFileInfo>
#include <QHash>
#include <QObject>
#include <QSet>
#include <QString>
#include <QStringList>

class QMediaPlayer;
class QTimer;
class QVideoSink;

// See docs/code/library.md.
namespace VideoThumbnails {
struct Cached
{
    QString thumbnail; // empty: not made
    qint64 durationMs = 0;
    bool failed = false;
};

QString key(const QFileInfo &video);

class Cache
{
public:
    explicit Cache(const QString &folder);

    Cached find(const QString &path) const;

private:
    QHash<QString, Cached> m_entries;
};

QString thumbnailPath(const QString &folder, const QFileInfo &video, qint64 durationMs);
QString failedPath(const QString &folder, const QFileInfo &video);
}

// Makes thumbnails one at a time on the thread it lives in (it needs that thread's event loop).
class VideoThumbnailMaker : public QObject
{
    Q_OBJECT

public:
    explicit VideoThumbnailMaker(const QString &folder, QObject *parent = nullptr);

    void request(const QString &path);

signals:
    void made(const QString &path, const QString &thumbnail, qint64 durationMs);

private:
    void next();
    void finish(const QString &thumbnail, qint64 durationMs);
    void onFrame();

    QString m_folder;
    QStringList m_queue;
    QSet<QString> m_queued;
    QMediaPlayer *m_player = nullptr;
    QVideoSink *m_sink = nullptr;
    QTimer *m_timeout = nullptr;
    QString m_current;
    qint64 m_durationMs = 0;
    qint64 m_targetUs = -1;
};
