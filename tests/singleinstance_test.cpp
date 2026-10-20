#include "singleinstance.h"

#include <QElapsedTimer>
#include <QFile>
#include <QProcess>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QThread>

#include <atomic>
#include <memory>

class SingleInstanceTest : public QObject
{
    Q_OBJECT

private slots:
    void lockCantBeTakenTwice();
    void dataDirsLockIndependently();
    void staleLockIsTakenOver();
    void activateReachesTheRunningCopy();
    void activateWithoutRunningCopyFails();
    void restartWaitsForTheLock();
    void waitingForTheLockTimesOut();

private:
    QTemporaryDir m_dir;
};

void SingleInstanceTest::lockCantBeTakenTwice()
{
    SingleInstance first(m_dir.path(), QStringLiteral("aaaa"));
    SingleInstance second(m_dir.path(), QStringLiteral("aaaa"));
    QVERIFY(first.tryLock());
    QVERIFY(!second.tryLock());
}

void SingleInstanceTest::dataDirsLockIndependently()
{
    SingleInstance real(m_dir.path(), QStringLiteral("bbbb"));
    SingleInstance scratch(m_dir.path(), QStringLiteral("cccc"));
    QVERIFY(real.tryLock());
    QVERIFY(scratch.tryLock());
}

void SingleInstanceTest::staleLockIsTakenOver()
{
    const QString lockPath = m_dir.filePath(QStringLiteral("lattis-dddd.lock"));

    // A lock file as a crashed copy leaves it: the right format, but its process is gone.
    QByteArray contents;
    {
        QLockFile lock(lockPath);
        QVERIFY(lock.tryLock(0));
        QFile file(lockPath);
        QVERIFY(file.open(QIODevice::ReadOnly));
        contents = file.readAll();
    }
    QProcess process;
    process.start(QStringLiteral("true"));
    QVERIFY(process.waitForStarted());
    const qint64 deadPid = process.processId();
    QVERIFY(process.waitForFinished());
    contents.replace(0, contents.indexOf('\n'), QByteArray::number(deadPid));
    QFile file(lockPath);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write(contents);
    file.close();

    SingleInstance instance(m_dir.path(), QStringLiteral("dddd"));
    QVERIFY(instance.tryLock());
}

void SingleInstanceTest::activateReachesTheRunningCopy()
{
    SingleInstance running(m_dir.path(), QStringLiteral("eeee"));
    QVERIFY(running.tryLock());
    QVERIFY(running.listen());
    QSignalSpy activated(&running, &SingleInstance::activateRequested);

    SingleInstance second(m_dir.path(), QStringLiteral("eeee"));
    QVERIFY(!second.tryLock());
    // activateRunning() blocks, so it runs on a worker thread while this thread's event
    // loop lets the running copy answer.
    bool sent = false;
    QThread *client = QThread::create([&] { sent = second.activateRunning(); });
    client->start();
    QTRY_COMPARE(activated.count(), 1);
    QVERIFY(client->wait(5000));
    delete client;
    QVERIFY(sent);
}

void SingleInstanceTest::restartWaitsForTheLock()
{
    auto quitting = std::make_unique<SingleInstance>(m_dir.path(), QStringLiteral("gggg"));
    QVERIFY(quitting->tryLock());

    // The restarted copy waits on a worker thread while the old one quits on this one.
    SingleInstance restarted(m_dir.path(), QStringLiteral("gggg"));
    std::atomic_bool locked = false;
    QThread *waiter = QThread::create([&] { locked = restarted.waitForLock(10000); });
    waiter->start();
    QTest::qWait(300);
    QVERIFY(!locked);
    quitting.reset();
    QVERIFY(waiter->wait(5000));
    delete waiter;
    QVERIFY(locked);

    // It holds it now: another start can't take it.
    SingleInstance third(m_dir.path(), QStringLiteral("gggg"));
    QVERIFY(!third.tryLock());
}

void SingleInstanceTest::waitingForTheLockTimesOut()
{
    SingleInstance running(m_dir.path(), QStringLiteral("hhhh"));
    QVERIFY(running.tryLock());
    SingleInstance restarted(m_dir.path(), QStringLiteral("hhhh"));
    QElapsedTimer timer;
    timer.start();
    QVERIFY(!restarted.waitForLock(300));
    QVERIFY(timer.elapsed() >= 250);
}

void SingleInstanceTest::activateWithoutRunningCopyFails()
{
    SingleInstance lonely(m_dir.path(), QStringLiteral("ffff"));
    QVERIFY(!lonely.activateRunning());
}

QTEST_GUILESS_MAIN(SingleInstanceTest)
#include "singleinstance_test.moc"
