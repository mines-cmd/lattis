#include "autostart.h"

#include <QFile>
#include <QTemporaryDir>
#include <QTest>

class AutostartTest : public QObject
{
    Q_OBJECT

private slots:
    void entryExec_data();
    void entryExec();
    void readWriteRemove();
    void readMissing();
    void readDisabled();
    void readForeign();
    void staleExecDetected();
};

void AutostartTest::entryExec_data()
{
    QTest::addColumn<QString>("program");
    QTest::addColumn<QString>("dataDir");
    QTest::addColumn<QString>("expected");

    QTest::newRow("plain program") << "/usr/bin/lattis" << QString() << "/usr/bin/lattis";
    QTest::newRow("program with spaces") << "/home/user/my apps/lattis" << QString()
                                         << "\"/home/user/my apps/lattis\"";
    QTest::newRow("program with percent") << "/opt/la%ttis" << QString() << "/opt/la%ttis";
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

QTEST_MAIN(AutostartTest)
#include "autostart_test.moc"
