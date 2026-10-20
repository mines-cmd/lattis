#include "steamdownloads.h"

#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QtConcurrent>

#include <algorithm>

#include <sys/stat.h>

namespace {
// Steam writes a line to content_log.txt for each change but a manifest only when an update
// starts or stops, so the tail is where "running" shows.
constexpr qint64 kLogTail = 512 * 1024;
}

SteamDownloads::SteamDownloads(QObject *parent)
    : QObject(parent)
{
    m_logTimer.setSingleShot(true);
    m_logTimer.setInterval(300);
    connect(&m_logTimer, &QTimer::timeout, this, &SteamDownloads::readLog);
    connect(&m_watcher, &QFileSystemWatcher::fileChanged, this, [this](const QString &path) {
        // A rotated log is replaced, which drops the watch.
        if (!m_watcher.files().contains(path) && QFileInfo::exists(path))
            m_watcher.addPath(path);
        m_logTimer.start();
    });
    m_pollTimer.setInterval(1000);
    connect(&m_pollTimer, &QTimer::timeout, this, &SteamDownloads::measure);
    connect(&m_sizes, &QFutureWatcher<QHash<qint64, qint64>>::finished, this, [this] {
        const QHash<qint64, qint64> sizes = m_sizes.result();
        QHash<qint64, Progress> next = m_progress;
        for (auto it = sizes.cbegin(); it != sizes.cend(); ++it) {
            const qint64 total = m_log.value(it.key()).stageTotal;
            if (total > 0)
                next[it.key()].percent = int(std::clamp<qint64>(it.value() * 100 / total, 0, 100));
        }
        setProgress(next);
    });
}

void SteamDownloads::setApps(const QString &logPath, const QList<App> &apps)
{
    if (logPath == m_logPath && apps.size() == m_apps.size()
        && std::equal(apps.cbegin(), apps.cend(), m_apps.cbegin(), [](const App &a, const App &b) {
               return a.appId == b.appId && a.downloadDir == b.downloadDir;
           }))
        return;
    m_apps = apps;
    if (!m_watcher.files().isEmpty())
        m_watcher.removePaths(m_watcher.files());
    m_logPath = logPath;
    if (m_apps.isEmpty()) {
        m_pollTimer.stop();
        m_log.clear();
        setProgress({});
        return;
    }
    if (QFileInfo::exists(m_logPath))
        m_watcher.addPath(m_logPath);
    readLog();
}

void SteamDownloads::readLog()
{
    QByteArray tail;
    QFile file(m_logPath);
    if (file.open(QIODevice::ReadOnly)) {
        file.seek(std::max<qint64>(0, file.size() - kLogTail));
        tail = file.readAll();
    }
    QList<qint64> ids;
    for (const App &app : std::as_const(m_apps))
        ids.append(app.appId);
    m_log = parseLog(tail, ids);

    QHash<qint64, Progress> next;
    bool anyRunning = false;
    for (const App &app : std::as_const(m_apps)) {
        Progress p = m_progress.value(app.appId);
        p.running = m_log.value(app.appId).running;
        anyRunning = anyRunning || p.running;
        next.insert(app.appId, p);
    }
    setProgress(next);
    if (anyRunning && !m_pollTimer.isActive()) {
        m_pollTimer.start();
        measure();
    } else if (!anyRunning) {
        m_pollTimer.stop();
        measure();
    }
}

void SteamDownloads::measure()
{
    if (m_sizes.isRunning())
        return;
    QList<App> apps = m_apps;
    m_sizes.setFuture(QtConcurrent::run([apps] {
        QHash<qint64, qint64> sizes;
        for (const App &app : apps)
            sizes.insert(app.appId, directorySize(app.downloadDir));
        return sizes;
    }));
}

void SteamDownloads::setProgress(const QHash<qint64, Progress> &progress)
{
    if (progress == m_progress)
        return;
    m_progress = progress;
    emit changed();
}

qint64 SteamDownloads::directorySize(const QString &path)
{
    // Allocated bytes, not file sizes: Steam preallocates the files, so their sizes run ahead of
    // what's been written, and the blocks match the manifest's BytesStaged.
    qint64 total = 0;
    QDirIterator it(path, QDir::Files | QDir::Hidden, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        struct stat info;
        if (::lstat(QFile::encodeName(it.next()).constData(), &info) == 0)
            total += qint64(info.st_blocks) * 512;
    }
    return total;
}

QHash<qint64, SteamDownloads::LogState> SteamDownloads::parseLog(const QByteArray &tail, const QList<qint64> &appIds)
{
    static const QRegularExpression stateRe(QStringLiteral("AppID (\\d+) state changed : (.*)"));
    static const QRegularExpression startedRe(QStringLiteral("AppID (\\d+) update started : .* stage \\d+/(\\d+)"));
    QHash<qint64, LogState> out;
    for (const qint64 id : appIds)
        out.insert(id, {});
    const QString text = QString::fromUtf8(tail);
    // Lines are in time order, so the last match for an app wins.
    for (const QStringView line : QStringView(text).split(u'\n')) {
        if (const auto m = stateRe.matchView(line); m.hasMatch()) {
            const auto it = out.find(m.captured(1).toLongLong());
            if (it != out.end())
                it->running = m.capturedView(2).contains(u"Update Running");
        } else if (const auto s = startedRe.matchView(line); s.hasMatch()) {
            const auto it = out.find(s.captured(1).toLongLong());
            if (it != out.end())
                it->stageTotal = s.captured(2).toLongLong();
        }
    }
    return out;
}
