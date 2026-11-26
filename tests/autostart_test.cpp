#include "autostart.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QScopeGuard>
#include <QTemporaryDir>
#include <QTest>

#include <unistd.h>

class AutostartTest : public QObject
{
    Q_OBJECT

private slots:
    void entryExec_data();
    void entryExec();
    void execDataDirReadsWhatEntryExecWrote_data();
    void execDataDirReadsWhatEntryExecWrote();
    void readWriteRemove();
    void readMissing();
    void readDisabled();
    void readForeign();
    void staleExecDetected();
    void writeFailures();
    void removeFailure();
    void currentProgramIsTheAppImage();
};

void AutostartTest::entryExec_data()
{
    QTest::addColumn<QString>("program");
    QTest::addColumn<QString>("dataDir");
    QTest::addColumn<QString>("expected");

    QTest::newRow("plain program") << "/usr/bin/lattis" << QString() << "/usr/bin/lattis";
    QTest::newRow("program with spaces") << "/home/user/my apps/lattis" << QString()
                                         << "\"/home/user/my apps/lattis\"";
    QTest::newRow("program with percent") << "/opt/la%ttis" << QString() << "/opt/la%%ttis";
    QTest::newRow("data dir with percent") << "/usr/bin/lattis" << "/mnt/100%d/lib"
                                           << "env LATTIS_DATA=/mnt/100%%d/lib /usr/bin/lattis";
    QTest::newRow("data dir with backslash") << "/usr/bin/lattis" << "/mnt/a\\b"
                                             << "env \"LATTIS_DATA=/mnt/a\\\\\\\\b\" /usr/bin/lattis";
    QTest::newRow("with data dir") << "/usr/bin/lattis" << "/home/user/.local/share/lattis"
                                   << "env LATTIS_DATA=/home/user/.local/share/lattis /usr/bin/lattis";
    QTest::newRow("data dir with spaces") << "/usr/bin/lattis" << "/home/user/my data"
                                          << "env \"LATTIS_DATA=/home/user/my data\" /usr/bin/lattis";
}

void AutostartTest::entryExec()
{
    QFETCH(QString, program);
    QFETCH(QString, dataDir);
    QFETCH(QString, expected);

    QCOMPARE(Autostart::entryExec(program, dataDir), expected);
}

void AutostartTest::execDataDirReadsWhatEntryExecWrote_data()
{
    QTest::addColumn<QString>("dataDir");
    QTest::newRow("plain") << "/home/user/.local/share/lattis";
    QTest::newRow("spaces") << "/home/user/my data";
    QTest::newRow("percent") << "/mnt/100%d/lib";
    QTest::newRow("backslash") << "/mnt/a\\b";
    QTest::newRow("quote and dollar") << "/mnt/\"a\"/$b";
}

void AutostartTest::execDataDirReadsWhatEntryExecWrote()
{
    QFETCH(QString, dataDir);
    QCOMPARE(Autostart::execDataDir(Autostart::entryExec("/opt/my apps/lattis", dataDir)), dataDir);
    QVERIFY(Autostart::execDataDir(Autostart::entryExec("/opt/my apps/lattis")).isEmpty());
    QVERIFY(Autostart::execDataDir(QStringLiteral("/usr/bin/lattis --minimized")).isEmpty());
}

void AutostartTest::readWriteRemove()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    // Nothing there yet.
    auto entry = Autostart::readEntry(dir.path());
    QVERIFY(!entry.exists);

    // Write our entry without a data dir.
    QVERIFY(Autostart::writeEntry(dir.path(), "/usr/bin/lattis").isEmpty());
    entry = Autostart::readEntry(dir.path());
    QVERIFY(entry.exists);
    QVERIFY(entry.ours);
    QVERIFY(entry.enabled);
    QCOMPARE(entry.exec, QStringLiteral("/usr/bin/lattis"));

    // Rewrite with a data dir.
    QVERIFY(Autostart::writeEntry(dir.path(), "/usr/bin/lattis", "/data/lib").isEmpty());
    entry = Autostart::readEntry(dir.path());
    QVERIFY(entry.ours);
    QCOMPARE(entry.exec, Autostart::entryExec("/usr/bin/lattis", "/data/lib"));

    // Remove.
    QVERIFY(Autostart::removeEntry(dir.path()).isEmpty());
    entry = Autostart::readEntry(dir.path());
    QVERIFY(!entry.exists);

    // Removing a missing entry is a no-op.
    QVERIFY(Autostart::removeEntry(dir.path()).isEmpty());
}

void AutostartTest::readMissing()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const auto entry = Autostart::readEntry(dir.path());
    QVERIFY(!entry.exists);
    QVERIFY(!entry.ours);
    QVERIFY(entry.enabled); // default
}

void AutostartTest::readDisabled()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("lattis.desktop"));

    // Hidden=true → disabled
    QFile f1(path);
    QVERIFY(f1.open(QIODevice::WriteOnly));
    f1.write("[Desktop Entry]\nType=Application\nHidden=true\nExec=/usr/bin/lattis\nX-Lattis-Autostart=true\n");
    f1.close();
    auto entry = Autostart::readEntry(dir.path());
    QVERIFY(entry.exists);
    QVERIFY(!entry.enabled);
    QVERIFY(entry.ours);

    // X-GNOME-Autostart-enabled=false → disabled
    QFile f2(path);
    QVERIFY(f2.open(QIODevice::WriteOnly));
    f2.write("[Desktop Entry]\nType=Application\nX-GNOME-Autostart-enabled=false\nExec=/usr/bin/lattis\nX-Lattis-Autostart=true\n");
    f2.close();
    entry = Autostart::readEntry(dir.path());
    QVERIFY(entry.exists);
    QVERIFY(!entry.enabled);
}

void AutostartTest::readForeign()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("lattis.desktop"));

    QFile f(path);
    QVERIFY(f.open(QIODevice::WriteOnly));
    f.write("[Desktop Entry]\nType=Application\nExec=/opt/lattis\n");
    f.close();

    const auto entry = Autostart::readEntry(dir.path());
    QVERIFY(entry.exists);
    QVERIFY(!entry.ours);
    QVERIFY(entry.enabled);
    QCOMPARE(entry.exec, QStringLiteral("/opt/lattis"));
}

void AutostartTest::staleExecDetected()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    // Write an entry with a stale path.
    QVERIFY(Autostart::writeEntry(dir.path(), "/old/path/lattis").isEmpty());
    const auto entry = Autostart::readEntry(dir.path());
    QVERIFY(entry.ours);
    QCOMPARE(entry.exec, QStringLiteral("/old/path/lattis"));

    // The expected exec for the new path differs.
    const QString newExec = Autostart::entryExec("/new/path/lattis");
    QVERIFY(entry.exec != newExec);

    // After rewriting with the new path, exec matches.
    QVERIFY(Autostart::writeEntry(dir.path(), "/new/path/lattis").isEmpty());
    const auto updated = Autostart::readEntry(dir.path());
    QCOMPARE(updated.exec, newExec);
}

void AutostartTest::writeFailures()
{
    if (::geteuid() == 0)
        QSKIP("root writes any folder");
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    // The folder can't be made: a file is where it goes.
    const QString blocked = dir.filePath(QStringLiteral("blocked"));
    QFile file(blocked);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.close();
    QCOMPARE(Autostart::writeEntry(blocked + QStringLiteral("/autostart"), "/usr/bin/lattis"),
             QStringLiteral("Could not create %1/autostart").arg(blocked));

    // A read-only folder: the entry that was there stays as it was, and no temporary file is left.
    const QString folder = dir.filePath(QStringLiteral("autostart"));
    QVERIFY(Autostart::writeEntry(folder, "/old/lattis").isEmpty());
    QVERIFY(QFile::setPermissions(folder, QFile::ReadOwner | QFile::ExeOwner));
    const auto reopen = qScopeGuard([&] { QFile::setPermissions(folder, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner); });
    const QString error = Autostart::writeEntry(folder, "/new/lattis");
    QVERIFY2(error.startsWith(QStringLiteral("Could not write %1/lattis.desktop: ").arg(folder)), qPrintable(error));
    QCOMPARE(Autostart::readEntry(folder).exec, QStringLiteral("/old/lattis"));
    QCOMPARE(QDir(folder).entryList(QDir::Files | QDir::Hidden), QStringList{"lattis.desktop"});
}

void AutostartTest::removeFailure()
{
    if (::geteuid() == 0)
        QSKIP("root writes any folder");
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString folder = dir.filePath(QStringLiteral("autostart"));
    QVERIFY(Autostart::writeEntry(folder, "/usr/bin/lattis").isEmpty());
    QVERIFY(QFile::setPermissions(folder, QFile::ReadOwner | QFile::ExeOwner));
    const auto reopen = qScopeGuard([&] { QFile::setPermissions(folder, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner); });
    QCOMPARE(Autostart::removeEntry(folder), QStringLiteral("Could not remove %1/lattis.desktop").arg(folder));
    QVERIFY(Autostart::readEntry(folder).exists);
}

void AutostartTest::currentProgramIsTheAppImage()
{
    QCOMPARE(Autostart::currentProgram(), QCoreApplication::applicationFilePath());
    qputenv("APPIMAGE", "/home/u/Lattis.AppImage");
    const auto unset = qScopeGuard([] { qunsetenv("APPIMAGE"); });
    // The file, not the program inside its mount, which is gone once it exits.
    QCOMPARE(Autostart::currentProgram(), QStringLiteral("/home/u/Lattis.AppImage"));
}

QTEST_MAIN(AutostartTest)
#include "autostart_test.moc"
