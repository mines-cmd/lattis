#include "singleinstance.h"

#include <QDir>
#include <QElapsedTimer>
#include <QLocalSocket>
#include <QThread>

namespace {
const QByteArray kActivate = QByteArrayLiteral("activate\n");
}

SingleInstance::SingleInstance(const QString &runtimeDir, const QString &key, QObject *parent)
    : QObject(parent)
    , m_lockPath(QStringLiteral("%1/lattis-%2.lock").arg(runtimeDir, key))
    , m_socketPath(QStringLiteral("%1/lattis-%2.socket").arg(runtimeDir, key))
    , m_lock(m_lockPath)
{
    // Only a dead owner makes the lock stale, never its age: the app runs for days.
    m_lock.setStaleLockTime(0);
}

SingleInstance::~SingleInstance()
{
    if (m_server)
        m_server->close();
}

bool SingleInstance::tryLock()
{
    QDir().mkpath(QFileInfo(m_lockPath).absolutePath());
    return m_lock.tryLock(0);
}

bool SingleInstance::waitForLock(int timeoutMs)
{
    QDir().mkpath(QFileInfo(m_lockPath).absolutePath());
    return m_lock.tryLock(timeoutMs);
}

bool SingleInstance::activateRunning()
{
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < 3000) {
        QLocalSocket socket;
        socket.connectToServer(m_socketPath);
        if (socket.waitForConnected(500)) {
            socket.write(kActivate);
            const bool sent = socket.waitForBytesWritten(1000);
            socket.disconnectFromServer();
            if (sent)
                return true;
        }
        QThread::msleep(100);
    }
    return false;
}

bool SingleInstance::listen()
{
    // We hold the lock, so a socket file left here is from a copy that crashed.
    QLocalServer::removeServer(m_socketPath);
    m_server = std::make_unique<QLocalServer>();
    m_server->setSocketOptions(QLocalServer::UserAccessOption);
    connect(m_server.get(), &QLocalServer::newConnection, this, [this] {
        while (QLocalSocket *socket = m_server->nextPendingConnection()) {
            connect(socket, &QLocalSocket::disconnected, socket, &QObject::deleteLater);
            connect(socket, &QLocalSocket::readyRead, this, [this, socket] {
                while (socket->canReadLine()) {
                    if (socket->readLine() == kActivate)
                        emit activateRequested();
                }
            });
        }
    });
    return m_server->listen(m_socketPath);
}
