#pragma once

#include <QLocalServer>
#include <QLockFile>
#include <QObject>

#include <memory>

// See docs/code/core.md.
class SingleInstance : public QObject
{
    Q_OBJECT

public:
    SingleInstance(const QString &runtimeDir, const QString &key, QObject *parent = nullptr);
    ~SingleInstance() override;

    bool tryLock();
    bool waitForLock(int timeoutMs);
    bool activateRunning();
    bool listen();

signals:
    void activateRequested();

private:
    QString m_lockPath;
    QString m_socketPath;
    QLockFile m_lock;
    std::unique_ptr<QLocalServer> m_server;
};
