#include "windowstate.h"

#include <QFile>
#include <QGuiApplication>
#include <QScreen>
#include <QTemporaryDir>
#include <QTest>

// Window sizes remembered between starts, on the offscreen platform.
class WindowStateTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void defaultsWithoutAFile();
    void sizeRemembered();
    void maximizedKeepsTheNormalSize();
    void minimizedChangesNothing();
    void damagedFileIgnored();
    void tilingDesktopsCantMinimize();

private:
    QString stateFile(const QTemporaryDir &dir) const { return dir.filePath(QStringLiteral("windows-key.toml")); }
};

void WindowStateTest::initTestCase()
{
    // canMinimize() reads it once.
    qputenv("XDG_CURRENT_DESKTOP", "sway:wlroots");
}

void WindowStateTest::defaultsWithoutAFile()
{
    QTemporaryDir dir;
    WindowState state(dir.path(), QStringLiteral("key"));
    QCOMPARE(state.width(QStringLiteral("main"), 900), 900);
    QCOMPARE(state.height(QStringLiteral("main"), 640), 640);
    QVERIFY(!state.maximized(QStringLiteral("main")));
    QVERIFY(!WindowState::fillsScreen(nullptr));
}

void WindowStateTest::sizeRemembered()
{
    QTemporaryDir dir;
    {
        WindowState state(dir.path(), QStringLiteral("key"));
        QWindow window;
        window.resize(500, 400);
        window.show();
        state.track(&window, QStringLiteral("settings"));
        window.resize(520, 410);
        // Written once the resizing settles.
        QTRY_VERIFY(QFile::exists(stateFile(dir)));
    }
    WindowState state(dir.path(), QStringLiteral("key"));
    QCOMPARE(state.width(QStringLiteral("settings"), 900), 520);
    QCOMPARE(state.height(QStringLiteral("settings"), 640), 410);
    QVERIFY(!state.maximized(QStringLiteral("settings")));
    // Another data dir has its own sizes.
    WindowState other(dir.path(), QStringLiteral("other"));
    QCOMPARE(other.width(QStringLiteral("settings"), 900), 900);
}

void WindowStateTest::maximizedKeepsTheNormalSize()
{
    QTemporaryDir dir;
    {
        WindowState state(dir.path(), QStringLiteral("key"));
        QWindow window;
        window.resize(500, 400);
        window.show();
        state.track(&window, QStringLiteral("main"));
        window.resize(510, 400);
        QTRY_VERIFY(QFile::exists(stateFile(dir)));
        QFile::remove(stateFile(dir));

        window.setGeometry(window.screen()->availableGeometry());
        window.setWindowStates(Qt::WindowMaximized);
        QVERIFY(WindowState::fillsScreen(&window));
        QTRY_VERIFY(QFile::exists(stateFile(dir)));
    }
    WindowState state(dir.path(), QStringLiteral("key"));
    QVERIFY(state.maximized(QStringLiteral("main")));
    QCOMPARE(state.width(QStringLiteral("main"), 900), 510);
}

void WindowStateTest::minimizedChangesNothing()
{
    QTemporaryDir dir;
    {
        WindowState state(dir.path(), QStringLiteral("key"));
        QWindow window;
        window.resize(500, 400);
        window.show();
        state.track(&window, QStringLiteral("main"));
        window.resize(530, 400);
        QTRY_VERIFY(QFile::exists(stateFile(dir)));
        QFile::remove(stateFile(dir));

        window.setWindowStates(Qt::WindowMinimized);
        window.resize(300, 200);
        QTRY_VERIFY(QFile::exists(stateFile(dir)));
    }
    WindowState state(dir.path(), QStringLiteral("key"));
    QCOMPARE(state.width(QStringLiteral("main"), 900), 530);
}

void WindowStateTest::damagedFileIgnored()
{
    QTemporaryDir dir;
    QFile file(stateFile(dir));
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("[main\nwidth = 700\n");
    file.close();
    WindowState state(dir.path(), QStringLiteral("key"));
    QCOMPARE(state.width(QStringLiteral("main"), 900), 900);

    // Sizes that make no sense are left out; the rest is kept.
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
    file.write("main = 3\n[settings]\nwidth = -5\nheight = 300\nmaximized = true\n");
    file.close();
    WindowState again(dir.path(), QStringLiteral("key"));
    QCOMPARE(again.width(QStringLiteral("settings"), 900), 900);
    QVERIFY(again.maximized(QStringLiteral("settings")));
}

void WindowStateTest::tilingDesktopsCantMinimize()
{
    QVERIFY(!WindowState::canMinimize());
}

QTEST_MAIN(WindowStateTest)

#include "windowstate_test.moc"
