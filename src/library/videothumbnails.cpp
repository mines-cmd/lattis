#include "videothumbnails.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QImage>
#include <QMediaPlayer>
#include <QTimer>
#include <QUrl>
#include <QVideoFrame>
#include <QVideoSink>

namespace {
constexpr int kThumbnailWidth = 480;
constexpr qint64 kFrameAtMs = 1000;
}

namespace VideoThumbnails {
QString key(const QFileInfo &video)
{
    QCryptographicHash hash(QCryptographicHash::Sha1);
    hash.addData(video.absoluteFilePath().toUtf8());
    hash.addData(QByteArray::number(video.size()));
    hash.addData(QByteArray::number(video.lastModified().toMSecsSinceEpoch()));
    return QString::fromLatin1(hash.result().toHex().left(24));
}

QString thumbnailPath(const QString &folder, const QFileInfo &video, qint64 durationMs)
{
    return QStringLiteral("%1/%2.%3.jpg").arg(folder, key(video)).arg(durationMs);
}

QString failedPath(const QString &folder, const QFileInfo &video)
{
    return QStringLiteral("%1/%2.failed").arg(folder, key(video));
}

Cache::Cache(const QString &folder)
{
    const QDir dir(folder);
    for (const QString &name : dir.entryList(QDir::Files)) {
        const QStringList parts = name.split(u'.');
        if (parts.size() == 2 && parts[1] == QLatin1String("failed")) {
            m_entries[parts[0]].failed = true;
        } else if (parts.size() == 3 && parts[2] == QLatin1String("jpg")) {
            Cached &c = m_entries[parts[0]];
            c.thumbnail = dir.filePath(name);
            c.durationMs = parts[1].toLongLong();
        }
    }
}

Cached Cache::find(const QString &path) const
{
    return m_entries.value(key(QFileInfo(path)));
}
}

VideoThumbnailMaker::VideoThumbnailMaker(const QString &folder, QObject *parent) : QObject(parent), m_folder(folder) {}

void VideoThumbnailMaker::request(const QString &path)
{
    if (m_queued.contains(path))
        return;
    m_queued.insert(path);
    m_queue.append(path);
    next();
}

void VideoThumbnailMaker::next()
{
    if (!m_current.isEmpty() || m_queue.isEmpty())
        return;
    if (!m_player) {
        m_player = new QMediaPlayer(this);
        m_sink = new QVideoSink(this);
        m_player->setVideoSink(m_sink);
        m_timeout = new QTimer(this);
        m_timeout->setSingleShot(true);
        m_timeout->setInterval(6000);
        connect(m_timeout, &QTimer::timeout, this, [this] { finish({}, 0); });
        connect(m_player, &QMediaPlayer::errorOccurred, this, [this] {
            if (!m_current.isEmpty())
                finish({}, 0);
        });
        connect(m_player, &QMediaPlayer::mediaStatusChanged, this, [this](QMediaPlayer::MediaStatus status) {
            if (m_current.isEmpty() || m_targetUs >= 0)
                return;
            if (status == QMediaPlayer::InvalidMedia)
                return finish({}, 0);
            if (status != QMediaPlayer::LoadedMedia)
                return;
            m_durationMs = m_player->duration();
            if (m_durationMs <= 0)
                return finish({}, 0);
            const qint64 at = qMin(kFrameAtMs, m_durationMs / 2);
            m_targetUs = at * 1000;
            if (at > 0)
                m_player->setPosition(at);
            m_player->pause();
        });
        connect(m_sink, &QVideoSink::videoFrameChanged, this, [this] { onFrame(); });
    }
    m_current = m_queue.takeFirst();
    m_targetUs = -1;
    m_durationMs = 0;
    const QFileInfo info(m_current);
    if (!info.exists() || QFile::exists(VideoThumbnails::failedPath(m_folder, info))) {
        finish({}, 0);
        return;
    }
    m_timeout->start();
    m_player->setSource(QUrl::fromLocalFile(m_current));
}

void VideoThumbnailMaker::onFrame()
{
    if (m_current.isEmpty() || m_targetUs < 0)
        return;
    const QVideoFrame frame = m_sink->videoFrame();
    // Frames from before the seek landed are still arriving.
    if (!frame.isValid() || (m_targetUs > 0 && frame.startTime() < m_targetUs - 400000))
        return;
    QImage image = frame.toImage();
    if (image.isNull())
        return finish({}, 0);
    if (image.width() > kThumbnailWidth)
        image = image.scaledToWidth(kThumbnailWidth, Qt::SmoothTransformation);
    QDir().mkpath(m_folder);
    const QString out = VideoThumbnails::thumbnailPath(m_folder, QFileInfo(m_current), m_durationMs);
    finish(image.save(out, "JPG", 80) ? out : QString(), m_durationMs);
}

void VideoThumbnailMaker::finish(const QString &thumbnail, qint64 durationMs)
{
    m_timeout->stop();
    const QString path = m_current;
    m_current.clear();
    m_queued.remove(path);
    m_targetUs = -1;
    if (m_player) {
        m_player->stop();
        m_player->setSource(QUrl());
    }
    if (thumbnail.isEmpty()) {
        QDir().mkpath(m_folder);
        QFile marker(VideoThumbnails::failedPath(m_folder, QFileInfo(path)));
        if (marker.open(QIODevice::WriteOnly))
            marker.close();
    } else {
        emit made(path, thumbnail, durationMs);
    }
    QMetaObject::invokeMethod(this, &VideoThumbnailMaker::next, Qt::QueuedConnection);
}
