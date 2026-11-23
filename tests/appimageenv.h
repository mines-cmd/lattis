#pragma once

#include <QByteArray>
#include <QFile>
#include <QString>
#include <QTest>

// Makes this process look like it runs inside an AppImage, for tests of the programs Lattis starts: each fake
// program writes its environment to a file (dumpScript), and noBundleIn() fails when the bundle's variables or
// folders reached it.
class AppImageEnv
{
public:
    static constexpr const char *kMount = "/tmp/.mount_lattistest";

    AppImageEnv()
    {
        set("APPDIR", kMount);
        set("APPIMAGE", "/home/u/Lattis.AppImage");
        set("ARGV0", "./Lattis.AppImage");
        set("OWD", "/home/u");
        set("LD_LIBRARY_PATH", QByteArray(kMount) + "/usr/lib:/opt/keep/lib");
        set("QT_PLUGIN_PATH", QByteArray(kMount) + "/usr/plugins");
        set("LATTIS_TEST_KEEP", "1");
    }

    ~AppImageEnv()
    {
        for (const QByteArray &name : m_names)
            qunsetenv(name.constData());
    }

    AppImageEnv(const AppImageEnv &) = delete;
    AppImageEnv &operator=(const AppImageEnv &) = delete;

    static QByteArray dumpScript(const QString &dumpPath, const QByteArray &extra = {})
    {
        return "#!/bin/sh\n" + extra + "env > '" + dumpPath.toUtf8() + ".tmp'\nmv '" + dumpPath.toUtf8() + ".tmp' '"
               + dumpPath.toUtf8() + "'\n";
    }

    static void noBundleIn(const QString &dumpPath)
    {
        QFile file(dumpPath);
        QVERIFY2(file.open(QIODevice::ReadOnly), qPrintable(dumpPath));
        const QByteArray text = file.readAll();
        QVERIFY2(text.contains("LATTIS_TEST_KEEP=1"), text.constData());
        QVERIFY2(text.contains("LD_LIBRARY_PATH=/opt/keep/lib\n"), text.constData());
        for (const char *leak : {"APPDIR=", "APPIMAGE=", "ARGV0=", "OWD=", "QT_PLUGIN_PATH=", ".mount_lattistest"})
            QVERIFY2(!text.contains(leak), text.constData());
    }

private:
    void set(const char *name, const QByteArray &value)
    {
        qputenv(name, value);
        m_names.append(name);
    }

    QList<QByteArray> m_names;
};
