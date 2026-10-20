#pragma once

#include <QFileSystemWatcher>
#include <QFutureWatcher>
#include <QHash>
#include <QList>
#include <QObject>
#include <QString>
#include <QTimer>

// See docs/code/steam.md.
class SteamDownloads : public QObject
{
    Q_OBJECT

public:
    struct App
    {
        qint64 appId = 0;
        QString downloadDir;
    };

    struct Progress
    {
        bool running = false;
        int percent = -1; // -1: unknown

        bool operator==(const Progress &) const = default;
    };

    explicit SteamDownloads(QObject *parent = nullptr);

    // The apps Steam has started updating; with none, nothing is watched or polled.
    void setApps(const QString &logPath, const QList<App> &apps);
    QHash<qint64, Progress> progress() const { return m_progress; }

    static qint64 directorySize(const QString &path);
    struct LogState
    {
        bool running = false;
        qint64 stageTotal = 0;
    };
    static QHash<qint64, LogState> parseLog(const QByteArray &tail, const QList<qint64> &appIds);

signals:
    void changed();

private:
    void readLog();
    void measure();
    void setProgress(const QHash<qint64, Progress> &progress);

    QString m_logPath;
    QList<App> m_apps;
    QHash<qint64, LogState> m_log;
    QHash<qint64, Progress> m_progress;
    QFileSystemWatcher m_watcher;
    QTimer m_logTimer;
    QTimer m_pollTimer;
    QFutureWatcher<QHash<qint64, qint64>> m_sizes;
};
